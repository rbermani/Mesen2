#include "pch.h"
#include "Shared/Video/SoftwareRenderer.h"
#include "Shared/Video/VideoRenderer.h"
#include "Shared/RenderedFrame.h"
#include "Shared/Emulator.h"
#include "Shared/NotificationManager.h"

SoftwareRenderer::SoftwareRenderer(Emulator* emu, void* windowHandle)
{
	_emu = emu;
	SetScreenSize(256, 240);
	_emu->GetVideoRenderer()->RegisterRenderingDevice(this);
}

SoftwareRenderer::~SoftwareRenderer()
{
	delete[] _textureBuffer[0];
	delete[] _textureBuffer[1];
}

void SoftwareRenderer::SetScreenSize(uint32_t width, uint32_t height)
{
	_frameWidth = width;
	_frameHeight = height;

	delete[] _textureBuffer[0];
	delete[] _textureBuffer[1];
	_textureBuffer[0] = new uint32_t[width * height];
	_textureBuffer[1] = new uint32_t[width * height];
}

void SoftwareRenderer::UpdateFrame(RenderedFrame& frame)
{
	if(_frameWidth != frame.Width || _frameHeight != frame.Height) {
		auto lock = _frameLock.AcquireSafe();
		if(_frameWidth != frame.Width || _frameHeight != frame.Height) {
			SetScreenSize(frame.Width, frame.Height);
		}
	}

	auto lock = _textureLock.AcquireSafe();
	memcpy(_textureBuffer[0], frame.FrameBuffer, frame.Width * frame.Height * sizeof(uint32_t));
	_needSwap = true;
}

void SoftwareRenderer::ClearFrame()
{
	//Clear current output and display black frame
	auto lock = _textureLock.AcquireSafe();
	memset(_textureBuffer[0], 0, _frameWidth * _frameHeight * sizeof(uint32_t));
	_needSwap = true;
}

struct SoftwareRendererSurface
{
	uint32_t* Buffer = nullptr;
	uint32_t Width = 0;
	uint32_t Height = 0;
};

struct SoftwareRendererFrame
{
	SoftwareRendererSurface Frame;
	SoftwareRendererSurface EmuHud;
	SoftwareRendererSurface ScriptHud;
};

void SoftwareRenderer::Render(RenderSurfaceInfo& emuHud, RenderSurfaceInfo& scriptHud)
{
	auto lock = _frameLock.AcquireSafe();
	
	if(_needSwap) {
		_needSwap = false;
		auto textureLock = _textureLock.AcquireSafe();
		std::swap(_textureBuffer[0], _textureBuffer[1]);
	}

	SoftwareRendererFrame frame = {
		{ _textureBuffer[1], _frameWidth, _frameHeight },
		{ emuHud.Buffer, emuHud.Width, emuHud.Height },
		{ scriptHud.Buffer, scriptHud.Width, scriptHud.Height }
	};

	_emu->GetNotificationManager()->SendNotification(ConsoleNotificationType::RefreshSoftwareRenderer, &frame);
}

void SoftwareRenderer::Reset()
{
}

void SoftwareRenderer::SetExclusiveFullscreenMode(bool fullscreen, void* windowHandle)
{
	//not supported
}
