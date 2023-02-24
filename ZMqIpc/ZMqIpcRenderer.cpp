
#include <assert.h>

#include "Core/Debugger/Debugger.h"
#include "Core/Shared/Emulator.h"
#include "Core/Shared/Video/VideoRenderer.h"
#include "Core/Shared/Video/VideoDecoder.h"
#include "Core/Shared/EmuSettings.h"
#include "Core/Shared/MessageManager.h"
#include "Core/Shared/RenderedFrame.h"

#include "ZMqIpcRenderer.h"

SimpleLock ZMqIpcRenderer::m_frameLock;

ZMqIpcRenderer::ZMqIpcRenderer(Emulator* emu, void* windowHandle) : RenderAbstraction(emu, windowHandle), m_Sock(m_Ctx, zmq::socket_type::pair)
{
	MessageManager::Log("ZMqIpcRenderer constructor");

	m_frameBuffer = nullptr;
	m_requiredWidth = 256;
	m_requiredHeight = 240;

	try {
		m_Sock.connect(ENDPOINT_NAME);
	} catch (const zmq::error_t& ex) {
		LogPosixIpcError("Socket creation error");
		m_SocketCreationSucceeded = false;
		return;
	}

	m_SocketCreationSucceeded = true;
}

ZMqIpcRenderer::~ZMqIpcRenderer()
{
 	delete[] m_frameBuffer;
	m_Sock.close();
	m_Ctx.shutdown();
}

void ZMqIpcRenderer::LogPosixIpcError(const char* msg)
{
	MessageManager::Log(msg);
}

void ZMqIpcRenderer::SetExclusiveFullscreenMode(bool fullscreen, void* windowHandle)
{
	// PosixIpc cannot use FullscreenMode
}

void ZMqIpcRenderer::Reset()
{
	RenderAbstraction::Reset();
}

void ZMqIpcRenderer::ClearFrame()
{
	RenderAbstraction::ClearFrame();

	if(m_frameBuffer == nullptr) {
		return;
	}

	auto lock = m_frameLock.AcquireSafe();
	memset(m_frameBuffer, 0, ((m_requiredWidth * m_requiredHeight) + PROTOCOL_DIMENSION_FIELDS) * BYTES_PER_PIXEL);
}

void ZMqIpcRenderer::UpdateFrame(RenderedFrame& frame)
{
	RenderAbstraction::UpdateFrame(frame);
	if (!m_SocketCreationSucceeded) {
		return;
	}
	size_t numberOfPixels = (frame.Width * frame.Height);
	size_t arrayElements = (numberOfPixels + PROTOCOL_DIMENSION_FIELDS);
	auto lock = m_frameLock.AcquireSafe();
	if(m_frameBuffer == nullptr || m_requiredWidth != frame.Width || m_requiredHeight != frame.Height) {
		printf("height %d, width %d nheight %d, nwidth %d\n", m_requiredHeight, m_requiredWidth, frame.Height, frame.Width);
		m_requiredWidth = frame.Width;
		m_requiredHeight = frame.Height;

		delete[] m_frameBuffer;
		m_frameBuffer = new uint16_t[arrayElements];
	}

	m_frameBuffer[0] = m_requiredHeight;
	m_frameBuffer[1] = m_requiredWidth;
	// Convert bit depth from 32bpp to 16bpp
	uint32_t* pixelptr = static_cast<uint32_t*>(frame.FrameBuffer);
	for (size_t pixelCount = 0; pixelCount < (numberOfPixels); pixelCount++) {
		uint32_t* curPixel = pixelptr+pixelCount;
		if (curPixel) {
			uint32_t pixel32bpp = *curPixel;
			// 16bpp R5G6B5 format
			uint8_t red = (pixel32bpp >> 19) & 0x1f;	// Pack into 5 bits and mask
			uint8_t green = (pixel32bpp >> 10) & 0x3f;// Pack into 6 bits and mask
			uint8_t blue = (pixel32bpp >> 3) & 0x1f;	// Pack into 5 bits and mask
			uint16_t pixel16bpp = (red << 11) | (green << 5) | blue;
			m_frameBuffer[2+pixelCount] = pixel16bpp;
		}
	}

	try {
		zmq::message_t outgoing(m_frameBuffer, arrayElements*BYTES_PER_PIXEL);
		zmq::send_result_t result;
		result = m_Sock.send(outgoing, zmq::send_flags::dontwait);
	} catch (const zmq::error_t& ex) {
		LogPosixIpcError("Socket write failed");
		m_SocketCreationSucceeded = false;
	}
}

void ZMqIpcRenderer::Render(RenderSurfaceInfo& emuHud, RenderSurfaceInfo& scriptHud)
{
	RenderAbstraction::Render(emuHud, scriptHud);
}
