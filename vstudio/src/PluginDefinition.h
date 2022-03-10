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

#ifndef PLUGINDEFINITION_H
#define PLUGINDEFINITION_H

#include "PluginInterface.h"

// ==================================================================
// FUNCTIONS AND CONSTANTS OF THIS TEMPLATE
// ==================================================================

const TCHAR NPP_PLUGIN_NAME[] = TEXT("Discord Rich Presence");
const int nbFunc = 3;

void pluginInit(HANDLE hModule);
void pluginCleanUp();
void commandMenuInit();
void commandMenuCleanUp();

bool setCommand(size_t index, TCHAR *cmdName, PFUNCPLUGINCMD pFunc, 
				ShortcutKey *sk = NULL, bool check0nInit = false);
				
// ==================================================================
// FUNCTIONS OF THE PLUGIN 
// ==================================================================

#define TITLE_MBOX_DRPC     TEXT("Discord RPC")
// Default client ID
#define DEFAULT_CLIENT_ID   938157386068279366
#define DEF_CLIENT_ID_STR   "938157386068279366"
// Client id minimum allowed 
#define MIN_CLIENT_ID       100000000000000000

void OptionsPlugin();
void About();

#endif // !PLUGINDEFINITION_H
