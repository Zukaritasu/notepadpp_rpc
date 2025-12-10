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

#include "PluginDefinition.h"
#include "PluginError.h"
#include "PluginResources.h"

#pragma warning(disable: 4996 4100)

#ifndef _DEBUG
#include "PluginInterface.h"

extern NppData nppData;
#endif // _DEBUG

#include <tchar.h>
#include <string>

void ShowErrorMessage(std::wstring message, HWND hWnd)
{
#ifdef _DEBUG
	_tprintf(_T(" > Error: %s\n"), message.c_str());
#else
	MessageBox(hWnd ? hWnd : nppData._nppHandle, message.c_str(),
		_T(TITLE_MBOX_DRPC), MB_ICONERROR | MB_OK);
#endif // _DEBUG
}

void ShowErrorMessage(std::string message, HWND hWnd)
{
#ifdef _DEBUG
	printf(" > Error: %s\n", message.c_str());
#else
	MessageBoxA(hWnd ? hWnd : nppData._nppHandle, message.c_str(),
		TITLE_MBOX_DRPC, MB_ICONERROR | MB_OK);
#endif // _DEBUG
}

void ShowWin32LastError()
{
	auto code = GetLastError();
	if (code != ERROR_SUCCESS)
	{
		LPTSTR message = nullptr;
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |  FORMAT_MESSAGE_ALLOCATE_BUFFER, 
					  nullptr, code, 0, reinterpret_cast<LPTSTR>(&message), 0, nullptr);
		if (message)
		{
			std::basic_string<TCHAR> msg_format(_T("An error has occurred in the plugin. Reason:\n\n"));
			ShowErrorMessage(msg_format.append(message).c_str());
			LocalFree(message);
		}
	}
}
