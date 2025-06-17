#pragma once
#include "Common.hpp"

namespace lux::cxx::archtype {

    /**
     * @brief Checks if an archetype signature contains all bits 
     *        required by a query signature.
     * 
     * @param arch_sig The signature of the archetype.
     * @param query_sig The signature representing required components.
     * @return true if arch_sig has all bits set that query_sig has.
     */
    inline bool matchSignature(const Signature& arch_sig, const Signature& query_sig) {
        return (arch_sig & query_sig) == query_sig;
    }

} // namespace lux::ecs
