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

#include "PluginConfig.h"
#include "PluginError.h"
#include "PluginResources.h"
#include "PluginDefinition.h"

#include <Windows.h>
#include <Shlwapi.h>
#include <strsafe.h>
#include <cstdio>
#include <fstream>

#pragma warning(disable: 4996)

#define DIRECTORY_PLUGIN_NAME TEXT("DiscordRPC")
#define FILE_CONFIG_NAME      TEXT("rpc_config.dat")

static bool CreateDirectoryPlugin(const TCHAR* dir)
{
	if (!PathFileExists(dir) && !CreateDirectory(dir, nullptr))
	{
		ShowLastError();
		return false;
	}
	else if (!PathIsDirectory(dir))
	{
		TCHAR error[520];
		StringCbPrintf(error, sizeof(error), 
					   CREATE_DIRECTORY_ERROR, dir);
		ShowErrorMessage(dir);
		return false;
	}
	return true;
}

static TCHAR* GetFileConfig(TCHAR* filename)
{
	int count = GetEnvironmentVariable(TEXT("LOCALAPPDATA"), 
				filename, MAX_PATH);
	if (count > 0)
	{
		lstrcat(filename, TEXT("\\") DIRECTORY_PLUGIN_NAME);
		if (CreateDirectoryPlugin(filename))
		{
			lstrcat(filename, TEXT("\\") FILE_CONFIG_NAME);
			return filename;
		}
	}
	else
		ShowLastError();
	return nullptr;
}

void LoadDefaultConfig(PluginConfig& config)
{
	config._enable    = true;
	config._show_sz   = true;
	config._client_id = DEFAULT_CLIENT_ID;
	config._show_lang = true;
}

void SavePluginConfig(const PluginConfig& config)
{
	TCHAR filename[MAX_PATH];
	if (GetFileConfig(filename) != nullptr)
	{
		std::ofstream file(filename);
		if (file.is_open())
		{
			file.write((const char*)&config, sizeof(PluginConfig));
			file.close();
#ifdef _DEBUG
			printf(" > Configuration has been saved successfully!\n");
#endif // _DEBUG
		}
	}
}

void LoadPluginConfig(PluginConfig& config)
{
	TCHAR filename[MAX_PATH];

	bool set_default = true;
	if (GetFileConfig(filename) != nullptr)
	{
		std::ifstream file(filename);
		if (file.is_open())
		{
			// Verify that the number of bytes read matches the
			// size of the structure 
			set_default = file.read((char*)&config,
				sizeof(PluginConfig)).gcount() != sizeof(PluginConfig);
			file.close();
#ifdef _DEBUG
			if (set_default)
				printf(" > Error loading configuration!\n");
			else
				printf(" > The configuration has been loaded successfully!\n");
#endif // _DEBUG
		}
	}
	if (set_default)
	{
		LoadDefaultConfig(config);
		SavePluginConfig(config);
	}
}