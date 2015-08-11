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

#include "protocollogin.h"

#include "outputmessage.h"
#include "rsa.h"
#include "tasks.h"

#include "configmanager.h"
#include "ban.h"

#include "gameserverconfig.h"

extern ConfigManager g_config;
extern GameserverConfig g_gameserver;

void ProtocolLogin::disconnectClient(const std::string& message, uint16_t version)
{
	auto output = OutputMessagePool::getOutputMessage();

	output->addByte(version >= 1076 ? 0x0B : 0x0A);
	output->addString(message);
	send(output);

	disconnect();
}

void ProtocolLogin::updatePremium(Account& account)
{
	bool save = false;
	time_t timeNow = time(nullptr);

	if (account.premiumDays != 0 && account.premiumDays != std::numeric_limits<uint16_t>::max()) {
		if (account.lastDay == 0) {
			account.lastDay = timeNow;
			save = true;
		}
		else {
			uint32_t days = (timeNow - account.lastDay) / 86400;
			if (days > 0) {
				if (days >= account.premiumDays) {
					account.premiumDays = 0;
					account.lastDay = 0;
				}
				else {
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

void ProtocolLogin::getCharacterList(const std::string& accountName, const std::string& password, uint16_t version)
{
	Account account;
	if (!IOLoginData::loginserverAuthentication(accountName, password, account)) {
		disconnectClient("Account name or password is not correct.", version);
		return;
	}

	auto output = OutputMessagePool::getOutputMessage();
	//Update premium days
	updatePremium(account);

	//Add session key
	output->addByte(0x28);
	output->addString(accountName + "\n" + password);

	const std::string& motd = g_config.getString(ConfigManager::MOTD);
	if (!motd.empty()) {
		//Add MOTD
		output->addByte(0x14);

		std::ostringstream ss;
		ss << g_config.getNumber(ConfigManager::MOTD_NUM) << "\n" << motd;
		output->addString(ss.str());
	}

	std::vector<GameServer> Gameservers = g_gameserver.getGameservers();

	//Add char list
	output->addByte(0x64);

	output->addByte(Gameservers.size());

	for (GameServer server : Gameservers) {
		output->addByte(server.worldid);
		output->addString(server.name);
		output->addString(server.ip);
		output->add<uint16_t>(server.port);
		output->addByte(server.preview);
	}

	uint8_t size = std::min<size_t>(std::numeric_limits<uint8_t>::max(), account.characters.size());
	output->addByte(size);
	for (uint8_t i = 0; i < size; i++) {
		output->addByte(account.characters[i].worldid);
		output->addString(account.characters[i].name);
	}

	if (g_config.getNumber(ConfigManager::CLIENT_VERSION_MIN) >= 1080) {
		output->addByte(g_config.getBoolean(ConfigManager::FREE_PREMIUM) || account.premiumDays > 0);
		output->add<uint32_t>(g_config.getBoolean(ConfigManager::FREE_PREMIUM) ? 0 : (time(nullptr) + (account.premiumDays * 86400)));
	}
	else {
		output->add<uint16_t>(g_config.getBoolean(ConfigManager::FREE_PREMIUM) ? 0xFFFF : account.premiumDays); //client displays free premium
	}

	send(output);

	disconnect();
}

void ProtocolLogin::onRecvFirstMessage(NetworkMessage& msg)
{
	msg.skipBytes(2); // client OS

	uint16_t version = msg.get<uint16_t>();
	if (version >= 971) {
		msg.skipBytes(17);
	} else {
		msg.skipBytes(12);
	}
	/*
	 * Skipped bytes:
	 * 4 bytes: protocolVersion
	 * 12 bytes: dat, spr, pic signatures (4 bytes each)
	 * 1 byte: 0
	 */

	if (version <= 760) {
		disconnectClient("Only clients with protocol " + g_config.getString(ConfigManager::CLIENT_VERSION_STR) + " allowed!", version);
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

	if (version < g_config.getNumber(ConfigManager::CLIENT_VERSION_MIN) || version > g_config.getNumber(ConfigManager::CLIENT_VERSION_MAX)) {
		disconnectClient("Only clients with protocol " + g_config.getString(ConfigManager::CLIENT_VERSION_STR) + " allowed!", version);
		return;
	}

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
		disconnectClient(ss.str(), version);
		return;
	}

	std::string accountName = msg.getString();
	if (accountName.empty()) {
		disconnectClient("Invalid account name.", version);
		return;
	}

	std::string password = msg.getString();
	auto thisPtr = std::dynamic_pointer_cast<ProtocolLogin>(shared_from_this());
	g_dispatcher.addTask(createTask(std::bind(&ProtocolLogin::getCharacterList, thisPtr, accountName, password, version)));
}
