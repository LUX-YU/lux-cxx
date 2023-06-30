#include <lux/cxx/dref/parser/ParserResultImpl.hpp>

namespace lux::cxx::dref
{
	ParserResultImpl::ParserResultImpl()
	{

	}

	ParserResultImpl::~ParserResultImpl() = default;

	void ParserResultImpl::appendClassMeta(const runtime::ClassMeta& mark)
	{
		_class_meta_list.push_back(mark);
	}

	void ParserResultImpl::appendClassMeta(runtime::ClassMeta&& mark)
	{
		_class_meta_list.push_back(std::move(mark));
	}

	void ParserResultImpl::bufferMeta(const std::string& name, runtime::TypeMeta* meta)
	{
		_meta_map[name] = meta;
	}

	bool ParserResultImpl::hasBufferedMeta(const std::string& name)
	{
		return _meta_map.contains(name);
	}

	runtime::TypeMeta* ParserResultImpl::getBufferedMeta(const std::string& name)
	{
		return _meta_map.contains(name) ? _meta_map[name] : nullptr;
	}
}