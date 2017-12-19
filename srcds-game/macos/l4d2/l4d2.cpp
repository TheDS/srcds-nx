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
#include "l4d2.h"

static IServerAPI *g_ServerAPI = nullptr;

static inline void dumpUnknownSymbols(const SymbolInfo *info, size_t len) {
	for (size_t i = 0; i < len; i++) {
		if (!info[i].address && info[i].name)
			puts(info[i].name);
	}
}

bool Left4Dead2::Init(IServerAPI *api) {
	g_ServerAPI = api;
	launcherMgr_ = nullptr;
	return true;
}

bool Left4Dead2::SetupLauncher(void *appSystemGroup) {
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

bool Left4Dead2::PreLoadModules(void *appSystemGroup) {
	if (!SetupLauncher(appSystemGroup))
		return false;

	AppSystemInfo_t sys_before[] =
	{
		{"inputsystem.dylib",	"InputSystemVersion001"},
		{"",					""}
	};

	g_ServerAPI->AddSystems(sys_before);

	return true;
}

bool Left4Dead2::PostLoadModules(void *appSystemGroup) {
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

	return true;
}

void Left4Dead2::Shutdown() {

}

IServerFixer *GetGameFixer() {
	return &Left4Dead2::GetInstance();
}

