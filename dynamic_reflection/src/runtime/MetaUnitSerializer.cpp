#include <lux/cxx/dref/runtime/Declaration.hpp>
#include <lux/cxx/dref/runtime/Type.hpp>
#include <lux/cxx/dref/runtime/MetaUnitSerializer.hpp> // 假设声明了下面这些接口
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>

namespace lux::cxx::dref
{
    static bool isAnyCXXMethodKind(EDeclKind k)
    {
        return (k == EDeclKind::CXX_METHOD_DECL
            || k == EDeclKind::CXX_CONSTRUCTOR_DECL
            || k == EDeclKind::CXX_CONVERSION_DECL
            || k == EDeclKind::CXX_DESTRUCTOR_DECL);
    }

    static void serializeFunctionCommon(const FunctionDecl* fn, nlohmann::json& j)
    {
        j["result_type_id"] = (fn->result_type ? fn->result_type->id : "");
        j["mangling"] = fn->mangling;
        j["is_variadic"] = fn->is_variadic;

        nlohmann::json paramsArr = nlohmann::json::array();
        for (auto* p : fn->params)
            paramsArr.push_back(p ? p->id : "");
        j["params"] = paramsArr;
    }

    static void fixupFunctionCommon(FunctionDecl* fn,
        const nlohmann::json& j,
        const std::unordered_map<std::string, Decl*>& declMap,
        const std::unordered_map<std::string, Type*>& typeMap)
    {
        // result_type
        if (j.contains("result_type_id")) {
            auto rid = j["result_type_id"].get<std::string>();
            if (!rid.empty()) {
                auto it = typeMap.find(rid);
                if (it != typeMap.end()) {
                    fn->result_type = it->second;
                }
            }
        }
        // params
        if (j.contains("params") && j["params"].is_array()) {
            for (auto& pIdJson : j["params"]) {
                auto pId = pIdJson.get<std::string>();
                if (!pId.empty()) {
                    auto it = declMap.find(pId);
                    if (it != declMap.end()) {
                        // 这里要判断它是不是 EDeclKind::PARAM_VAR_DECL
                        if (it->second->kind == EDeclKind::PARAM_VAR_DECL) {
                            fn->params.push_back(static_cast<ParmVarDecl*>(it->second));
                        }
                    }
                }
            }
        }
    }

    static std::unique_ptr<Decl> instantiateDecl(EDeclKind k)
    {
        switch (k)
        {
        case EDeclKind::ENUM_DECL:             return std::make_unique<EnumDecl>();
        case EDeclKind::RECORD_DECL:           return std::make_unique<RecordDecl>();
        case EDeclKind::CXX_RECORD_DECL:       return std::make_unique<CXXRecordDecl>();
        case EDeclKind::FIELD_DECL:            return std::make_unique<FieldDecl>();
        case EDeclKind::FUNCTION_DECL:         return std::make_unique<FunctionDecl>();
        case EDeclKind::CXX_METHOD_DECL:       return std::make_unique<CXXMethodDecl>();
        case EDeclKind::CXX_CONSTRUCTOR_DECL:  return std::make_unique<CXXConstructorDecl>();
        case EDeclKind::CXX_CONVERSION_DECL:   return std::make_unique<CXXConversionDecl>();
        case EDeclKind::CXX_DESTRUCTOR_DECL:   return std::make_unique<CXXDestructorDecl>();
        case EDeclKind::PARAM_VAR_DECL:        return std::make_unique<ParmVarDecl>();
        case EDeclKind::VAR_DECL:              return std::make_unique<VarDecl>();
        default:
            return nullptr;
        }
    }

    static nlohmann::json serialize(const Decl* d)
    {
        nlohmann::json j;
        if (!d) return j;

        // 通用字段
        j["id"] = d->id;
        j["__kind"] = declKindToString(d->kind);

        // 如果是 NamedDecl（比如大多数 Decl 都继承 NamedDecl）
        if (auto* nd = asNamedDecl(d))
        {
            j["name"] = nd->name;
            j["fq_name"] = nd->full_qualified_name;
            j["spelling"] = nd->spelling;
            j["attributes"] = nd->attributes;
            j["is_anonymous"] = nd->is_anonymous;
            j["type_id"] = (nd->type ? nd->type->id : "");
        }

        // 分发
        switch (d->kind)
        {
        case EDeclKind::ENUM_DECL:
        {
            auto* en = static_cast<const EnumDecl*>(d);
            j["is_scoped"] = en->is_scoped;
            j["underlying_type_id"] = (en->underlying_type ? en->underlying_type->id : "");

            nlohmann::json enumerArr = nlohmann::json::array();
            for (auto& e : en->enumerators)
            {
                nlohmann::json ej;
                ej["name"] = e.name;
                ej["signed_value"] = e.signed_value;
                ej["unsigned_value"] = e.unsigned_value;
                enumerArr.push_back(ej);
            }
            j["enumerators"] = enumerArr;
        }
        break;

        case EDeclKind::RECORD_DECL:
            // 无额外字段
            break;

        case EDeclKind::CXX_RECORD_DECL:
        {
            auto* cxxr = static_cast<const CXXRecordDecl*>(d);
            // bases
            {
                nlohmann::json arr = nlohmann::json::array();
                for (auto* base : cxxr->bases)
                    arr.push_back(base ? base->id : "");
                j["bases"] = arr;
            }
            // ctors
            {
                nlohmann::json arr = nlohmann::json::array();
                for (auto* ctor : cxxr->constructor_decls)
                    arr.push_back(ctor ? ctor->id : "");
                j["constructor_decls"] = arr;
            }
            // dtor
            j["destructor_decl"] = (cxxr->destructor_decl ? cxxr->destructor_decl->id : "");
            // normal method
            {
                nlohmann::json arr = nlohmann::json::array();
                for (auto* m : cxxr->method_decls)
                    arr.push_back(m ? m->id : "");
                j["method_decls"] = arr;
            }
            // static method
            {
                nlohmann::json arr = nlohmann::json::array();
                for (auto* sm : cxxr->static_method_decls)
                    arr.push_back(sm ? sm->id : "");
                j["static_method_decls"] = arr;
            }
            // fields
            {
                nlohmann::json arr = nlohmann::json::array();
                for (auto* f : cxxr->field_decls)
                    arr.push_back(f ? f->id : "");
                j["field_decls"] = arr;
            }
            j["is_abstract"] = cxxr->is_abstract;
        }
        break;

        case EDeclKind::FIELD_DECL:
        {
            auto* f = static_cast<const FieldDecl*>(d);
            j["visibility"] = (int)f->visibility;
            j["offset"] = (uint64_t)f->offset;
            j["parent_class_id"] = (f->parent_class ? f->parent_class->id : "");
        }
        break;

        case EDeclKind::FUNCTION_DECL:
        {
            auto* fn = static_cast<const FunctionDecl*>(d);
            serializeFunctionCommon(fn, j);
        }
        break;

        case EDeclKind::CXX_METHOD_DECL:
        case EDeclKind::CXX_CONSTRUCTOR_DECL:
        case EDeclKind::CXX_CONVERSION_DECL:
        case EDeclKind::CXX_DESTRUCTOR_DECL:
        {
            auto* method = static_cast<const CXXMethodDecl*>(d);
            serializeFunctionCommon(method, j);
            j["visibility"] = (int)method->visibility;
            j["is_static"] = method->is_static;
            j["is_virtual"] = method->is_virtual;
            j["is_const"] = method->is_const;
            j["is_volatile"] = method->is_volatile;
            j["parent_class_id"] = (method->parent_class ? method->parent_class->id : "");
        }
        break;

        case EDeclKind::PARAM_VAR_DECL:
        {
            auto* p = static_cast<const ParmVarDecl*>(d);
            j["index"] = (uint64_t)p->index;
        }
        break;

        case EDeclKind::VAR_DECL:
            // 无额外字段
            break;

        default:
            // do nothing
            break;
        }

        return j;
    }


    //============ 反序列化 ============//
    /**
     * 一阶段：创建对象，不做指针修复
     */
    static std::unique_ptr<Decl> createDeclFirstPass(const nlohmann::json& j)
    {
        if (!j.contains("__kind")) return nullptr;
        EDeclKind k = stringToDeclKind(j["__kind"].get<std::string>());

        auto declPtr = instantiateDecl(k);
        if (!declPtr) return nullptr;

        Decl* raw = declPtr.get();
        raw->kind = k;
        raw->id = j.value("id", "");

        // NamedDecl common?
        if (auto* nd = asNamedDecl(raw))
        {
            nd->name = j.value("name", "");
            nd->full_qualified_name = j.value("fq_name", "");
            nd->spelling = j.value("spelling", "");
            nd->is_anonymous = j.value("is_anonymous", false);

            if (j.contains("attributes") && j["attributes"].is_array())
                nd->attributes = j["attributes"].get<std::vector<std::string>>();
        }

        // 再填子类字段
        switch (k)
        {
        case EDeclKind::ENUM_DECL:
        {
            auto* en = static_cast<EnumDecl*>(raw);
            en->is_scoped = j.value("is_scoped", false);
            // enumerators
            if (j.contains("enumerators") && j["enumerators"].is_array())
            {
                for (auto& ej : j["enumerators"])
                {
                    Enumerator e;
                    e.name = ej.value("name", "");
                    e.signed_value = ej.value("signed_value", (int64_t)0);
                    e.unsigned_value = ej.value("unsigned_value", (uint64_t)0);
                    en->enumerators.push_back(e);
                }
            }
        }
        break;

        case EDeclKind::CXX_RECORD_DECL:
        {
            auto* cxxr = static_cast<CXXRecordDecl*>(raw);
            cxxr->is_abstract = j.value("is_abstract", false);
        }
        break;

        case EDeclKind::FIELD_DECL:
        {
            auto* f = static_cast<FieldDecl*>(raw);
            f->visibility = (EVisibility)j.value("visibility", (int)EVisibility::INVALID);
            f->offset = j.value("offset", (uint64_t)0);
        }
        break;

        case EDeclKind::FUNCTION_DECL:
        {
            auto* fn = static_cast<FunctionDecl*>(raw);
            fn->mangling = j.value("mangling", "");
            fn->is_variadic = j.value("is_variadic", false);
        }
        break;

        case EDeclKind::CXX_METHOD_DECL:
        case EDeclKind::CXX_CONSTRUCTOR_DECL:
        case EDeclKind::CXX_CONVERSION_DECL:
        case EDeclKind::CXX_DESTRUCTOR_DECL:
        {
            auto* method = static_cast<CXXMethodDecl*>(raw);
            method->mangling = j.value("mangling", "");
            method->is_variadic = j.value("is_variadic", false);
            method->visibility = (EVisibility)j.value("visibility", (int)EVisibility::INVALID);
            method->is_static = j.value("is_static", false);
            method->is_virtual = j.value("is_virtual", false);
            method->is_const = j.value("is_const", false);
            method->is_volatile = j.value("is_volatile", false);
        }
        break;

        case EDeclKind::PARAM_VAR_DECL:
        {
            auto* p = static_cast<ParmVarDecl*>(raw);
            p->index = j.value("index", (uint64_t)0);
        }
        break;

        default:
            // ...
            break;
        }

        return declPtr;
    }


    /**
     * 二阶段：根据 JSON 中的 id 字符串，修复指向其他 Decl / Type 的指针
     */
    static void fixupDecl(Decl* d,
        const nlohmann::json& j,
        const std::unordered_map<std::string, Decl*>& declMap,
        const std::unordered_map<std::string, Type*>& typeMap)
    {
        if (!d) return;

        // NamedDecl->type
        if (auto* nd = asNamedDecl(d))
        {
            if (j.contains("type_id")) {
                auto tid = j["type_id"].get<std::string>();
                if (!tid.empty()) {
                    auto it = typeMap.find(tid);
                    if (it != typeMap.end()) {
                        nd->type = it->second;
                    }
                }
            }
        }

        switch (d->kind)
        {
        case EDeclKind::ENUM_DECL:
        {
            auto* en = static_cast<EnumDecl*>(d);
            if (j.contains("underlying_type_id")) {
                auto uid = j["underlying_type_id"].get<std::string>();
                if (!uid.empty()) {
                    auto it = typeMap.find(uid);
                    if (it != typeMap.end()) {
                        en->underlying_type = static_cast<BuiltinType*>(it->second);
                    }
                }
            }
        }
        break;
        case EDeclKind::CXX_RECORD_DECL:
        {
            auto* cxxr = static_cast<CXXRecordDecl*>(d);
            // bases
            if (j.contains("bases") && j["bases"].is_array()) {
                for (auto& baseId : j["bases"]) {
                    auto s = baseId.get<std::string>();
                    if (!s.empty()) {
                        auto it = declMap.find(s);
                        if (it != declMap.end() && it->second->kind == EDeclKind::CXX_RECORD_DECL)
                        {
                            cxxr->bases.push_back(static_cast<CXXRecordDecl*>(it->second));
                        }
                    }
                }
            }
            // ctors
            if (j.contains("constructor_decls") && j["constructor_decls"].is_array()) {
                for (auto& cid : j["constructor_decls"]) {
                    auto s = cid.get<std::string>();
                    if (!s.empty()) {
                        auto it = declMap.find(s);
                        if (it != declMap.end() && it->second->kind == EDeclKind::CXX_CONSTRUCTOR_DECL)
                        {
                            cxxr->constructor_decls.push_back(static_cast<CXXConstructorDecl*>(it->second));
                        }
                    }
                }
            }
            // dtor
            if (j.contains("destructor_decl")) {
                auto did = j["destructor_decl"].get<std::string>();
                if (!did.empty()) {
                    auto it = declMap.find(did);
                    if (it != declMap.end() && it->second->kind == EDeclKind::CXX_DESTRUCTOR_DECL)
                    {
                        cxxr->destructor_decl = static_cast<CXXDestructorDecl*>(it->second);
                    }
                }
            }
            // method_decls
            if (j.contains("method_decls") && j["method_decls"].is_array()) {
                for (auto& mid : j["method_decls"]) {
                    auto s = mid.get<std::string>();
                    if (!s.empty()) {
                        auto it = declMap.find(s);
                        if (it != declMap.end() && isAnyCXXMethodKind(it->second->kind))
                        {
                            cxxr->method_decls.push_back(static_cast<CXXMethodDecl*>(it->second));
                        }
                    }
                }
            }
            // static_method_decls
            if (j.contains("static_method_decls") && j["static_method_decls"].is_array()) {
                for (auto& smid : j["static_method_decls"]) {
                    auto s = smid.get<std::string>();
                    if (!s.empty()) {
                        auto it = declMap.find(s);
                        if (it != declMap.end() && isAnyCXXMethodKind(it->second->kind))
                        {
                            cxxr->static_method_decls.push_back(static_cast<CXXMethodDecl*>(it->second));
                        }
                    }
                }
            }
            // field_decls
            if (j.contains("field_decls") && j["field_decls"].is_array()) {
                for (auto& fid : j["field_decls"]) {
                    auto s = fid.get<std::string>();
                    if (!s.empty()) {
                        auto it = declMap.find(s);
                        if (it != declMap.end() && it->second->kind == EDeclKind::FIELD_DECL)
                        {
                            cxxr->field_decls.push_back(static_cast<FieldDecl*>(it->second));
                        }
                    }
                }
            }
        }
        break;
        case EDeclKind::FIELD_DECL:
        {
            auto* f = static_cast<FieldDecl*>(d);
            if (j.contains("parent_class_id")) {
                auto pcid = j["parent_class_id"].get<std::string>();
                if (!pcid.empty()) {
                    auto it = declMap.find(pcid);
                    if (it != declMap.end() && it->second->kind == EDeclKind::CXX_RECORD_DECL)
                    {
                        f->parent_class = static_cast<CXXRecordDecl*>(it->second);
                    }
                }
            }
        }
        break;

        case EDeclKind::FUNCTION_DECL:
        {
            auto* fn = static_cast<FunctionDecl*>(d);
            fixupFunctionCommon(fn, j, declMap, typeMap);
        }
        break;

        case EDeclKind::CXX_METHOD_DECL:
        case EDeclKind::CXX_CONSTRUCTOR_DECL:
        case EDeclKind::CXX_CONVERSION_DECL:
        case EDeclKind::CXX_DESTRUCTOR_DECL:
        {
            auto* method = static_cast<CXXMethodDecl*>(d);
            fixupFunctionCommon(method, j, declMap, typeMap);

            // parent_class_id
            if (j.contains("parent_class_id")) {
                auto s = j["parent_class_id"].get<std::string>();
                if (!s.empty()) {
                    auto it = declMap.find(s);
                    if (it != declMap.end() && it->second->kind == EDeclKind::CXX_RECORD_DECL)
                    {
                        method->parent_class = static_cast<CXXRecordDecl*>(it->second);
                    }
                }
            }
        }
        break;

        default:
            // no extra fix
            break;
        }
    }

    // Type
    static std::unique_ptr<Type> instantiateType(ETypeKinds k)
    {
        switch (k)
        {
        case ETypeKinds::Builtin:          return std::make_unique<BuiltinType>();
        case ETypeKinds::Pointer:          return std::make_unique<PointerType>();
        case ETypeKinds::LvalueReference:  return std::make_unique<LValueReferenceType>();
        case ETypeKinds::RvalueReference:  return std::make_unique<RValueReferenceType>();
        case ETypeKinds::Record:           return std::make_unique<RecordType>();
        case ETypeKinds::Enum:             return std::make_unique<EnumType>();
        case ETypeKinds::Function:         return std::make_unique<FunctionType>();
        default:
            return std::make_unique<UnsupportedType>();
        }
    }

    static nlohmann::json serialize(const Type* t)
    {
        nlohmann::json j;
        if (!t) return j;

        j["id"] = t->id;
        j["__kind"] = typeKindToString(t->kind);

        j["name"] = t->name;
        j["type_kind"] = (int)t->kind;
        j["is_const"] = t->is_const;
        j["is_volatile"] = t->is_volatile;
        j["size"] = t->size;
        j["align"] = t->align;

        switch (t->kind)
        {
        case ETypeKinds::Builtin:
        {
            auto* b = static_cast<const BuiltinType*>(t);
            j["builtin_type"] = (int)b->builtin_type;
        }
        break;
        case ETypeKinds::Pointer:
        {
            auto* p = static_cast<const PointerType*>(t);
            j["pointee_id"] = (p->pointee ? p->pointee->id : "");
            j["is_pointer_to_member"] = p->is_pointer_to_member;
        }
        break;
        case ETypeKinds::LvalueReference:
        {
            auto* lv = static_cast<const LValueReferenceType*>(t);
            j["referred_id"] = (lv->referred_type ? lv->referred_type->id : "");
        }
        break;
        case ETypeKinds::RvalueReference:
        {
            auto* rv = static_cast<const RValueReferenceType*>(t);
            j["referred_id"] = (rv->referred_type ? rv->referred_type->id : "");
        }
        break;
        case ETypeKinds::Record:
        {
            auto* rt = static_cast<const RecordType*>(t);
            j["decl_id"] = (rt->decl ? rt->decl->id : "");
        }
        break;
        case ETypeKinds::Enum:
        {
            auto* et = static_cast<const EnumType*>(t);
            j["decl_id"] = (et->decl ? et->decl->id : "");
        }
        break;
        case ETypeKinds::Function:
        {
            auto* ft = static_cast<const FunctionType*>(t);
            j["result_type_id"] = (ft->result_type ? ft->result_type->id : "");
            j["is_variadic"] = ft->is_variadic;

            nlohmann::json arr = nlohmann::json::array();
            for (auto* pt : ft->param_types)
                arr.push_back(pt ? pt->id : "");
            j["param_types"] = arr;
        }
        break;
        default:
            // ETypeKinds::Unsupported or others
            break;
        }

        return j;
    }

    //========== 反序列化 ==========//
    static std::unique_ptr<Type> createTypeFirstPass(const nlohmann::json& j)
    {
        if (!j.contains("__kind")) return nullptr;
        auto kindStr = j["__kind"].get<std::string>();
        ETypeKinds k = stringToTypeKind(kindStr);

        auto tptr = instantiateType(k);
        if (!tptr) return nullptr;

        Type* raw = tptr.get();
        raw->id = j.value("id", "");
        raw->kind = k;
        raw->name = j.value("name", "");
        raw->is_const = j.value("is_const", false);
        raw->is_volatile = j.value("is_volatile", false);
        raw->size = j.value("size", 0);
        raw->align = j.value("align", 0);

        switch (k)
        {
        case ETypeKinds::Builtin:
        {
            auto* b = static_cast<BuiltinType*>(raw);
            b->builtin_type = (BuiltinType::EBuiltinKind)j.value("builtin_type", 0);
        }
        break;
        case ETypeKinds::Pointer:
        {
            auto* p = static_cast<PointerType*>(raw);
            p->is_pointer_to_member = j.value("is_pointer_to_member", false);
        }
        break;
        case ETypeKinds::Function:
        {
            auto* ft = static_cast<FunctionType*>(raw);
            ft->is_variadic = j.value("is_variadic", false);
        }
        break;
        default:
            // ...
            break;
        }

        return tptr;
    }

    static void fixupType(Type* t,
        const nlohmann::json& j,
        const std::unordered_map<std::string, Type*>& typeMap,
        const std::unordered_map<std::string, Decl*>& declMap)
    {
        if (!t) return;

        switch (t->kind)
        {
        case ETypeKinds::Pointer:
        {
            auto* p = static_cast<PointerType*>(t);
            if (j.contains("pointee_id")) {
                auto pid = j["pointee_id"].get<std::string>();
                if (!pid.empty()) {
                    auto it = typeMap.find(pid);
                    if (it != typeMap.end())
                        p->pointee = it->second;
                }
            }
        }
        break;
        case ETypeKinds::LvalueReference:
        {
            auto* lv = static_cast<LValueReferenceType*>(t);
            if (j.contains("referred_id")) {
                auto rid = j["referred_id"].get<std::string>();
                if (!rid.empty()) {
                    auto it = typeMap.find(rid);
                    if (it != typeMap.end())
                        lv->referred_type = it->second;
                }
            }
        }
        break;
        case ETypeKinds::RvalueReference:
        {
            auto* rv = static_cast<RValueReferenceType*>(t);
            if (j.contains("referred_id")) {
                auto rid = j["referred_id"].get<std::string>();
                if (!rid.empty()) {
                    auto it = typeMap.find(rid);
                    if (it != typeMap.end())
                        rv->referred_type = it->second;
                }
            }
        }
        break;
        case ETypeKinds::Record:
        {
            auto* rt = static_cast<RecordType*>(t);
            if (j.contains("decl_id")) {
                auto did = j["decl_id"].get<std::string>();
                if (!did.empty()) {
                    auto itd = declMap.find(did);
                    if (itd != declMap.end())
                        rt->decl = static_cast<TagDecl*>(itd->second);
                }
            }
        }
        break;
        case ETypeKinds::Enum:
        {
            auto* et = static_cast<EnumType*>(t);
            if (j.contains("decl_id")) {
                auto did = j["decl_id"].get<std::string>();
                if (!did.empty()) {
                    auto itd = declMap.find(did);
                    if (itd != declMap.end())
                        et->decl = static_cast<TagDecl*>(itd->second);
                }
            }
        }
        break;
        case ETypeKinds::Function:
        {
            auto* ft = static_cast<FunctionType*>(t);
            // result_type
            if (j.contains("result_type_id")) {
                auto rid = j["result_type_id"].get<std::string>();
                if (!rid.empty()) {
                    auto it = typeMap.find(rid);
                    if (it != typeMap.end())
                        ft->result_type = it->second;
                }
            }
            // param_types
            if (j.contains("param_types") && j["param_types"].is_array()) {
                for (auto& pjid : j["param_types"]) {
                    auto ps = pjid.get<std::string>();
                    if (!ps.empty()) {
                        auto it = typeMap.find(ps);
                        if (it != typeMap.end())
                            ft->param_types.push_back(it->second);
                    }
                }
            }
        }
        break;
        default:
            // ...
            break;
        }
    }

    static inline int indexOfDecl(const std::vector<std::unique_ptr<Decl>>& vec, const Decl* d)
    {
        for (int i = 0; i < (int)vec.size(); i++) {
            if (vec[i].get() == d)
                return i;
        }
        return -1;
    }

    /**
     * 工具函数：给定指针 t，找到它在 data.types 里的下标。
     */
    static inline int indexOfType(const std::vector<std::unique_ptr<Type>>& vec, const Type* t)
    {
        for (int i = 0; i < (int)vec.size(); i++) {
            if (vec[i].get() == t)
                return i;
        }
        return -1;
    }

    nlohmann::json serializeMetaUnitData(const MetaUnitData& data)
    {
        nlohmann::json root;

        // 1) declarations
        {
            nlohmann::json declArr = nlohmann::json::array();
            for (auto& d : data.declarations)
            {
                declArr.push_back(serialize(d.get()));
            }
            root["declarations"] = declArr;
        }

        // 2) types
        {
            nlohmann::json typeArr = nlohmann::json::array();
            for (auto& t : data.types)
            {
                typeArr.push_back(serialize(t.get()));
            }
            root["types"] = typeArr;
        }

        // 3) 不需要序列化 declaration_map / type_map
        // 4) type alias
		nlohmann::json jsonAlias = nlohmann::json::object();
        for (auto& kv : data.type_alias_map) {
            jsonAlias[kv.first] = (kv.second ? kv.second->id : "");
        }
		root["type_alias_map"] = jsonAlias;

        // 5) marked_declarations 用“下标”
        {
            nlohmann::json arr = nlohmann::json::array();
            for (auto* d : data.marked_declarations)
            {
                int idx = indexOfDecl(data.declarations, d);
                arr.push_back(idx);  // 如果找不到，则是 -1， 你也可根据需要做处理
            }
            root["marked_declarations"] = arr;
        }

        // 6) marked_types 用“下标”
        {
            nlohmann::json arr = nlohmann::json::array();
            for (auto* t : data.marked_types)
            {
                int idx = indexOfType(data.types, t);
                arr.push_back(idx);
            }
            root["marked_types"] = arr;
        }

        return root;
    }

    void deserializeMetaUnitData(const nlohmann::json& root, MetaUnitData& data)
    {
        //------------------ 先处理 Declarations ------------------//
        if (root.contains("declarations") && root["declarations"].is_array())
        {
            for (auto& item : root["declarations"])
            {
                auto d = createDeclFirstPass(item);
                if (d)
                {
                    Decl* raw = d.get();
                    if (!raw->id.empty()) {
                        data.declaration_map[raw->id] = raw;
                    }
                    data.declarations.push_back(std::move(d));
                }
            }
        }

        //------------------ 先处理 Types ------------------//
        if (root.contains("types") && root["types"].is_array())
        {
            for (auto& item : root["types"])
            {
                auto t = createTypeFirstPass(item);
                if (t)
                {
                    Type* raw = t.get();
                    if (!raw->id.empty()) {
                        data.type_map[raw->id] = raw;
                    }
                    data.types.push_back(std::move(t));
                }
            }
        }

        //------------------ 二阶段fixup: Declarations ------------------//
        {
            const auto& arr = root["declarations"];
            for (size_t i = 0; i < data.declarations.size(); i++)
            {
                auto* d = data.declarations[i].get();
                fixupDecl(d, arr[i], data.declaration_map, data.type_map);
            }
        }

        //------------------ 二阶段fixup: Types ------------------//
        {
            const auto& arr = root["types"];
            for (size_t i = 0; i < data.types.size(); i++)
            {
                auto* t = data.types[i].get();
                fixupType(t, arr[i], data.type_map, data.declaration_map);
            }
        }

		// ------------------ type alias ------------------//
        if (root.contains("type_alias_map") && root["type_alias_map"].is_object())
        {
			for (auto& kv : root["type_alias_map"].items())
			{
				auto& alias = kv.key();
				auto id = kv.value().get<std::string>();
				if (!id.empty()) {
					auto it = data.type_map.find(id);
					if (it != data.type_map.end()) {
						data.type_alias_map[alias] = it->second;
					}
				}
			}
        }

        //------------------ marked_declarations (index) ------------------//
        if (root.contains("marked_declarations") && root["marked_declarations"].is_array())
        {
            for (auto& idxVal : root["marked_declarations"])
            {
                int idx = idxVal.get<int>();
                if (idx >= 0 && idx < (int)data.declarations.size()) {
                    data.marked_declarations.push_back(data.declarations[idx].get());
                }
                else {
                    // 如果越界，你可选择跳过或抛异常
                    // 这里简单跳过
                }
            }
        }

        //------------------ marked_types (index) ------------------//
        if (root.contains("marked_types") && root["marked_types"].is_array())
        {
            for (auto& idxVal : root["marked_types"])
            {
                int idx = idxVal.get<int>();
                if (idx >= 0 && idx < (int)data.types.size()) {
                    data.marked_types.push_back(data.types[idx].get());
                }
                else {
                    // 同理可跳过或报错
                }
            }
        }
    }

    std::string MetaUnitImpl::toJson()
    {
        auto json = serializeMetaUnitData(*_data);
        json["version"] = this->_version;
		json["name"] = this->_name;

		return nlohmann::to_string(json);
    }

    void MetaUnitImpl::fromJson(const std::string& json, MetaUnitImpl& unit)
    {
		nlohmann::json j = nlohmann::json::parse(json);
		auto data = std::make_unique<MetaUnitData>();
		deserializeMetaUnitData(j, *data);
		std::string name    = j.value("name", "");
		std::string version = j.value("version", "");

		unit = MetaUnitImpl(std::move(data), std::move(name), std::move(version));
    }
} // namespace lux::cxx::dref
