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

#ifndef _INCLUDE_SRCDS_IGAMELIB_H_
#define _INCLUDE_SRCDS_IGAMELIB_H_

#include <stddef.h>

using CreateInterfaceFn = void *(*)(const char *, int *);

struct SymbolInfo
{
	const char *name;
	void *address;
};

class IGameLib
{
public:
	virtual CreateInterfaceFn GetFactory() = 0;
	virtual void *ResolveSymbol(const char *symbol) = 0;
	virtual void *ResolveHiddenSymbol(const char *symbol) = 0;
	virtual size_t ResolveHiddenSymbols(SymbolInfo *list, const char **names) = 0;
	virtual void *FindPattern(const char *pattern, size_t len) = 0;
	virtual void Close() = 0;
};

#endif // _INCLUDE_SRCDS_IGAMELIB_H_
