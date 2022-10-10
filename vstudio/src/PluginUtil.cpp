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

extern HINSTANCE hPlugin;

// Exclusive use buffer of the GetRCStringA function to save a string resource
static char    aglobal_buf[512];
// Exclusive use buffer of the GetRCStringW function to save a string resource
static wchar_t wglobal_buf[512];

LPWSTR GetRCStringW(unsigned ids)
{
	wglobal_buf[0] = 0;
	LoadStringW(hPlugin, ids, wglobal_buf, ARRAYSIZE(wglobal_buf));
	return wglobal_buf;
}

LPSTR GetRCStringA(unsigned ids)
{
	aglobal_buf[0] = 0;
	LoadStringA(hPlugin, ids, aglobal_buf, ARRAYSIZE(aglobal_buf));
	return aglobal_buf;
}
