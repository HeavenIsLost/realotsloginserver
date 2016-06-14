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

#ifndef FS_CONFIGMANAGER_H_6BDD23BD0B8344F4B7C40E8BE6AF6F39
#define FS_CONFIGMANAGER_H_6BDD23BD0B8344F4B7C40E8BE6AF6F39

#include <lua.hpp>

class ConfigManager
{
	public:
		ConfigManager();

		enum boolean_config_t {
			FREE_PREMIUM,
			BIND_ONLY_GLOBAL_ADDRESS,
			OPTIMIZE_DATABASE,

			LAST_BOOLEAN_CONFIG /* this must be the last one */
		};

		enum string_config_t {
			DUMMY_STR,
			SERVER_NAME,
			OWNER_NAME,
			OWNER_EMAIL,
			URL,
			LOCATION,
			MAP_NAME,
			MAP_AUTHOR,
			IP,
			MOTD,
			WORLD_TYPE,
			MYSQL_HOST,
			MYSQL_USER,
			MYSQL_PASS,
			MYSQL_DB,
			MYSQL_SOCK,
			DEFAULT_PRIORITY,
			CLIENT_VERSION_STR,

			LAST_STRING_CONFIG /* this must be the last one */
		};

		enum integer_config_t {
			SQL_PORT,
			LOGIN_PORT,
			MOTD_NUM,
			MAX_PACKETS_PER_SECOND,
			CLIENT_VERSION_MIN,
			CLIENT_VERSION_MAX,
			MAX_PLAYERS,
			RATE_EXPERIENCE,
			RATE_SKILL,
			RATE_LOOT,
			RATE_MAGIC,
			RATE_SPAWN,
			STATUS_PORT,
			STATUSQUERY_TIMEOUT,
			MONSTER_COUNT,
			NPC_COUNT,

			MAP_WIDTH,
			MAP_HEIGHT,

			LAST_INTEGER_CONFIG /* this must be the last one */
		};

		bool load();
		bool reload();

		const std::string& getString(string_config_t _what) const;
		int32_t getNumber(integer_config_t _what) const;
		bool getBoolean(boolean_config_t _what) const;

	private:
		static std::string getGlobalString(lua_State* L, const char* identifier, const char* _default);
		static int32_t getGlobalNumber(lua_State* L, const char* identifier, const int32_t _default = 0);
		static bool getGlobalBoolean(lua_State* L, const char* identifier, const bool _default);

		std::string string[LAST_STRING_CONFIG];
		int32_t integer[LAST_INTEGER_CONFIG];
		bool boolean[LAST_BOOLEAN_CONFIG];

		bool loaded;
};

#endif
