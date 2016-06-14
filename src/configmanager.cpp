/**
 * The Forgotten Server - a free and open-source MMORPG server emulator
 * Copyright (C) 2015  Mark Samman <mark.samman@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "otpch.h"
#include "tools.h"
#include "configmanager.h"

#if LUA_VERSION_NUM >= 502
#define lua_strlen lua_rawlen
#endif

ConfigManager::ConfigManager()
{
	loaded = false;
}

bool ConfigManager::load()
{
	lua_State* L = luaL_newstate();
	if (!L) {
		throw std::runtime_error("Failed to allocate memory");
	}

	luaL_openlibs(L);

	if (luaL_dofile(L, "config.lua")) {
		std::cout << "[Error - ConfigManager::load] " << lua_tostring(L, -1) << std::endl;
		lua_close(L);
		return false;
	}

	//parse config
	if (!loaded) { //info that must be loaded one time (unless we reset the modules involved)
		boolean[BIND_ONLY_GLOBAL_ADDRESS] = getGlobalBoolean(L, "bindOnlyGlobalAddress", false);
		boolean[OPTIMIZE_DATABASE] = getGlobalBoolean(L, "startupDatabaseOptimization", true);

		string[IP] = getGlobalString(L, "ip", "127.0.0.1");
		string[MYSQL_HOST] = getGlobalString(L, "mysqlHost", "127.0.0.1");
		string[MYSQL_USER] = getGlobalString(L, "mysqlUser", "forgottenserver");
		string[MYSQL_PASS] = getGlobalString(L, "mysqlPass", "");
		string[MYSQL_DB] = getGlobalString(L, "mysqlDatabase", "forgottenserver");
		string[MYSQL_SOCK] = getGlobalString(L, "mysqlSock", "");
		string[CLIENT_VERSION_STR] = getGlobalString(L, "clientVersionStr", "10.80");

		integer[SQL_PORT] = getGlobalNumber(L, "mysqlPort", 3306);
		integer[LOGIN_PORT] = getGlobalNumber(L, "port", 7171);
		integer[CLIENT_VERSION_MIN] = getGlobalNumber(L, "clientVersionMin", 1080);
		integer[CLIENT_VERSION_MAX] = getGlobalNumber(L, "clientVersionMax", 1080);

		integer[STATUS_PORT] = getGlobalNumber(L, "statusProtocolPort", 7171);

		string[MAP_NAME] = getGlobalString(L, "mapName", "forgotten");
		string[MAP_AUTHOR] = getGlobalString(L, "mapAuthor", "Unknown");
	}

	boolean[FREE_PREMIUM] = getGlobalBoolean(L, "freePremium", false);
	string[DEFAULT_PRIORITY] = getGlobalString(L, "defaultPriority", "high");

	string[SERVER_NAME] = getGlobalString(L, "serverName", "");
	string[OWNER_NAME] = getGlobalString(L, "ownerName", "");
	string[OWNER_EMAIL] = getGlobalString(L, "ownerEmail", "");
	string[URL] = getGlobalString(L, "url", "");
	string[LOCATION] = getGlobalString(L, "location", "");

	string[MOTD] = getGlobalString(L, "motd", "");
	integer[MOTD_NUM] = getGlobalNumber(L, "motdNum", 0);

	integer[MAX_PLAYERS] = getGlobalNumber(L, "maxPlayers");

	integer[RATE_EXPERIENCE] = getGlobalNumber(L, "rateExp", 5);
	integer[RATE_SKILL] = getGlobalNumber(L, "rateSkill", 3);
	integer[RATE_LOOT] = getGlobalNumber(L, "rateLoot", 2);
	integer[RATE_MAGIC] = getGlobalNumber(L, "rateMagic", 3);
	integer[RATE_SPAWN] = getGlobalNumber(L, "rateSpawn", 1);

	integer[STATUSQUERY_TIMEOUT] = getGlobalNumber(L, "statusTimeout", 5000);

	integer[MAX_PACKETS_PER_SECOND] = getGlobalNumber(L, "maxPacketsPerSecond", 25);

	integer[MONSTER_COUNT] = getGlobalNumber(L, "monsterCount", 0);
	integer[NPC_COUNT] = getGlobalNumber(L, "npcCount", 0);
	integer[MAP_WIDTH] = getGlobalNumber(L, "mapWidth", 0);
	integer[MAP_HEIGHT] = getGlobalNumber(L, "mapHeight", 0);

	loaded = true;
	lua_close(L);
	return true;
}

bool ConfigManager::reload()
{
	bool result = load();
	return result;
}

const std::string& ConfigManager::getString(string_config_t _what) const
{
	if (_what >= LAST_STRING_CONFIG) {
		std::cout << "[Warning - ConfigManager::getString] Accessing invalid index: " << _what << std::endl;
		return string[DUMMY_STR];
	}
	return string[_what];
}

int32_t ConfigManager::getNumber(integer_config_t _what) const
{
	if (_what >= LAST_INTEGER_CONFIG) {
		std::cout << "[Warning - ConfigManager::getNumber] Accessing invalid index: " << _what << std::endl;
		return 0;
	}
	return integer[_what];
}

bool ConfigManager::getBoolean(boolean_config_t _what) const
{
	if (_what >= LAST_BOOLEAN_CONFIG) {
		std::cout << "[Warning - ConfigManager::getBoolean] Accessing invalid index: " << _what << std::endl;
		return false;
	}
	return boolean[_what];
}

std::string ConfigManager::getGlobalString(lua_State* L, const char* identifier, const char* _default)
{
	lua_getglobal(L, identifier);
	if (!lua_isstring(L, -1)) {
		return _default;
	}

	size_t len = lua_strlen(L, -1);
	std::string ret(lua_tostring(L, -1), len);
	lua_pop(L, 1);
	return ret;
}

int32_t ConfigManager::getGlobalNumber(lua_State* L, const char* identifier, const int32_t _default)
{
	lua_getglobal(L, identifier);
	if (!lua_isnumber(L, -1)) {
		return _default;
	}

	int32_t val = lua_tonumber(L, -1);
	lua_pop(L, 1);
	return val;
}

bool ConfigManager::getGlobalBoolean(lua_State* L, const char* identifier, const bool _default)
{
	lua_getglobal(L, identifier);
	if (!lua_isboolean(L, -1)) {
		if (!lua_isstring(L, -1)) {
			return _default;
		}

		size_t len = lua_strlen(L, -1);
		std::string ret(lua_tostring(L, -1), len);
		lua_pop(L, 1);
		return booleanString(ret);
	}

	int val = lua_toboolean(L, -1);
	lua_pop(L, 1);
	return val != 0;
}
