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

#include "stringutil.h"
#include <assert.h>
#include <ctype.h>
#include <string.h>

unsigned int strncopy(char *dest, const char *src, size_t count)
{
	assert(dest);
	assert(count > 0);

	if (!count)
		return 0;

	char *start = dest;
	while (*src && --count) {
		*dest++ = *src++;
	}

	*dest = '\0';

	return dest - start;
}

size_t strformat(char *buffer, size_t maxlen, const char *fmt, ...)
{
	assert(buffer);

	va_list ap;
	va_start(ap, fmt);
	size_t len = vsnprintf(buffer, maxlen, fmt, ap);
	va_end(ap);

	if (len >= maxlen) {
		buffer[maxlen - 1] = '\0';
		return maxlen - 1;
	}

	return len;
}

size_t strvformat(char *buffer, size_t maxlen, const char *fmt, va_list ap)
{
	assert(buffer);

	size_t len = vsnprintf(buffer, maxlen, fmt, ap);

	if (len >= maxlen) {
		buffer[maxlen - 1] = '\0';
		return (maxlen - 1);
	}

	return len;
}

char *strtrim(char *buffer)
{
	assert(buffer);

	char *start, *end;
	size_t len;

	start = buffer;

	// Find the first character that isn't whitespace
	while (isspace(*start))
		start++;

	// Get the length of the string starting from |start|
	len = strlen(start);

	// If the length is 0, all that's left is a null terminator
	// If the length is 1, all that's left is something that can't be whitespace
	if (len < 2)
		return start;

	// Set end to the last character
	end = &start[len - 1];

	// Find the last character that isn't whitespace
	while (isspace(*end))
		end--;

	// Null terminate at the first whitespace character after it
	end[1] = '\0';

	return start;
}

char *strip_comments(char *buffer)
{
	assert(buffer);

	char *i = buffer;

	// Find the first instance of two backslashes
	while (*i != '\0' && *i != '/' && i[1] != '\0' && i[1] != '/')
		i++;

	*i = '\0';

	return buffer;
}

void splitkv(char *buffer, char *keybuf, size_t keylen, char *valuebuf, size_t valuelen)
{
	assert(buffer);
	assert(keybuf);
	assert(valuebuf);

	size_t c, i, len = strlen(buffer);
	size_t keystart, keyend, valstart, valend;
	bool quote = false;

	for (i = 0; i < len; i++) {
		if (!isspace(buffer[i]))
			break;
	}

	if (buffer[i] == '"') {
		quote = true;
		i++;
	}

	keystart = i;

	for (; i < len; i++) {
		if (quote) {
			if (buffer[i] == '"')
				break;
		} else if (isspace(buffer[i])) {
			break;
		}
	}

	keyend = i++;
	quote = false;

	for (; i < len; i++) {
		if (!isspace(buffer[i]))
			break;
	}

	if (buffer[i] == '"') {
		quote = true;
		i++;
	}

	valstart = i;

	for (i = valstart; i < len; i++) {
		if (quote) {
			if (buffer[i] == '"')
				break;
		} else if (isspace(buffer[i])) {
			break;
		}
	}

	valend = i;

	// Copy key into key buffer
	for (c = 0, i = keystart; i < keyend && c < keylen; i++, c++)
		keybuf[c] = buffer[i];
	if (c == keylen)
		c--;
	keybuf[c] = '\0';

	// Copy value into value buffer
	for (c = 0, i = valstart; i < valend && c < valuelen; c++, i++)
		valuebuf[c] = buffer[i];
	if (c == valuelen)
		c--;
	valuebuf[c] = '\0';
}
