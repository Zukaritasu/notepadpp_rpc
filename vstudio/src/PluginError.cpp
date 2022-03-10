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

#include "PluginError.h"

#pragma warning(disable: 4996)

#ifndef _DEBUG
#include "PluginInterface.h"

extern NppData nppData;
#endif // _DEBUG

#include <tchar.h>

void ShowErrorMessage(const TCHAR* message, HWND hWnd)
{
#ifdef _DEBUG
	_tprintf(TEXT(" > %s\n"), message);
#else
	MessageBox(hWnd ? hWnd : nppData._nppHandle, msg,
		TITLE_MBOX_DRPC, MB_ICONERROR | MB_OK);
#endif // _DEBUG
}

#define GetErrorMessage(pmsg, code) \
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | \
				  FORMAT_MESSAGE_ALLOCATE_BUFFER, \
				  nullptr, code, 0, reinterpret_cast<TCHAR*>(pmsg), 0, nullptr)

void ShowLastError()
{
	unsigned long error_code = GetLastError();
	if (error_code != 0)
	{
		TCHAR* message = nullptr;
		if (GetErrorMessage(&message, error_code) > 0 && message)
		{
			ShowErrorMessage(message);
			LocalFree(message);
		}
	}
}

#define DISCORD_ERROR_FORMAT \
	TEXT("An error has occurred in Discord. Error code: %d")

#ifdef UNICODE
#define BUF_E_SIZE (sizeof(DISCORD_ERROR_FORMAT) / 2)
#else
#define BUF_E_SIZE (sizeof(DISCORD_ERROR_FORMAT))
#endif // UNICODE

void ShowDiscordError(EDiscordResult result)
{
	if (result != DiscordResult_Ok)
	{
		TCHAR error[BUF_E_SIZE]{};
		_stprintf(error, DISCORD_ERROR_FORMAT, static_cast<int>(result));
		ShowErrorMessage(error);
	}
}
