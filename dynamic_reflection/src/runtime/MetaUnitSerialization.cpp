#include <lux/cxx/dref/runtime/MetaUnit.hpp>
#include <lux/cxx/dref/runtime/MetaUnitImpl.hpp>
#include <lux/cxx/lan_model/declaration.hpp>
#include <unordered_map>

// =============================
// 2) BinaryWriter (写入工具)
// =============================
namespace lux::cxx::dref
{
    class BinaryWriter
    {
    public:
        // 供外部直接访问底层buffer
        std::vector<std::byte> buffer;

        // 写入一个POD (TriviallyCopyable) 类型
        template<typename T>
        void writePod(const T& val)
        {
            static_assert(std::is_trivially_copyable_v<T>,
                          "writePod only supports trivially copyable types.");
            const auto* raw = reinterpret_cast<const std::byte*>(&val);
            buffer.insert(buffer.end(), raw, raw + sizeof(T));
        }

        // 写入std::string
        void writeString(const std::string& str)
        {
            // 先写长度(64位)
            uint64_t len = static_cast<uint64_t>(str.size());
            writePod(len);
            // 再写内容
            const auto* raw = reinterpret_cast<const std::byte*>(str.data());
            buffer.insert(buffer.end(), raw, raw + len);
        }

        // 写 std::vector<T> (元素需提供 serialize 函数)
        template<typename T, typename F>
        void writeVector(const std::vector<T>& vec, F&& elementSerializer)
        {
            // 先写size
            uint64_t sz = static_cast<uint64_t>(vec.size());
            writePod(sz);
            for (const auto& elem : vec)
            {
                elementSerializer(*this, elem);
            }
        }

        // 写 std::deque<T>
        template<typename T, typename F>
        void writeDeque(const std::deque<T>& dq, F&& elementSerializer)
        {
            uint64_t sz = static_cast<uint64_t>(dq.size());
            writePod(sz);
            for (const auto& elem : dq)
            {
                elementSerializer(*this, elem);
            }
        }

        // 写 std::unordered_map<Key, Value>
        template<typename K, typename V, typename KF, typename VF>
        void writeUnorderedMap(const std::unordered_map<K, V>& m,
                               KF&& keySerializer, VF&& valSerializer)
        {
            uint64_t sz = static_cast<uint64_t>(m.size());
            writePod(sz);
            for (const auto& kv : m)
            {
                keySerializer(*this, kv.first);
                valSerializer(*this, kv.second);
            }
        }

        // 写一个可空指针
        //    pointerSerializer: 形如  void(BinaryWriter&, const T*)
        template<typename T, typename F>
        void writePointer(const T* ptr, F&& pointerSerializer)
        {
            if (!ptr)
            {
                // 写个标志, false表示null
                writePod(bool(false));
                return;
            }
            // 非空
            writePod(bool(true));
            // 直接序列化指向的对象
            pointerSerializer(*this, *ptr);
        }
    };

} // namespace lux::cxx::dref

// =============================
// 3) 针对各结构的序列化函数
//    （在此完整写出）
// =============================
namespace lux::cxx::dref
{
    // 为了避免过多函数名冲突，这里统一用 serializeXxx(...) 命名
    // 并「内联」在头文件中以保证编译期可见。

    // 声明在前，以便相互调用
    inline void serializeType(BinaryWriter&, const lan_model::Type&);
    inline void serializeDeclarationBase(BinaryWriter&, const lan_model::Declaration&);
    inline void serializeMemberDeclaration(BinaryWriter&, const lan_model::MemberDeclaration&);
    inline void serializeFieldDeclaration(BinaryWriter&, const lan_model::FieldDeclaration&);
    inline void serializeParameterDeclaration(BinaryWriter&, const lan_model::ParameterDeclaration&);
    inline void serializeCallableDeclCommon(BinaryWriter&, const lan_model::CallableDeclCommon&);
    inline void serializeFunctionDeclaration(BinaryWriter&, const lan_model::FunctionDeclaration&);
    inline void serializeMemberFunctionDeclaration(BinaryWriter&, const lan_model::MemberFunctionDeclaration&);
    inline void serializeConstructorDeclaration(BinaryWriter&, const lan_model::ConstructorDeclaration&);
    inline void serializeDestructorDeclaration(BinaryWriter&, const lan_model::DestructorDeclaration&);
    inline void serializeClassDeclaration(BinaryWriter&, const lan_model::ClassDeclaration&);
    inline void serializeEnumerationDeclaration(BinaryWriter&, const lan_model::EnumerationDeclaration&);
    inline void serializeEnumerator(BinaryWriter&, const lan_model::Enumerator&);

    // =======================
    //  辅助: 序列化枚举类型
    // =======================
    inline void serializeETypeKind(BinaryWriter& bw, lan_model::ETypeKind kind)
    {
        // ETypeKind 是一个 enum class, 可直接 writePod
        bw.writePod(kind);
    }
    inline void serializeEDeclarationKind(BinaryWriter& bw, lan_model::EDeclarationKind kind)
    {
        bw.writePod(kind);
    }
    inline void serializeVisibility(BinaryWriter& bw, lan_model::Visibility vis)
    {
        bw.writePod(vis);
    }

    // =======================
    //  (1) Type
    // =======================
    inline void serializeType(BinaryWriter& bw, const lan_model::Type& t)
    {
        // 写自身字段
        serializeETypeKind(bw, t.type_kind);
        bw.writeString(t.name);
        bw.writePod(t.is_const);
        bw.writePod(t.is_volatile);
        bw.writePod(t.size);
        bw.writePod(t.align);

        // 写指针字段:
        //  (a) pointee_type
        bw.writePointer(t.pointee_type, [](BinaryWriter& wb, const lan_model::Type& pointee){
            serializeType(wb, pointee);
        });
        //  (b) declaration
        bw.writePointer(t.declaration, [](BinaryWriter& wb, const lan_model::Declaration& decl){
            // 我们需要根据decl的kind去序列化正确的派生类型
            // 但是声明结构太复杂…… 这里可直接调 serializeDeclarationBase + 具体派生
            // 为简化，这里直接用一个大switch，或者做一个inline函数:
            switch (decl.declaration_kind)
            {
            case lan_model::EDeclarationKind::CLASS:
                // 强转 & 序列化
                serializeClassDeclaration(wb, static_cast<const lan_model::ClassDeclaration&>(decl));
                break;
            case lan_model::EDeclarationKind::ENUMERATION:
                serializeEnumerationDeclaration(wb, static_cast<const lan_model::EnumerationDeclaration&>(decl));
                break;
            case lan_model::EDeclarationKind::FUNCTION:
                serializeFunctionDeclaration(wb, static_cast<const lan_model::FunctionDeclaration&>(decl));
                break;
            case lan_model::EDeclarationKind::MEMBER_DATA:
                serializeFieldDeclaration(wb, static_cast<const lan_model::FieldDeclaration&>(decl));
                break;
            case lan_model::EDeclarationKind::MEMBER_FUNCTION:
                serializeMemberFunctionDeclaration(wb, static_cast<const lan_model::MemberFunctionDeclaration&>(decl));
                break;
            case lan_model::EDeclarationKind::CONSTRUCTOR:
                serializeConstructorDeclaration(wb, static_cast<const lan_model::ConstructorDeclaration&>(decl));
                break;
            case lan_model::EDeclarationKind::DESTRUCTOR:
                serializeDestructorDeclaration(wb, static_cast<const lan_model::DestructorDeclaration&>(decl));
                break;
            case lan_model::EDeclarationKind::PARAMETER:
                serializeParameterDeclaration(wb, static_cast<const lan_model::ParameterDeclaration&>(decl));
                break;
            default:
                // 基础 or unknown
                serializeDeclarationBase(wb, decl);
                break;
            }
        });
        //  (c) element_type
        bw.writePointer(t.element_type, [](BinaryWriter& wb, const lan_model::Type& et){
            serializeType(wb, et);
        });
    }

    // =======================
    //  (2) Declaration 基类
    // =======================
    inline void serializeDeclarationBase(BinaryWriter& bw, const lan_model::Declaration& d)
    {
        serializeEDeclarationKind(bw, d.declaration_kind);

        // 指针 type
        bw.writePointer(d.type, [](BinaryWriter& wb, const lan_model::Type& t){
            serializeType(wb, t);
        });

        bw.writeString(d.name);
        bw.writeString(d.full_qualified_name);
        bw.writeString(d.spelling);
        bw.writeString(d.usr);
        bw.writeString(d.attribute);
    }

    // =======================
    //  (3) MemberDeclaration
    // =======================
    inline void serializeMemberDeclaration(BinaryWriter& bw, const lan_model::MemberDeclaration& md)
    {
        // 先序列化基类: Declaration
        serializeDeclarationBase(bw, md);

        // 再写自己的字段
        // class_declaration
        bw.writePointer(md.class_declaration, [](BinaryWriter& wb, const lan_model::ClassDeclaration& cd){
            serializeClassDeclaration(wb, cd);
        });
        // visibility
        serializeVisibility(bw, md.visibility);
    }

    // =======================
    //  (4) FieldDeclaration
    // =======================
    inline void serializeFieldDeclaration(BinaryWriter& bw, const lan_model::FieldDeclaration& fd)
    {
        // 先写 MemberDeclaration 基
        serializeMemberDeclaration(bw, fd);
        // 自己字段
        bw.writePod(fd.offset);
    }

    // =======================
    //  (5) ParameterDeclaration
    // =======================
    inline void serializeParameterDeclaration(BinaryWriter& bw, const lan_model::ParameterDeclaration& pd)
    {
        // 先写 Declaration 基
        serializeDeclarationBase(bw, pd);
        // 自己字段
        bw.writePod(pd.index);
    }

    // =======================
    //  (6) CallableDeclCommon
    // =======================
    inline void serializeCallableDeclCommon(BinaryWriter& bw, const lan_model::CallableDeclCommon& cdc)
    {
        // result_type
        bw.writePointer(cdc.result_type, [](BinaryWriter& wb, const lan_model::Type& t){
            serializeType(wb, t);
        });
        // mangling
        bw.writeString(cdc.mangling);
        // parameter_decls
        bw.writeVector(cdc.parameter_decls, [](BinaryWriter& wb, const lan_model::ParameterDeclaration& p){
            serializeParameterDeclaration(wb, p);
        });
    }

    // =======================
    //  (7) FunctionDeclaration
    // =======================
    inline void serializeFunctionDeclaration(BinaryWriter& bw, const lan_model::FunctionDeclaration& fd)
    {
        // 先写基类(Declaration)
        serializeDeclarationBase(bw, fd);
        // 再写 CallableDeclCommon
        serializeCallableDeclCommon(bw, fd);
    }

    // =======================
    //  (8) MemberFunctionDeclaration
    // =======================
    inline void serializeMemberFunctionDeclaration(BinaryWriter& bw, const lan_model::MemberFunctionDeclaration& mfd)
    {
        // MemberDeclaration
        serializeMemberDeclaration(bw, mfd);
        // CallableDeclCommon
        serializeCallableDeclCommon(bw, mfd);
        // 自己字段
        bw.writePod(mfd.is_static);
        bw.writePod(mfd.is_virtual);
    }

    // =======================
    //  (9) ConstructorDeclaration
    // =======================
    inline void serializeConstructorDeclaration(BinaryWriter& bw, const lan_model::ConstructorDeclaration& cd)
    {
        // MemberDeclaration
        serializeMemberDeclaration(bw, cd);
        // CallableDeclCommon
        serializeCallableDeclCommon(bw, cd);
    }

    // =======================
    //  (10) DestructorDeclaration
    // =======================
    inline void serializeDestructorDeclaration(BinaryWriter& bw, const lan_model::DestructorDeclaration& dd)
    {
        // MemberDeclaration
        serializeMemberDeclaration(bw, dd);
        // CallableDeclCommon
        serializeCallableDeclCommon(bw, dd);
        // 自己字段
        bw.writePod(dd.is_virtual);
    }

    // =======================
    //  (11) ClassDeclaration
    // =======================
    inline void serializeClassDeclaration(BinaryWriter& bw, const lan_model::ClassDeclaration& cd)
    {
        // 先写基类: Declaration
        serializeDeclarationBase(bw, cd);

        // 写 base_class_decls (vector<ClassDeclaration*>)
        bw.writeVector(cd.base_class_decls, [](BinaryWriter& wb, const lan_model::ClassDeclaration* basePtr){
            // 指针
            if (!basePtr)
            {
                wb.writePod(bool(false));
                return;
            }
            wb.writePod(bool(true));
            // 递归
            serializeClassDeclaration(wb, *basePtr);
        });

        // 写 constructor_decls
        bw.writeVector(
            cd.constructor_decls, 
            [](BinaryWriter& wb, const lan_model::ConstructorDeclaration& decl){
                serializeConstructorDeclaration(wb, decl);
            }
        );

        // 写 destructor_decl
        // 这里是一个对象，而非容器/指针
        serializeDestructorDeclaration(bw, cd.destructor_decl);

        // 写 field_decls
        bw.writeVector(cd.field_decls, [](BinaryWriter& wb, const lan_model::FieldDeclaration& fdecl){
            serializeFieldDeclaration(wb, fdecl);
        });

        // 写 method_decls
        bw.writeVector(cd.method_decls, [](BinaryWriter& wb, const lan_model::MemberFunctionDeclaration& mfdecl){
            serializeMemberFunctionDeclaration(wb, mfdecl);
        });

        // 写 static_method_decls
        bw.writeVector(cd.static_method_decls, [](BinaryWriter& wb, const lan_model::MemberFunctionDeclaration& smfdecl){
            serializeMemberFunctionDeclaration(wb, smfdecl);
        });
    }

    // =======================
    //  (12) EnumerationDeclaration
    // =======================
    inline void serializeEnumerator(BinaryWriter& bw, const lan_model::Enumerator& e)
    {
        bw.writeString(e.name);
        bw.writePod(e.value);
    }

    inline void serializeEnumerationDeclaration(BinaryWriter& bw, const lan_model::EnumerationDeclaration& ed)
    {
        // 基类: Declaration
        serializeDeclarationBase(bw, ed);

        // 自己字段
        // enumerators
        bw.writeVector(ed.enumerators, [](BinaryWriter& wb, const lan_model::Enumerator& enumer){
            serializeEnumerator(wb, enumer);
        });
        // is_scope
        bw.writePod(ed.is_scope);

        // underlying_type
        bw.writePointer(ed.underlying_type, [](BinaryWriter& wbx, const lan_model::Type& t){
            serializeType(wbx, t);
        });
    }

    // ======================
    // 4) MetaUnit 的序列化
    // ======================
    inline std::vector<std::byte> MetaUnit::serializeBinary(const MetaUnit& mu)
    {
        BinaryWriter bw;
        if (!mu._impl)
        {
            // 如果 _impl 是空，返回一个空buffer
            return bw.buffer;
        }

        const MetaUnitImpl& impl = *mu._impl;
        // 写 _id
        bw.writePod(impl._id);
        // 写 _name
        bw.writeString(impl._name);
        // 写 _version
        bw.writeString(impl._version);

        // 取得 data
        const auto& data = *(impl._data);

        // (1) 写 marked_decl_lists
        //     1.1) class_decl_list
        bw.writeDeque(data.marked_decl_lists.class_decl_list,
            [](BinaryWriter& w, const lan_model::ClassDeclaration& decl){
                serializeClassDeclaration(w, decl);
            }
        );
        //     1.2) function_decl_list
        bw.writeDeque(data.marked_decl_lists.function_decl_list,
            [](BinaryWriter& w, const lan_model::FunctionDeclaration& fdecl){
                serializeFunctionDeclaration(w, fdecl);
            }
        );
        //     1.3) enumeration_decl_list
        bw.writeDeque(data.marked_decl_lists.enumeration_decl_list,
            [](BinaryWriter& w, const lan_model::EnumerationDeclaration& edecl){
                serializeEnumerationDeclaration(w, edecl);
            }
        );

        // (2) 写 unmarked_decl_lists
        //     2.1) class_decl_list
        bw.writeDeque(data.unmarked_decl_lists.class_decl_list,
            [](BinaryWriter& w, const lan_model::ClassDeclaration& decl){
                serializeClassDeclaration(w, decl);
            }
        );
        //     2.2) function_decl_list
        bw.writeDeque(data.unmarked_decl_lists.function_decl_list,
            [](BinaryWriter& w, const lan_model::FunctionDeclaration& fdecl){
                serializeFunctionDeclaration(w, fdecl);
            }
        );
        //     2.3) enumeration_decl_list
        bw.writeDeque(data.unmarked_decl_lists.enumeration_decl_list,
            [](BinaryWriter& w, const lan_model::EnumerationDeclaration& edecl){
                serializeEnumerationDeclaration(w, edecl);
            }
        );

        // (3) 写 decl_map
        //     key: size_t, value: Declaration*
        bw.writeUnorderedMap(
            data.decl_map,
            [](BinaryWriter& w, std::size_t key){
                w.writePod(key);
            },
            [](BinaryWriter& w, const lan_model::Declaration* declPtr){
                // 和前面 pointer 做法一致
                if (!declPtr)
                {
                    w.writePod(bool(false));
                    return;
                }
                w.writePod(bool(true));
                // 根据kind序列化
                switch (declPtr->declaration_kind)
                {
                case lan_model::EDeclarationKind::CLASS:
                    serializeClassDeclaration(w, static_cast<const lan_model::ClassDeclaration&>(*declPtr));
                    break;
                case lan_model::EDeclarationKind::ENUMERATION:
                    serializeEnumerationDeclaration(w, static_cast<const lan_model::EnumerationDeclaration&>(*declPtr));
                    break;
                case lan_model::EDeclarationKind::FUNCTION:
                    serializeFunctionDeclaration(w, static_cast<const lan_model::FunctionDeclaration&>(*declPtr));
                    break;
                case lan_model::EDeclarationKind::MEMBER_DATA:
                    serializeFieldDeclaration(w, static_cast<const lan_model::FieldDeclaration&>(*declPtr));
                    break;
                case lan_model::EDeclarationKind::MEMBER_FUNCTION:
                    serializeMemberFunctionDeclaration(w, static_cast<const lan_model::MemberFunctionDeclaration&>(*declPtr));
                    break;
                case lan_model::EDeclarationKind::CONSTRUCTOR:
                    serializeConstructorDeclaration(w, static_cast<const lan_model::ConstructorDeclaration&>(*declPtr));
                    break;
                case lan_model::EDeclarationKind::DESTRUCTOR:
                    serializeDestructorDeclaration(w, static_cast<const lan_model::DestructorDeclaration&>(*declPtr));
                    break;
                case lan_model::EDeclarationKind::PARAMETER:
                    serializeParameterDeclaration(w, static_cast<const lan_model::ParameterDeclaration&>(*declPtr));
                    break;
                default:
                    // BASIC / UNKNOWN
                    serializeDeclarationBase(w, *declPtr);
                    break;
                }
            }
        );

        // (4) 写 type_list
        bw.writeDeque(data.type_list,
            [](BinaryWriter& w, const lan_model::Type& ty){
                serializeType(w, ty);
            }
        );

        // (5) 写 type_map
        //     key: size_t, value: Type*
        bw.writeUnorderedMap(
            data.type_map,
            [](BinaryWriter& w, std::size_t key){
                w.writePod(key);
            },
            [](BinaryWriter& w, const lan_model::Type* tptr){
                if (!tptr)
                {
                    w.writePod(bool(false));
                    return;
                }
                w.writePod(bool(true));
                serializeType(w, *tptr);
            }
        );

        return bw.buffer;
    }

} // end namespace lux::cxx::dref