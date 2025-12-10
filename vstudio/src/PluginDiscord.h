// Copyright (C) 2022 Zukaritasu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <Windows.h>
#include <memory>

#include "PluginThread.h"
#include "PluginConfig.h"
#include "PluginUtil.h"
#include "TextEditorInfo.h"
#include "DiscordRichPresence.hpp"

class RichPresence
{
public:
	RichPresence() {}
	RichPresence(const RichPresence&) = delete;

	void InitializePresence();
	void Update() noexcept;
	void Close() noexcept;
	
private:
	class PresenceTemp
	{
	public:
		PresenceTemp() = default;
		PresenceTemp(const PresenceTemp&) = delete;
		PresenceTemp& operator=(const PresenceTemp&) = delete;

		void operator=(const Presence& p)
		{
			AutoUnlock lock(mutex);
			m_pTemp = p;
		}
		
		operator Presence()
		{
			AutoUnlock lock(mutex);
			return m_pTemp;
		}
	private:
		Presence m_pTemp;
		BasicMutex mutex;
	};

	
	DiscordRichPresence _drp;
	Presence            _p;
	PresenceTemp        _pTemp;
				
	TextEditorInfo		_editorInfo;

	// Thread that calls the Discord callbacks every few seconds
	BasicThread*        _callbacks  = nullptr;
	// Thread that increments the idle time counter every second
	BasicThread*        _idleTimer  = nullptr;

	void UpdateAssets() noexcept;
	void Connect(volatile bool* keepRunning = nullptr) noexcept;

	static void CallBacks(void* data, volatile bool* keepRunning = nullptr) noexcept;
	static void IdleTimer(void* data, volatile bool* keepRunning = nullptr) noexcept;
};
