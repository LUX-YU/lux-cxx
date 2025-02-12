#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <cstring>
#include <type_traits>

namespace lux::cxx::dref
{
    template<typename T>
    concept TriviallySerializable = std::is_trivially_copyable_v<T>;

    class BinaryReader
    {
    public:
        BinaryReader(const std::byte* data, size_t size)
            : _data(data), _size(size), _pos(0)
        {
        }

        template<TriviallySerializable T>
        T readPod()
        {
            if (_pos + sizeof(T) > _size)
                throw std::runtime_error("BinaryReader out of range when reading POD");
            T val;
            std::memcpy(&val, _data + _pos, sizeof(T));
            _pos += sizeof(T);
            return val;
        }

        std::string readString()
        {
            uint64_t len = readPod<uint64_t>();
            if (_pos + len > _size)
                throw std::runtime_error("BinaryReader out of range when reading string");
            std::string s(reinterpret_cast<const char*>(_data + _pos), len);
            _pos += len;
            return s;
        }

    private:
        const std::byte* _data;
        size_t           _size;
        size_t           _pos;
    };

    struct LoadedObject
    {
        ObjectKind kind;
        void* objPtr;   // 真正的对象指针 (Type* or ClassDeclaration* ...)

        // 这里临时存放从文件读到的字段
        // （以便二阶段用来修复指针）
        ObjectRecord record;
    };

    static void readAllRecords(BinaryReader& br,
        std::vector<LoadedObject>& loadedObjects)
    {
        // 先读对象总数
        uint64_t count = br.readPod<uint64_t>();
        loadedObjects.reserve(count);
        // 预先创建 loadedObjects 元素
        for (uint64_t i = 0; i < count; ++i)
        {
            LoadedObject lo;
            lo.record.id = 0;
            lo.record.kind = ObjectKind::Invalid;
            lo.objPtr = nullptr;
            loadedObjects.push_back(std::move(lo));
        }

        // 逐个读取
        for (uint64_t i = 0; i < count; ++i)
        {
            // 先读 id
            std::size_t id = br.readPod<std::size_t>();
            // 再读 kind
            ObjectKind kind = br.readPod<ObjectKind>();

            // 在 loadedObjects 里找第 (id-1) 个位置
            // 注意：我们假设 id 是从1开始，顺序不一定跟 i+1 一致
            if (id == 0 || id > count)
                throw std::runtime_error("Invalid object ID in file.");

            auto& lo = loadedObjects[id - 1];
            lo.record.id = id;
            lo.record.kind = kind;

            // 根据kind创建对象
            switch (kind)
            {
            case ObjectKind::Type:
            {
                auto* t = new lm::Type();
                lo.objPtr = t;

                t->type_kind = br.readPod<lm::ETypeKind>();
                lo.record.type_name = br.readString();
                t->name = lo.record.type_name;

                t->is_const = br.readPod<bool>();
                t->is_volatile = br.readPod<bool>();
                t->size = br.readPod<int>();
                t->align = br.readPod<int>();

                lo.record.pointee_id = br.readPod<std::size_t>();
                lo.record.decl_id = br.readPod<std::size_t>();
                lo.record.elemtype_id = br.readPod<std::size_t>();
            }
            break;
            case ObjectKind::ClassDecl:
            {
                auto* c = new lm::ClassDeclaration();
                lo.objPtr = c;

                c->declaration_kind = br.readPod<lm::EDeclarationKind>();
                lo.record.classdecl_name = br.readString();
                lo.record.classdecl_fullq = br.readString();
                lo.record.classdecl_spelling = br.readString();
                c->name = lo.record.classdecl_name;
                c->full_qualified_name = lo.record.classdecl_fullq;
                c->spelling = lo.record.classdecl_spelling;

                // baseclass_ids
                uint64_t bcount = br.readPod<uint64_t>();
                lo.record.baseclass_ids.resize(bcount);
                for (uint64_t bi = 0; bi < bcount; ++bi)
                {
                    lo.record.baseclass_ids[bi] = br.readPod<std::size_t>();
                }
            }
            break;
            case ObjectKind::EnumDecl:
            {
                auto* e = new lm::EnumerationDeclaration();
                lo.objPtr = e;

                e->declaration_kind = br.readPod<lm::EDeclarationKind>();
                lo.record.enumdecl_name = br.readString();
                lo.record.enumdecl_fullq = br.readString();
                lo.record.enumdecl_spelling = br.readString();
                e->name = lo.record.enumdecl_name;
                e->full_qualified_name = lo.record.enumdecl_fullq;
                e->spelling = lo.record.enumdecl_spelling;

                lo.record.is_scope = br.readPod<bool>();
                e->is_scope = lo.record.is_scope;

                lo.record.underlying_id = br.readPod<std::size_t>();
            }
            break;
            default:
                // 其他类型不演示
                break;
            }

            // 记录到 loadedObjects
            lo.kind = kind;
            loadedObjects[id - 1] = lo;
        }
    }
}

namespace lux::cxx::dref
{
    static void fixPointerReferences(std::vector<LoadedObject>& loadedObjects)
    {
        auto getPtrByID = [&](std::size_t id) -> void*
            {
                if (id == 0) return nullptr;
                if (id > loadedObjects.size())
                    throw std::runtime_error("Pointer fix: invalid reference ID");
                return loadedObjects[id - 1].objPtr;
            };

        for (auto& lo : loadedObjects)
        {
            switch (lo.kind)
            {
            case ObjectKind::Type:
            {
                auto* t = static_cast<lm::Type*>(lo.objPtr);
                // pointee_id
                if (lo.record.pointee_id != 0)
                {
                    t->pointee_type = static_cast<lm::Type*>(
                        getPtrByID(lo.record.pointee_id));
                }
                // decl_id => 可能指向 ClassDecl 或 EnumDecl
                if (lo.record.decl_id != 0)
                {
                    auto* declPtr = getPtrByID(lo.record.decl_id);
                    // 我们这里假设是ClassDecl或EnumDecl
                    // 真实情况可能还要判断类型
                    t->declaration = static_cast<lm::Declaration*>(declPtr);
                }
                // elemtype_id
                if (lo.record.elemtype_id != 0)
                {
                    t->element_type = static_cast<lm::Type*>(
                        getPtrByID(lo.record.elemtype_id));
                }
            }
            break;
            case ObjectKind::ClassDecl:
            {
                auto* c = static_cast<lm::ClassDeclaration*>(lo.objPtr);
                // baseclass_ids
                c->base_class_decls.clear();
                for (auto baseID : lo.record.baseclass_ids)
                {
                    if (baseID == 0)
                    {
                        c->base_class_decls.push_back(nullptr);
                        continue;
                    }
                    auto* basePtr = static_cast<lm::ClassDeclaration*>(
                        getPtrByID(baseID));
                    c->base_class_decls.push_back(basePtr);
                }
            }
            break;
            case ObjectKind::EnumDecl:
            {
                auto* e = static_cast<lm::EnumerationDeclaration*>(lo.objPtr);
                // underlying_id
                if (lo.record.underlying_id != 0)
                {
                    e->underlying_type = static_cast<lm::Type*>(
                        getPtrByID(lo.record.underlying_id));
                }
            }
            break;
            default:
                // ...
                break;
            }
        }
    }
}

namespace lux::cxx::dref
{
    static void rebuildMetaUnitData(MetaUnitData& data,
        const std::vector<LoadedObject>& loadedObjects)
    {
        // 清空旧容器
        data.type_list.clear();
        data.marked_decl_lists.class_decl_list.clear();
        data.marked_decl_lists.enum_decl_list.clear();
        data.unmarked_decl_lists.class_decl_list.clear();
        data.unmarked_decl_lists.enum_decl_list.clear();

        // 简化：全部塞到 "marked_decl_lists" 里 
        // 或者你可以根据原本信息做区分
        for (auto& lo : loadedObjects)
        {
            switch (lo.kind)
            {
            case ObjectKind::Type:
            {
                auto* t = static_cast<lm::Type*>(lo.objPtr);
                data.type_list.push_back(*t);
                // 这里 push_back 会拷贝对象，导致原指针失效
                // 若想保留原指针，需要改用指针容器(比如 std::vector<lm::Type*>)
            }
            break;
            case ObjectKind::ClassDecl:
            {
                auto* c = static_cast<lm::ClassDeclaration*>(lo.objPtr);
                data.marked_decl_lists.class_decl_list.push_back(*c);
            }
            break;
            case ObjectKind::EnumDecl:
            {
                auto* e = static_cast<lm::EnumerationDeclaration*>(lo.objPtr);
                data.marked_decl_lists.enum_decl_list.push_back(*e);
            }
            break;
            default:
                break;
            }
        }

        // 注意：上面是 "拷贝" 到 deque 里，反而把 new 出的对象浪费了
        // 若要真正复用对象，需要让 "type_list" / "class_decl_list" 储存指针，
        // 或者在 fixPointerReferences 之后再 move 构造到容器，这就看你具体设计。

        // 另外，这里不会自动销毁 new 出的对象。你需要自己管理内存。
        // 一个常见做法：直接在 data 里存 std::vector<std::unique_ptr<Type>>，
        // 这样反序列化完就能把 new 出来的对象放进 unique_ptr，避免泄漏。
    }

    MetaUnit MetaUnit::deserializeBinary(const std::byte* data, size_t size)
    {
        BinaryReader br(data, size);

        // 1) 先读 _id, _name, _version
        std::size_t id = br.readPod<std::size_t>();
        std::string name = br.readString();
        std::string version = br.readString();

        // 2) 读取所有 ObjectRecord => 分配对象 => 存到 loadedObjects
        std::vector<LoadedObject> loadedObjects;
        readAllRecords(br, loadedObjects);

        // 3) 第二阶段：指针修复
        fixPointerReferences(loadedObjects);

        // 4) 构造 MetaUnitData & MetaUnitImpl
        auto dataPtr = std::make_unique<MetaUnitData>();
        rebuildMetaUnitData(*dataPtr, loadedObjects);

        // 5) 创建新的 impl
        auto impl = std::make_unique<MetaUnitImpl>(std::move(dataPtr), name, version);
        impl->_id = id;  // 恢复

        // 6) 返回 MetaUnit
        return MetaUnit(std::move(impl));
    }
}
