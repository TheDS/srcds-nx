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
#include "doi.h"
#include "sh_include.h"
#include "detourhelpers.h"
#include "signature.h"

static IServerAPI *g_ServerAPI = nullptr;

#if defined(PLATFORM_X64)
static IDetour *detMatSysLoadModule;

DETOUR_DECL_STATIC1(MaterialSys_LoadModule, void *, const char *, pModuleName) {
	pModuleName = "engine/doi/shaderapiempty.ovrd.dylib";

	void *handle = dlopen(pModuleName, RTLD_NOW);

	detMatSysLoadModule->Destroy();
	detMatSysLoadModule = nullptr;

	return handle;
}

DETOUR_DECL_STATIC1(Sys_LoadModule, void *, const char *, pModuleName) {
	void *handle;

	handle = DETOUR_STATIC_CALL(Sys_LoadModule)(pModuleName);

	if (handle && strstr(pModuleName, "materialsystem")) {
		GameLibrary matsys(g_ServerAPI, "materialsystem");
		void *matLoadModule = matsys->ResolveHiddenSymbol("_Z14Sys_LoadModulePKc");
		if (!matLoadModule) {
			printf("Failed to find symbol Sys_LoadModule in materialsystem\n");
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
#endif

DETOUR_DECL_MEMBER2(CBaseFileSystem_FindFirst,
                    const char *, const char *, pWildcard, int *, pHandle) {
	g_ServerAPI->FixPath(pWildcard);

	return DETOUR_MEMBER_CALL(CBaseFileSystem_FindFirst)(pWildcard, pHandle);
}

bool DayOfInfamy::Init(IServerAPI *api) {
	g_ServerAPI = api;
	fileFindFirst_ = nullptr;
	sysLoadModule_ = nullptr;

#if defined(PLATFORM_X64)
	GameLibrary dedicated(api, "dedicated");
	if (!dedicated) {
		printf("Failed to load dedicated library\n");
		return false;
	}

	void *loadModule = dedicated->ResolveHiddenSymbol("_Z14Sys_LoadModulePKc");
	if (!loadModule) {
		printf("Failed to locate symbol Sys_LoadModule in dedicated\n");
		return false;
	}

	sysLoadModule_ = DETOUR_CREATE_STATIC(Sys_LoadModule, loadModule);
	if (!sysLoadModule_) {
		printf("Failed to create detour for Sys_LoadModule\n");
		return false;
	}

	sysLoadModule_->Enable();
#endif

	return Insurgency::Init(api);
}

bool DayOfInfamy::PreLoadModules(void *appSystemGroup) {
	if (!Insurgency::PreLoadModules(appSystemGroup))
		return false;

	GameLibrary dedicated(g_ServerAPI, "dedicated");
	const char lib[] = "bin/vscript.dylib";
	char *badLib = (char *)dedicated->FindPattern(lib, sizeof(lib) - 1);
	if (!badLib) {
		printf("Warning: Unable to locate bad library, bin/vscript.dylib. Server may crash on exit\n");
	} else {
		// Prevent a crash on exit
		SetMemPatchable(badLib, 36);
		strcpy(badLib, "libvstdlib.dylib");
		strcpy(badLib + sizeof(lib), "VEngineCvar007");
		SetMemExec(badLib, 36);	// These strings are actually in executable memory
	}

	void *findFirst = dedicated->ResolveHiddenSymbol("_ZN15CBaseFileSystem9FindFirstEPKcPi");
	if (!findFirst) {
		printf("Failed to locate symbol _ZN15CBaseFileSystem9FindFirstEPKcPi in dedicated\n");
		return false;
	}

	fileFindFirst_ = DETOUR_CREATE_MEMBER(CBaseFileSystem_FindFirst, findFirst);
	if (!fileFindFirst_) {
		printf("Failed to create detour for CBaseFileSystem::FindFirst\n");
		return false;
	}

	fileFindFirst_->Enable();

	return true;
}

bool DayOfInfamy::PostLoadModules(void *appSystemGroup) {
	GameLibrary engine(g_ServerAPI, "engine");
	if (!engine) {
		printf("Failed to load existing copy of engine.dylib\n");
		return false;
	}

	PatchMapStatus(engine);

	return true;
}

void DayOfInfamy::PatchMapStatus(GameLibrary engine) {
	// Signature in middle of status command for printing the current map
#if defined (PLATFORM_X86)
	constexpr auto sig = MAKE_SIG("8B BB ? ? ? ? 80 BF");
	constexpr char PATCH_OFFS = 6;
	constexpr char JUMP_OFFS = 0x12;
	constexpr char SHORT_JMP_SIZE = 2;
	constexpr auto check = MAKE_SIG("8B 83"); // mov eax, [ebx+0x????] ; g_MainViewOrigin
#elif defined (PLATFORM_X64)
	constexpr auto sig = MAKE_SIG("4C 8D 25 ? ? ? ? 41 80");
	constexpr char PATCH_OFFS = 7;
	constexpr char JUMP_OFFS = 0x17;
	constexpr char SHORT_JMP_SIZE = 2;
	constexpr auto check = MAKE_SIG("48 8D 05"); // lea eax, [g_MainViewOrigin]
#endif

	char *p = (char *)engine->FindPattern(sig.pattern, sig.length);
	if (p) {
		p += PATCH_OFFS;

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

void DayOfInfamy::Shutdown() {
	if (fileFindFirst_)
		fileFindFirst_->Destroy();

	if (sysLoadModule_)
		sysLoadModule_->Destroy();

	return Insurgency::Shutdown();
}

IServerFixer *GetGameFixer() {
	return &DayOfInfamy::GetInstance();
}
