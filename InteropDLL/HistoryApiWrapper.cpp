#include "Common.h"
#include "Core/Shared/Emulator.h"
#include "Core/Shared/EmuSettings.h"
#include "Core/Shared/RewindManager.h"
#include "Core/Shared/HistoryViewer.h"
#include "Core/Shared/Interfaces/IRenderingDevice.h"
#include "Core/Shared/Interfaces/IAudioDevice.h"
#include "Core/Shared/Video/VideoRenderer.h"
#include "Core/Shared/Audio/SoundMixer.h"
#include "Core/Shared/Movies/MovieManager.h"
#include "Shared/Video/SoftwareRenderer.h"
#include "InteropNotificationListeners.h"

#ifdef __ZMQ_IPC_RENDERER__
	#include "ZMqIpc/ZMqIpcRenderer.h"
	#include "Linux/SdlSoundManager.h"
	#include "Linux/LinuxKeyManager.h"
#elif _WIN32
	#include "Windows/Renderer.h"
	#include "Windows/SoundManager.h"
	#include "Windows/WindowsKeyManager.h"
#else
	#include "Linux/SdlRenderer.h"
	#include "Linux/SdlSoundManager.h"
	#include "Linux/LinuxKeyManager.h"
#endif

extern unique_ptr<Emulator> _emu;

unique_ptr<Emulator> _historyPlayer;
unique_ptr<IRenderingDevice> _historyRenderer;
unique_ptr<IAudioDevice> _historySoundManager;

HistoryViewer* _historyViewer = nullptr;

static InteropNotificationListeners _listeners;

extern "C"
{
	DllExport bool __stdcall HistoryViewerEnabled()
	{
		return _emu->GetRewindManager()->HasHistory();
	}

	DllExport void __stdcall HistoryViewerRelease()
	{
		_historyPlayer->Release();
		_historyRenderer.reset();
		_historySoundManager.reset();
		_historyPlayer.reset();
		_historyViewer = nullptr;
	}

	DllExport void __stdcall HistoryViewerInitialize(void* windowHandle, void* viewerHandle)
	{
		_historyPlayer.reset(new Emulator());
		_historyPlayer->Initialize();
		_historyPlayer->GetSettings()->CopySettings(*_emu->GetSettings());

		_historyViewer = _historyPlayer->GetHistoryViewer();
		if(!_historyViewer->Initialize(_emu.get())) {
			HistoryViewerRelease();
			return;
		}

		_historyPlayer->GetSettings()->GetEmulationConfig().EmulationSpeed = 100;

#ifdef __ZMQ_IPC_RENDERER__
		_historyRenderer.reset(new ZMqIpcRenderer(_emu.get(), viewerHandle));
		_historySoundManager.reset(new SdlSoundManager(_historyPlayer.get()));
#elif _WIN32
		_historyRenderer.reset(new Renderer(_historyPlayer.get(), (HWND)viewerHandle));
		_historySoundManager.reset(new SoundManager(_historyPlayer.get(), (HWND)windowHandle));
#elif __APPLE__
		_historyRenderer.reset(new SoftwareRenderer(_historyPlayer.get()));
		_historySoundManager.reset(new SdlSoundManager(_historyPlayer.get()));
#else
		_historyRenderer.reset(new SdlRenderer(_historyPlayer.get(), viewerHandle));
		_historySoundManager.reset(new SdlSoundManager(_historyPlayer.get()));
#endif
	}

	DllExport HistoryViewerState __stdcall HistoryViewerGetState()
	{
		return _historyViewer ? _historyViewer->GetState() : HistoryViewerState {};
	}

	DllExport void __stdcall HistoryViewerSetOptions(HistoryViewerOptions options)
	{
		if(_historyViewer) {
			_historyViewer->SetOptions(options);
		}
	}

	DllExport bool __stdcall HistoryViewerCreateSaveState(const char* outputFile, uint32_t position)
	{
		return _historyViewer ? _historyViewer->CreateSaveState(outputFile, position) : false;
	}

	DllExport bool __stdcall HistoryViewerSaveMovie(const char* movieFile, uint32_t startPosition, uint32_t endPosition)
	{
		return _historyViewer ? _historyViewer->SaveMovie(movieFile, startPosition, endPosition) : false;
	}

	DllExport void __stdcall HistoryViewerResumeGameplay(uint32_t resumePosition)
	{
		if(_historyViewer) {
			_historyViewer->ResumeGameplay(resumePosition);
		}
	}

	DllExport void __stdcall HistoryViewerSetPosition(uint32_t seekPosition)
	{
		if(_historyViewer) {
			_historyViewer->SeekTo(seekPosition);
		}
	}

	DllExport INotificationListener* __stdcall HistoryViewerRegisterNotificationCallback(NotificationListenerCallback callback)
	{
		return _listeners.RegisterNotificationCallback(callback, _historyPlayer.get());
	}

	DllExport void __stdcall HistoryViewerUnregisterNotificationCallback(INotificationListener* listener)
	{
		_listeners.UnregisterNotificationCallback(listener);
	}
}