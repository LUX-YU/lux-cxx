#pragma once
#include <lux/cxx/dref/runtime/Marker.hpp>

#include <string>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <array>
#include <map>
#include <unordered_map>
#include <variant>
#include <optional>
#include <set>

// ============================================================
// 1. Union support
// ============================================================
union LUX_META(marked) TestUnion
{
    int     i;
    float   f;
    char    c;
    double  d;
};

// ============================================================
// 2. More builtin types: char, wchar_t, unsigned, float, etc.
// ============================================================
struct LUX_META(marked) BuiltinTypesStruct
{
    bool                b;
    char                c;
    wchar_t             wc;
    char16_t            c16;
    char32_t            c32;
    signed char         sc;
    unsigned char       uc;
    short               s;
    unsigned short      us;
    int                 i;
    unsigned int        ui;
    long                l;
    unsigned long       ul;
    long long           ll;
    unsigned long long  ull;
    float               f;
    double              d;
    long double         ld;
};

// ============================================================
// 3. Variadic function
// ============================================================
void LUX_META(marked) TestVariadicFunction(int count, ...) {}

// ============================================================
// 4. volatile method
// ============================================================
class LUX_META(marked) TestVolatileClass
{
public:
    void volatile_method() volatile {}
    void const_volatile_method() const volatile {}
};

// ============================================================
// 5. Member pointer types  
// ============================================================
struct LUX_META(marked) MemberPointerHolder
{
    int                     TestUnion::* data_member_ptr;
    void (TestVolatileClass::* method_ptr)();
};

// ============================================================
// 6. Virtual destructor + virtual inheritance
// ============================================================
class LUX_META(marked) VirtualBase
{
public:
    virtual ~VirtualBase() {}
    virtual void interface_method() = 0;
protected:
    int base_val;
};

class LUX_META(marked) VirtualDerived : virtual public VirtualBase
{
public:
    void interface_method() override {}
    int derived_val;
};

// ============================================================
// 7. Nested class
// ============================================================
class LUX_META(marked) OuterClass
{
public:
    struct InnerStruct
    {
        int inner_a;
        double inner_b;
    };

    InnerStruct nested_field;

    void method_taking_inner(InnerStruct& s) {}
};

// ============================================================
// 8. Operator overloads
// ============================================================
struct LUX_META(marked) OperatorStruct
{
    int value;

    OperatorStruct operator+(const OperatorStruct& other) const
    {
        return OperatorStruct{value + other.value};
    }

    bool operator==(const OperatorStruct& other) const
    {
        return value == other.value;
    }

    OperatorStruct& operator=(const OperatorStruct& other)
    {
        value = other.value;
        return *this;
    }
};

// ============================================================
// 9. Conversion operator  
// ============================================================
class LUX_META(marked) ConversionClass
{
public:
    operator int() const { return val; }
    explicit operator bool() const { return val != 0; }
private:
    int val;
};

// ============================================================
// 10. Static field
// ============================================================
struct LUX_META(marked) StaticFieldStruct
{
    static int static_count;
    int instance_val;

    static int get_count() { return static_count; }
};

// ============================================================
// 11. Const field
// ============================================================
struct LUX_META(marked) ConstFieldStruct
{
    const int         ci = 42;
    const double      cd = 3.14;
    const std::string cs = "hello";
};

// ============================================================
// 12. noexcept function
// ============================================================
struct LUX_META(marked) NoexceptStruct
{
    void safe_method() noexcept {}
    int  safe_getter() const noexcept { return 0; }
};

// ============================================================
// 13. Return pointer and reference from functions
// ============================================================
class LUX_META(marked) ComplexReturnTypes
{
public:
    int*             return_pointer()       { return &val; }
    const int*       return_const_pointer() const { return &val; }
    int&             return_ref()           { return val; }
    const int&       return_const_ref() const { return val; }
    int&&            return_rvalue_ref()    { return static_cast<int&&>(val); }
private:
    int val = 0;
};

// ============================================================
// 14. Multiple inheritance (non-virtual)
// ============================================================
struct LUX_META(marked) MixinA
{
    int a_val;
    void a_method() {}
};

struct LUX_META(marked) MixinB
{
    double b_val;
    void b_method() {}
};

struct LUX_META(marked) MultiDerived : public MixinA, public MixinB
{
    char c_val;
    void own_method() {}
};

// ============================================================
// 15. Enum with explicit underlying type
// ============================================================
enum class LUX_META(marked) SmallEnum : uint8_t
{
    A = 0,
    B = 1,
    C = 255
};

enum class LUX_META(marked) SignedEnum : int16_t
{
    NEG = -100,
    ZERO = 0,
    POS = 100
};

// ============================================================
// 16. Deeply nested namespaces
// ============================================================
namespace ns1
{
    namespace ns2
    {
        namespace ns3
        {
            struct LUX_META(marked) DeepNestedStruct
            {
                int deep_val;
                void deep_method(const std::string& s) {}
            };
        }
    }
}

// ============================================================
// 17. extern "C" function
// ============================================================
extern "C"
{
    void LUX_META(marked) ExternCFunction(int x, double y) {}
}

// ============================================================
// 18. Anonymous struct/union inside a class
// ============================================================
struct LUX_META(marked) ContainsAnonymous
{
    union
    {
        int   as_int;
        float as_float;
    };

    struct
    {
        int x;
        int y;
    } point;
};

// ============================================================
// 19. Default parameters (parser can see param type but not default value)
// ============================================================
void LUX_META(marked) FuncWithDefaults(
    int a,
    double b = 3.14,
    const std::string& c = "default"
) {}

// ============================================================
// 20. nullptr_t parameter  
// ============================================================
void LUX_META(marked) FuncWithNullptr(std::nullptr_t np) {}

// ============================================================
// 21. Function taking and returning complex types
// ============================================================
struct LUX_META(marked) ComplexParamStruct
{
    const std::vector<int>& ref_to_vector(const std::vector<int>& v) { return v; }
    std::vector<int>*       ptr_to_vector_return() { return nullptr; }
};

// ============================================================
// 22. Template specialization types (STL containers as fields)
//     Critical for game engine ECS component support
// ============================================================
struct LUX_META(marked) ECSComponent
{
    // Simple template: std::vector<int>
    std::vector<int> positions;
    
    // Template with non-type argument: std::array<float, 3>
    std::array<float, 3> velocity;
    
    // Template with two type arguments: std::map<std::string, int>
    std::map<std::string, int> attributes;
    
    // Variant: std::variant<int, float, std::string>
    std::variant<int, float, std::string> flexible_data;
    
    // Nested template: std::unordered_map<std::string, std::vector<int>>
    std::unordered_map<std::string, std::vector<int>> grouped_data;
    
    // Optional: std::optional<double>
    std::optional<double> optional_value;
    
    // Set: std::set<std::string>
    std::set<std::string> tags;
    
    // Vector of vectors: std::vector<std::vector<float>>
    std::vector<std::vector<float>> matrix;
};

// ============================================================
// 23. Member exclusion via LUX_META(no_reflect)
// ============================================================
struct LUX_META(marked) SelectiveReflectStruct
{
    int visible_field_1;
    float visible_field_2;
    LUX_META(no_reflect) int hidden_field;
    LUX_META(no_reflect) double another_hidden;

    void visible_method() {}
    LUX_META(no_reflect) void hidden_method() {}
    int visible_getter() { return 0; }
    LUX_META(no_reflect) void hidden_setter(int v) {}
};
