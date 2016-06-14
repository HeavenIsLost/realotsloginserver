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

#include "protocolold.h"

#include "outputmessage.h"
#include "rsa.h"
#include "tasks.h"

#include "configmanager.h"
#include "ban.h"

#include "gameserverconfig.h"

extern ConfigManager g_config;
extern GameserverConfig g_gameserver;

void ProtocolOld::disconnectClient(const std::string& message)
{
	auto output = OutputMessagePool::getOutputMessage();
	output->addByte(0x0A);
	output->addString(message);
	send(output);

	disconnect();
}

void ProtocolOld::updatePremium(Account& account)
{
	bool save = false;
	time_t timeNow = time(nullptr);

	if (account.premiumDays != 0 && account.premiumDays != std::numeric_limits<uint16_t>::max()) {
		if (account.lastDay == 0) {
			account.lastDay = timeNow;
			save = true;
		} else {
			uint32_t days = (timeNow - account.lastDay) / 86400;
			if (days > 0) {
				if (days >= account.premiumDays) {
					account.premiumDays = 0;
					account.lastDay = 0;
				} else {
					account.premiumDays -= days;
					time_t remainder = (timeNow - account.lastDay) % 86400;
					account.lastDay = timeNow - remainder;
				}

				save = true;
			}
		}
	}
	else if (account.lastDay != 0) {
		account.lastDay = 0;
		save = true;
	}

	if (save && !IOLoginData::saveAccount(account)) {
		std::cout << "> ERROR: Failed to save account: " << account.name << "!" << std::endl;
	}
}

void ProtocolOld::getCharacterList(const uint32_t accountNumber, const std::string& password)
{
	Account account;
	if (!IOLoginData::loginserverAuthentication(accountNumber, password, account)) {
		disconnectClient("Please enter a valid account number and password.");
		return;
	}

	auto output = OutputMessagePool::getOutputMessage();
	//Update premium days
	//disabled, i believe its done by a cron php script
	//updatePremium(account);

	const std::string& motd = g_config.getString(ConfigManager::MOTD);
	if (!motd.empty()) {
		//Add MOTD
		output->addByte(0x14);

		std::ostringstream ss;
		ss << g_config.getNumber(ConfigManager::MOTD_NUM) << "\n" << motd;
		output->addString(ss.str());
	}

	//Add char list
	output->addByte(0x64);

	std::vector<GameServer> Gameservers = g_gameserver.getGameservers();
	std::map<uint32_t, GameServer> serverList;

	for (GameServer server : Gameservers) {
		serverList.emplace(server.worldid, server);
	}

	uint8_t size = std::min<size_t>(std::numeric_limits<uint8_t>::max(), account.characters.size());
	output->addByte(size);
	for (uint8_t i = 0; i < size; i++) {
		GameServer server = serverList[account.characters[i].worldid];

		output->addString(account.characters[i].name);
		output->addString(server.name);
		output->add<uint32_t>(inet_addr(server.ip.c_str()));
		output->add<uint16_t>(server.port);
	}

	output->add<uint16_t>(g_config.getBoolean(ConfigManager::FREE_PREMIUM) ? 0xFFFF : account.premiumDays);

	send(output);

	disconnect();
}

void ProtocolOld::onRecvFirstMessage(NetworkMessage& msg)
{

	/*uint16_t clientOS =*/ msg.get<uint16_t>();
	uint16_t version = msg.get<uint16_t>();
	msg.skipBytes(12);

	if (version <= 760) {
		disconnectClient("Only clients with protocol " + g_config.getString(ConfigManager::CLIENT_VERSION_STR) + " allowed!");
		return;
	}

	if (!Protocol::RSA_decrypt(msg)) {
		disconnect();
		return;
	}

	uint32_t key[4];
	key[0] = msg.get<uint32_t>();
	key[1] = msg.get<uint32_t>();
	key[2] = msg.get<uint32_t>();
	key[3] = msg.get<uint32_t>();
	enableXTEAEncryption();
	setXTEAKey(key);

	disableChecksum();

	if (version < g_config.getNumber(ConfigManager::CLIENT_VERSION_MIN) || version > g_config.getNumber(ConfigManager::CLIENT_VERSION_MAX)) {
		disconnectClient("Only clients with protocol " + g_config.getString(ConfigManager::CLIENT_VERSION_STR) + " allowed!");
		return;
	}

	//todo, ban function
	/*
	BanInfo banInfo;
	auto connection = getConnection();
	if (!connection) {
		return;
	}

	if (IOBan::isIpBanned(connection->getIP(), banInfo)) {
		if (banInfo.reason.empty()) {
			banInfo.reason = "(none)";
		}

		std::ostringstream ss;
		ss << "Your IP has been banned until " << formatDateShort(banInfo.expiresAt) << " by " << banInfo.bannedBy << ".\n\nReason specified:\n" << banInfo.reason;
		disconnectClient(ss.str());
		return;
	}
	*/

	uint32_t accountNumber = msg.get<uint32_t>();

	std::string password = msg.getString();

	auto thisPtr = std::dynamic_pointer_cast<ProtocolOld>(shared_from_this());
	g_dispatcher.addTask(createTask(std::bind(&ProtocolOld::getCharacterList, thisPtr, accountNumber, password)));
	
}
