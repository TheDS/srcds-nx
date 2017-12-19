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

#ifndef _INCLUDE_SRCDS_STEAMLIBUPDATER_H_
#define _INCLUDE_SRCDS_STEAMLIBUPDATER_H_

#include "amtl/am-linkedlist.h"
#include "amtl/am-string.h"
#include "amtl/am-vector.h"
#include "url_fopen/url_fopen.h"

struct InstallEntry_t;
enum class ManifestType;

using namespace ke;

enum class SteamUniverse {
	Steam_Public,
	Steam_PublicBeta
};

enum class ManifestType {
	Manifest_Default = -1,
	Manifest_InstallList,
	Manifest_Url
};

struct ManifestEntry_t
{
	AString filename;
	AString sha2;
};

struct InstallEntry_t
{
	AString filename;
	int64_t size;
	time_t timestamp;
	uint32_t crc;
};

class SteamLibUpdater
{
public:
	SteamLibUpdater();
	~SteamLibUpdater();

	void Update(SteamUniverse universe);
private:
	bool IsUpdateAvailable(SteamUniverse universe, URL_FILE * &manifest);
	void ParseManifests();
	void DeleteOldVZips();
	void DeleteOldFiles();

	bool VerifyInstall();
	bool VerifyVZip(const char *vzip, const char *shastr);
	bool VerifyInstalledFiles(LinkedList<AString> &files);

	void DownloadVZip(const char *filename, const char *shortName);
	bool DecompressVZip(const char *path, const char *shortName, LinkedList<InstallEntry_t> &list,
	                    LinkedList<AString> &filter);
	void ExtractFiles(LinkedList<AString> &files);
	void WriteInstallList(LinkedList<InstallEntry_t> &list);
private:
	static constexpr const char *GetManifestName(SteamUniverse universe, ManifestType type);
	static void HexStringToBytes(AString str, unsigned char bytes[], size_t nBytes);
	static void AddToSortedInstallList(InstallEntry_t &entry, LinkedList<InstallEntry_t> &list);
	static int mkpath(const char *path, mode_t mode);
private:
	SteamUniverse universe_;
	unsigned long version_;
	Vector<ManifestEntry_t> vzips_;
	LinkedList<InstallEntry_t> installedFiles_;
};

#endif // _INCLUDE_SRCDS_STEAMLIBUPDATER_H_
