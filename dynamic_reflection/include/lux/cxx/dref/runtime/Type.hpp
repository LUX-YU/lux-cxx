#pragma once

#include <string>
#include <vector>
#include <clang-c/Index.h>

namespace lux::cxx::dref {

// 可根据需要扩展
enum class ETypeKind {
    BUILTIN,
    POINTER,
    LVALUE_REFERENCE,
    RVALUE_REFERENCE,
    RECORD,    // class/struct/union
    ENUM,      // enumeration
    FUNCTION,  // function type
    ARRAY,
    UNSUPPORTED
};

//=============================
// 基类：Type
//=============================
class Type
{
public:
    virtual ~Type() = default;

    // 你也可以在基类里存储 “是否const/volatile” 等
    std::string name;
    std::string id;
    ETypeKind   kind;
    bool        is_const{false};
    bool        is_volatile{false};
    int         size;
    int         align;
};

class UnsupportedType : public Type{};

class TagType : public Type
{
public:
    TagDecl* decl = nullptr;
};

//==================================================================
// 1) BuiltinType：如 int, float, bool, char, double, etc
//==================================================================
class BuiltinType final : public Type
{
public:
    static constexpr auto static_kind = ETypeKind::BUILTIN;

	enum class EBuiltinKind {
        VOID                = CXType_Void,
        BOOL                = CXType_Bool,
	    CHAR_U              = CXType_Char_U,
        UCHAR               = CXType_UChar,
        CHAR16_T            = CXType_Char16,
        CHAR32_T            = CXType_Char32,
        UNSIGNED_SHORT_INT  = CXType_UShort,
        UNSIGNED_INT        = CXType_UInt,
        UNSIGNED_LONG_INT   = CXType_ULong,
        UNSIGNED_LONG_LONG_INT  = CXType_ULongLong,
        EXTENDED_UNSIGNED   = CXType_UInt128,
        SIGNED_CHAR_S       = CXType_Char_S,
        SIGNED_SIGNED_CHAR  = CXType_SChar,
        WCHAR_T             = CXType_WChar,
        SHORT_INT           = CXType_Short,
        INT                 = CXType_Int,
        LONG_INT            = CXType_Long,
        LONG_LONG_INT       = CXType_LongLong,
        EXTENDED_SIGNED     = CXType_Int128,
        FLOAT               = CXType_Float,
        DOUBLE              = CXType_Double,
        LONG_DOUBLE         = CXType_LongDouble,
	    NULLPTR             = CXType_NullPtr,
	    OVERLOAD            = CXType_Overload,
	    DEPENDENT           = CXType_Dependent,
	    OBJC_IDENTIFIER     = CXType_ObjCId,
	    OBJC_CLASS          = CXType_ObjCClass,
	    OBJC_SEL            = CXType_ObjCSel,
	    FLOAT_128           = CXType_Float128,
	    HALF                = CXType_Half,
	    FLOAT16             = CXType_Float16,
	    SHORT_ACCUM         = CXType_ShortAccum,
	    ACCUM               = CXType_Accum,
	    LONG_ACCUM          = CXType_LongAccum,
	    UNSIGNED_SHORT_ACCUM = CXType_UShortAccum,
	    UNSIGNED_ACCUM      = CXType_UAccum,
	    UNSIGNED_LONG_ACCUM = CXType_UShortAccum,
	    BFLOAT16            = CXType_BFloat16,
	    IMB_128             = CXType_Ibm128,
	};

    bool isSignedInteger() const {
        switch (builtin_type) {
            // 以下为签名整型
        case EBuiltinKind::SIGNED_CHAR_S:
        case EBuiltinKind::SIGNED_SIGNED_CHAR:
        case EBuiltinKind::SHORT_INT:
        case EBuiltinKind::INT:
        case EBuiltinKind::LONG_INT:
        case EBuiltinKind::LONG_LONG_INT:
        case EBuiltinKind::EXTENDED_SIGNED:
            return true;
        default:
            return false;
        }
    }

    bool isFloat() const {
        switch (builtin_type) {
        case EBuiltinKind::FLOAT:
        case EBuiltinKind::DOUBLE:
        case EBuiltinKind::LONG_DOUBLE:
        case EBuiltinKind::FLOAT_128:
        case EBuiltinKind::HALF:
        case EBuiltinKind::FLOAT16:
        case EBuiltinKind::BFLOAT16:
            return true;
        default:
            return false;
        }
    }

    bool isIntegral() const {
        switch (builtin_type) {
            // Bool 和字符类型也属于整型
        case EBuiltinKind::BOOL:
        case EBuiltinKind::CHAR_U:
        case EBuiltinKind::UCHAR:
        case EBuiltinKind::CHAR16_T:
        case EBuiltinKind::CHAR32_T:
        case EBuiltinKind::WCHAR_T:
            // 无符号整型
        case EBuiltinKind::UNSIGNED_SHORT_INT:
        case EBuiltinKind::UNSIGNED_INT:
        case EBuiltinKind::UNSIGNED_LONG_INT:
        case EBuiltinKind::UNSIGNED_LONG_LONG_INT:
        case EBuiltinKind::EXTENDED_UNSIGNED:
            // 签名整型（见 isSignedInteger()）
        case EBuiltinKind::SIGNED_CHAR_S:
        case EBuiltinKind::SIGNED_SIGNED_CHAR:
        case EBuiltinKind::SHORT_INT:
        case EBuiltinKind::INT:
        case EBuiltinKind::LONG_INT:
        case EBuiltinKind::LONG_LONG_INT:
        case EBuiltinKind::EXTENDED_SIGNED:
            return true;
        default:
            return false;
        }
    }

    bool isAccum() const
    {
        switch (builtin_type)
        {
        case EBuiltinKind::SHORT_ACCUM:
        case EBuiltinKind::ACCUM:
        case EBuiltinKind::LONG_ACCUM:
        case EBuiltinKind::UNSIGNED_SHORT_ACCUM:
        case EBuiltinKind::UNSIGNED_ACCUM:
            return true;
        default:
            return false;
        }
    }

    EBuiltinKind builtin_type;
};

//==================================================================
// 2) PointerType
//==================================================================
class PointerType final : public Type
{
public:
    static constexpr auto static_kind = ETypeKind::POINTER;
    // 指向的底层类型
    Type* pointee = nullptr;
    bool  is_pointer_to_member{false};
};

//==================================================================
// 3) ReferenceType -> LValueReferenceType / RValueReferenceType
//==================================================================
class ReferenceType : public Type
{
public:
    Type* referred_type = nullptr; // 指向被引用的类型
};

class LValueReferenceType final : public ReferenceType
{
public:
    static constexpr auto static_kind = ETypeKind::LVALUE_REFERENCE;
};

class RValueReferenceType final : public ReferenceType
{
public:
    static constexpr auto static_kind = ETypeKind::RVALUE_REFERENCE;
};

//==================================================================
// 4) RecordType
//==================================================================
class CXXRecordDecl;
class RecordType final : public TagType
{
public:
    static constexpr auto static_kind = ETypeKind::RECORD;
};

//==================================================================
// 5) EnumType
//==================================================================
class EnumType final : public TagType
{
public:
    static constexpr auto static_kind = ETypeKind::ENUM;
};

//==================================================================
// 6) FunctionType
//==================================================================
class FunctionType final : public Type
{
public:
    // 函数的返回类型
    Type* resultType = nullptr;
    // 形参类型列表
    std::vector<Type*> paramTypes;
    // 是否可变参
    bool isVariadic = false;
};

//==================================================================
// 7) 你还可以继续加 ArrayType, etc.
//==================================================================

} // namespace lux::cxx::lan_model
