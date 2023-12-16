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

#define IDD_PLUGIN_OPTIONS              107
#define IDC_EDIT_CID                    1003
#define IDC_CLIENT_ID                   1004
#define IDC_ENABLE                      1007
#define IDC_HIDE_STATE                  1008
#define IDC_RESET                       1009
#define IDC_SHOW_LANGICON               1010
#define IDC_SHOW_ELAPSED_TIME           1011
#define IDC_HIDE_DETAILS                1012
#define IDC_DETAILS                     1013
#define IDC_VARIABLES                   1014
#define IDC_STATE                       1015
#define IDC_CREATEAPP                   1016
#define IDC_LARGETEXT                   1017
#define IDC_DOC_HELP                    1018
#define IDC_STATIC                      -1
#define IDC_GROUD_ID                    -1


#define PLUGIN_VERSION                  1,9,623,1
#define PLUGIN_VERSION_STR              "1.9.623.1"

#define PLUGIN_ABOUT \
	TEXT("Discord Rich Presence by Zukaritasu\n\nVersion " PLUGIN_VERSION_STR "\n\nLicense GPLv3\n\nShows in discord the file that is currently being edited in Notepad++.")

#define TITLE_MBOX_DRPC                 "Discord Rich Presence"

#define DEF_DETAILS_FORMAT              "Editing: %(file)"
#define DEF_STATE_FORMAT                "Size: %(size)"
#define DEF_LARGE_TEXT_FORMAT           "Editing a %(LANG) file"
#define DEF_APPLICATION_ID_STR          "938157386068279366"
#define DEF_APPLICATION_ID              938157386068279366
#define RPC_UPDATE_TIME                 1000

#define MIN_CLIENT_ID                   ((__int64)1E16)
#define MIN_LENGTH_CLIENT_ID            17

#define IDS_ENABLERPC                   101
#define IDS_CLIENTID                    102
#define IDS_HIDE_STATE                  103
#define IDS_HIDE_DETAILS                104
#define IDS_SHOWLANGICON                105
#define IDS_SHOWELAPSEDTIME             106
#define IDS_CURRENTFILENAME             107
#define IDS_BTNRESET                    108
#define IDS_ALLVARIABLESTIP             109
#define IDS_DETAILSTIP                  110
#define IDS_STATETIP                    111
#define IDS_CREATEAPPTIP                112
#define IDS_ANCIICHARS                  113
#define IDS_INVALIDID                   114
#define IDS_LARGETEXTTIP                115
#define IDS_DOC_HELP                    116
