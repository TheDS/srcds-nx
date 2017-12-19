/**
 * vim: set ts=4 :
 * =============================================================================
 * Source Dedicated Server NX
 * Copyright (C) 2011-2017 Scott Ehlert and AlliedModders LLC.
 * All rights reserved.
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, AlliedModders LLC gives you permission to link the
 * code of this program (as well as its derivative works) to "Half-Life 2," the
 * "Source Engine," the "Steamworks SDK," and any Game MODs that run on software
 * by the Valve Corporation.  You must obey the GNU General Public License in
 * all respects for all other code used.  Additionally, AlliedModders LLC grants
 * this exception to all derivative works.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <list>
#include <type_traits>
#include "gmod.h"

static IServerAPI *g_ServerAPI = nullptr;

struct GameDepotInfo
{
	int unknown;            // Appears to always be 14
	int depotid;            // Game App ID
	const char *title;      // Game Name
	const char *folder;     // Game Directory
	bool mounted;           // Determines if game content is mounted
	bool vpk;               // Seems to flag games with VPK files before Steam3 became prevalent
	bool owned;             // Determines if game is owned
	bool installed;         // Determines if game is installed
};

using DepotGetList = std::list<GameDepotInfo> &(*)(void *);
using DepotLoad = void (*)(void *);
using FillDepotList = void (*)(std::list<GameDepotInfo> &);

DepotGetList g_GetDepotList;
DepotLoad g_LoadDepots;
FillDepotList g_FillDepotList;

DETOUR_DECL_MEMBER0(GameDepotSys_Clear, void) {
	static bool initialized = false;

	if (!initialized) {
		initialized = true;

		// Fill depot list with supported games
		std::list<GameDepotInfo> &depots = g_GetDepotList(this);
		g_FillDepotList(depots);

		// Dedicated servers on Linux and Windows set these for all games
		for (GameDepotInfo depot : depots) {
			depot.owned = true;
			depot.installed = true;
		}

		g_LoadDepots(this);
	}
}

DETOUR_DECL_MEMBER2(GameDepotSys_Mount, bool, GameDepotInfo &, info, bool, unknown) {
	return true;
}

DETOUR_DECL_MEMBER3(CBaseFileSystem_AddVPKFile,
					void, const char *, pszName, const char *, pPathID, unsigned int, addType) {
	g_ServerAPI->FixPath(pszName);

	DETOUR_MEMBER_CALL(CBaseFileSystem_AddVPKFile)(pszName, pPathID, addType);
}

static inline void dumpUnknownSymbols(const SymbolInfo *info, size_t len) {
	for (size_t i = 0; i < len; i++) {
		if (!info[i].address && info[i].name)
			puts(info[i].name);
	}
}

bool GarrysMod::Init(IServerAPI *api) {
	g_ServerAPI = api;
	return true;
}

bool GarrysMod::SetupLauncher(void *appSystemGroup) {
	launcher_.Load(g_ServerAPI, "launcher");
	if (!launcher_) {
		printf("Failed to load launcher\n");
		return false;
	}

	SymbolInfo info[2];
	memset(info, 0, sizeof(info));
	const char *symbols[] =
	{
		"_ZN15CAppSystemGroup9AddSystemEP10IAppSystemPKc",
		"g_CocoaMgr",
		nullptr
	};

	size_t notFound = launcher_->ResolveHiddenSymbols(info, symbols);
	if (notFound != 0) {
		printf("Failed to locate the following symbols for launcher:\n");
		dumpUnknownSymbols(info, ARRAY_LENGTH(info));
		return false;
	}

	typedef void (*AddSystem_t)(void *, void *, const char *);
	AddSystem_t AppSysGroup_AddSystem = (AddSystem_t)info[0].address;

	AppSysGroup_AddSystem(appSystemGroup, info[1].address, "CocoaMgrInterface006");

	return true;
}

bool GarrysMod::PreLoadModules(void *appSystemGroup) {
	if (!SetupLauncher(appSystemGroup))
		return false;

	GameLibrary fs(g_ServerAPI, "filesystem_stdio");
	if (!fs) {
		printf("Failed to load filesystem_stdio\n");
		return false;
	}

	SymbolInfo info[6];
	memset(info, 0, sizeof(info));
	const char *symbols[] =
	{
		"_ZN9GameDepot6System5ClearEv",
		"_ZN9GameDepot6System7GetListEv",
		"_ZN9GameDepot6System4LoadEv",
		"_ZN9GameDepot6System5MountERN16IGameDepotSystem11InformationEb",
		"_Z13FillDepotListRNSt3__14listIN16IGameDepotSystem11InformationENS_9allocatorIS2_EEEE",
		"_ZN15CBaseFileSystem10AddVPKFileEPKcS1_j",
		nullptr
	};

	size_t notFound = fs->ResolveHiddenSymbols(info, symbols);
	if (notFound != 0) {
		printf("Failed to locate the following symbols for filesystem_stdio:\n");
		dumpUnknownSymbols(info, ARRAY_LENGTH(info));
		return false;
	}

	void *depotClear, *depotMount, *addVPK;
	depotClear = info[0].address;
	g_GetDepotList = (DepotGetList)info[1].address;
	g_LoadDepots = (DepotLoad)info[2].address;
	depotMount = info[3].address;
	g_FillDepotList = (FillDepotList)info[4].address;
	addVPK = info[5].address;

	depotSetup_ = DETOUR_CREATE_MEMBER(GameDepotSys_Clear, depotClear);
	if (depotSetup_) {
		depotSetup_->Enable();
	} else {
		printf("Failed to create detour for GameDepot::System::Setup!\n");
		return false;
	}

	depotMount_ = DETOUR_CREATE_MEMBER(GameDepotSys_Mount, depotMount);
	if (depotMount_) {
		depotMount_->Enable();
	} else {
		printf("Failed to create detour for GameDepot::System::Mount!\n");
		return false;
	}

	addVPK_ = DETOUR_CREATE_MEMBER(CBaseFileSystem_AddVPKFile, addVPK);
	if (addVPK_) {
		addVPK_->Enable();
	} else {
		printf("Failed to create detour for CBaseFileSystem::AddVPKFile!\n");
		return false;
	}

	return true;
}

bool GarrysMod::PostLoadModules(void *appSystemGroup) {
	return true;
}

const char *GarrysMod::GetAddSearchPath(PatternType &type, size_t &len,
                                        AddSearchPathType &prototype) {
	static const char sym[] = "_ZN15CBaseFileSystem13AddSearchPathEPKcS1_j";
	type = PatternType::Symbol;
	len = sizeof(sym) - 1;
	prototype = AddSearchPathType::StringStringInt;
	return sym;
}

void GarrysMod::Shutdown() {
	if (addVPK_)
		addVPK_->Destroy();

	if (depotSetup_)
		depotSetup_->Destroy();

	if (depotMount_)
		depotMount_->Destroy();
}

IServerFixer *GetGameFixer() {
	return &GarrysMod::GetInstance();
}

