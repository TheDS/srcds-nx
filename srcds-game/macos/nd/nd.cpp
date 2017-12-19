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

#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include "nd.h"

static IServerAPI *g_ServerAPI = nullptr;

static IDetour *detMatSysLoadModule;

DETOUR_DECL_MEMBER0(CUploadGameStats_UpdateConnection, void) {
	// Do nothing
}

DETOUR_DECL_STATIC2(MaterialSys_LoadModule, void *, const char *, pModuleName, int, flags) {
	pModuleName = "engine/nd/shaderapiempty.ovrd.dylib";

	void *handle = dlopen(pModuleName, RTLD_NOW);

	detMatSysLoadModule->Destroy();
	detMatSysLoadModule = nullptr;

	return handle;
}

DETOUR_DECL_STATIC2(Sys_LoadModule, void *, const char *, pModuleName, int, flags) {
	void *handle;

	handle = DETOUR_STATIC_CALL(Sys_LoadModule)(pModuleName, flags);

	if (handle && strstr(pModuleName, "materialsystem")) {
		GameLibrary matsys(g_ServerAPI, "materialsystem");
		void *matLoadModule = matsys->ResolveHiddenSymbol("_Z14Sys_LoadModulePKc9Sys_Flags");
		if (!matLoadModule) {
			printf("Failed to find symbol _Z14Sys_LoadModulePKc9Sys_Flags in materialsystem\n");
			return nullptr;
		}

		detMatSysLoadModule = DETOUR_CREATE_STATIC(MaterialSys_LoadModule, matLoadModule);
		if (!detMatSysLoadModule) {
			printf("Failed to create detour for materialsystem`Sys_LoadModule\n");
			return nullptr;
		}

		detMatSysLoadModule->Enable();
	}

	return handle;
}

static inline void dumpUnknownSymbols(const SymbolInfo *info, size_t len) {
	for (size_t i = 0; i < len; i++) {
		if (!info[i].address && info[i].name)
			puts(info[i].name);
	}
}

bool NuclearDawn::Init(IServerAPI *api) {
	g_ServerAPI = api;
	sysLoadModule_ = nullptr;
	gameStatsUpdate_ = nullptr;
	launcherMgr_ = nullptr;

	GameLibrary dedicated(api, "dedicated");
	if (!dedicated) {
		printf("Failed to load dedicated library\n");
		return false;
	}

	void *loadModule = dedicated->ResolveHiddenSymbol("_Z14Sys_LoadModulePKc9Sys_Flags");
	if (!loadModule) {
		printf("Failed to locate symbol _Z14Sys_LoadModulePKc9Sys_Flags in dedicated\n");
		return false;
	}

	sysLoadModule_ = DETOUR_CREATE_STATIC(Sys_LoadModule, loadModule);
	if (!sysLoadModule_) {
		printf("Failed to create detour for Sys_LoadModule\n");
		return false;
	}

	sysLoadModule_->Enable();

	return true;
}

bool NuclearDawn::SetupLauncher(void *appSystemGroup) {
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
	launcherMgr_ = info[1].address;

	AppSysGroup_AddSystem(appSystemGroup, launcherMgr_, "CocoaMgrInterface006");

	return true;
}

bool NuclearDawn::PreLoadModules(void *appSystemGroup) {
	if (!SetupLauncher(appSystemGroup))
		return false;

	return true;
}

bool NuclearDawn::PostLoadModules(void *appSystemGroup) {
	GameLibrary engine(g_ServerAPI, "engine");
	if (!engine) {
		printf("Failed to load engine\n");
		return false;
	}

	void **engineCocoa = (void **)engine->ResolveHiddenSymbol("g_pLauncherMgr");
	if (!engineCocoa) {
		printf("Failed to locate symbol g_pLauncherCocoaMgr in engine\n");
		return false;
	}

	*engineCocoa = launcherMgr_;

	void *updateGameStats = engine->ResolveHiddenSymbol("_ZN16CUploadGameStats16UpdateConnectionEv");
	if (!updateGameStats) {
		printf("Failed to locate CUploadGameStats::UpdateConnection\n");
		return false;
	}

	gameStatsUpdate_ = DETOUR_CREATE_MEMBER(CUploadGameStats_UpdateConnection, updateGameStats);
	if (!gameStatsUpdate_) {
		printf("Failed to create detour for CUploadGameStats::UpdateConnection\n");
		return false;
	}

	gameStatsUpdate_->Enable();

	return true;
}

void NuclearDawn::Shutdown() {
	if (gameStatsUpdate_)
		gameStatsUpdate_->Destroy();

	if (detMatSysLoadModule)
		detMatSysLoadModule->Destroy();

	if (sysLoadModule_)
		sysLoadModule_->Destroy();
}

IServerFixer *GetGameFixer() {
	return &NuclearDawn::GetInstance();
}

