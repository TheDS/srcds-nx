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

#ifndef _INCLUDE_SRCDS_DOI_H_
#define _INCLUDE_SRCDS_DOI_H_

#include "IServerFixer.h"
#include "ins.h"

class DayOfInfamy : public Insurgency
{
public:
	bool Init(IServerAPI *api);
	bool PreLoadModules(void *appSystemGroup);
	bool PostLoadModules(void *appSystemGroup);
	void Shutdown();
	static inline DayOfInfamy &GetInstance() {
		static DayOfInfamy fixer;
		return fixer;
	}
private:
	void PatchMapStatus(GameLibrary engine);
private:
	IDetour *fileFindFirst_;
	IDetour *sysLoadModule_;
};

#endif // _INCLUDE_SRCDS_DOI_H_

