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

#include "GameDetector.h"
#include "GameLib.h"
#include <stdio.h>

static const char *engineStrings[] =
{
	"gmod",
	"sdk2013",
	"l4d",
	"l4d2",
	"nd",
	"csgo",
	"ins",
	"doi"
};

GameDetector::GameDetector(int argc, char *argv[]) : gameEngine_(Engine_Unknown) {
	DetectGameName(argc, argv);
	DetectGameEngine();
}

const AString& GameDetector::GetGameName() const {
	return gameName_;
}

EngineBranch GameDetector::GetEngineBranch() const {
	return gameEngine_;
}

const char *GameDetector::GetEngineString() const {
	if (gameEngine_ == Engine_Unknown)
		return nullptr;

	return engineStrings[gameEngine_];
}

void GameDetector::DetectGameName(int argc, char *argv[]) {
	// Look for game name on the command line
	for (int i = 0; i < argc; i++)
	{
		if (strcmp(argv[i], "-game") == 0 && ++i != argc)
		{
			gameName_ = argv[i];
			break;
		}
	}
}

void GameDetector::DetectGameEngine()
{
	GameLib vstdlib("vstdlib");
	GameLib engine("engine");
	CreateInterfaceFn engineFactory;
	CreateInterfaceFn vstdlibFactory;

	if (!vstdlib) {
		printf("Failed to load vstdlib library.\n");
		return;
	}
	if (!engine) {
		printf("Failed to load engine library.\n");
		return;
	}
	if ((engineFactory = engine.GetFactory()) == nullptr) {
		printf("Failed to find engine factory.\n");
		return;
	}
	if ((vstdlibFactory = vstdlib.GetFactory()) == nullptr) {
		printf("Failed to load vstdlib library.\n");
		return;
	}

	if (engineFactory("VEngineServer023", nullptr)) {
		if (engineFactory("EngineTraceServer003", nullptr) &&
			vstdlibFactory("VEngineCvar004", nullptr)) {
			gameEngine_ = Engine_SDK2013;
		} else if (engineFactory("IEngineSoundServer004", nullptr)) {
			if (gameName_.compare("doi") == 0)
				gameEngine_ = Engine_DayOfInfamy;
			else
				gameEngine_ = Engine_Insurgency;
		} else {
			gameEngine_ = Engine_CSGO;
		}
	} else if (engineFactory("VEngineServer022", nullptr) &&
			   vstdlibFactory("VEngineCvar007", nullptr)) {
		GameLib datacache("datacache");
		CreateInterfaceFn datacacheFactory = datacache.GetFactory();

		if (datacacheFactory("VPrecacheSystem001", nullptr)) {
			if (gameName_.compare("nucleardawn") == 0)
				gameEngine_ = Engine_NuclearDawn;
			else
				gameEngine_ = Engine_Left4Dead2;
		} else {
			gameEngine_ = Engine_Left4Dead;
		}
	} else if (engineFactory("VEngineServer021", nullptr) &&
			   engineFactory("EngineTraceServer003", nullptr)) {
		gameEngine_ = Engine_GarrysMod;
	}
}
