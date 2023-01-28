#pragma once
//////////////////////////////////////////////////////////////////////
// Dynamic multi-buffered texture.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_RenderBackend/textureGpu.h>
#include <vector>
#include <string>

class ScreenCapture
{
public:
	ScreenCapture() : m_bufferCount(0), m_writeBuffer(0), m_readIndex(nullptr), m_stagingBuffers(nullptr), m_frame(0), m_readCount(0), m_recordingFrame(0), m_captures(0) {}
	~ScreenCapture();

	bool create(u32 width, u32 height, u32 bufferCount);
	void resize(u32 newWidth, u32 newHeight);
	bool changeBufferCount(u32 newBufferCount, bool forceRealloc=false);

	void update(bool flush = false);
	void captureFrame(const char* outputPath);

	void captureFrontBufferToMemory(u32* mem);

	void beginRecording(const char* path);
	void endRecording();
	
private:
	struct Capture
	{
		std::string outputPath;
		std::vector<u8> imageData;

		u32 bufferIndex;
		s32 frame;
	};

	u32 m_bufferCount;
	u32 m_captureHead;
	u32 m_captureCount;
		
	u32* m_readIndex;
	u32 m_readCount;

	u32 m_writeBuffer;
	s32 m_frame;
	u32 m_width;
	u32 m_height;

	bool m_recordingStarted;
	u32  m_recordingFrame;

	f32 m_recordingFramerate = 15.0;
	s32 m_recordingFrameStart = 0;
	f64 m_recordingTimeStart = 0.0;
	f64 m_recordingFrameLast = 0.0;

	Capture* m_captures;
	u32* m_stagingBuffers;

private:
	void freeBuffers();
	void writeFramesToDisk();
	void recordImages();
};
