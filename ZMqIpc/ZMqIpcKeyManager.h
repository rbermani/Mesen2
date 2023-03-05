#pragma once
#include <unordered_map>
#include <vector>
#include <thread>
#include "Linux/LinuxKeyManager.h"
#include "Utilities/AutoResetEvent.h"
//#include "Shared/Interfaces/IKeyManager.h"
#include "Shared/KeyDefinitions.h"

#ifdef _WIN32
using KeyAbstraction = WindowsKeyManager;
#else
using KeyAbstraction = LinuxKeyManager;
#endif

class LinuxGameController;
class Emulator;

class ZMqIpcKeyManager : public KeyAbstraction
{
private:
	static constexpr int BaseMouseButtonIndex = 0x200;
	static constexpr int BaseGamepadIndex = 0x1000;

	//Emulator* _emu;
	std::vector<shared_ptr<LinuxGameController>> m_controllers;

	vector<KeyDefinition> m_keyDefinitions;
	bool m_keyState[0x205];
	//std::unordered_map<uint16_t, string> _keyNames;
	//std::unordered_map<string, uint16_t> _keyCodes;

	bool m_disableAllKeys;

	void StartUpdateDeviceThread();
	void CheckForGamepads(bool logInformation);

public:
	ZMqIpcKeyManager(Emulator* emu);
	virtual ~ZMqIpcKeyManager();

	void RefreshState();
	bool IsKeyPressed(uint16_t key);
	bool IsMouseButtonPressed(MouseButton button);
	std::vector<uint16_t> GetPressedKeys();
	string GetKeyName(uint16_t key);
	uint16_t GetKeyCode(string keyName);

	void UpdateDevices();
	bool SetKeyState(uint16_t scanCode, bool state);
	void ResetKeyState();

	void SetDisabled(bool disabled);
};
