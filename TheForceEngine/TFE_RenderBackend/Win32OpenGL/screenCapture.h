#pragma once
//////////////////////////////////////////////////////////////////////
// Dynamic multi-buffered texture.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_RenderBackend/textureGpu.h>
#include <vector>
#include <string>


// Used for capturing screenshots and GIFs. Also draws UI when capture
// starts or ends.
class ScreenCapture
{
public:
	enum ScreenCaptureState
	{
		// The system is inactive.
		IDLE,
		// We are counting down before starting to capture a GIF.
		COUNTDOWN,
		// We are currently capturing a GIF.
		RECORDING,
		// We are displaying a confirmation message about the captured GIF.
		CONFIRMATION
	};

	ScreenCapture() : m_bufferCount(0), m_captureHead(0), m_captureCount(0), m_readIndex(nullptr), m_readCount(0), m_writeBuffer(0),
		m_frame(0), m_width(0), m_height(0), m_recordingFrame(0), m_captures(0), m_stagingBuffers(nullptr), m_state(IDLE) {}
	~ScreenCapture();

	bool create(u32 width, u32 height, u32 bufferCount);
	void resize(u32 newWidth, u32 newHeight);
	bool changeBufferCount(u32 newBufferCount, bool forceRealloc=false);

	void update(bool flush = false);
	void captureFrame(const char* outputPath);

	void captureFrontBufferToMemory(u32* mem);

	void beginRecording(const char* path, bool skipCountdown);
	void endRecording();
	
	ScreenCaptureState getState();
	bool wantsToDrawGui();

	void drawGui();
	
private:
	struct Capture
	{
		std::string outputPath;
		std::vector<u8> imageData;

		u32 bufferIndex;
		s32 frame;
	};

	const f32 COUNTDOWN_DURATION = 2; // In seconds.
	const f32 CONFIRMATION_WIDTH = 320;
	const f32 CONFIRMATION_DURATION = 4; // In seconds.

	u32 m_bufferCount;
	u32 m_captureHead;
	u32 m_captureCount;
		
	u32* m_readIndex;
	u32 m_readCount;

	u32 m_writeBuffer;
	s32 m_frame;
	u32 m_width;
	u32 m_height;

	ScreenCaptureState m_state;
	f64 m_countdownTimeStart = 0.0;
	bool m_showFullCountdown = true;
	bool m_countdownFinished = false;
	f64 m_confirmationTimeStart = 0.0;
	u32  m_recordingFrame;
	std::string m_capturePath;
	std::string m_confirmationMessage;

	s32 m_recordingFrameStart = 0;
	f64 m_recordingTimeStart = 0.0;
	f64 m_recordingFrameLast = 0.0;

	Capture* m_captures;
	u32* m_stagingBuffers;

private:
	// If `fullCountdown` is false, we'll just flash a brief message before capture starts.
	void startCountdown(bool fullCountdown);
	void startCapture();
	void freeBuffers();
	void writeFramesToDisk();
	void recordImages();
};
