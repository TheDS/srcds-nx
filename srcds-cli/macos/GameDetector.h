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

#ifndef _INCLUDE_SRCDS_GAMEDETECTOR_H_
#define _INCLUDE_SRCDS_GAMEDETECTOR_H_

#include "amtl/am-string.h"

using namespace ke;

enum EngineBranch
{
	Engine_Unknown = -1,
	Engine_GarrysMod,
	Engine_SDK2013,      // AKA Orange Box or Source 2009
	Engine_Left4Dead,
	Engine_Left4Dead2,
	Engine_NuclearDawn,
	Engine_CSGO,
	Engine_Insurgency,
	Engine_DayOfInfamy
};

// Detects game and Source engine branch
class GameDetector
{
private:
	GameDetector(int argc, char **argv);
public:
	const AString &GetGameName() const;
	const char *GetEngineString() const;
	EngineBranch GetEngineBranch() const;
	static inline GameDetector &GetInstance(int argc, char **argv) {
		static GameDetector detector(argc, argv);
		initialized_ = true;
		return detector;
	}
	static inline GameDetector &GetInstance() {
		return GetInstance(0, nullptr);
	}
	static bool IsInitialized() {
		return initialized_;
	}
private:
	void DetectGameName(int argc, char **argv);
	void DetectGameEngine();
private:
	static inline bool initialized_ = false;
	EngineBranch gameEngine_;
	AString gameName_;
};

#endif // _INCLUDE_SRCDS_GAMEDETECTOR_H_
