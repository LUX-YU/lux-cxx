# lux-cxx 基础能力扩充迁移说明

本文记录本轮实际落地的 API、兼容边界、持久化规则和下游迁移建议。最低语言标准为 C++20；C++23 构建使用标准库中已经可用的设施。

## 明确不包含的能力

- 不引入 UUID 类型、解析器、适配器或第三方 UUID 依赖。`lux-engine`、`lux-robotics` 和 `lux-communication` 中现有 UUID 保持原状。
- 不引入自定义 Contract 类型、宏或全局失败处理器，避免与标准 C++ Contract 设施形成重名和语义冲突。内部不变量继续使用普通 `assert`。
- 不引入第二套 `Expected`。`lux/cxx/compile_time/expected.hpp` 及外部 `expected_impl.hpp` 保持原有小写 API：C++23 且标准库可用时别名到 `std::expected`，C++20 使用现有 `tl::expected`。
- 不引入 `inplace_move_only_function`、`MoveOnlyFunction`、`FunctionRef` 或 `ScopeExit` 等重复类型。
- 不引入统一 `Channel`、第二套线程池、调度器、通用无锁 MPMC 队列、动态库加载器或全局日志系统。

## 命名分类

模仿标准库或预计由标准库提供的设施采用小写下划线：

- `expected`、`unexpected`、`move_only_function`
- `function_ref`、`scope_exit`
- traits、concept 和标准容器式嵌套类型，例如 `enable_enum_flags`、`key_type`、`value_type`、`allocator_type`

lux 专属领域类型和大型组件采用大驼峰：

- `Delegate`、`FixedText`、`EnumFlags`
- `StrongId`、`StableNameIdView`、`StableNameId`、`SchemaVersion`
- `SharedBytes`、`BinaryReader`、`Timestamp`、`Quantity`
- `SlotMap`、`StableSlotMap`、`AdmissionGate`、`DiagnosticRecord`
- `AbiFingerprint`、`reflection::ir::MetaUnit`

标准兼容接口保留标准拼写，例如 `has_value`、`value_or`、`use_count`。lux 专属成员函数使用小写驼峰，例如 `isValid`、`tryEmplace`、`waitPush`。

## Core 与 Compile Time

### Callable

`lux/cxx/core/move_only_function.hpp` 中的 `move_only_function<Signature>` 是唯一拥有型 move-only callable：

- 公开 ABI 常量 `inplace_size == 32` 和 `inplace_alignment == alignof(std::max_align_t)`。
- 小 callable 原地存储；容量过大、过对齐或移动构造可能抛异常的 callable 使用堆存储。
- `move_only_function<Sig>::stores_inplace<F>` 与实际选择条件完全相同。严格无分配路径必须对具体 `F` 使用 `static_assert`。
- 支持空状态、移动、重置、交换和自移动保护；复制被删除。

`inplace_move_only_function` 已删除且没有兼容别名。`function_ref<Signature>` 是两个指针大小的借用视图，不接受 callable 临时对象；调用者负责保证被引用对象的生命周期。`Delegate<Signature>` 以最多两个指针保存自由函数或编译期成员函数绑定，不分配内存。`scope_exit<Callable>` 遵循标准式命名。

### 值类型与算术

- `FixedText<N, Char>`：固定容量、截断可检测、constexpr 构造/赋值/追加/比较。
- `EnumFlags<E>`：仅在 `enable_enum_flags<E>` 显式启用后参与位运算。
- `StrongId<Tag, Rep, InvalidValue>`：与 `Rep` 等尺寸，不提供隐式整数转换和算术。
- `StableNameIdView<Tag, Hash>` 与 `StableNameId<Tag, Hash, Allocator>`：默认冻结为 64 位 FNV-1a；哈希相等仍同时比较文本以处理碰撞。
- `SchemaVersion`、`SchemaId<Tag>`、`SchemaIdView<Tag>`。
- `checkedAdd`、`checkedSub`、`checkedMul`、`checkedNarrow`、`saturatingAdd`、`alignUpChecked` 均为 constexpr，可恢复错误返回现有 `expected`。

`TypeToken`/`typeToken<T>()` 封装编译器类型名和其哈希，只允许同一进程、同一构建内使用。禁止写入文件、网络协议、schema、插件协议或 ABI 指纹。

## Algorithm、Memory 与 Binary

### SHA-256 与内容标识

`Sha256Digest` 是严格 32 字节值类型，支持 constexpr 比较、十六进制解析和写入调用方提供的 64 字节字符区。`Sha256` 支持增量输入、字节 span、字符串和常量求值。`ContentId<Tag, Digest>` 默认使用 `Sha256Digest`，Tag 防止不同内容域误用。

### SharedBytes 与 PMR

`SharedBytes<Allocator>` 只用于明确的跨线程或跨生命周期共享：

- `copyOf` 显式复制并取得所有权。
- `fromOwner` 使用 aliasing `shared_ptr` 显式接管外部 owner 的生命周期。
- `subspan` 创建共享切片。
- 热路径只读取 `std::span<const std::byte>`，不要反复复制 `SharedBytes` 以免引用计数进入循环。

新增 `CountingMemoryResource`、`BudgetMemoryResource`、`FailingMemoryResource`，均装饰标准 PMR 上游资源；没有重复实现标准资源。

### Binary 规范

- `BasicBinaryReader<EndianPolicy>` 借用输入 span，错误为粘滞状态，不分配。
- `BasicBinarySpanWriter<EndianPolicy>` 写入调用方缓冲区，不分配。
- `BasicBinaryVectorWriter<Container, EndianPolicy>` 是显式拥有型适配器。
- 默认别名为小端 `BinaryReader`、`BinarySpanWriter`、`BinaryVectorWriter`；同时提供大端 reader/span writer。
- varint 必须是最短编码；有符号 varint 使用 zigzag；布尔值只接受 0/1。
- 默认 `EFloatingPointPolicy::CANONICAL`：所有 NaN 写为固定 quiet-NaN，负零写为正零；读取拒绝非规范 NaN 和负零。`PRESERVE_BITS` 显式保留原始位。
- `EScalarKind : std::uint8_t` 编号已冻结；`ScalarSchema` 为三字节结构，不使用编译器类型哈希。

整数、浮点位处理、varint、zigzag、端序、span 读写均有 C++20 constexpr 路径且不依赖 constexpr `memcpy`。从 `std::byte` span 建立 `std::string_view` 仍受 C++20 `reinterpret_cast` 常量求值规则限制，因此 `readString` 是运行时路径。

## Time 与 Units

- `Timestamp<Domain, Duration, Rep>` 与 `Rep` 等尺寸；不同 Domain 之间不能隐式比较。
- 时间域包括 `SteadyTimeDomain`、`SystemTimeDomain`、`SensorTimeDomain<Tag>`、`RemoteTimeDomain<Tag>`。
- `ClockMapping<From, To, ScaleRep, Duration, Rep>` 保存双原点、比例、不确定度和修订号，映射时验证比例和结果范围。
- `ManualClock<Domain, Duration, Rep>` 用于确定性测试。
- `Quantity<Tag, Rep, Ratio>` 提供显式单位值；`quantityCast<Target>` 拒绝范围外和有损整数转换。
- 默认领域别名包括 `ByteCount`、`KibibyteCount`、`Frequency`、`Kilohertz`、`Angle`。

## Container

`SlotMap<Value, Tag, IndexType, GenerationType, Allocator>` 现在 allocator-aware，并支持仅移动值、异常安全插入、代际退休和 `tryEmplace`。公共嵌套类型使用 `key_type`、`value_type`、`size_type`、`index_type`、`generation_type`；有效性查询为 `isValid`。

`StableSlotMap<Value, Tag, Aux, BlockSize, Allocator, Index, Generation>` 使用块级分配：

- Value 在每个块内连续存放，元数据与 Value 分离；没有逐元素 `unique_ptr`。
- 代际空闲链表提供陈旧 handle 检测；代际达到最大值时槽位永久退休。
- `dense_indices_` 保存确定迭代顺序；擦除使用 swap-and-pop 更新稠密位置。
- Value 地址在 map 增长和其他元素擦除期间保持稳定。
- `Aux` 为每个槽位提供可替换的轻量附加数据。

`SmallVector<Value, InlineCapacity, Allocator>` 的堆回退使用 allocator，并覆盖过对齐值、仅移动值、异常安全和 allocator 传播。`ObjectPool`、`pool_ptr`、`intrusive_ptr` 与 `intrusive_ref_counter` 保持原有小写标准式命名。

## Concurrent

队列的单元素布尔接口已经硬切换：

| 旧接口 | 新接口 |
|---|---|
| `push(value)` | `waitPush(value)`，返回 `EQueuePushResult` |
| `push(value, timeout)` | `waitPush(value, timeout[, stop_token])` |
| `pop(out)` | `waitPop(out)`，返回 `EQueuePopResult` |
| `pop(out, timeout)` | `waitPop(out, timeout[, stop_token])` |
| `try_push(value)` | `tryPush(value)` |
| `try_pop(out)` | `tryPop(out)` |

结果区分 `ACCEPTED`/`FULL`/`CLOSED`/`TIMEOUT`/`CANCELLED` 和 `VALUE`/`EMPTY`/`CLOSED_AND_DRAINED`/`TIMEOUT`/`CANCELLED`。`EQueueState` 区分 `OPEN`、`CLOSED`、`DRAINED`。批量接口暂时保留原有名称和返回方式，以避免在没有逐条失败位置定义前引入含糊的新格式。

`AdmissionGate<Counter>` 与指针大小的移动型 `AdmissionTicket` 管理关闭和在途生产者，不分配；Ticket 必须早于 Gate 销毁。`BudgetGate<ItemCounter, ByteCounter>` 以移动型 `BudgetReservation` 同时管理条目/字节预算，第二项申请失败时回滚第一项，不隐式阻塞。

`ThreadPool` 的任务存储改用 Core 中唯一的 `move_only_function<void()>`。常见任务包装通过 `stores_inplace` 编译期检查 SBO 覆盖。

## Diagnostic 与 ABI

`BasicDiagnosticRecord<MessageCapacity, Clock, Category, Code>` 是固定容量值类型，包含严重级别、稳定分类、错误码、域时间戳、序列、进程内线程号、`FixedText` 消息、截断标志和 `std::source_location`。默认 `DiagnosticRecord` 使用 192 字节消息和 steady clock。模块不提供 logger、sink 或后台线程。

ABI 模块提供：

- `SemanticVersion`
- 标准布局的 `AbiStringView`、`AbiByteView`
- 严格 32 字节 `AbiFingerprint`
- 配置期生成的只读 `AbiBuildInfo`

规范字段包括编译器及版本、目标、指针宽度、端序、标准库/CRT、异常、RTTI、sanitizer、Debug ABI 和 API 主版本；字段串使用固定顺序，再由 SHA-256 生成指纹。安装目录中的 `share/lux-cxx/lux-cxx-sdk.json` 同步记录版本、revision、dirty、目标、编译器、位宽、端序和 C++ 标准。C 边界只应复制 `AbiStringView`/`AbiByteView` 的布局定义，不得暴露 STL、异常、 callable 或 C++ 所有权类型。

## Reflection IR 与格式变化

新 IR 位于 `lux::cxx::reflection::ir`，避免在兼容期与旧 `reflection::MetaUnit` 重名：

- `MetaNodeId`、`MetaStringId` 是 32 位强类型索引。
- `MetaNodeRecord`、`MetaAttributeRecord`、字符串表和字符区均为连续数组。
- Builder 使用开放寻址的连续字符串驻留索引，不进行逐节点分配。
- `BasicMetaUnit<Storage>` 冻结所有权后只公开 span/view 查询。
- `MetaIrBinary.hpp` 定义魔数 `LXMI`、版本 2、最短 varint 字段和严格引用/数量/字符串字节上限；相同 IR 产生确定字节流。版本 1 直接拒绝，不提供双读。

Clang parser 的公开结果和 generator 的输入已经切换为紧凑 `MetaUnit`；template JSON 只是 IR 内的一份无独立身份投影。generator 当前仍为既有模板回调重建私有 legacy view，旧虚继承 AST/JSON serializer 也仍作为过渡实现存在。完成所有模板与外部消费者迁移前不删除这些实现；未来 C++26 静态反射前端只能接入同一 Builder，不另建 IR。

## 头文件和符号迁移

| 旧位置/符号 | 新位置/符号 |
|---|---|
| `lux/cxx/compile_time/move_only_function.hpp` | `lux/cxx/core/move_only_function.hpp`，名称不变 |
| `inplace_move_only_function<Sig, ...>` | 删除；使用 `move_only_function<Sig>` 并 `static_assert(stores_inplace<F>)` |
| serialization 内部重复 `function_ref` 实现 | `lux/cxx/core/function_ref.hpp`；serialization 保留命名空间别名 |
| `SlotMap::key_t` | `SlotMap::key_type` |
| `SlotMap::is_valid` | `SlotMap::isValid` |
| `SlotKey::is_null` / `valid` | `SlotKey::isNull` / `isValid` |
| 队列单元素布尔接口 | 上文明确状态接口 |

没有 `Expected.hpp`、`Contract.hpp` 或 `Contract.cpp`；这些误建文件不属于 API。

## 所有权、分配和线程安全

- `function_ref`、`Delegate`、`FixedText`、`StableNameIdView`、Binary span reader/writer、`Timestamp` 和 `AdmissionTicket` 不分配。
- `StableNameId`、`SharedBytes::copyOf`、Binary vector writer、容器扩容和 MetaUnitBuilder 可以分配，并支持显式 allocator 的地方使用调用方 allocator。
- `SharedBytes` 的共享引用计数线程安全，但其指向字节是只读视图；不要由别名 owner 并发修改底层内存。
- Binary reader/writer、容器和 Builder 本身不提供内部同步。
- Admission/Budget 原语使用原子操作；移动 Ticket/Reservation 只允许单一所有者。
- `StableSlotMap` handle 可以跨增长保存，但 map 本身移动、销毁或对应值擦除后，旧地址/handle 规则按容器文档处理。

## 测试与性能结果

模块测试包含 constexpr/运行时双路径、极值与非法输入、模板实例、仅移动和过对齐类型、allocator、异常路径、并发关闭/排空/取消/超时、确定性二进制以及随机变异语料。新增公共头文件各自作为独立翻译单元编译。安装测试会在隔离前缀重新安装，再由独立项目执行 `find_package(lux-cxx COMPONENTS ...)`、编译和运行。

本地验收环境为 MSVC 19.44、Windows x64：

- C++20 Debug：全目标构建及 32/32 项模块/安装消费测试通过，使用 `tl::expected`。
- C++23 Debug：独立配置全目标构建及 32/32 项模块/安装消费测试通过，使用 `std::expected`。
- Reflection C++20 Debug 与 RelWithDebInfo：parser→紧凑 IR→generator/serialization 全链路构建通过，49/49 项注册测试通过；其中包含生成器、JSON/XML serialization、IR v2 的自包含、确定性往返、截断和随机变异测试。
- 安装消费：隔离安装、`find_package`、编译和执行通过。

RelWithDebInfo、100 万个 `Position` 的本地中位数微基准：

- `StableSlotMap` 插入约 22.55 ns/项。
- `vector<unique_ptr<Position>>` 逐元素堆分配基线约 33.61 ns/项。
- StableSlotMap 插入快约 33%，同时把约百万次逐元素分配降为块级分配；迭代约 1.94 ns/项，与该次基线 1.92 ns/项相当。

跨 GCC/Clang/MSVC、C++20/C++23、Debug/RelWithDebInfo 以及 ASan/UBSan/TSan 的矩阵定义在 `.github/workflows/ci.yml`。性能门禁需要 CI 保存历史基线后再启用“中位数回退超过 5% 阻止合并”；当前仓库没有可供可靠比较的历史工件，因此本文不虚构回退结论。

## 稳定等级和剩余限制

稳定：Core（除下述实验项）、SHA-256、`ContentId`、`SharedBytes`、PMR decorators、Binary、Time 基础值、Units、加固后的容器接口。

实验性：`AdmissionGate`/`BudgetGate`、`ClockMapping`、ABI 指纹字段集合、`reflection::ir`。它们不放入临时命名空间，但在至少一个下游迁移周期前允许兼容性调整。

当前限制：

- Reflection 的公开 parser/generator 传输已使用紧凑 IR，但 parser 内部仍构造 legacy AST，generator 仍重建私有 legacy template view；彻底删除旧 AST/serializer 仍需迁完全部模板与外部消费者。
- ABI sanitizer/Debug 字段来自配置期 flags；多配置生成器应为每个配置使用独立构建目录，避免共享一份 BuildInfo。
- Binary UTF-8 目前验证长度和边界，不执行 Unicode 规范化；协议层必须自行选择并冻结规范化形式。
- 批量队列接口尚未统一为结构化结果，待定义“部分成功”语义后再硬切换。

## 下游建议迁移顺序

1. `lux-engine`：先迁移 `move_only_function` 头文件、队列状态、`SlotMap` 命名和 Binary；随后用 `ContentId`/ABI 指纹保护缓存和插件边界。
2. `lux-communication`：迁移 Binary/ScalarSchema、Timestamp/ClockMapping、Admission/Budget；协议中的 UUID 保持不变。
3. `lux-robotics`：在 communication 时间域稳定后迁移 Sensor/Remote timestamp、单位量和 SharedBytes；再接入反射 IR 二进制元数据。
4. 三个下游完成一个周期的 golden、性能和 ABI 对照后，删除 parser/generator 内部的 legacy AST/template view 与旧 serializer，使紧凑 IR 成为唯一内存模型。

本轮没有直接修改任何下游仓库。
