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

#ifndef _INCLUDE_SRCDS_GAMESHARED_H_
#define _INCLUDE_SRCDS_GAMESHARED_H_

#include "IServerFixer.h"
#include "detours.h"
#include "am-string.h"

class GameShared
{
public:
	bool Init(IServerAPI *api, IServerFixer *fixer);
	void Shutdown();
	static inline GameShared &GetInstance() {
		static GameShared fix;
		return fix;
	}
	static ke::AString GetExecutablePath();
	static void FixPath(const char *path);
	static void AddSystems(AppSystemInfo_t *systems);
private:
	IDetour *sysLoadModules_;
	IDetour *debugString_;
	static inline bool gotPath_ = false;
};

#endif // _INCLUDE_SRCDS_GAMESHARED_H_
