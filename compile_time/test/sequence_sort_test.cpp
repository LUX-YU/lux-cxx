#include <lux/cxx/compile_time/sequence_sort.hpp>
#include <lux/cxx/compile_time/type_info.hpp>
#include <iostream>
#include <typeinfo>

struct Position
{
    float x, y;
};

struct Velocity
{
    float vx, vy;
};

struct Health
{
    int hp;
    Health(int h = 100) : hp(h) {}
};

template<typename... Ts>
struct TestStruct
{
    using seq_type = std::index_sequence<lux::cxx::type_hash<Ts>()...>;
};

int main(int argc, char* argv[])
{
    using seq_type    = typename TestStruct<const Position&, const Velocity&, const Health&>::seq_type;
    using sorted_type = typename lux::cxx::quick_sort_sequence<seq_type>::type;

    std::cout << "sorted_type:" << typeid(sorted_type).name() << std::endl;

    return 0;
}