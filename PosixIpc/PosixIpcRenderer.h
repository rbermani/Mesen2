#pragma once

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>

#include "Linux/SdlRenderer.h"
#include "Shared/Video/SoftwareRenderer.h"
#include "Core/Shared/Interfaces/IRenderingDevice.h"
#include "Utilities/SimpleLock.h"
#include "Core/Shared/Video/VideoRenderer.h"
#include "Core/Shared/RenderedFrame.h"
#include "zmq.hpp"

#ifdef __APPLE__
using RenderAbstraction = SoftwareRenderer;
#else
using RenderAbstraction = SdlRenderer;
#endif

class PosixIpcRenderer : public RenderAbstraction
{
private:
	inline static const std::string ENDPOINT_NAME = "ipc:///tmp/mesenPosixIpcRenderer";
	inline static const uint8_t BYTES_PER_PIXEL = 4;
	inline static const uint8_t PROTOCOL_DIMENSION_FIELDS = 2;

	static SimpleLock m_frameLock;
	uint32_t* m_frameBuffer = nullptr;

	zmq::context_t m_Ctx;
	zmq::socket_t m_Sock;
	bool m_SocketCreationSucceeded = false;

	uint32_t m_requiredHeight = 0;
	uint32_t m_requiredWidth = 0;

	void LogPosixIpcError(const char* msg);

public:
	PosixIpcRenderer(Emulator* emu, void* windowHandle);
	~PosixIpcRenderer();

	void ClearFrame() override;
	void UpdateFrame(RenderedFrame& frame) override;
	void Render(RenderSurfaceInfo& emuHud, RenderSurfaceInfo& scriptHud) override;
	void Reset() override;

	void SetExclusiveFullscreenMode(bool fullscreen, void* windowHandle) override;
};
