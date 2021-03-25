#pragma once

#include <stdint.h>
#include <math.h>
#include <float.h>
#include <atomic>

typedef uint64_t u64;
typedef int64_t s64;
typedef uint32_t u32;
typedef int32_t s32;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint8_t u8;
typedef int8_t s8;
typedef float f32;
typedef double f64;

typedef std::atomic<uint32_t> atomic_u32;
typedef std::atomic<int32_t>  atomic_s32;
typedef std::atomic<float>    atomic_f32;
typedef std::atomic<bool>     atomic_bool;

struct Vec2f
{
	union
	{
		struct { f32 x, z; };
		f32 m[2];
	};
};

struct Vec3f
{
	union
	{
		struct { f32 x, y, z; };
		f32 m[3];
	};
};

struct Vec4f
{
	union
	{
		struct { f32 x, y, z, w; };
		f32 m[4];
	};
};

struct Vec2i
{
	union
	{
		struct { s32 x, z; };
		s32 m[2];
	};
};

struct Vec3i
{
	union
	{
		struct { s32 x, y, z; };
		s32 m[3];
	};
};

struct Vec4i
{
	union
	{
		struct { s32 x, y, z, w; };
		s32 m[4];
	};
};

struct Mat3
{
	union
	{
		struct { Vec3f m0, m1, m2; };
		struct { Vec3f m[3]; };
	};
};

struct Mat4
{
	union
	{
		struct { Vec4f m0, m1, m2, m3; };
		struct { Vec4f m[4]; };
	};
};

#ifdef _WIN32
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#endif

#ifdef _WIN32
#define TFE_STDCALL __stdcall
#else
#define TFE_STDCALL
#endif

#define TFE_ARRAYSIZE(arr) (sizeof(arr)/sizeof(*arr))         // Size of a static C-style array. Don't use on pointers!

#define FLAG_BIT(bit) (1u << u32(bit))
#define SIGN_BIT(x) ((x)<0?1:0)

// Move to math
#define PI 3.14159265358979323846f
