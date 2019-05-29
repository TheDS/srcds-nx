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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "csgo.h"
#include "sh_include.h"
#include "detourhelpers.h"
#include "signature.h"

static IServerAPI *g_ServerAPI = nullptr;

GameLibrary g_EmptyShader;

static void **g_pShaderAPI;
static void **g_pShaderAPIDX8;
static void **g_pShaderDevice;
static void **g_pShaderDeviceDx8;
static void **g_pShaderDeviceMgr;
static void **g_pShaderDeviceMgrDx8;
static void **g_pShaderShadow;
static void **g_pShaderShadowDx8;
static void **g_pHWConfig;
static void **g_pHardwareConfig;

static IDetour *detSetShaderApi;

DETOUR_DECL_MEMBER1(CMaterialSystem_SetShaderAPI, void, const char *, pModuleName) {
	CreateInterfaceFn shaderFactory;

	g_EmptyShader.Load(g_ServerAPI, "shaderapiempty");
	if (!g_EmptyShader)
	{
		printf("Failed to load shader API from %s\n", pModuleName);
		detSetShaderApi->Destroy();
		return;
	}

	shaderFactory = g_EmptyShader->GetFactory();
	if (!shaderFactory)
	{
		printf("Failed to get shader factory from %s\n", pModuleName);
		detSetShaderApi->Destroy();
		return;
	}

	*g_pShaderAPI = shaderFactory("ShaderApi029", NULL);
	*g_pShaderDevice = shaderFactory("ShaderDevice001", NULL);
	*g_pShaderDeviceMgr = shaderFactory("ShaderDeviceMgr001", NULL);
	*g_pShaderShadow = shaderFactory("ShaderShadow010", NULL);
	*g_pHWConfig = shaderFactory("MaterialSystemHardwareConfig013", NULL);

	*g_pShaderAPIDX8 = *g_pShaderAPI;
	*g_pShaderDeviceDx8 = *g_pShaderDevice;
	*g_pShaderDeviceMgrDx8 = *g_pShaderDeviceMgr;
	*g_pShaderShadowDx8 = *g_pShaderShadow;
	*g_pHardwareConfig = *g_pHWConfig;

	detSetShaderApi->Destroy();
}

DETOUR_DECL_STATIC1(Sys_LoadModule, void *, const char *, pModuleName) {
	void *handle = nullptr;

	// The matchmaking_ds lib is not shipped, so replace with matchmaking.dylib
	if (char *libName = strstr(const_cast<char *>(pModuleName), "matchmaking_ds.dylib"))
	{
		strcpy(libName, "matchmaking.dylib");
		return DETOUR_STATIC_CALL(Sys_LoadModule)(pModuleName);
	}

	handle = DETOUR_STATIC_CALL(Sys_LoadModule)(pModuleName);

	if (handle && strstr(pModuleName, "materialsystem")) {
		GameLibrary matsys(g_ServerAPI, "materialsystem");
		void *setShaderApi = nullptr;
		void ***vptr = (void ***)matsys->GetFactory()("VMaterialSystem080", nullptr);
		void **vtable = *vptr;
		setShaderApi = vtable[10]; // IMaterialSystem::SetShaderAPI

		detSetShaderApi = DETOUR_CREATE_MEMBER(CMaterialSystem_SetShaderAPI, setShaderApi);
		if (!detSetShaderApi) {
			printf("Failed to create detour for CMaterialSystem::SetShaderAPI\n");
			return NULL;
		}

		detSetShaderApi->Enable();

		// g_pShaderAPI: CShaderDeviceBase::GetWindowSize
		{
			constexpr auto sig = MAKE_SIG("55 48 89 E5 48 8B 3D ? ? ? ? 48 8B 07 48 8B 80 A8 00 00 00");
			constexpr int offset = sig.offsetOfWild();
			char *p = (char *)matsys->FindPattern(sig.pattern, sig.length);
			if (!p) {
				printf("Failed to find signature to locate g_pShaderAPI\n");
				return nullptr;
			}
			uint32_t shaderOffs = *(uint32_t *)(p + offset);
			g_pShaderAPI = (void **)(p + offset + 4 + shaderOffs);
		}

		// g_pShaderAPIDX8: CMatRenderContext::SetLights
		{
			constexpr auto sig = MAKE_SIG("55 48 89 E5 48 8D 05 ? ? ? ? 48 8B 38 48 8B 07 48 8B 80 38 03 00 00");
			constexpr int offset = sig.offsetOfWild();
			char *p = (char *)matsys->FindPattern(sig.pattern, sig.length);
			if (!p) {
				printf("Failed to find signature to locate g_pShaderAPIDX8\n");
				return nullptr;
			}
			uint32_t shaderOffs = *(uint32_t *)(p + offset);
			g_pShaderAPIDX8 = (void **)(p + offset + 4 + shaderOffs);
		}

		// g_pShaderDevice: CMatRenderContext::DestoryStaticMesh
		{
			constexpr auto sig = MAKE_SIG("55 48 89 E5 48 8D 05 ? ? ? ? 48 8B 38 48 8B 07 48 8B 80 C0 00 00 00");
			constexpr int offset = sig.offsetOfWild();
			char *p = (char *)matsys->FindPattern(sig.pattern, sig.length);
			if (!p) {
				printf("Failed to find signature to locate g_pShaderDevice\n");
				return nullptr;
			}
			uint32_t shaderOffs = *(uint32_t *)(p + offset);
			g_pShaderDevice = (void **)(p + offset + 4 + shaderOffs);
		}

		// g_pShaderDeviceDx8: CTempMeshDX8::GetDynamicMesh
		{
			constexpr auto sig = MAKE_SIG("55 48 89 E5 53 50 48 8B 5F 18 48 8D 05 ? ? ? ? 48");
			constexpr int offset = sig.offsetOfWild();
			char *p = (char *)matsys->FindPattern(sig.pattern, sig.length);
			if (!p) {
				printf("Failed to find signature to locate g_pShaderDeviceDx8\n");
				return nullptr;
			}
			uint32_t shaderOffs = *(uint32_t *)(p + offset);
			g_pShaderDeviceDx8 = (void **)(p + offset + 4 + shaderOffs);
		}

		// g_pShaderDeviceMgr: CMaterialSystem::GetModeCount
		{
			constexpr auto sig = MAKE_SIG("55 48 89 E5 48 8D 05 ? ? ? ? 48 8B 38 48 8B 07 48 8B 40 60");
			constexpr int offset = sig.offsetOfWild();
			char *p = (char *)matsys->FindPattern(sig.pattern, sig.length);
			if (!p) {
				printf("Failed to find signature to locate g_pShaderDeviceMgr\n");
				return nullptr;
			}
			uint32_t shaderOffs = *(uint32_t *)(p + offset);
			g_pShaderDeviceMgr = (void **)(p + offset + 4 + shaderOffs);
		}

		// g_pShaderDeviceMgrDx8: CShaderAPIDx8::OnDeviceInit
		{
			constexpr auto sig = MAKE_SIG("55 48 89 E5 41 57 41 56 53 50 48 89 FB E8 ? ? ? ? 48 8D 05 ? ? ? ? 48 8B 38 48 8B 07 8B 73 08");
			constexpr int offset = sig.offsetOfWild(5);
			char *p = (char *)matsys->FindPattern(sig.pattern, sig.length);
			if (!p) {
				printf("Failed to find signature to locate g_pShaderDeviceMgrDx8\n");
				return nullptr;
			}
			uint32_t shaderOffs = *(uint32_t *)(p + offset);
			g_pShaderDeviceMgrDx8 = (void **)(p + offset + 4 + shaderOffs);
		}

		// g_pShaderShadow: CMaterialSystem::Init (toward end of function)
		{
			constexpr auto sig = MAKE_SIG("48 8D 05 ? ? ? ? 48 C7 00 00 00 00 00 48 C7 83 ? ? ? ? 00 00 00 00 31 C0");
			constexpr int offset = sig.offsetOfWild();
			char *p = (char *)matsys->FindPattern(sig.pattern, sig.length);
			if (!p) {
				printf("Failed to find signature to locate g_pShaderShadow\n");
				return nullptr;
			}
			uint32_t shaderOffs = *(uint32_t *)(p + offset);
			g_pShaderShadow = (void **)(p + offset + 4 + shaderOffs);
		}

		// g_pShaderShadowDx8: CShaderAPIDx8::OnDeviceInit (middle of function)
		{
			constexpr auto sig = MAKE_SIG("48 8D 05 ? ? ? ? 48 8B 38 48 8B 07 FF 10 4C 89 FF");
			constexpr int offset = sig.offsetOfWild();
			char *p = (char *)matsys->FindPattern(sig.pattern, sig.length);
			if (!p) {
				printf("Failed to find signature to locate g_pShaderShadowDx8\n");
				return nullptr;
			}
			uint32_t shaderOffs = *(uint32_t *)(p + offset);
			g_pShaderShadowDx8 = (void **)(p + offset + 4 + shaderOffs);
		}

		// g_pHWConfig: GetHardwareConfig (static)
		{
			constexpr auto sig = MAKE_SIG("55 48 89 E5 48 8B 0D ? ? ? ? 31 C0");
			constexpr int offset = sig.offsetOfWild();
			char *p = (char *)matsys->FindPattern(sig.pattern, sig.length);
			if (!p) {
				printf("Failed to find signature to locate g_pHWConfig\n");
				return nullptr;
			}
			uint32_t shaderOffs = *(uint32_t *)(p + offset);
			g_pHWConfig = (void **)(p + offset + 4 + shaderOffs);
		}

		// g_pHardwareConfig: CShaderAPIDx8::OnAdapterSet
		{
			constexpr auto sig = MAKE_SIG("55 48 89 E5 41 56 53 48 89 FB E8 ? ? ? ? 84 C0 74 38 4C 8D 35 ? ? ? ? 49");
			constexpr int offset = sig.offsetOfWild(5);
			char *p = (char *)matsys->FindPattern(sig.pattern, sig.length);
			if (!p) {
				printf("Failed to find signature to locate g_pHardwareConfig\n");
				return nullptr;
			}
			uint32_t shaderOffs = *(uint32_t *)(p + offset);
			g_pHardwareConfig = (void **)(p + offset + 4 + shaderOffs);
		}
	}

	return handle;
}

bool CSGO::Init(IServerAPI *api) {
	g_ServerAPI = api;
	return true;
}

bool CSGO::PreLoadModules(void *appSystemGroup) {
	void *loadModule = nullptr;
	GameLibrary fs(g_ServerAPI, "filesystem_stdio");

	if (fs) {
		//loadModule = fs.ResolveHiddenSymbol("_Z14Sys_LoadModulePKc");
		constexpr auto sig = MAKE_SIG("55 48 89 E5 41 57 41 56 41 54 53 48 81 EC 10 08 00 00");
		loadModule = fs->FindPattern(sig.pattern, sig.length);
		if (!loadModule) {
			printf("Failed to find signature for filesystem_stdio.dylib\n");
			printf("_Z14Sys_LoadModulePKc");
			return false;
		}
		if (loadModule) {
			fsLoadModule_ = DETOUR_CREATE_STATIC(Sys_LoadModule, loadModule);
			if (fsLoadModule_)
				fsLoadModule_->Enable();
			else {
				printf("Failed to create detour for filesystem_stdio`Sys_LoadModule!\n");
				return false;
			}
		}
	} else {
		printf("Failed to preload filesystem_stdio\n");
		return false;
	}

	return true;
}

bool CSGO::PostLoadModules(void *appSystemGroup) {
	GameLibrary dedicated(g_ServerAPI, "dedicated");
	if (!dedicated) {
		printf("Failed to load existing copy of dedicated.dylib\n");
		return false;
	}

	typedef void *(*FindSystem_t)(void *, const char *);
	FindSystem_t AppSysGroup_FindSystem = (FindSystem_t)dedicated->ResolveHiddenSymbol("_ZN15CAppSystemGroup10FindSystemEPKc");
	if (!AppSysGroup_FindSystem) {
		printf("Failed to resolve dedicated symbol: _ZN15CAppSystemGroup10FindSystemEPKc\n");
		return false;
	}

	void *sdl = AppSysGroup_FindSystem(appSystemGroup, "SDLMgrInterface001");

	GameLibrary engine(g_ServerAPI, "engine");
	if (!engine) {
		printf("Failed to load existing copy of engine.dylib\n");
		return false;
	}

	// CGame::GetMainWindowAddress
	constexpr auto sig = MAKE_SIG("55 48 89 E5 53 50 48 89 FB 48 8D 05 ? ? ? ? 48 8B 38 48 8B 07 FF 90 08 01 00 00");
	constexpr int offset = sig.offsetOfWild();
	//void **engineSdl = engine.ResolveHiddenSymbol<void **>("g_pLauncherMgr");
	char *p = (char *)engine->FindPattern(sig.pattern, sig.length);
	if (!p) {
		printf("Failed to find signature for engine.dylib\n");
		printf("g_pLauncherMgr");
		return false;
	}

	uint32_t launcherOffs = *(uint32_t *)(p + offset);
	void **engineSdl = (void **)(p + offset + 4 + launcherOffs);
	*engineSdl = sdl;

	// Patch status command to show current map on dedicated server
	PatchMapStatus(engine);

	return true;
}

void CSGO::PatchMapStatus(GameLibrary engine)
{
	// Signature in middle of Host_PrintStatus for printing the current map
	constexpr auto sig = MAKE_SIG("4C 8D 2D ? ? ? ? 49 8B 45 00 4C 89 EF FF 90");
	char *p = (char *)engine->FindPattern(sig.pattern, sig.length);
	if (p) {
		p += 7;

		constexpr char JUMP_OFFS = 0x25;
		constexpr char SHORT_JMP_SIZE = 2;
		constexpr auto check = MAKE_SIG("49 8B 45 00"); // mov rax, [r13+0]

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

const char *CSGO::GetAddSearchPath(PatternType &type, size_t &len, AddSearchPathType &prototype) {
	static constexpr auto sig = MAKE_SIG("55 48 89 E5 41 57 41 56 41 55 41 54 53 48 81 EC 58 0A 00 00");
	type = PatternType::Signature;
	len = sig.length;
	prototype = AddSearchPathType::StringStringInt;
	return sig.pattern;
}

void CSGO::Shutdown() {
	if (fsLoadModule_)
		fsLoadModule_->Destroy();
}

IServerFixer *GetGameFixer() {
	return &CSGO::GetInstance();
}
