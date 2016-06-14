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
#include "server.h"

#ifndef _WIN32
#include <csignal> // for sigemptyset()
#endif

#include "configmanager.h"
#include "rsa.h"
#include "protocolold.h"
#include "databasemanager.h"
#include "scheduler.h"
#include "databasetasks.h"
#include "gameserverconfig.h"
#include "protocolstatus.h"

DatabaseTasks g_databaseTasks;
Dispatcher g_dispatcher;
Scheduler g_scheduler;

ConfigManager g_config;
GameserverConfig g_gameserver;
RSA g_RSA;

std::mutex g_loaderLock;
std::condition_variable g_loaderSignal;
std::unique_lock<std::mutex> g_loaderUniqueLock(g_loaderLock);

ServiceManager* g_serviceManager = nullptr;

void startupErrorMessage(const std::string& errorStr)
{
	std::cout << "> ERROR: " << errorStr << std::endl;
	g_loaderSignal.notify_all();
}

void mainLoader(int argc, char* argv[], ServiceManager* servicer);

void badAllocationHandler()
{
	// Use functions that only use stack allocation
	puts("Allocation failed, server out of memory.\nDecrease the size of your map or compile in 64 bits mode.\n");
	getchar();
	exit(-1);
}

void shutdown()
{
	std::cout << "Shutting down..." << std::flush;

	g_scheduler.shutdown();
	g_databaseTasks.shutdown();
	g_dispatcher.shutdown();

	if (g_serviceManager) {
		g_serviceManager->stop();
	}

	ConnectionManager::getInstance().closeAll();

	std::cout << " done!" << std::endl;
}

int main(int argc, char* argv[])
{
	// Setup bad allocation handler
	std::set_new_handler(badAllocationHandler);

#ifndef _WIN32
	// ignore sigpipe...
	struct sigaction sigh;
	sigh.sa_handler = SIG_IGN;
	sigh.sa_flags = 0;
	sigemptyset(&sigh.sa_mask);
	sigaction(SIGPIPE, &sigh, nullptr);
#endif

	ServiceManager serviceManager;

	g_serviceManager = &serviceManager;

	g_dispatcher.start();
	g_scheduler.start();

	g_dispatcher.addTask(createTask(std::bind(mainLoader, argc, argv, &serviceManager)));

	g_loaderSignal.wait(g_loaderUniqueLock);

	if (serviceManager.is_running()) {
		std::cout << ">> " << STATUS_SERVER_NAME << " Online!" << std::endl << std::endl;
#ifdef _WIN32
		SetConsoleCtrlHandler([](DWORD) -> BOOL {
			g_dispatcher.addTask(createTask([]() {
				g_dispatcher.addTask(createTask([]() {
					shutdown();
				}));

				g_scheduler.stop();
				g_databaseTasks.stop();
				g_dispatcher.stop();
			}));
			ExitThread(0);
		}, 1);
#endif
		serviceManager.run();
	} else {
		std::cout << ">> No services running. The server is NOT online." << std::endl;
		g_dispatcher.addTask(createTask([]() {
			g_dispatcher.addTask(createTask([]() {
				g_scheduler.shutdown();
				g_databaseTasks.shutdown();
				g_dispatcher.shutdown();
			}));
			g_scheduler.stop();
			g_databaseTasks.stop();
			g_dispatcher.stop();
		}));
	}

	g_scheduler.join();
	g_databaseTasks.join();
	g_dispatcher.join();
	return 0;
}

void mainLoader(int, char*[], ServiceManager* services)
{

	srand(static_cast<unsigned int>(OTSYS_TIME()));
#ifdef _WIN32
	SetConsoleTitle(STATUS_SERVER_NAME);
#endif
	std::cout << STATUS_SERVER_NAME << " - Version " << STATUS_SERVER_VERSION << std::endl;
	std::cout << "Compiled with " << BOOST_COMPILER << std::endl;
	std::cout << "Compiled on " << __DATE__ << ' ' << __TIME__ << " for platform ";

#if defined(__amd64__) || defined(_M_X64)
	std::cout << "x64" << std::endl;
#elif defined(__i386__) || defined(_M_IX86) || defined(_X86_)
	std::cout << "x86" << std::endl;
#elif defined(__arm__)
	std::cout << "ARM" << std::endl;
#else
	std::cout << "unknown" << std::endl;
#endif
	std::cout << std::endl;

	std::cout << "A server developed by " << STATUS_SERVER_DEVELOPERS << std::endl;
	std::cout << std::endl;

	// read global config
	std::cout << ">> Loading config" << std::endl;
	if (!g_config.load()) {
		startupErrorMessage("Unable to load config.lua!");
		return;
	}

	std::cout << ">> Loading gameserver config..." << std::endl;

	if (!g_gameserver.load()) {
		startupErrorMessage("Unable to load gameservers!");
		return;
	}

#ifdef _WIN32
	const std::string& defaultPriority = g_config.getString(ConfigManager::DEFAULT_PRIORITY);
	if (strcasecmp(defaultPriority.c_str(), "high") == 0) {
		SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
	} else if (strcasecmp(defaultPriority.c_str(), "above-normal") == 0) {
		SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
	}
#endif

	//set RSA key
	
	//cipsoft
	const char* p("12017580013707233233987537782574702577133548287527131234152948150506251412291888866940292054989907714155267326586216043845592229084368540020196135619327879");
	const char* q("11898921368616868351880508246112101394478760265769325412746398405473436969889506919017477758618276066588858607419440134394668095105156501566867770737187273");
	
	//opentibia
	/*
	const char* p("14299623962416399520070177382898895550795403345466153217470516082934737582776038882967213386204600674145392845853859217990626450972452084065728686565928113");
	const char* q("7630979195970404721891201847792002125535401292779123937207447574596692788513647179235335529307251350570728407373705564708871762033017096809910315212884101");
	*/

	g_RSA.setKey(p, q);

	std::cout << ">> Establishing database connection..." << std::flush;

	Database* db = Database::getInstance();
	if (!db->connect()) {
		startupErrorMessage("Failed to connect to database.");
		return;
	}

	std::cout << " MySQL " << Database::getClientVersion() << std::endl;

	// run database manager
	std::cout << ">> Running database manager" << std::endl;

	if (!DatabaseManager::isDatabaseSetup()) {
		startupErrorMessage("The database you have specified in config.lua is empty, please import the schema.sql to your database.");
		return;
	}

	g_databaseTasks.start();

	if (g_config.getBoolean(ConfigManager::OPTIMIZE_DATABASE) && !DatabaseManager::optimizeTables()) {
		std::cout << "> No tables were optimized." << std::endl;
	}

	// Game client protocols
	services->add<ProtocolOld>(g_config.getNumber(ConfigManager::LOGIN_PORT));
	// OT protocols
	services->add<ProtocolStatus>(g_config.getNumber(ConfigManager::STATUS_PORT));

	//check each 1 minute for player amount and record
	ProtocolStatus::getPlayerRecordAndPlayerAmount();

	std::cout << ">> Loaded all modules, server starting up..." << std::endl;

#ifndef _WIN32
	if (getuid() == 0 || geteuid() == 0) {
		std::cout << "> Warning: " << STATUS_SERVER_NAME << " has been executed as root user, please consider running it as a normal user." << std::endl;
	}
#endif

	g_loaderSignal.notify_all();
}
