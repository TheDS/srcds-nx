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

#ifndef _INCLUDE_SRCDS_ISERVERAPI_H_
#define _INCLUDE_SRCDS_ISERVERAPI_H_

#include "IGameLib.h"
#include "IDetour.h"

struct AppSystemInfo_t
{
	const char *m_pModuleName;
	const char *m_pInterfaceName;
};

class IServerAPI
{
public:
	virtual IDetour *CreateDetour(void *callbackfunction, void **trampoline, void *addr) = 0;
	virtual void FixPath(const char *path) = 0;
	virtual void GetArgs(int &argc, char ** &argv) = 0;
	virtual void AddSystems(AppSystemInfo_t *systems) = 0;
protected:
	friend class GameLibrary;
	virtual IGameLib *LoadLibrary(const char *name) = 0;
};

class GameLibrary {
public:
	inline GameLibrary() : lib_(nullptr) { }

	inline GameLibrary(IServerAPI *api, const char *name) {
		Load(api, name);
	}

	inline ~GameLibrary() {
		if (lib_)
			lib_->Close();
	}

	inline void Load(IServerAPI *api, const char *name) {
		lib_ = api->LoadLibrary(name);
	}

	inline IGameLib *operator->() const {
		return lib_;
	}

	inline operator bool() const {
		return lib_ != nullptr;
	}
private:
	IGameLib *lib_;
};

#endif // _INCLUDE_SRCDS_ISERVERAPI_H_
