#pragma once

#if defined __LUX_PARSE_TIME__
#	define LUX_META(...) __attribute__((annotate(#__VA_ARGS__)))

// Token-paste helpers — generate a unique proxy name per macro expansion.
#	define LUX_REFLECT_DETAIL_CAT_IMPL(a, b) a##b
#	define LUX_REFLECT_DETAIL_CAT(a, b)      LUX_REFLECT_DETAIL_CAT_IMPL(a, b)

// Non-intrusive reflection of an EXTERNAL (third-party) type you cannot annotate.
// Write this in YOUR own code, at global or namespace scope (no trailing ';'):
//
//     LUX_REFLECT_EXTERNAL(serializable, ::ext::Color)      // struct / class / union
//     LUX_REFLECT_EXTERNAL_ENUM(serializable, ::ext::Mode)  // enum
//
// It emits a tiny annotated "proxy" record (only at parse time) that references
// the foreign type through `using _lux_target = T;`. The parser detects the
// "lux_external" sentinel, follows _lux_target to the real foreign AST node, and
// runs the normal record/enum parser on it — producing meta identical to the
// intrusive LUX_META path. The proxy itself is never reflected or emitted.
//
// Use __VA_ARGS__ for the type so a template argument list such as
// `::a::b<c, d>` (which contains commas) survives macro argument splitting.
#	define LUX_REFLECT_EXTERNAL(MARKER, ...)                                   \
		struct __attribute__((annotate("lux_external; " #MARKER)))             \
		LUX_REFLECT_DETAIL_CAT(_lux_ext_reflect_, __COUNTER__)                 \
		{ using _lux_target = __VA_ARGS__; };

// Enums flow through the same proxy; the parser dispatches record vs. enum from
// the resolved cursor kind, so this is just a readable alias.
#	define LUX_REFLECT_EXTERNAL_ENUM(MARKER, ...) LUX_REFLECT_EXTERNAL(MARKER, __VA_ARGS__)
#else
#	define LUX_META(...)
#	define LUX_REFLECT_EXTERNAL(MARKER, ...)
#	define LUX_REFLECT_EXTERNAL_ENUM(MARKER, ...)
#endif
