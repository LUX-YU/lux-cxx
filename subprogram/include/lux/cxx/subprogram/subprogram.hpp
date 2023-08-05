#pragma once
#include <algorithm>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <functional>
#include <vector>

// PS : Support only c++20 or above

namespace lux::cxx
{
    using SubProgramFunc = std::function<int (int, char*[])>;

    template<class T> concept class_has_void_entry = requires(T t){
        {t()} -> std::same_as<int>;
    };

    template<class T> concept class_has_argument_entry = requires(T t){
        {t(std::declval<int>(), std::declval<char*[]>())} -> std::same_as<int>;
    };

    template<class T> concept class_has_entry = 
        class_has_void_entry<T> || 
        class_has_argument_entry<T>;

    using FunctionMap = std::unordered_map<std::string, SubProgramFunc>;

    // need c++17
    inline FunctionMap __inline_map;

    static inline FunctionMap& getMap()
    {
        return __inline_map;
    }

    #define __func_map getMap()

    class SubProgramRegister
    {
    public:
        static void registProgram(const std::string& name, SubProgramFunc func)
        {
            if(!__func_map.count(name))
            {
                __func_map[name] = std::move(func);
            }
        }

        static bool hasSubProgram(const std::string& name)
        {
            return __func_map.count(name);
        }

        static std::vector<std::string> listSubProgram()
        {
            std::vector<std::string> list;
            for(auto& [key, func] : __func_map)
            {
                list.push_back(key);
            }
            return list;
        }

        static std::vector<std::string> listSortedSubProgram()
        {
            std::vector<std::string> list;
            for(auto& [key, func] : __func_map)
            {
                list.push_back(key);
            }
            std::sort(list.begin(), list.end());
            return list;
        }

        // if not exist, return -255
        static int invokeSubProgram(const std::string& name, int argc, char* argv[])
        {
            if(__func_map.count(name)) return __func_map[name](argc, argv);
            return -255;
        }
    };

    template<class T>
    struct ProgramClassEntryRegister
    {
        // enable after c++ 20
        template<class... Args> requires class_has_entry<T>
        explicit ProgramClassEntryRegister(const std::string& name, Args&&... args)
        {
            auto regist_wrapper = [... args = std::forward<Args>(args)](int argc, char* argv[]){
                T t{args...};
                if constexpr( class_has_void_entry<T> )
                    return t();
                else
                    return t(argc, argv);
            };
            SubProgramRegister::registProgram(name, std::move(regist_wrapper));
        }
    };

    struct ProgramFuncEntryRegister
    {
        template<class Func>
        ProgramFuncEntryRegister(const std::string& name, Func&& func)
        requires std::is_convertible_v<Func, std::function<int (void)>>
        {
            SubProgramRegister::registProgram(
                name, [_func = std::forward<Func>(func)](int argc, char* argv[]){return _func();}
            );
        }

        template<class Func>
        ProgramFuncEntryRegister(const std::string& name, Func&& func)
        requires std::is_convertible_v<Func, std::function<int (int, char*[])>>
        {
            SubProgramRegister::registProgram(
                name, [_func = std::forward<Func>(func)](int argc, char* argv[]){return _func(argc, argv);}
            );
        }
    };

    /*
        example:
        class SampleClass
        {
        public:
            SampleClass(int, double){}

            int operator()(){}
            or
            int operator()(int argc, char* argv[]){}
        };
        RegistClassSubprogramEntry(SampleClass, "test_class" ,  1, 2.125)
    */
    #define RegistClassSubprogramEntry(type, title, ...)\
    static ::lux::cxx::ProgramClassEntryRegister<type> func_name ## _regist_helper(title, __VA_ARGS__);

    /*
        example:
        static int __main(int argc, char* argv){return 0;}
        RegistFuncSubprogramEntry(__main, "test_func")
    */
    #define RegistFuncSubprogramEntry(type, title)\
    static ::lux::cxx::ProgramFuncEntryRegister func_name ## _regist_helper(title, &type);
}
