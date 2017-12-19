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
#include "ins.h"
#include "sh_include.h"
#include "detourhelpers.h"
#include "signature.h"

static IServerAPI *g_ServerAPI = nullptr;

DETOUR_DECL_MEMBER0(CSDLMgr_Init, int) {
	return 1;
}

static inline void dumpUnknownSymbols(const SymbolInfo *info, size_t len) {
	for (size_t i = 0; i < len; i++) {
		if (!info[i].address && info[i].name)
			puts(info[i].name);
	}
}

bool Insurgency::Init(IServerAPI *api) {
	g_ServerAPI = api;
	sdlInit_ = nullptr;
	return true;
}

bool Insurgency::SetupLauncher(void *appSystemGroup) {
	launcher_.Load(g_ServerAPI, "launcher");
	if (!launcher_) {
		printf("Failed to load launcher\n");
		return false;
	}

	SymbolInfo info[3];
	memset(info, 0, sizeof(info));
	const char *symbols[] =
	{
		"_ZN15CAppSystemGroup9AddSystemEP10IAppSystemPKc",
		"_Z12CreateSDLMgrv",
		"_ZN7CSDLMgr4InitEv",
		nullptr
	};

	size_t notFound = launcher_->ResolveHiddenSymbols(info, symbols);
	if (notFound != 0) {
		printf("Failed to locate the following symbols for launcher:\n");
		dumpUnknownSymbols(info, ARRAY_LENGTH(info));
		return false;
	}

	using AddSystemFn = void (*)(void *, void *, const char *);
	using CreateSDLMgrFn = void *(*)(void);
	AddSystemFn AppSysGroup_AddSystem = (AddSystemFn)info[0].address;
	CreateSDLMgrFn CreateSDLMgr = (CreateSDLMgrFn)info[1].address;
	void *initSDL = info[2].address;

	sdlInit_ = DETOUR_CREATE_MEMBER(CSDLMgr_Init, initSDL);
	if (!sdlInit_) {
		printf("Failed to create detour for CSDLMgr::Init!\n");
		return false;
	}

	sdlInit_->Enable();

	AppSysGroup_AddSystem(appSystemGroup, CreateSDLMgr(), "SDLMgrInterface001");

	return true;
}

bool Insurgency::PreLoadModules(void *appSystemGroup) {
	if (!SetupLauncher(appSystemGroup))
		return false;

	return true;
}

bool Insurgency::PostLoadModules(void *appSystemGroup) {
	GameLibrary engine(g_ServerAPI, "engine");
	if (!engine) {
		printf("Failed to load existing copy of engine.dylib\n");
		return false;
	}

	PatchMapStatus(engine);

	return true;
}

void Insurgency::PatchMapStatus(GameLibrary engine) {
	// Signature in middle of status command for printing the current map
	constexpr auto sig = MAKE_SIG("8B BB ? ? ? ? 8B 07 89 3C 24 FF 50 60 84 C0 75 43");
	char *p = (char *)engine->FindPattern(sig.pattern, sig.length);
	if (p) {
		p += 6;

		constexpr char JUMP_OFFS = 0x15;
		constexpr char SHORT_JMP_SIZE = 2;
		constexpr auto check = MAKE_SIG("8B 07"); // mov eax, [edi]

		// Make sure the place we're jumping to matches what we expect
		if (memcmp(p + JUMP_OFFS + SHORT_JMP_SIZE, check.pattern, check.length) != 0)
			return;

		SetMemPatchable(p, SHORT_JMP_SIZE);
		*(char *)p++ = 0xEB;      // jmp
		*(char *)p = JUMP_OFFS;   // offset
		SetMemExec(p, SHORT_JMP_SIZE);

		// Patch the map string to remove the location
		const char mapString[] = "map     : %s at";
		char *str = (char*)engine->FindPattern(mapString, sizeof(mapString) - 1);
		if (str) {
			SetMemPatchable(str, sizeof(mapString) - 1);
			strcpy(str + 12, "\n");
			SetMemExec(str, sizeof(mapString) - 1);
		}
	}
}

void Insurgency::Shutdown() {
	if (sdlInit_)
		sdlInit_->Destroy();
}
