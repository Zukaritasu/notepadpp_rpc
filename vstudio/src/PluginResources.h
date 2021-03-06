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

#define IDD_PLUGIN_OPTIONS              107
#define IDC_EDIT_CID                    1003
#define IDC_CLIENT_ID                   1004
#define IDC_ENABLE                      1007
#define IDC_SHOW_FILESZ                 1008
#define IDC_RESET                       1009
#define IDC_SHOW_LANGICON               1010
#define IDC_SHOW_ELAPSED_TIME           1011
#define IDC_CURRENT_FILENAME            1012
#define IDC_STATIC                      -1
#define IDC_GROUD_ID                    -1


#define PLUGIN_VERSION                  1,4,262,1
#define PLUGIN_VERSION_LONG             "1.4.262.1"

#define PLUGIN_ABOUT \
	TEXT("Discord Rich Presence by Zukaritasu\n\nVersion " PLUGIN_VERSION_LONG "\n\nLicense GPLv3\n\nShows in discord the file that is currently being edited in Notepad++.")

#define CREATE_DIRECTORY_ERROR \
	TEXT("Could not create directory [%s] because a file with the same name exists")

#define IDS_ENABLERPC                   101
#define IDS_CLIENTID                    102
#define IDS_SHOWSIZE                    103
#define IDS_SHOWLANGICON                104
#define IDS_SHOWELAPSEDTIME             105
#define IDS_CURRENTFILENAME             106
#define IDS_BTNRESET                    107

#define IDI_DISCORD                     101
#define IDI_DISCORD_DARK                102

#define IDB_DISCORD                     101
