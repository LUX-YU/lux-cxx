#pragma once
/*
 * Copyright (c) 2025 Chenhui Yu
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

// Enable Attributes
// 

#include <cstddef>
#define LUX_REF_MARK_PREFIX "LUX::META;"

#if defined __LUX_PARSE_TIME__
#define LUX_REFL(...) __attribute__((annotate(LUX_REF_MARK_PREFIX #__VA_ARGS__)))
#else
#define LUX_REFL(...)
#endif

namespace lux::cxx::dref
{
    enum class EMetaType
    {
        ClASS,
        STRUCT,
        ENUMERATOR
    };

    enum class EVisibility
    {
        INVALID,
        PUBLIC,
        PROTECTED,
        PRIVATE
    };

    struct FieldInfo
    {
        const char* name;
        size_t      offset;
        EVisibility visibility;
        size_t      index;
    };

    template <typename T> class type_meta;
}
