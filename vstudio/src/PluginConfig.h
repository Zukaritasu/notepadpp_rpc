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

#include <stdlib.h>

#define MAX_FORMAT_BUF 128

struct PluginConfig
{
	__int64 _client_id;
	char    _details_format[MAX_FORMAT_BUF];
	char    _state_format[MAX_FORMAT_BUF];
	char    _large_text_format[MAX_FORMAT_BUF];
	bool    _enable;
	bool    _hide_state;
	bool    _lang_image;
	bool    _elapsed_time;
	bool    _hide_details;

	PluginConfig& operator=(const PluginConfig& pg);
};

void LoadConfig(PluginConfig& config);
void GetDefaultConfig(PluginConfig& config);
void SaveConfig(const PluginConfig& config);
