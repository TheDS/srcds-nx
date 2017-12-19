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

#ifndef _INCLUDE_SRCDS_ISERVERFIX_H_
#define _INCLUDE_SRCDS_ISERVERFIX_H_

#include "platform.h"
#include "IServerAPI.h"

enum class PatternType {
	Default = -1,
	Symbol,
	Signature
};

enum class AddSearchPathType {
	StringStringInt,
	StringStringIntBool
};

class IServerFixer
{
public:
	virtual bool Init(IServerAPI *api) = 0;
	virtual bool PreLoadModules(void *appSystemGroup) = 0;
	virtual bool PostLoadModules(void *appSystemGroup) = 0;
	virtual const char *GetAddSearchPath(PatternType &type, size_t &len,
	                                     AddSearchPathType &prototype) {
		type = PatternType::Default;
		len = 0;
		prototype = AddSearchPathType::StringStringInt;
		return nullptr;
	}
	virtual void Shutdown() = 0;
};

SRCDS_API IServerFixer *GetGameFixer();

#endif // _INCLUDE_SRCDS_ISERVERFIX_H_
