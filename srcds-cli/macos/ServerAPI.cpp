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

#include "ServerAPI.h"
#include "CDetour/detours.h"
#include "GameShared.h"
#include "HSGameLib.h"

ServerAPI::ServerAPI(int argc, char **argv) : argc_(argc), argv_(argv) {

}

IGameLib *ServerAPI::LoadLibrary(const char *name) {
	HSGameLib *lib = new HSGameLib(name);

	if (!lib->IsLoaded()) {
		delete lib;
		return nullptr;
	}

	return lib;
}

IDetour *ServerAPI::CreateDetour(void *callbackfunction, void **trampoline, void *addr) {
	CDetour *detour = new CDetour(callbackfunction, trampoline);

	if (!detour->Init(addr)) {
		delete detour;
		return nullptr;
	}

	return detour;
}

void ServerAPI::FixPath(const char *path) {
	GameShared::FixPath(path);
}

void ServerAPI::GetArgs(int &argc, char ** &argv) {
	argc = argc_;
	argv = argv_;
}

void ServerAPI::AddSystems(AppSystemInfo_t *systems) {
	GameShared::AddSystems(systems);
}
