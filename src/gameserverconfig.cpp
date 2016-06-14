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
#include "gameserverconfig.h"
#include "pugicast.h"

GameserverConfig::GameserverConfig()
{	
	loaded = false;
}

bool GameserverConfig::load()
{
	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file("gameservers.xml");
	if (!result) {
		printXMLError("Error - GameserverConfig::load", "gameservers.xml", result);
		return false;
	}
	
	gameservers.clear();

	for (auto catNode : doc.child("servers").children()) {
		GameServer gs;
		gs.name = catNode.attribute("name").as_string();
		gs.ip = catNode.attribute("ip").as_string();
		gs.port = pugi::cast<uint16_t>(catNode.attribute("port").value());
		gs.worldid = pugi::cast<uint16_t>(catNode.attribute("id").value());
		gameservers.push_back(gs);
	}
	
	loaded = true;
	return true;
}

bool GameserverConfig::reload()
{
	bool result = load();
	return result;
}