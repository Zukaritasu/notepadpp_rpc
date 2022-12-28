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

#include "PluginUtil.h"
#include "PluginInterface.h"

#ifdef _DEBUG
#include <stdio.h>
#endif // _DEBUG

#include <assert.h>

extern HINSTANCE hPlugin;
extern NppData   nppData;


// Exclusive use buffer of the GetRCString function to save a string resource
static TCHAR string_buffer[512];

LPTSTR GetRCString(unsigned ids)
{
	string_buffer[0] = 0;
	LoadString(hPlugin, ids, string_buffer, 512);
	return string_buffer;
}

HWND GetCurrentScintilla()
{
	int which = -1;
	SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
	if (which == -1)
		return nullptr;
	return (which == 0) ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;
}
