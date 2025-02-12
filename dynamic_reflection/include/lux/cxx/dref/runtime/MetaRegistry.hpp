#pragma once
#include <lux/cxx/dref/runtime/MetaUnit.hpp>
#include <memory>

namespace lux::cxx::dref
{
	class DrefInformationRegistryImpl;
    class DrefInformationRegistry
    {
    public:
        DrefInformationRegistry();



    private:
		std::unique_ptr<DrefInformationRegistryImpl> _impl;
    };
}