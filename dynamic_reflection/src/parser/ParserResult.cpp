#include <lux/cxx/dref/parser/ParserResult.hpp>
#include <lux/cxx/dref/parser/ParserResultImpl.hpp>

#include <algorithm>
#include <iterator>

namespace lux::cxx::dref
{
    ParserResult::ParserResult()
    {
        _impl = std::make_unique<ParserResultImpl>();
    }

    ParserResult::~ParserResult() = default;

    std::vector<runtime::ClassMeta> ParserResult::findMarkedClassMetaByName(const std::string& mark)
    {
        std::vector<runtime::ClassMeta> ret;

        auto& list = _impl->_class_meta_list;
        std::copy_if(list.begin(), list.end(), std::back_inserter(ret),
            [&mark](const auto& info)
            {
                return true; //info.mark_name() == mark;
            }
        );
        return ret;
    }

    std::vector<runtime::ClassMeta> ParserResult::allMarkedClassMeta()
    {
        return _impl->_class_meta_list;
    }

    void ParserResult::appendClassMeta(runtime::ClassMeta mark)
    {
        _impl->appendClassMeta(std::move(mark));
    }
}