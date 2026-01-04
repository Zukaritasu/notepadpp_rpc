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
#include "PluginUtil.h"

#include <windows.h>
#include <shlwapi.h>
#include <tchar.h>
#include <fstream>
#include <cstring>
#include <yaml-cpp\yaml.h>

#pragma warning(disable: 4996)

extern NppData nppData;

PluginConfig& PluginConfig::operator=(const PluginConfig& pg)
{
	memcpy(this, &pg, sizeof(PluginConfig));
	return *this;
}

void ConfigManager::LoadDefaultConfig(PluginConfig& config)
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
	config._idle_time         = DEF_IDLE_TIME;

	strncpy(config._details_format, DEF_DETAILS_FORMAT, MAX_FORMAT_BUF - 1);
	strncpy(config._state_format, DEF_STATE_FORMAT, MAX_FORMAT_BUF - 1);
	strncpy(config._large_text_format, DEF_LARGE_TEXT_FORMAT, MAX_FORMAT_BUF - 1);
}

PluginConfig ConfigManager::GetConfig() noexcept
{
	AutoUnlock lock(m_mutex);
	return m_config;
}

bool ConfigManager::SetConfig(const PluginConfig& newConfig, bool save) noexcept
{
	AutoUnlock lock(m_mutex);
	m_config = newConfig;

	if (save) return SaveConfig();
	return true;
}

void ConfigManager::LoadConfig()
{
	AutoUnlock lock(m_mutex);

	std::wstring configPath = GetConfigFilePath();

	if (!PathFileExists(configPath.c_str()))
	{
		LoadDefaultConfig(m_config);
		return;
	}

	std::ifstream stream(configPath);
	if (!stream.is_open())
		throw std::runtime_error("The file does not exist or could not be opened: " PLUGIN_CONFIG_FILENAME);

	YAML::Node config = YAML::Load(stream);
	stream.close();

	if (!config.IsDefined() || config.IsNull())
		throw std::runtime_error("The configuration file format is invalid: " PLUGIN_CONFIG_FILENAME);

	m_config._hide_details      = config["hideDetails"].as<bool>(false);
	m_config._elapsed_time      = config["elapsedTime"].as<bool>(true);
	m_config._enable            = config["enable"].as<bool>(true);
	m_config._lang_image        = config["langImage"].as<bool>(true);
	m_config._hide_state        = config["hideState"].as<bool>(false);
	m_config._client_id         = config["clientId"].as<__int64>(DEF_APPLICATION_ID);
	m_config._refreshTime       = config["refreshTime"].as<unsigned>(DEF_REFRESH_TIME);
	m_config._button_repository = config["buttonRepository"].as<bool>(false);
	m_config._hide_if_private   = config["hideIfPrivate"].as<bool>(false);
	m_config._hide_idle_status  = config["hideIdleStatus"].as<bool>(false);
	m_config._idle_time         = config["idleTime"].as<int>(DEF_IDLE_TIME);

	if (m_config._client_id < MIN_CLIENT_ID)
	{
		m_config._client_id = DEF_APPLICATION_ID;
	}

	if (m_config._refreshTime < RPC_UPDATE_TIME)
	{
		m_config._refreshTime = DEF_REFRESH_TIME;
	}

	strncpy(m_config._details_format,
		config["detailsFormat"].as<std::string>(DEF_DETAILS_FORMAT).c_str(), MAX_FORMAT_BUF - 1);
	strncpy(m_config._state_format,
		config["stateFormat"].as<std::string>(DEF_STATE_FORMAT).c_str(), MAX_FORMAT_BUF - 1);
	strncpy(m_config._large_text_format,
		config["largeTextFormat"].as<std::string>(DEF_LARGE_TEXT_FORMAT).c_str(), MAX_FORMAT_BUF - 1);
}

bool ConfigManager::SaveConfig()
{
	AutoUnlock lock(m_mutex);

	try
	{
		std::wstring configPath;

		try
		{
			configPath = GetConfigFilePath();
		}
		catch (const std::exception& e)
		{
			ShowErrorMessage(std::string("Error getting the configuration file path. ") + e.what());
			return false;
		}

		YAML::Node node;

		node["hideDetails"]      = m_config._hide_details;
		node["elapsedTime"]      = m_config._elapsed_time;
		node["enable"]           = m_config._enable;
		node["langImage"]        = m_config._lang_image;
		node["hideState"]        = m_config._hide_state;
		node["clientId"]         = m_config._client_id;
		node["detailsFormat"]    = m_config._details_format;
		node["stateFormat"]      = m_config._state_format;
		node["largeTextFormat"]  = m_config._large_text_format;
		node["refreshTime"]      = m_config._refreshTime;
		node["buttonRepository"] = m_config._button_repository;
		node["hideIfPrivate"]    = m_config._hide_if_private;
		node["hideIdleStatus"]   = m_config._hide_idle_status;
		node["idleTime"]         = m_config._idle_time;

		std::ofstream out(configPath);
		out << node;
		out.close();

		return true;
	}
	catch (const std::exception& e)
	{
		ShowErrorMessage(std::string("Error writing to the configuration file. ") + e.what());
	}

	return false;
}

std::wstring ConfigManager::GetConfigFilePath()
{
	const size_t requiredLength = NppSendMessage(nppData._nppHandle, NPPM_GETPLUGINSCONFIGDIR, 0, NULL);
	if (requiredLength == 0)
		throw std::runtime_error("Failed to get plugin config directory.");

	std::wstring configDir;
	configDir.resize(requiredLength);

	NppSendMessage(nppData._nppHandle, NPPM_GETPLUGINSCONFIGDIR, requiredLength + 1, (LPARAM)configDir.data());
	configDir.append(L"\\").append(_T(PLUGIN_CONFIG_FILENAME));
	return configDir;
}

PluginConfig ConfigManager::GetDefaultConfig()
{
	PluginConfig config;
	LoadDefaultConfig(config);

	return config;
}
