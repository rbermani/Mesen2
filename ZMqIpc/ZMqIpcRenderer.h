#pragma once

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

class ZMqIpcRenderer : public RenderAbstraction
{
private:
	inline static const std::string ENDPOINT_NAME = "ipc:///tmp/mesenZMqIpcRenderer";
	inline static const uint8_t BYTES_PER_PIXEL = 2;
	inline static const uint8_t PROTOCOL_DIMENSION_FIELDS = 2;

	static SimpleLock m_frameLock;
	uint16_t* m_frameBuffer = nullptr;

	zmq::context_t m_Ctx;
	zmq::socket_t m_Sock;
	bool m_SocketCreationSucceeded = false;

	uint16_t m_requiredHeight = 0;
	uint16_t m_requiredWidth = 0;

	void LogPosixIpcError(const char* msg);

public:
	ZMqIpcRenderer(Emulator* emu, void* windowHandle);
	~ZMqIpcRenderer();

	void ClearFrame() override;
	void UpdateFrame(RenderedFrame& frame) override;
	void Render(RenderSurfaceInfo& emuHud, RenderSurfaceInfo& scriptHud) override;
	void Reset() override;

	void SetExclusiveFullscreenMode(bool fullscreen, void* windowHandle) override;
};
