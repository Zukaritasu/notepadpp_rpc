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
#include "PresenceTextFormat.h"
#include "DiscordRichPresence.hpp"

class RichPresence
{
public:
	RichPresence() {}
	RichPresence(const RichPresence&) = delete;

	void Init();
	void Update(bool updateLook = true) noexcept;
	void Close() noexcept;
	
private:
	DiscordRichPresence _drp;
	Presence _p;
	PresenceTextFormat format;

	BasicThread*  _callbacks = nullptr;
	BasicThread*  _status    = nullptr;
	volatile bool _active    = false;

	void UpdateAssets() noexcept;
	void Connect() noexcept;

	static void CallBacks(void* data, volatile bool* keepRunning = nullptr) noexcept;
	static void Status(void* data, volatile bool* keepRunning = nullptr) noexcept;
};
