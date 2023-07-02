#pragma once

/////////////////////
// MACROS         
////////////////////

#define EXPAND_MACRO(x) x
#define STRINGIFY_MACRO(x) STRINGIFY(x)
#define STRINGIFY(x) #x

#define CAT(Arg1, Arg2) CAT_INTERNAL(Arg1, Arg2)
#define CAT_INTERNAL(Arg1, Arg2) Arg1##Arg2

#define BIT(x) (1UL << x)
#define MiB (1UL << 20)

#ifdef APP_COMPILER_MSVC
#define FORCEINLINE __forceinline

#define PUSH_IGNORE_WARNING _Pragma("warning(push, 0)")
#define POP_IGNORE_WARNING _Pragma("warning(pop)")

#elif defined APP_COMPILER_GNUC
#define FORCEINLINE inline __attribute__((__always_inline__))

#define PUSH_IGNORE_WARNING \
_Pragma("GCC diagnostic push")\
_Pragma("GCC diagnostic ignored \"-Wall\"")\
_Pragma("GCC diagnostic ignored \"-Wextra\"")
_Pragma("GCC diagnostic ignored \"-Wshadow\"")
#define POP_IGNORE_WARNING _Pragma("GCC diagnostic pop")

#pragma GCC diagnostic ignored "-Wparentheses"

#elif defined APP_COMPILER_CLANG
#if __has_attribute(__always_inline__)
#define FORCEINLINE inline __attribute__((__always_inline__))

#define PUSH_IGNORE_WARNING \
_Pragma("clang diagnostic push")\
_Pragma("clang diagnostic ignored \"-Wall\"")\
_Pragma("clang diagnostic ignored \"-Wextra\"")
_Pragma("clang diagnostic ignored \"-Wshadow\"")
#define POP_IGNORE_WARNING _Pragma("clang diagnostic pop")

#pragma clang diagnostic ignored "-Wparentheses"
#else
#define FORCEINLINE inline
#endif

#endif

/* better avoid std::bind */
#define BIND_MEMBER_FUNCTION(function) [this](auto&&... args) -> decltype(auto) { return this->function(std::forward<decltype(args)>(args)...); }

#ifdef APP_COMPILER_MSVC
#pragma warning( disable : 26812 ) // enum/enum class
#pragma warning( disable : 26451 ) // Arithmetic overflow e.g. (size_t)(5ul + 6ul)
#endif


#define EMBEDDED_RESOURCE(name, type)												\
extern "C" {																		\
	extern int8_t _binary_##name##_##type##_start, _binary_##name##_##type##_end;	\
	static const int8_t& name##_start = _binary_##name##_##type##_start;			\
	static const int8_t& name##_end = _binary_##name##_##type##_end;				\
}

#define MAX_FRAMES_IN_FLIGHT 3

#define USE_MULTISAMPLE 0

#define PRINT_BENCHMARK 0
#define PRINT_BENCHMARK_VERBOSE 0

#ifndef INVERT_VIEWPORT
#define INVERT_VIEWPORT 0
#endif

#ifndef USE_ASTC
#define USE_ASTC 0
#endif

/* not a lot of sense in enabling it for a 2D game */
#ifndef ENABLE_ANISOTROPY
#define ENABLE_ANISOTROPY 0
#endif

#ifndef VIEWPORT_FRAME_TIME
#define VIEWPORT_FRAME_TIME 1
#endif

#ifndef RENDER_BOXCOLLIDER
#define RENDER_BOXCOLLIDER 0
#endif

#ifdef APP_WINDOWS
#define NOMINMAX 
#endif

/////////////////////
// enums
////////////////////

using byte = uint8_t;
using word = uint16_t;
using dword = uint32_t;
using quadword = uint64_t;

namespace gs {

	enum class queue_family { graphics = 0, compute, transfer, present };
	enum class projection { perspective = 0, orthographic = 1 };
	enum class axis { x, y, z };

	struct extent2d
	{ 
		extent2d() = default;
		extent2d(uint32_t x, uint32_t y) : width(x), height(y) {}
		uint32_t width = 0, height = 0;

		bool operator==(const extent2d& other) const { return (width == other.width && height == other.height); }
		bool operator!=(const extent2d& other) const { return (width != other.width || height != other.height); }
	};

	struct extent3d
	{ 
		extent3d() = default;
		extent3d(uint32_t x, uint32_t y, uint32_t z) : width(x), height(y), depth(z) {}
		uint32_t width = 0, height = 0, depth = 0;

		bool operator==(const extent3d& other) const { return (width == other.width && height == other.height && depth == other.depth); }
		bool operator!=(const extent3d& other) const { return (width != other.width || height != other.height || depth != other.depth); }
	};

}