#include <cstring>

#include "screenCapture.h"
#include "openGL_Caps.h"
#include "../renderBackend.h"
#include <TFE_System/system.h>
#include <TFE_Asset/imageAsset.h>	// For image saving, this should be refactored...
#include <TFE_Asset/gifWriter.h>
#include <GL/glew.h>
#include <assert.h>

#ifdef _DEBUG
	#define CHECK_GL_ERROR checkGlError();
#else
	#define CHECK_GL_ERROR
#endif
#define CAPTURE_FRAME_DELAY 3
#define FLUSH_READ_COUNT 1
#define RECORD_FLUSH_COUNT 1

namespace
{
	void checkGlError()
	{
		GLenum error = glGetError();
		if (error == GL_NO_ERROR) { return; }

		TFE_System::logWrite(LOG_ERROR, "Dynamic Texture", "GL Error = %x", error);
		assert(error == GL_NO_ERROR);
	}
}

ScreenCapture::~ScreenCapture()
{
	freeBuffers();
}

bool ScreenCapture::create(u32 width, u32 height, u32 bufferCount)
{
	m_width  = width;
	m_height = height;

	return changeBufferCount(bufferCount);
}

void ScreenCapture::resize(u32 newWidth, u32 newHeight)
{
	if (newWidth == m_width && newHeight == m_height) { return; }
	// Flush any pending screenshots.
	update(true);

	m_width = newWidth;
	m_height = newHeight;
	changeBufferCount(m_bufferCount, true);
}

bool ScreenCapture::changeBufferCount(u32 newBufferCount, bool forceRealloc/* = false*/)
{
	if (newBufferCount == m_bufferCount && !forceRealloc) { return false; }
	freeBuffers();

	m_bufferCount = newBufferCount;
	m_writeBuffer = 0;

	const size_t bufferSize = m_width * m_height * 4;
	if (OpenGL_Caps::supportsPbo())
	{
		m_stagingBuffers = new u32[m_bufferCount];
		glGenBuffers(m_bufferCount, m_stagingBuffers);
		for (u32 i = 0; i < m_bufferCount; i++)
		{
			glBindBuffer(GL_PIXEL_PACK_BUFFER, m_stagingBuffers[i]);
			glBufferData(GL_PIXEL_PACK_BUFFER, bufferSize, nullptr, GL_STREAM_READ);
		}
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
		CHECK_GL_ERROR
	}

	delete[] m_captures;
	m_captures = new Capture[m_bufferCount];
	m_captureHead = 0;
	m_captureCount = 0;
	for (u32 i = 0; i < m_bufferCount; i++)
	{
		m_captures[i].bufferIndex = 0;
		m_captures[i].frame = -1;
		m_captures[i].imageData.resize(bufferSize);
	}

	delete[] m_readIndex;
	m_readIndex = new u32[m_bufferCount];
	m_readCount = 0;

	return m_bufferCount;
}

void ScreenCapture::update(bool flush)
{
	// Automatically capture frames when recording.
	if (m_recordingStarted)
	{
		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);

		f64 recordingTime = 0.0;
		if (displayInfo.refreshRate)
		{
			recordingTime = f64(m_frame - m_recordingFrameStart) / f64(displayInfo.refreshRate);
		}
		else if (m_recordingTimeStart == 0.0f)
		{
			m_recordingTimeStart = TFE_System::getTime();
			recordingTime = 0.0f;
		}
		else
		{
			recordingTime = f32(TFE_System::getTime() - m_recordingTimeStart);
		}

		f64 recordingFrame = floor(recordingTime * m_recordingFramerate);
		if (m_recordingFrameLast != recordingFrame)
		{
			captureFrame("");
			m_recordingFrameLast = recordingFrame;
		}
	}

	// Handle capture
	m_frame++;
	if (!m_captureCount) { return; }
	
	bool popHead = false;
	if (!OpenGL_Caps::supportsPbo())
	{
		glReadBuffer(GL_BACK);
		glReadPixels(0, 0, m_width, m_height, GL_RGBA, GL_UNSIGNED_BYTE, m_captures[m_captureHead].imageData.data());
		popHead = true;
	}
	else if (flush || m_frame > (m_captures[m_captureHead].frame + CAPTURE_FRAME_DELAY))
	{
		// Copy from staging data to read buffer [readBuffer].
		glBindBuffer(GL_PIXEL_PACK_BUFFER, m_stagingBuffers[m_captures[m_captureHead].bufferIndex]);
		void* imageData = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, m_captures[m_captureHead].imageData.size(), GL_MAP_READ_BIT);
		memcpy(m_captures[m_captureHead].imageData.data(), imageData, m_captures[m_captureHead].imageData.size());
		glUnmapBuffer(GL_PIXEL_PACK_BUFFER);

		// Cleanup.
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
		CHECK_GL_ERROR

		popHead = true;
	}

	if (popHead)
	{
		m_readIndex[m_readCount++] = m_captureHead;
		m_captureHead = (m_captureHead + 1) % m_bufferCount;
		m_captureCount--;
	}

	// Handle write to disk or adding a frame to the recording.
	if (m_recordingStarted && (m_readCount >= RECORD_FLUSH_COUNT || flush))
	{
		// Save the images.
		recordImages();
	}
	else if (!m_recordingStarted && (m_readCount >= FLUSH_READ_COUNT || flush))
	{
		writeFramesToDisk();
	}
}

void ScreenCapture::captureFrame(const char* outputPath)
{
	if (m_bufferCount > 1 && OpenGL_Caps::supportsPbo())
	{
		// Async copy from GPU data to staging buffer [writeBuffer].
		glBindBuffer(GL_PIXEL_PACK_BUFFER, m_stagingBuffers[m_writeBuffer]);
		glReadPixels(0, 0, m_width, m_height, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	}
	const u32 index = (m_captureHead + m_captureCount) % m_bufferCount;
	m_captures[index].bufferIndex = m_writeBuffer;
	m_captures[index].outputPath = outputPath;
	m_captures[index].frame = m_frame;
	m_captureCount++;

	// Update buffer indices.
	m_writeBuffer = (m_writeBuffer + 1) % m_bufferCount;
}

void ScreenCapture::beginRecording(const char* path)
{
	m_recordingStarted = true;
	m_recordingFrame = 0;
	m_recordingFrameStart = m_frame;
	m_recordingTimeStart = 0.0;
	m_recordingFrameLast = -1.0;

	TFE_GIF::startGif(path, m_width, m_height, (u32)m_recordingFramerate);
}

void ScreenCapture::endRecording()
{
	update(true);
	m_recordingStarted = false;

	TFE_GIF::write();
}

void ScreenCapture::writeFramesToDisk()
{
	if (!m_readCount) { return; }

	for (u32 i = 0; i < m_readCount; i++)
	{
		const u32 index = m_readIndex[i];
		TFE_Image::writeImage(m_captures[index].outputPath.c_str(), m_width, m_height, (u32*)m_captures[index].imageData.data());
	}
	m_readCount = 0;
}

void ScreenCapture::recordImages()
{
	if (!m_readCount) { return; }

	for (u32 i = 0; i < m_readCount; i++)
	{
		const u32 index = m_readIndex[i];
		TFE_GIF::addFrame(m_captures[index].imageData.data());
	}
	m_readCount = 0;
}

void ScreenCapture::freeBuffers()
{
	if (OpenGL_Caps::supportsPbo())
	{
		if (m_bufferCount)
		{
			glDeleteBuffers(m_bufferCount, m_stagingBuffers);
		}
		delete[] m_stagingBuffers;
	}
	delete[] m_readIndex;
	m_readIndex = nullptr;

	m_bufferCount = 0;
}
