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

#ifndef _INCLUDE_SRCDS_COMMON_STRUTIL_H_
#define _INCLUDE_SRCDS_COMMON_STRUTIL_H_

#include <stdio.h>
#include <stdarg.h>

//
// Utility functions for C strings
//

// Copies |count| characters from |src| into |dest| and ensures null termination
unsigned int strncopy(char *dest, const char *src, size_t count);

// Alternative to snprintf that ensures null termination
size_t strformat(char *buffer, size_t maxlen, const char *fmt, ...);

// Alternative to vspnrintf that ensure null termination
size_t strvformat(char *buffer, size_t maxlen, const char *fmt, va_list ap);

// Returns pointer to string with whitespace removed from its left and right sides
char *strtrim(char *buffer);

// Returns pointer to string with single-line C++ comments (two back slashes) removed
char *strip_comments(char *buffer);

// Splits a key/value pair separated by spaces and places them into the given buffers
void splitkv(char *buffer, char *keybuf, size_t keylen, char *valuebuf, size_t valuelen);

#endif // _INCLUDE_SRCDS_COMMON_STRUTIL_H_
