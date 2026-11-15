# ManualContext：并发、寿命与性能合同

本文件描述交付代码实际实现的合同，不是“以后增加”的设计清单。

## 1. 抽象边界

公开命名空间为 `lux::cxx::execution`。唯一执行资源是 `ManualContext`。
Scheduler 是借用句柄；Sender 是惰性描述；operation state 由 `connect` 的调用方或 stdexec 启动设施拥有。
Context 不拥有线程、不自动创建 Scope、不保管 TaskId，不实现 IO、计时器、HTTP、GPU 或业务消息。

通用性通过 stdexec 协议获得，不通过 `Server<IO,Handler,...>`、policy 参数矩阵或虚函数继承获得。

## 2. 公开类型与成员归属

| 类型 | 成员/职责 | 寿命及线程合同 |
|---|---|---|
| `ManualContextConfig` | `initial_queue_capacity` | 纯值、trivial、standard-layout；只是队列预分配提示，不是严格容量 |
| `Wakeup` | `context` + `notify` | 创建时注入的运行依赖，不属于配置；目标活到所有调用退出 |
| `ManualContext` | queue、consumer token、Wakeup、ContextState、消费重入标记 | 不可复制移动；不拥有线程；一个消费者，多个生产者 |
| `ManualContext::Scheduler` | Context 指针、可选 Producer token 指针、ScheduleMode | 小值、非拥有；模式和 producer 身份参与相等比较 |
| `ManualContext::Producer` | Context 引用、moodycamel ProducerToken | 可选优化；不可移动；通过同一 token 的 start 不得并发或重入 |
| `ScheduleSender`（私有） | Scheduler | 惰性；不分配、不接纳工作 |
| `ScheduleOperation<R>`（私有） | 节点、Context 借用、producer 借用、模式、具体 Receiver | 不可移动；start 一次；终态前不能销毁 |
| `ContextState`（私有） | gate 原子、pending 原子 | 只维护调度准入、发布者尾部和已接纳节点；不追踪业务任务 |

没有 `std::function`、`shared_ptr`、`optional`、线程 ID、独立任务表或协程包装类。
模板仅用于实际 Receiver 的类型。Queue 没有公开 policy 参数。

## 3. API

```cpp
ManualContext(ManualContextConfig = default_manual_context_config(), Wakeup = {});
Scheduler get_scheduler(ScheduleMode = ScheduleMode::cancellable) noexcept;
std::size_t run_ready(std::size_t budget) noexcept;
bool close() noexcept;
bool is_closed() const noexcept;
std::size_t pending() const noexcept;
bool is_idle() const noexcept;

ManualContext::Producer producer(context);
auto producer_scheduler = producer.get_scheduler();
```

所有 Scheduler 值均指向真实 Context，没有“默认空 Scheduler”。
`get_scheduler` 不启动操作；`connect` 只构造接收状态；`start` 才决定是否准入。

## 4. 完成语义

ScheduleSender 的完成签名是：

```cpp
set_value_t()
set_stopped_t()
set_error_t(ScheduleError)
```

- Context 开放时接纳的节点，value/stopped 均由 `run_ready` 的调用线程交付。
- 即使 start 前已经请求停止，也仍入队，由消费者交付 stopped，不在生产者线程抢先销毁节点。
- cancellable 模式在出队完成点读取 stop token；与读取并发的停止请求可以发生在这个点之后，此时 value 合法。
- delivery 模式只忽略本次 schedule 的 stop token。它不能把上游 stopped 变为 value；需要时先用 stdexec 的 upon_stopped/upon_error 形成业务 Outcome，再迁移并处理终态。
- close 后开始的节点以 `ScheduleError::context_closed` 在 start 所在线程立即结束。
- delivery 不绕过 closed；必须让 Context 开放到所有必要回程完成。
- 因而 Env 仅承诺 value/stopped 的 completion scheduler，不谎报 error 也在消费者线程。
- 入队分配失败为 fail-fast，绝不静默丢弃节点。不将队列配置容量冒充可恢复业务背压。

每个 operation 的 start 仅能调用一次；这是 stdexec 操作合同，不增加 started/finished 原子重复追踪。
取消不会删除一个尚在队列中的 operation。销毁已 start 但尚未完成的 operation 是调用者违约。

## 5. 发布者尾部与并发关闭证明

`gate_` 是一个 `atomic<size_t>`：最高位表示关闭，其余位是正在使用 Context 的发布者数。
`try_enter_publisher()` 用 CAS 完成“确认开放 + 增加计数”；close 用 CAS 完成“封闭 + 为关闭通知计入一名发布者”。
两者的线性化点位于同一个原子变量，避免“先读 closed、再加另一个计数器”的竞态。

接纳路径：

```text
CAS 获得发布者资格
    → pending++
    → queue.enqueue(node)
    → wakeup（若提供）
    → gate--，最后一次访问 Context
```

pending 必须在发布节点前增加：消费者可以在 enqueue 尚未返回时完成节点并销毁它。
发布成功后，发布者不再解引用 node 或 operation。

消费路径：

```text
取出至多 64 个节点
    → 每个节点调用 complete
    → 不再访问该节点
    → 所有回调返回后 pending -= 本批数量
```

pending 保守地涵盖整批已取出的节点及其回调。因此回调内部不能因为自己的 value 已产生就宣称 Context 已空。

`gate--` 在通知回调返回之后执行，使用 acq_rel；它是该发布调用最后一次访问 Context 的操作。
消费者即使已处理节点，仍能通过 gate 发现该生产者尚在通知尾部。
close 自己的通知同样被计数，不出现 close 回调尚在执行而 idle 已稳定的遗漏。

`is_idle()` **不是跨线程销毁授权**。开放时它只是一瞬间观察；关闭后仍不能排除其他调用者将来使用一个借用句柄。
必须同时保证：所有相关 Scope 已结束；不会再 start；所有 producer/close 调用已返回；run_ready 已返回；所有显式 Producer/借用句柄不再被使用。

## 6. 唤醒与休眠

不提供内部阻塞 run，也不提供 `queue.empty()` 驱动的等待。
Wakeup 必须是可保留的通知，或由宿主提供完整的 epoch/predicate 握手。单纯无谓词的 condition_variable.notify_one 不够。

安全的通用宿主模式：

```text
记录宿主通知 epoch
    → 处理外部事件
    → run_ready(budget)
    → 若仍有 pending 或刚完成工作，继续驱动
    → 再检查完成条件
    → 使用该 epoch 的谓词等待
```

同样，eventfd 的可读状态保留到消费，可直接融入 epoll。包内有真实 Linux 适配例子。

单次 concurrentqueue 出队失败可能只是当时未观察到数据。pending 非零时不要仅凭一次出队失败就无限等待下一次通知。
notify 在每次成功发布后调用，不做复杂的空→非空合并；OS 通知合并可由宿主 notifier 完成。

**最终发布者计数归零之后不再触碰 notifier。** 所以若仅等待 publisher tail 消失，不应在最后一次普通通知之后直接睡到下一个通知；该尾部本身不会制造额外事件。
正式退出优先同步相关生产者的返回。共享线程池不必整体销毁：Scope 已真正结束且不再有新调度时，可在退出阶段短暂观察 is_idle 等待已知、有限的发布尾部；这不是普通渲染循环的节流机制。

## 7. 正确关闭次序

1. 业务层停止新根任务准入。
2. 对可取消工作请求停止，但继续驱动 Context、GPU/IO 后端等真实完成源。
3. 让所有已接纳任务结束，必要终态通过 delivery 调度返回。
4. 结束/断开生产回调来源，并同步发布者尾部（示例中通过 pool/worker join）。
5. close Context，防止晚到的 start 成功。
6. 消费此前已经接纳的剩余节点，并保证消费者调用已返回。
7. 销毁 Producer、Scope/操作状态、Context；notifier 目标最后退出。

步骤 4/5/6 的具体编排可由宿主调整，但不能让 close 提前阻断必要回程，不能销毁未完成 operation，也不能把 context.close() 当成业务取消。

析构不执行用户代码、不阻塞、不自动 drain。如果仍有 pending/publisher 或正在 run_ready，直接 terminate 暴露合同破坏，而不是默默丢任务。
析构不要求先调用 close：若全部外部使用已经同步结束，空 Context 可以直接销毁。

DLL/插件代码寿命由宿主负责。操作状态释放不等于执行该 completion 的机器码已经退栈；插件须活到相关派发/发布调用返回。本组件不把 shared_ptr<Plugin> 塞进每个节点。

## 8. 性能合同

- 节点嵌入真实 operation state，队列只搬运 Node*。
- ConsumerToken 是 Context 值成员；显式 Producer 是可选优化。
- 每批出队至多 64，批量更新 pending，避免每个完成都执行一遍计数原子 RMW。
- Context 不创建线程，不在 run_ready 中 sleep/yield，不使用忙等同步锁。
- gate 和 pending 分离到不同的内部 64-byte 对齐块；不声称所有平台 cache line 都是 64 字节。
- run_ready 的预算是节点数；下游代码可同步运行较久，没有时间片或抢占保证。
- enqueue 可以分配；显式 token 创建可以分配；默认隐式生产者首次使用可以分配。
- 稳态零分配需真实容量与复用测试。提供的 allocation_probe 只检查预热后的直接 connect/start/pump，不涵盖 spawn、协程或业务负载。
- 不保证跨生产者全局 FIFO。需要顺序的业务应使用 sequence、单 producer 或自身协议。
- 共用 CPU pool 的多个 worker 可能同时发布；此时使用默认 scheduler，不能随便共用一个显式 Producer token。
- 本组件不声称 wait-free、硬实时、全路径 lock-free，或固定百分比的性能提升。

## 9. 为什么不直接复用当前 AdmissionGate？

检查到 lux-cxx 的 AdmissionGate 使用独立 closed/in_flight 原子和 optional ticket。
本组件需要封闭与已接纳发布者计数的单原子线性化，以及覆盖 notifier 尾部，而不是普通的业务容量票据。
因此只有一个私有 ContextState，既不修改已有 AdmissionGate，也不额外公开一套 Admission/Lease 框架。
这不等于断言已有 AdmissionGate 在其原有合同下错误。

## 10. 与草案的差异

保留：唯一重对象、手动推进、stdexec 协议、concurrentqueue、嵌入节点、非拥有 Scheduler、共享外部线程池、C++20。
明确的生产决策：无队列退化实现；关闭返回 typed error；入队 OOM fail-fast；取消只在消费点；delivery 为同一 Scheduler 类型的显式模式；正式退出必须追踪 publisher tail。
示例采用 counting_scope/get_token/join，不再混用 async_scope 的接口。

未加入：通用 EventLoop/HTTP/TCP/Timer/GPU、自己的 Task/Future/Scope、同步 wait API、owner thread id、每个 operation 的 shared ownership。
