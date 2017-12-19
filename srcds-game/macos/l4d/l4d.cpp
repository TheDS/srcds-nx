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
#include <limits.h>
#include <crt_externs.h>
#include "l4d.h"

static IServerAPI *g_ServerAPI = nullptr;

static IDetour *detSetShaderApi;

// void CMaterialSystem::SetShaderAPI(const char *)
DETOUR_DECL_MEMBER1(CMaterialSystem_SetShaderAPI, void, const char *, pModuleName) {
	char module[PATH_MAX];
	pModuleName = Left4Dead::FixLibraryExt(pModuleName, module, sizeof(module));

	DETOUR_MEMBER_CALL(CMaterialSystem_SetShaderAPI)(pModuleName);

	detSetShaderApi->Destroy();
	detSetShaderApi = nullptr;
}

DETOUR_DECL_STATIC1(Sys_LoadModule, void *, const char *, pModuleName) {
	void *handle;
	char module[PATH_MAX];
	pModuleName = Left4Dead::FixLibraryExt(pModuleName, module, sizeof(module));

	handle = DETOUR_STATIC_CALL(Sys_LoadModule)(pModuleName);

	if (handle && strstr(module, "materialsystem")) {
		GameLibrary matsys(g_ServerAPI, "materialsystem");
		void *setShaderApi = matsys->ResolveHiddenSymbol("_ZN15CMaterialSystem12SetShaderAPIEPKc");
		if (!setShaderApi) {
			printf("Failed to find symbol _ZN15CMaterialSystem12SetShaderAPIEPKc in materialsystem\n");
			return nullptr;
		}

		detSetShaderApi = DETOUR_CREATE_MEMBER(CMaterialSystem_SetShaderAPI, setShaderApi);
		if (!detSetShaderApi) {
			printf("Failed to create detour for CMaterialsSystem::SetShaderAPI\n");
			return nullptr;
		}

		detSetShaderApi->Enable();
	}

	return handle;
}

static inline void dumpUnknownSymbols(const SymbolInfo *info, size_t len) {
	for (size_t i = 0; i < len; i++) {
		if (!info[i].address && info[i].name)
			puts(info[i].name);
	}
}

bool Left4Dead::Init(IServerAPI *api) {
	g_ServerAPI = api;
	sysLoadModule_ = nullptr;
	launcherMgr_ = nullptr;

	GameLibrary dedicated(api, "dedicated");
	if (!dedicated) {
		printf("Failed to load dedicated library\n");
		return false;
	}

	SymbolInfo info[3];
	memset(info, 0, sizeof(info));
	const char *symbols[] =
	{
		"_Z14Sys_LoadModulePKc",
		"g_FileSystem_Stdio",
		"_ZL17g_pBaseFileSystem",
		nullptr
	};

	size_t notFound = dedicated->ResolveHiddenSymbols(info, symbols);
	if (notFound != 0) {
		printf("Failed to locate the following symbols for dedicated:\n");
		dumpUnknownSymbols(info, ARRAY_LENGTH(info));
		return false;
	}

	void *loadModule = info[0].address;
	void *fileSystemStdio = info[1].address;
	void **pBaseFileSystem = (void **)info[2].address;

	sysLoadModule_ = DETOUR_CREATE_STATIC(Sys_LoadModule, loadModule);
	if (!sysLoadModule_) {
		printf("Failed to create detour for Sys_LoadModule\n");
		return false;
	}

	sysLoadModule_->Enable();

	// Work around conflicts between FileSystem_Stdio and FileSystem_Steam
	*pBaseFileSystem = fileSystemStdio;

	if (!BuildCmdLine())
		return false;

	return true;
}

bool Left4Dead::SetupLauncher(void *appSystemGroup) {
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

bool Left4Dead::PreLoadModules(void *appSystemGroup) {
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

bool Left4Dead::PostLoadModules(void *appSystemGroup) {
	GameLibrary engine(g_ServerAPI, "engine");
	if (!engine) {
		printf("Failed to load engine\n");
		return false;
	}

	void **engineCocoa = (void **)engine->ResolveHiddenSymbol("g_pLauncherCocoaMgr");
	if (!engineCocoa) {
		printf("Failed to locate symbol g_pLauncherCocoaMgr in engine\n");
		return false;
	}

	*engineCocoa = launcherMgr_;

	AppSystemInfo_t sys_after[] =
	{
		{"vguimatsurface.dylib",	"VGUI_Surface030"},
		{"vgui2.dylib",				"VGUI_ivgui008"},
		{"",						""}
	};

	g_ServerAPI->AddSystems(sys_after);

	return true;
}

void Left4Dead::Shutdown() {
	if (detSetShaderApi)
		detSetShaderApi->Destroy();

	if (sysLoadModule_)
		sysLoadModule_->Destroy();
}

IServerFixer *GetGameFixer() {
	return &Left4Dead::GetInstance();
}

// Replace .so extension with .dylib
const char *Left4Dead::FixLibraryExt(const char *pModuleName, char *buffer, size_t maxLength) {
	size_t origLen = strlen(pModuleName);

	// 3 extra chars are needed to do this.
	// NOTE: 2nd condition is NOT >= due to null terminator.
	if (origLen > 3 && maxLength > origLen + 3)
	{
		size_t baseLen = origLen - 3;
		if (strncmp(pModuleName + baseLen, ".so", 3) == 0)
		{
			// Yes, this should be safe now
			memcpy(buffer, pModuleName, baseLen);
			strcpy(buffer + baseLen, ".dylib");

			return buffer;
		}
	}

	return pModuleName;
}

bool Left4Dead::BuildCmdLine() {
	GameLibrary tier0(g_ServerAPI, "tier0");

	if (!tier0) {
		printf("Failed to load tier0 while attempting to build command line\n");
		return false;
	}

	char *cmdline = (char *)tier0->ResolveHiddenSymbol("_ZL12linuxCmdline");
	if (!cmdline) {
		printf("Failed to find symbol _ZL12linuxCmdline in tier0\n");
		return false;
	}

	const int maxCmdLine = 512;
	int len = 0;
	int argc;
	char **argv;

	g_ServerAPI->GetArgs(argc, argv);

	for (int i = 0; i < argc; i++) {
		// Account for spaces between command line paramaters and null terminator
		len += strlen(argv[i]) + 1;
	}

	if (len > maxCmdLine) {
		printf("Command line too long, %d max\n", maxCmdLine);
		return false;
	}

	cmdline[0] = '\0';
	for (int i = 0; i < argc; i++) {
		if (i > 0)
			strcat(cmdline, " ");
		strcat(cmdline, argv[i]);
	}

	return true;
}

