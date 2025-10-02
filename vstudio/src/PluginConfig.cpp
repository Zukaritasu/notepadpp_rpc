// Copyright (C) 2022 - 2025 Zukaritasu
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
#include "PluginResources.h"
#include "PluginDefinition.h"
#include "PluginError.h"

#include <windows.h>
#include <shlwapi.h>
#include <tchar.h>
#include <fstream>
#include <cstring>
#include <yaml-cpp\yaml.h>

#pragma warning(disable: 4996)

extern NppData nppData;

static TCHAR* GetFileNameConfig(TCHAR* buffer, size_t size)
{
	SendMessage(nppData._nppHandle, NPPM_GETPLUGINSCONFIGDIR, size, (LPARAM)buffer);
	_tcsncat(buffer, _T("\\" PLUGIN_CONFIG_FILENAME), size);
	return buffer;
}

void GetDefaultConfig(PluginConfig& config)
{
	config._hide_details      = false;
	config._elapsed_time      = true;
	config._enable            = true;
	config._lang_image        = true;
	config._hide_state        = false;
	config._client_id         = DEF_APPLICATION_ID;
	config._refreshTime       = DEF_REFRESH_TIME;
	config._button_repository = false;
	config._hide_if_private   = false;
	config._hide_idle_status  = false;
	config._idle_time		  = DEF_IDLE_TIME;

	strcpy(config._details_format, DEF_DETAILS_FORMAT);
	strcpy(config._state_format, DEF_STATE_FORMAT);
	strcpy(config._large_text_format, DEF_LARGE_TEXT_FORMAT);
}

void ShowIOException(const std::string& desc, const std::exception& e)
{
	std::string message = desc + ". " + e.what();
	MessageBoxA(nppData._nppHandle, message.c_str(), TITLE_MBOX_DRPC, MB_OK | MB_ICONERROR);
}

void LoadConfig(PluginConfig& config)
{
	TCHAR file[MAX_PATH] = { _T('\0') };
	GetFileNameConfig(file, MAX_PATH);

	if (!PathFileExists(file))
	{
		GetDefaultConfig(config);
		return;
	}

	try
	{
		std::ifstream stream(file);
		YAML::Node __config = YAML::Load(stream);
		stream.close();

		config._hide_details      = __config["hideDetails"].as<bool>(false);
		config._elapsed_time      = __config["elapsedTime"].as<bool>(true);
		config._enable            = __config["enable"].as<bool>(true);
		config._lang_image        = __config["langImage"].as<bool>(true);
		config._hide_state        = __config["hideState"].as<bool>(false);
		config._client_id         = __config["clientId"].as<__int64>(DEF_APPLICATION_ID);
		config._refreshTime       = __config["refreshTime"].as<unsigned>(DEF_REFRESH_TIME);
		config._button_repository = __config["buttonRepository"].as<bool>(false);
		config._hide_if_private   = __config["hideIfPrivate"].as<bool>(false);
		config._hide_idle_status  = __config["hideIdleStatus"].as<bool>(false);
		config._idle_time		  = __config["idleTime"].as<int>(DEF_IDLE_TIME);

		if (config._client_id < MIN_CLIENT_ID)
		{
			config._client_id = DEF_APPLICATION_ID;
		}

		if (config._refreshTime < (RPC_UPDATE_TIME / 60))
		{
			config._refreshTime = DEF_REFRESH_TIME;
		}

		strncpy(config._details_format, 
			__config["detailsFormat"].as<std::string>(DEF_DETAILS_FORMAT).c_str(), MAX_FORMAT_BUF);
		strncpy(config._state_format, 
			__config["stateFormat"].as<std::string>(DEF_STATE_FORMAT).c_str(), MAX_FORMAT_BUF);
		strncpy(config._large_text_format, 
			__config["largeTextFormat"].as<std::string>(DEF_LARGE_TEXT_FORMAT).c_str(), MAX_FORMAT_BUF);
	}
	catch (const std::exception& e)
	{
		ShowIOException("Configuration file read error", e);
		GetDefaultConfig(config);
	}
}

void SaveConfig(const PluginConfig& config)
{
	TCHAR file[MAX_PATH] = { 0 };
	GetFileNameConfig(file, MAX_PATH);

	try
	{
		YAML::Node node;

		node["hideDetails"]      = config._hide_details;
		node["elapsedTime"]      = config._elapsed_time;
		node["enable"]           = config._enable;
		node["langImage"]        = config._lang_image;
		node["hideState"]        = config._hide_state;
		node["clientId"]         = config._client_id;
		node["detailsFormat"]    = config._details_format;
		node["stateFormat"]      = config._state_format;
		node["largeTextFormat"]  = config._large_text_format;
		node["refreshTime"]      = config._refreshTime;
		node["buttonRepository"] = config._button_repository;
		node["hideIfPrivate"]    = config._hide_if_private;
		node["hideIdleStatus"]   = config._hide_idle_status;
		node["idleTime"]		 = config._idle_time;

		std::ofstream out(file);
		out << node;
		out.close();
	}
	catch (const std::exception& e)
	{
		ShowIOException("Error writing to the configuration file", e);
	}
}

PluginConfig& PluginConfig::operator=(const PluginConfig& pg)
{
	memcpy(this, &pg, sizeof(PluginConfig));
	return *this;
}
