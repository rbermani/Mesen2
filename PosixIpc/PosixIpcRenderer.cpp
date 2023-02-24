
#include <assert.h>

#include "Core/Debugger/Debugger.h"
#include "Core/Shared/Emulator.h"
#include "Core/Shared/Video/VideoRenderer.h"
#include "Core/Shared/Video/VideoDecoder.h"
#include "Core/Shared/EmuSettings.h"
#include "Core/Shared/MessageManager.h"
#include "Core/Shared/RenderedFrame.h"


#include "PosixIpcRenderer.h"

SimpleLock PosixIpcRenderer::m_frameLock;

PosixIpcRenderer::PosixIpcRenderer(Emulator* emu, void* windowHandle) : m_Sock(m_Ctx, zmq::socket_type::pair) , RenderAbstraction(emu, windowHandle)
{
	MessageManager::Log("PosixIpcRenderer constructor");

	m_frameBuffer = nullptr;
	m_requiredWidth = 256;
	m_requiredHeight = 240;

   //zmq::socket_t m_Sock(m_Ctx, zmq::socket_type::pair);
	try {
		m_Sock.connect(ENDPOINT_NAME);
	} catch (const error_t& ex) {
		LogPosixIpcError("Socket creation error");
		m_SocketCreationSucceeded = false;
		return;
	}
   //m_DataSocket = socket(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK, 0);
	// m_DataSocket = socket(AF_UNIX, SOCK_DGRAM, 0);
   // if (m_DataSocket == -1) {
	// 	LogPosixIpcError("Socket creation error");
   // 	return;
   // }

	// int flags = fcntl(m_DataSocket, F_GETFL, 0);
	// if (flags == -1) {
	// 	LogPosixIpcError("fcntl() get failed");
	// 	return;
	// }

	// if ((fcntl(m_DataSocket, F_SETFL, flags | O_NONBLOCK)) == -1) {
	// 	LogPosixIpcError("fcntl() set failed");
	// 	return;
	// }
	// memset(&m_TargetAddr, 0, sizeof(struct sockaddr_un));
   // m_TargetAddr.sun_family = AF_UNIX;
   // strncpy(m_TargetAddr.sun_path, SOCKET_NAME.c_str(), sizeof(m_TargetAddr.sun_path) - 1);

	m_SocketCreationSucceeded = true;
}

PosixIpcRenderer::~PosixIpcRenderer()
{
 	delete[] m_frameBuffer;
	m_Sock.close();
	m_Ctx.shutdown();
}

void PosixIpcRenderer::LogPosixIpcError(const char* msg)
{
	MessageManager::Log(msg);
}

void PosixIpcRenderer::SetExclusiveFullscreenMode(bool fullscreen, void* windowHandle)
{
	// PosixIpc cannot use FullscreenMode
}

void PosixIpcRenderer::Reset()
{
	RenderAbstraction::Reset();
}

void PosixIpcRenderer::ClearFrame()
{
	RenderAbstraction::ClearFrame();

	if(m_frameBuffer == nullptr) {
		return;
	}

	auto lock = m_frameLock.AcquireSafe();
	memset(m_frameBuffer, 0, m_requiredWidth * m_requiredHeight * BYTES_PER_PIXEL);
}

void PosixIpcRenderer::UpdateFrame(RenderedFrame& frame)
{
	RenderAbstraction::UpdateFrame(frame);
	if (!m_SocketCreationSucceeded) {
		return;
	}
	auto lock = m_frameLock.AcquireSafe();
	if(m_frameBuffer == nullptr || m_requiredWidth != frame.Width || m_requiredHeight != frame.Height) {
		printf("height %d, width %d nheight %d, nwidth %d\n", m_requiredHeight, m_requiredWidth, frame.Height, frame.Width );
		m_requiredWidth = frame.Width;
		m_requiredHeight = frame.Height;

		delete[] m_frameBuffer;
		m_frameBuffer = new uint32_t[frame.Width * frame.Height];
		memset(m_frameBuffer, 0, frame.Width * frame.Height * BYTES_PER_PIXEL);
	}

	memcpy(m_frameBuffer, frame.FrameBuffer, frame.Width * frame.Height * BYTES_PER_PIXEL);


	size_t frameBufferLen = (m_requiredHeight * m_requiredWidth);
	size_t OutgoingBufferLen = frameBufferLen + PROTOCOL_DIMENSION_FIELDS;
	auto OutgoingPacket = std::make_unique<uint32_t[]>(OutgoingBufferLen);

	OutgoingPacket[0] = m_requiredHeight;
	OutgoingPacket[1] = m_requiredWidth;

	memcpy(&OutgoingPacket[2], m_frameBuffer, frameBufferLen);
	zmq::message_t outgoing(OutgoingPacket.get(), OutgoingBufferLen*(sizeof(uint32_t)));
	zmq::send_result_t result;
	try {
		result = m_Sock.send(outgoing, zmq::send_flags::dontwait);
	} catch (const error_t& ex) {
		LogPosixIpcError("Socket write failed");
		m_SocketCreationSucceeded = false;
	}

	// if ((result = sendto(m_DataSocket,
	// 		OutgoingPacket.get(),
	// 		(OutgoingBufferLen*sizeof(uint32_t)), 0,
	// 		reinterpret_cast<sockaddr*>(&m_TargetAddr),
	// 		sizeof(m_TargetAddr)
	// 	)) == -1) {
	// 		if (errno != EWOULDBLOCK || errno != EAGAIN) {
	// 			//printf("Socket write failed %d", errno);
	// 			LogPosixIpcError("Socket write failed");
	// 			m_SocketCreationSucceeded = false;
	// 		}
	// }
}

void PosixIpcRenderer::Render(RenderSurfaceInfo& emuHud, RenderSurfaceInfo& scriptHud)
{
	RenderAbstraction::Render(emuHud, scriptHud);
}
