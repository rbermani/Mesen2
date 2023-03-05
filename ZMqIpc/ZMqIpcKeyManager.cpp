#include <algorithm>
#include "ZMqIpcKeyManager.h"
#include "LinuxGameController.h"
#include "Utilities/FolderUtilities.h"
#include "Shared/Emulator.h"
#include "Shared/KeyDefinitions.h"

ZMqIpcKeyManager::ZMqIpcKeyManager(Emulator* emu) : KeyAbstraction(emu)
{
	ResetKeyState();
	m_keyDefinitions = KeyDefinition::GetSharedKeyDefinitions();

	vector<string> buttonNames = {
		"A", "B", "C", "X", "Y", "Z", "L1", "R1", "L2", "R2", "Select", "Start", "L3", "R3",
		"X+", "X-", "Y+", "Y-", "Z+", "Z-",
		"X2+", "X2-", "Y2+", "Y2-", "Z2+", "Z2-",
		"Right", "Left", "Down", "Up",
		"Right 2", "Left 2", "Down 2", "Up 2",
		"Right 3", "Left 3", "Down 3", "Up 3",
		"Right 4", "Left 4", "Down 4", "Up 4",
		"Trigger", "Thumb", "Thumb2", "Top", "Top2",
		"Pinkie", "Base", "Base2", "Base3", "Base4",
		"Base5", "Base6", "Dead"
	};

	for(int i = 0; i < 20; i++) {
		for(int j = 0; j < (int)buttonNames.size(); j++) {
			m_keyDefinitions.push_back({ "Pad" + std::to_string(i + 1) + " " + buttonNames[j], (uint32_t)(ZMqIpcKeyManager::BaseGamepadIndex + i * 0x100 + j) });
		}
	}

	m_disableAllKeys = false;
}

ZMqIpcKeyManager::~ZMqIpcKeyManager()
{
}

void ZMqIpcKeyManager::RefreshState()
{
	KeyAbstraction::RefreshState();
	// not implemented
}

bool ZMqIpcKeyManager::IsKeyPressed(uint16_t key)
{
	KeyAbstraction::IsKeyPressed(key);
	if(m_disableAllKeys || key == 0) {
		return false;
	}

	if(key >= ZMqIpcKeyManager::BaseGamepadIndex) {
		uint8_t gamepadPort = (key - ZMqIpcKeyManager::BaseGamepadIndex) / 0x100;
		uint8_t gamepadButton = (key - ZMqIpcKeyManager::BaseGamepadIndex) % 0x100;
		if(m_controllers.size() > gamepadPort) {
			return m_controllers[gamepadPort]->IsButtonPressed(gamepadButton);
		}
	} else if(key < 0x205) {
		return m_keyState[key] != 0;
	}
	return false;
}

bool ZMqIpcKeyManager::IsMouseButtonPressed(MouseButton button)
{
	return KeyAbstraction::IsMouseButtonPressed(button);
}

vector<uint16_t> ZMqIpcKeyManager::GetPressedKeys()
{
	KeyAbstraction::GetPressedKeys();
	vector<uint16_t> pressedKeys;
	for(size_t i = 0; i < m_controllers.size(); i++) {
		for(int j = 0; j <= 54; j++) {
			if(m_controllers[i]->IsButtonPressed(j)) {
				pressedKeys.push_back(ZMqIpcKeyManager::BaseGamepadIndex + i * 0x100 + j);
			}
		}
	}

	for(int i = 0; i < 0x205; i++) {
		if(m_keyState[i]) {
			pressedKeys.push_back(i);
		}
	}
	return pressedKeys;
}

string ZMqIpcKeyManager::GetKeyName(uint16_t key)
{
	return KeyAbstraction::GetKeyName(key);
}

uint16_t ZMqIpcKeyManager::GetKeyCode(string keyName)
{
	return KeyAbstraction::GetKeyCode(keyName);
}

void ZMqIpcKeyManager::UpdateDevices()
{
	KeyAbstraction::UpdateDevices();
	// not implemented
}

bool ZMqIpcKeyManager::SetKeyState(uint16_t scanCode, bool state)
{
	KeyAbstraction::SetKeyState(scanCode, state);
	if(scanCode < 0x205 && m_keyState[scanCode] != state) {
		m_keyState[scanCode] = state;
		return true;
	}
	return false;
}

void ZMqIpcKeyManager::ResetKeyState()
{
	KeyAbstraction::ResetKeyState();
}

void ZMqIpcKeyManager::SetDisabled(bool disabled)
{
	KeyAbstraction::SetDisabled(disabled);
	m_disableAllKeys = disabled;
}
