#pragma once
#include <DXL2_System/types.h>
#include <vector>
#include <string>

class Stream
{
public:
	enum Origin
	{
		ORIGIN_START = 0,
		ORIGIN_END,
		ORIGIN_CURRENT,
		ORIGIN_COUNT
	};
public:
	Stream()  {};
	~Stream() {};

	virtual void seek(u32 offset, Origin origin=ORIGIN_START)=0;
	virtual size_t getLoc()=0;
	virtual size_t getSize()=0;

	virtual void read(s8*  ptr, u32 count=1)=0;
	virtual void read(u8*  ptr, u32 count=1)=0;
	virtual void read(s16* ptr, u32 count=1)=0;
	virtual void read(u16* ptr, u32 count=1)=0;
	virtual void read(s32* ptr, u32 count=1)=0;
	virtual void read(u32* ptr, u32 count=1)=0;
	virtual void read(s64* ptr, u32 count=1)=0;
	virtual void read(u64* ptr, u32 count=1)=0;
	//virtual void read(f16* ptr, u32 count=1)=0;
	virtual void read(f32* ptr, u32 count=1)=0;
	virtual void read(f64* ptr, u32 count=1)=0;
	virtual void read(std::string* ptr, u32 count=1)=0;
	virtual void readBuffer(void* ptr, u32 size, u32 count=1)=0;

	virtual void write(const s8*  ptr, u32 count=1)=0;
	virtual void write(const u8*  ptr, u32 count=1)=0;
	virtual void write(const s16* ptr, u32 count=1)=0;
	virtual void write(const u16* ptr, u32 count=1)=0;
	virtual void write(const s32* ptr, u32 count=1)=0;
	virtual void write(const u32* ptr, u32 count=1)=0;
	virtual void write(const s64* ptr, u32 count=1)=0;
	virtual void write(const u64* ptr, u32 count=1)=0;
	//virtual void write(const f16* ptr, u32 count=1)=0;
	virtual void write(const f32* ptr, u32 count=1)=0;
	virtual void write(const f64* ptr, u32 count=1)=0;
	virtual void write(const std::string* ptr, u32 count=1)=0;
	virtual void writeBuffer(const void* ptr, u32 size, u32 count=1)=0;

	virtual void writeString(const char* fmt, ...)=0;
};