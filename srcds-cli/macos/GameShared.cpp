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

#define _DARWIN_BETTER_REALPATH
#include "GameShared.h"
#include "HSGameLib.h"
#include <stdio.h>
#include <mach-o/dyld.h>
#include <dlfcn.h>

static IServerAPI *g_ServerAPI = nullptr;
static IServerFixer *g_ServerFixer = nullptr;

static HSGameLib *g_Dedicated;
static void *g_AppSystemGroup;

// Path to bundle
static char *g_AppBundlePath;
static size_t g_AppBundlePathLen;

static IDetour *steamLoadModule;
static IDetour *addSearchPath;

DETOUR_DECL_STATIC2(Sys_SteamLoadModule, void *, const char *, pModuleName, int, flags)
{
	if (strstr(pModuleName, "steamservice"))
		return nullptr;
	else
		return DETOUR_STATIC_CALL(Sys_SteamLoadModule)(pModuleName, flags);
}

bool BlockSteamService()
{
	HSGameLib steamclient("steamclient");

	if (!steamclient) {
		printf("Failed to load steamclient\n");
		return false;
	}

	void *loadModule = steamclient.ResolveHiddenSymbol<void *>("_Z14Sys_LoadModulePKc9Sys_Flags");
	if (!loadModule) {
		printf("Failed to find symbol _Z14Sys_LoadModulePKc9Sys_Flags in steamclient\n");
		return false;
	}

	steamLoadModule = DETOUR_CREATE_STATIC(Sys_SteamLoadModule, loadModule);

	if (steamLoadModule) {
		steamLoadModule->Enable();
	} else {
		printf("Failed to create detour for steamclient`Sys_LoadModule!\n");
		return false;
	}

	return true;
}

// Detour for function in filesystem_stdio or dedicated libraries
//
// Search paths must be adjusted to account for the fact that the program binary is inside an
// app bundle on OS X (SrcDS.app/Contents/MacOS). This app bundle path must be removed if it
// exists in the given pPath.
//
// For example, /SteamApps/common/Counter-Strike Source/Srcds.app/Contents/MacOS/cstrike
// Should become: /SteamApps/common/Counter-Strike Source/cstrike
DETOUR_DECL_MEMBER3(CBaseFileSystem_AddSearchPath,
                    void, const char *, pPath, const char *, pPathID, int, addType) {
	GameShared::FixPath(pPath);

	// Call original function with the modified path
	DETOUR_MEMBER_CALL(CBaseFileSystem_AddSearchPath)(pPath, pPathID, addType);
}

// Alternate version of AddSearchPath
DETOUR_DECL_MEMBER4(CBaseFileSystem_AddSearchPathB,
                    void, const char *, pPath, const char *, pPathID, int, addType, bool, unknown) {
	GameShared::FixPath(pPath);

	DETOUR_MEMBER_CALL(CBaseFileSystem_AddSearchPathB)(pPath, pPathID, addType, unknown);
}

// Detour for function in dedicated library.
// This detour is particularly important because it sets up many of the other detours.
DETOUR_DECL_MEMBER1(CSys_LoadModules, bool, void *, appSystemGroup) {
	g_AppSystemGroup = appSystemGroup;

	if (!g_ServerFixer->PreLoadModules(appSystemGroup))
		return false;

	if (!DETOUR_MEMBER_CALL(CSys_LoadModules)(appSystemGroup))
		return false;

	if (!g_ServerFixer->PostLoadModules(appSystemGroup))
		return false;

	// HACK: Get current executable path. The working directory is then changed to this new path in
	//       order to avoid a problem where the game is unable to find its files and crash.
	chdir(GameShared::GetExecutablePath().chars());

	PatternType type;
	size_t len;
	AddSearchPathType searchProto;
	const char *searchSym = g_ServerFixer->GetAddSearchPath(type, len, searchProto);
	const char *altSearchSym = nullptr;
	void *searchPathFn = nullptr;

	switch (type) {
		case PatternType::Default:
			searchSym = "_ZN15CBaseFileSystem13AddSearchPathEPKcS1_15SearchPathAdd_t";
			altSearchSym = "_ZN15CBaseFileSystem13AddSearchPathEPKcS1_15SearchPathAdd_tb";
			[[fallthrough]];
		case PatternType::Symbol:
			searchPathFn = g_Dedicated->ResolveHiddenSymbol<void *>(searchSym);
			if (!searchPathFn) {
				HSGameLib filesys("filesystem_stdio");
				searchPathFn = filesys.ResolveHiddenSymbol<void *>(searchSym);

				if (!searchPathFn && altSearchSym) {
					searchPathFn = g_Dedicated->ResolveHiddenSymbol<void *>(altSearchSym);
					if (!searchPathFn)
						searchPathFn = filesys.ResolveHiddenSymbol<void *>(altSearchSym);
					if (type == PatternType::Default && searchPathFn)
						searchProto = AddSearchPathType::StringStringIntBool;
				}
			}
			break;
		case PatternType::Signature:
			searchPathFn = g_Dedicated->FindPattern(searchSym, len);
			if (!searchPathFn) {
				HSGameLib filesys("filesystem_stdio");
				searchPathFn = filesys.FindPattern(searchSym, len);
			}
			break;
	}

	if (!searchPathFn) {
		printf("Failed to find symbol for CBaseFileSystem::AddSearchPath\n");
		return false;
	}

	switch (searchProto) {
		case AddSearchPathType::StringStringInt:
			addSearchPath = DETOUR_CREATE_MEMBER(CBaseFileSystem_AddSearchPath, searchPathFn);
			break;
		case AddSearchPathType::StringStringIntBool:
			addSearchPath = DETOUR_CREATE_MEMBER(CBaseFileSystem_AddSearchPathB, searchPathFn);
			break;
	}

	if (!addSearchPath)
	{
		printf("Failed to create detour for CBaseFileSystem::AddVPK\n");
		return false;
	}

	addSearchPath->Enable();

	if (!BlockSteamService())
		return false;

	return true;
}

// Detour for function in tier0 library
DETOUR_DECL_STATIC1(Plat_DebugString, void, const char *, str)
{
	// Doing nothing here prevents duplicate message from being printed in the terminal
}

bool GameShared::Init(IServerAPI *api, IServerFixer *fixer) {
	g_ServerAPI = api;
	g_ServerFixer = fixer;

	g_Dedicated = new HSGameLib("dedicated");
	if (!g_Dedicated->IsValid())
	{
		printf("Failed to load and parse dedicated library.\n");
		return false;
	}

	void *sysLoad = g_Dedicated->ResolveHiddenSymbol<void *>("_ZN4CSys11LoadModulesEP24CDedicatedAppSystemGroup");
	if (!sysLoad) {
		printf("Failed to find symbol: _ZN4CSys11LoadModulesEP24CDedicatedAppSystemGroup\n");
		return false;
	}

	sysLoadModules_ = DETOUR_CREATE_MEMBER(CSys_LoadModules, sysLoad);
	if (!sysLoadModules_) {
		printf("Failed to create detour for CSys::LoadModules\n");
		return false;
	}

	sysLoadModules_->Enable();

	GameLib tier0("tier0");
	if (tier0.IsLoaded()) {
		auto debugStringAddr = tier0.ResolveSymbol<void *>("Plat_DebugString");
		debugString_ = DETOUR_CREATE_STATIC(Plat_DebugString, debugStringAddr);

		if (debugString_)
			debugString_->Enable();
	}

	return true;
}

void GameShared::Shutdown() {
	if (steamLoadModule)
		steamLoadModule->Destroy();

	if (addSearchPath)
		addSearchPath->Destroy();

	if (debugString_)
		debugString_->Destroy();

	if (sysLoadModules_)
		sysLoadModules_->Destroy();

	delete g_Dedicated;
}

// Get current executable path and strip off both the bundle path and the executable name.
// This is done to refer to the parent directory of the bundle (srcds-cli.bundle).
ke::AString GameShared::GetExecutablePath() {
	static ke::AString execPath;

	if (gotPath_)
		return execPath;

	char exePathBuf[PATH_MAX];
	char *exePath = exePathBuf;
	uint32_t size = sizeof(exePathBuf);
	bool alloc = false;
	if (_NSGetExecutablePath(exePath, &size) == -1) {
		exePath = new char[size];
		alloc = true;
	}

	// The path returned by _NSGetExecutablePath could be a symbolic link, so get the real path
	char *linkPath = realpath(exePath, nullptr);
	if (linkPath) {
		int numSeps = 0;
		size_t pathLen = strlen(linkPath);

		// Starting from the end of the path string, find the 4th path separator
		for (size_t i = pathLen - 1; i < pathLen; i--) {
			if (linkPath[i] == PLATFORM_SEP_CHAR) {
				numSeps++;

				if (numSeps == 1) {
					// Strip the executable name.
					// This is required because the app bundle path will be copied later
					linkPath[i] = '\0';
				} else if (numSeps == 4) {
					// Copy the app bundle path
					g_AppBundlePathLen = strlen(&linkPath[i]);
					g_AppBundlePath = new char[g_AppBundlePathLen + 1];
					strcpy(g_AppBundlePath, &linkPath[i]);

					// Now strip the app bundle path
					linkPath[i] = '\0';

					break;
				}
			}
		}

		execPath = linkPath;
		gotPath_ = true;

		free(linkPath);
	}

	// Cleanup
	if (alloc)
		delete[] exePath;

	return execPath;
}

void GameShared::FixPath(const char *path) {
	if (gotPath_) {
		// Find the app bundle path
		char *removePos = strcasestr(path, g_AppBundlePath);

		if (removePos)
		{
			// Calculate length of path string before and including app bundle path
			// In the case of /SteamApps/common/Counter-Strike Source/Srcds.app/Contents/MacOS/cstrike,
			// this should be the length of everything up to the "/cstrike" substring.
			size_t prefixLen = removePos - path + g_AppBundlePathLen;

			// Get the full length of the path
			size_t len = strlen(path);

			// Move the substring after the app bundle path up to the app bundle path's position
			// In the case of the above example, "/cstrike" would be moved to right after the
			// "Counter-Strike Source" substring.
			memmove(removePos, path + prefixLen, len - prefixLen + 1);
		}
	}
}

void GameShared::AddSystems(AppSystemInfo_t *systems) {
	using AddSystemsFn = bool (*)(void *, AppSystemInfo_t *);
	static auto AppSysGroup_AddSystems =
		g_Dedicated->ResolveHiddenSymbol<AddSystemsFn>("_ZN15CAppSystemGroup10AddSystemsEP15AppSystemInfo_t");

	if (AppSysGroup_AddSystems && g_AppSystemGroup) {
		AppSysGroup_AddSystems(g_AppSystemGroup, systems);
		return;
	}

	if (!AppSysGroup_AddSystems)
		printf("Failed to find symbol: _ZN15CAppSystemGroup10AddSystemsEP15AppSystemInfo_t\n");

	if (!g_AppSystemGroup)
		printf("Call aborted. AddSystems not ready yet!\n");
}
