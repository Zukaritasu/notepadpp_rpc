// Copyright (C) 2022 - 2023 Zukaritasu
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

#include "resource.h"

#define PLUGIN_VERSION                  2,1,679,1
#define PLUGIN_VERSION_STR              "2.1.679.1"

#define PLUGIN_ABOUT \
	TEXT("Discord Rich Presence by Zukaritasu\n\nVersion " PLUGIN_VERSION_STR "\n\nLicense GPLv3\n\nShows in discord the file that is currently being edited in Notepad++.")

#define TITLE_MBOX_DRPC                 "Discord Rich Presence"

#define DEF_DETAILS_FORMAT              "Editing: %(file)"
#define DEF_STATE_FORMAT                "Size: %(size)"
#define DEF_LARGE_TEXT_FORMAT           "Editing a %(LANG) file"
#define DEF_APPLICATION_ID_STR          "938157386068279366"
#define DEF_APPLICATION_ID              938157386068279366
#define RPC_UPDATE_TIME                 15000
#define RPC_TIME_RECONNECTION			2000
#define DEF_REFRESH_TIME                1000
#define DEF_IDLE_TIME                   300 // seconds

#define MIN_CLIENT_ID                   ((__int64)1E16)
#define MIN_LENGTH_CLIENT_ID            17
