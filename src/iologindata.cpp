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

#include "iologindata.h"
#include "configmanager.h"
#include "tools.h"

extern ConfigManager g_config;

Account IOLoginData::loadAccount(uint32_t accno)
{
	Account account;

	std::ostringstream query;
	query << "SELECT `id`, `name`, `password`, `type`, `premdays`, `lastday` FROM `accounts` WHERE `id` = " << accno;
	DBResult_ptr result = Database::getInstance()->storeQuery(query.str());
	if (!result) {
		return account;
	}

	account.id = result->getNumber<uint32_t>("id");
	account.name = result->getString("name");
	account.accountType = static_cast<AccountType_t>(result->getNumber<int32_t>("type"));
	account.premiumDays = result->getNumber<uint16_t>("premdays");
	account.lastDay = result->getNumber<time_t>("lastday");
	return account;
}

bool IOLoginData::saveAccount(const Account& acc)
{
	std::ostringstream query;
	query << "UPDATE `users` SET `premium_days` = " << acc.premiumDays << ", `last_ts` = " << acc.lastDay << " WHERE `login` = " << acc.accountNumber;
	return Database::getInstance()->executeQuery(query.str());
}

bool IOLoginData::loginserverAuthentication(const uint32_t accountNumber, const std::string& password, Account& account)
{
	Database* db = Database::getInstance();
	std::ostringstream query;
	query << "SELECT  `passwd`, `premium_days`, `last_ts` FROM `users` WHERE `login` = " << accountNumber;
	DBResult_ptr result = db->storeQuery(query.str());
	if (!result) {
		return false;
	}

	//plain
	if (password != result->getString("passwd")) {
		return false;
	}

	account.accountNumber = accountNumber;

	account.lastDay = result->getNumber<uint64_t>("last_ts");
	account.premiumDays = result->getNumber<uint16_t>("premium_days");

	query.str(std::string());
	query << "SELECT `charname`,`playerdelete` FROM `players` WHERE `account_nr` = " << accountNumber << " ORDER BY charname ASC";
	result = db->storeQuery(query.str());
	if (result) {
		do {
			if (result->getNumber<uint64_t>("playerdelete") == 0) {
				Character character;
				character.name = result->getString("charname");
				//character.worldid = result->getNumber<uint16_t>("world");
				//disabled multi world support
				character.worldid = 0;

				account.characters.push_back(character);
			}
		} while (result->next());
	}
	return true;
}

AccountType_t IOLoginData::getAccountType(uint32_t accountId)
{
	std::ostringstream query;
	query << "SELECT `type` FROM `accounts` WHERE `id` = " << accountId;
	DBResult_ptr result = Database::getInstance()->storeQuery(query.str());
	if (!result) {
		return ACCOUNT_TYPE_NORMAL;
	}
	return static_cast<AccountType_t>(result->getNumber<uint16_t>("type"));
}

void IOLoginData::setAccountType(uint32_t accountId, AccountType_t accountType)
{
	std::ostringstream query;
	query << "UPDATE `accounts` SET `type` = " << static_cast<uint16_t>(accountType) << " WHERE `id` = " << accountId;
	Database::getInstance()->executeQuery(query.str());
}

void IOLoginData::addPremiumDays(uint32_t accountId, int32_t addDays)
{
	std::ostringstream query;
	query << "UPDATE `accounts` SET `premdays` = `premdays` + " << addDays << " WHERE `id` = " << accountId;
	Database::getInstance()->executeQuery(query.str());
}

void IOLoginData::removePremiumDays(uint32_t accountId, int32_t removeDays)
{
	std::ostringstream query;
	query << "UPDATE `accounts` SET `premdays` = `premdays` - " << removeDays << " WHERE `id` = " << accountId;
	Database::getInstance()->executeQuery(query.str());
}
