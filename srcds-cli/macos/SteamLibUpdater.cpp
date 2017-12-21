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

#include "SteamLibUpdater.h"
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include "lzma/LzmaLib.h"
#include "lzma/Sha256.h"
#include "miniz/miniz.h"
#include "cocoa_helpers.h"
#include "stringutil.h"

#pragma pack(push)
#pragma pack(1)

struct VZHeader
{
	char V;
	char Z;
	char version;
	uint32_t rtime_created;
};

struct VZFooter
{
	uint32_t crc;
	uint32_t size;
	char z;
	char v;
};

#pragma pack(pop)

#define BASE_URL "https://steamcdn-a.akamaihd.net/client/"
#define STEAM_MANIFEST_RELEASE	"steam_client_osx"
#define STEAM_MANIFEST_BETA		"steam_client_publicbeta_osx"
#define EXT_MANIFEST	".manifest"
#define EXT_INSTALL 	".installed"

SteamLibUpdater::SteamLibUpdater() : version_(0) {
	curl_global_init(CURL_GLOBAL_SSL);
}

SteamLibUpdater::~SteamLibUpdater() {
	curl_global_cleanup();
}

void SteamLibUpdater::Update(SteamUniverse universe) {
	universe_ = universe;

	URL_FILE *manifest = nullptr;
	if (!IsUpdateAvailable(universe, manifest))
		return;

	char buffer[384], key[192], value[192];
	int level = 0;

	char *shortName = nullptr;

	url_rewind(manifest);
	DeleteOldVZips();
	DeleteOldFiles();

	chdir("package");
	FILE *outFile = fopen(GetManifestName(universe_, ManifestType::Default), "wb");
	if (!outFile) {
		printf("Failed to write steam manifest file\n");
		return;
	}

	size_t nread;
	char manifestBuf[1024];

	do {
		nread = url_fread(manifestBuf, 1, sizeof(manifestBuf), manifest);
		fwrite(manifestBuf, 1, nread, outFile);
	} while (nread);

	fclose(outFile);

	printf("Updating Steam libraries...\n");
	url_rewind(manifest);

	LinkedList<InstallEntry_t> list;

	while (!url_feof(manifest) && url_fgets(buffer, sizeof(buffer), manifest) != nullptr) {
		char *pBuf = buffer;
		pBuf = strip_comments(pBuf);
		pBuf = strtrim(pBuf);

		if (*pBuf == '{')
			level++;
		else if (*pBuf == '}' && level > 0)
			level--;
		else {
			splitkv(pBuf, key, sizeof(key), value, sizeof(value));

			if (level == 1) {
				if (strcasecmp(key, "bins_osx") == 0 ||
					strcasecmp(key, "bins_client_osx") == 0 ||
					strcasecmp(key, "breakpad_osx") == 0)
					shortName = strdup(key);
				continue;
			}

			if (shortName && level == 2) {
				if (strcasecmp(key, "zipvz") == 0) {
					chdir("package");
					DownloadVZip(value, shortName);
					LinkedList<AString> filter;
					DecompressVZip(value, shortName, list, filter);
					free(shortName);
					shortName = nullptr;
				}
			}
		}
	}

	if (shortName)
		free(shortName);

	url_fclose(manifest);

	WriteInstallList(list);
}

bool SteamLibUpdater::IsUpdateAvailable(SteamUniverse universe, URL_FILE * &manifest) {
	printf("Checking for Steam library update...\n");

	AString steamPath = GetSteamPath();
	chdir(steamPath.chars());

	mkdir("package", 0755);
	chdir("package");

	bool universeChanged = false;
	SteamUniverse prevUniverse;
	if (IsUniverseChange(prevUniverse)) {
		universeChanged = true;
		if (prevUniverse == SteamUniverse::Public && universe == SteamUniverse::PublicBeta)
			printf("Switching to beta version of Steam libraries\n");
		else
			printf("Switching to non-beta version of Steam libraries\n");
		ParseManifests(prevUniverse);
	} else {
		ParseManifests(universe);
	}

	if (universe == SteamUniverse::PublicBeta) {
		FILE *beta = fopen("beta", "w");
		if (beta)
			fclose(beta);
	} else {
		unlink("beta");
	}

	if (version_ != 0)
		printf("Installed version: %lu\n", version_);
	else
		printf("Installed version: None\n");

	manifest = url_fopen(GetManifestName(universe_, ManifestType::Url), "r");

	if (!manifest) {
		printf("Failed to download Steam manifest!\n");
		return true;
	}

	char buffer[256], key[128], value[128];
	int level = 0;

	while (!url_feof(manifest) && url_fgets(buffer, sizeof(buffer), manifest)) {
		char *pBuf = buffer;
		pBuf = strip_comments(pBuf);
		pBuf = strtrim(pBuf);

		if (*pBuf == '{')
			level++;
		else if (*pBuf == '}' && level > 0)
			level--;
		else {
			splitkv(pBuf, key, sizeof(key), value, sizeof(value));
			if (level == 1) {
				if (strcasecmp(key, "version") == 0) {
					unsigned long manifestVersion;
					if (sscanf(value, "%lu", &manifestVersion) == 1) {
						printf("Latest version: %s\n", value);
						if (manifestVersion == version_) {
							if (universeChanged) {
								// Special case if universe changed and version remained the same
								const char *oldManifest = GetManifestName(prevUniverse, ManifestType::Default);
								const char *newManifest = GetManifestName(universe, ManifestType::Default);
								rename(oldManifest, newManifest);
								unlink(GetManifestName(prevUniverse, ManifestType::InstallList));
								WriteInstallList(installedFiles_);
							}
							printf("Verifying installation...\n");

							if (VerifyInstall()) {
								printf("No Steam library update required\n");
								return false;
							}

							printf("Corruption detected\n");
							return true;
						}
					}
					break;
				}
			}
		}
	}

	return true;
}

bool SteamLibUpdater::IsUniverseChange(SteamUniverse &changedFrom) {
	FILE *beta = fopen("beta", "r");

	if (beta) {
		fclose(beta);
		if (universe_ == SteamUniverse::Public) {
			changedFrom = SteamUniverse::PublicBeta;
			return true;
		}
	} else if (universe_ == SteamUniverse::PublicBeta) {
		changedFrom = SteamUniverse::Public;
		return true;
	}

	return false;
}

void SteamLibUpdater::ParseManifests(SteamUniverse universe) {
	FILE *currentManifest = fopen(GetManifestName(universe, ManifestType::Default), "rt");

	if (!currentManifest)
		return;

	int level = 0;
	char buffer[512], key[256], value[256];
	bool gotFile = false;
	ManifestEntry_t manifestEntry;

	while (!feof(currentManifest) && fgets(buffer, sizeof(buffer), currentManifest)) {
		char *pBuf = buffer;
		pBuf = strip_comments(pBuf);
		pBuf = strtrim(pBuf);

		if (*pBuf == '{')
			level++;
		else if (*pBuf == '}' && level > 0)
			level--;
		else {
			splitkv(pBuf, key, sizeof(key), value, sizeof(value));

			if (level == 1) {
				if (strcasecmp(key, "version") == 0) {
					if (sscanf(value, "%lu", &version_) != 1) {
						fclose(currentManifest);
						return;
					}
				} else if (strcmp(key, "bins_osx") == 0 ||
						   strcmp(key, "bins_client_osx") == 0 ||
						   strcmp(key, "breakpad_osx") == 0) {
					gotFile = true;
				}
			} else if (gotFile && level == 2) {
				if (strcmp(key, "zipvz") == 0)
					manifestEntry.filename = value;
				else if (strcmp(key, "sha2vz") == 0)
					manifestEntry.sha2 = value;

				if (manifestEntry.filename.length() && manifestEntry.sha2.length()) {
					vzips_.append(manifestEntry);
					manifestEntry.filename = nullptr;
					manifestEntry.sha2 = nullptr;
					gotFile = false;
				}
			}
		}
	}

	fclose(currentManifest);

	FILE *installList = fopen(GetManifestName(universe, ManifestType::InstallList), "rt");

	if (!installList)
		return;

	CSha256 sha256;
	Byte digest[32];
	Sha256_Init(&sha256);

	while (!feof(installList) && fgets(buffer, sizeof(buffer), installList)) {
		if (strncmp(buffer, "SHA2=", 5) == 0) {
			AString origSha(&buffer[5], 64);

			Byte orig[32];

			// Convert SHA256 digest string to array of bytes
			HexStringToBytes(origSha, orig, sizeof(orig));

			Sha256_Final(&sha256, digest);

			if (memcmp(&orig, &digest, sizeof(digest)) == 0) {
				break;
			} else {
				installedFiles_.clear();
				fclose(installList);
				return;
			}
		}

		Sha256_Update(&sha256, (Byte *)buffer, strlen(buffer));

		char *comma = strchr(buffer, ',');
		if (comma)
			*comma = '\0';
		else
			return;

		InstallEntry_t entry;
		entry.filename = buffer;
		sscanf(comma + 1, "%lld;%ld;%u", &entry.size, &entry.timestamp, &entry.crc);

		installedFiles_.append(entry);
	}

	fclose(installList);
}

void SteamLibUpdater::DeleteOldVZips() {
	chdir("package");

	for (ManifestEntry_t vzip : vzips_)
		unlink(vzip.filename.chars());
}

void SteamLibUpdater::DeleteOldFiles() {
	Vector<AString> directories;

	// First delete all files
	for (InstallEntry_t entry : installedFiles_) {
		const char *filename = entry.filename.chars();

		if (entry.size == -1)
			directories.append(AString(filename));
		else
			unlink(filename);
	}

	// Then delete directories
	for (auto it = directories.end(); it-- != directories.begin();)
		rmdir((*it).chars());
}

bool SteamLibUpdater::VerifyInstall() {
	if (vzips_.length() == 0)
		return false;

	for (ManifestEntry_t vzip : vzips_) {
		if (!VerifyVZip(vzip.filename.chars(), vzip.sha2.chars()))
			return false;
	}

	chdir("..");
	LinkedList<AString> files;
	if (!VerifyInstalledFiles(files))
		ExtractFiles(files);

	return true;
}

bool SteamLibUpdater::VerifyVZip(const char *vzip, const char *shastr) {
	int vfd = open(vzip, O_RDONLY);
	if (vfd == -1)
		return false;

	struct stat statbuf;
	if (fstat(vfd, &statbuf) != 0)
		return false;

	unsigned char *src = (unsigned char *)mmap(NULL, statbuf.st_size, PROT_READ, MAP_PRIVATE, vfd, 0);
	close(vfd);
	if (src == MAP_FAILED)
		return false;

	AString origSha(shastr);
	CSha256 sha256;
	Byte digest[32];
	Sha256_Init(&sha256);
	Sha256_Update(&sha256, src, statbuf.st_size);
	Sha256_Final(&sha256, digest);

	Byte orig[32];

	HexStringToBytes(origSha, orig, sizeof(orig));

	bool result = false;

	// Compare digest of vzip that was downloaded to what the manifest says it should be
	if (memcmp(&orig, &digest, sizeof(digest)) == 0)
		result = true;

	munmap(src, statbuf.st_size);

	return result;
}

bool SteamLibUpdater::VerifyInstalledFiles(LinkedList<AString> &files) {
	if (installedFiles_.length() == 0) {
		printf("Found corrupted install list. Re-extracting all Steam files\n");
		return false;
	}

	for (InstallEntry_t entry : installedFiles_) {
		struct stat statbuf;
		const char *filename = entry.filename.chars();
		if (lstat(filename, &statbuf) != 0) {
			files.append(AString(filename));
			continue;
		}

		if (entry.size == -1)
			continue;

		if (entry.size != -2 &&
			(statbuf.st_size != entry.size || statbuf.st_mtimespec.tv_sec != entry.timestamp)) {
			files.append(AString(filename));
			continue;
		}

		unsigned char *data;
		if (entry.size == -2) {
			data = new unsigned char[statbuf.st_size];
			readlink(filename, (char*)data, statbuf.st_size);
		} else {
			int fd = open(filename, O_RDONLY);
			if (fd == -1) {
				files.append(AString(filename));
				continue;
			}

			data = (unsigned char *)mmap(nullptr, statbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
			close(fd);

			if (data == MAP_FAILED) {
				files.append(AString(filename));
				continue;
			}
		}

		if (mz_crc32(MZ_CRC32_INIT, data, statbuf.st_size) != entry.crc)
			files.append(AString(filename));

		if (entry.size == -2)
			delete [] data;
		else
			munmap(data, statbuf.st_size);
	}

	return files.length() == 0;
}

void SteamLibUpdater::DownloadVZip(const char *filename, const char *shortName) {
	AString url(BASE_URL);
	url.append(filename);

	URL_FILE *file = url_fopen(url.chars(), "r");
	FILE *outf = fopen(filename, "wb");
	size_t nread;
	char buffer[4096];

	const char *sizePos = strrchr(filename, '_') + 1;
	size_t size;
	sscanf(sizePos, "%zu", &size);
	size_t total_read = 0;

	do {
		nread = url_fread(buffer, 1, sizeof(buffer), file);
		total_read += nread;
		printf("Downloading %s... %.2f%%\r", shortName, double(total_read) / size * 100.0f);
		fflush(stdout);
		fwrite(buffer, 1, nread, outf);
	} while (nread);

	printf("\n");

	fclose(outf);
	url_fclose(file);
}

bool SteamLibUpdater::DecompressVZip(const char *path, const char *shortName,
                                     LinkedList<InstallEntry_t> &list,
                                     LinkedList<AString> &filter) {
	int vfd = open(path, O_RDONLY);
	if (vfd == -1) {
		printf("Failed to open vzip file!\n");
		return false;
	}

	struct stat statbuf;
	if (fstat(vfd, &statbuf) != 0) {
		printf("Failed to stat vzip file!\n");
		return false;
	}

	unsigned char *src = (unsigned char *)mmap(NULL, statbuf.st_size, PROT_READ, MAP_PRIVATE, vfd, 0);
	close(vfd);
	if (src == MAP_FAILED)
	{
		printf("Failed to mmap file\n");
		return false;
	}

	size_t dlen = *(unsigned int *)&src[statbuf.st_size - sizeof(VZFooter) + sizeof(VZFooter::crc)];

	unsigned char *dest = (unsigned char *)malloc(dlen);
	size_t slen = statbuf.st_size - sizeof(VZHeader) - sizeof(VZFooter) - LZMA_PROPS_SIZE;

	bool freeName = false;
	if (!shortName) {
		const char *firstPeriod = strchr(path, '.');
		size_t size = firstPeriod - path + 1;
		char *p = new char[size];
		strncopy(p, path, size);
		shortName = p;
		freeName = true;
	}
	printf("Decompressing %s... ", shortName);
	fflush(stdout);

	LzmaUncompress(dest, &dlen, &src[sizeof(VZHeader) + LZMA_PROPS_SIZE], &slen,
	               &src[sizeof(VZHeader)], LZMA_PROPS_SIZE);

	printf("100.00%%\n");

	mz_zip_archive zip_archive;
	memset(&zip_archive, 0, sizeof(zip_archive));
	mz_zip_reader_init_mem(&zip_archive, dest, dlen, 0);
	chdir("..");

	mz_uint numFiles = mz_zip_reader_get_num_files(&zip_archive);
	for (mz_uint i = 0; i < numFiles; i++)
	{
		printf("Extracting %s... %.2f%%\r", shortName, double(i) / numFiles * 100.0f);
		fflush(stdout);

		mz_zip_archive_file_stat file_stat;
		if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat))
		{
			printf("Failed to stat file in zip archive!\n");
			break;
		}

		// Fix slashes, ugh
		size_t len = strlen(file_stat.m_filename);
		for (size_t i = 0; i < len; i++) {
			if (file_stat.m_filename[i] == '\\')
				file_stat.m_filename[i] = '/';
		}

		for (AString name : filter) {
			if (name.compare(file_stat.m_filename) == 0)
				continue;
		}

		InstallEntry_t entry;
		entry.filename = file_stat.m_filename;
		entry.timestamp = file_stat.m_time;
		entry.crc = file_stat.m_crc32;

		if (mz_zip_reader_is_file_a_directory(&zip_archive, i))
		{
			entry.size = -1;
			mkpath(file_stat.m_filename, 0755);
		}
		else
		{
			mode_t attr = (file_stat.m_external_attr >> 16) & 0xFFFF;
			if (S_ISLNK(attr))
			{
				char lnkPath[PATH_MAX];
				if (file_stat.m_uncomp_size < sizeof(lnkPath))
				{
					mz_zip_reader_extract_to_mem(&zip_archive, i, lnkPath, sizeof(lnkPath), 0);

					// Ensure that link path name is null terminated
					lnkPath[file_stat.m_uncomp_size] = '\0';

					// Remove old symlink if it exists
					unlink(file_stat.m_filename);

					// Create new symbolic link
					symlink(lnkPath, file_stat.m_filename);
				}
				entry.size = -2;
			}
			else
			{
				mz_zip_reader_extract_to_file(&zip_archive, i, file_stat.m_filename, 0);
				chmod(file_stat.m_filename, 0755);
				entry.size = file_stat.m_uncomp_size;
			}
		}

		AddToSortedInstallList(entry, list);
	}

	printf("Extracting %s... %.2f%%\n", shortName, 100.0f);

	mz_zip_reader_end(&zip_archive);

	munmap(src, statbuf.st_size);

	if (freeName)
		delete [] shortName;

	return true;
}

void SteamLibUpdater::ExtractFiles(LinkedList<AString> &files) {
	LinkedList<InstallEntry_t> list;
	for (ManifestEntry_t vzip : vzips_) {
		chdir("package");
		DecompressVZip(vzip.filename.chars(), nullptr, list, files);
	}

	chdir("package");
	FILE *installManifest = fopen(GetManifestName(universe_, ManifestType::InstallList), "w+");

	for (InstallEntry_t e : list) {
		fprintf(installManifest, "%s,%lld;%ld;%u\n", e.filename.chars(), e.size, e.timestamp, e.crc);
	}

	rewind(installManifest);
	size_t read;
	char line[1024];

	CSha256 sha256;
	Byte digest[32];
	Sha256_Init(&sha256);

	do
	{
		read = fread(line, 1, sizeof(line), installManifest);
		Sha256_Update(&sha256, (Byte *)line, read);
	} while (read > 0);

	Sha256_Final(&sha256, digest);

	fprintf(installManifest, "SHA2=");

	for (Byte b : digest) {
		fprintf(installManifest, "%02X", b);
	}

	fprintf(installManifest, "\n");

	fclose(installManifest);
}

void SteamLibUpdater::WriteInstallList(LinkedList<InstallEntry_t> &list) {
	chdir("package");
	FILE *installManifest = fopen(GetManifestName(universe_, ManifestType::InstallList), "w+");

	for (InstallEntry_t e : list) {
		fprintf(installManifest, "%s,%lld;%ld;%u\n", e.filename.chars(), e.size, e.timestamp, e.crc);
	}

	rewind(installManifest);
	size_t read;
	char line[1024];

	CSha256 sha256;
	Byte digest[32];
	Sha256_Init(&sha256);

	do
	{
		read = fread(line, 1, sizeof(line), installManifest);
		Sha256_Update(&sha256, (Byte *)line, read);
	} while (read > 0);

	Sha256_Final(&sha256, digest);

	fprintf(installManifest, "SHA2=");

	for (Byte b : digest) {
		fprintf(installManifest, "%02X", b);
	}

	fprintf(installManifest, "\n");

	fclose(installManifest);
}

constexpr const char *SteamLibUpdater::GetManifestName(SteamUniverse universe, ManifestType type) {
	switch (universe) {
		case SteamUniverse::Public:
			switch (type) {
				case ManifestType::Default:
					return STEAM_MANIFEST_RELEASE EXT_MANIFEST;
				case ManifestType::InstallList:
					return STEAM_MANIFEST_RELEASE EXT_INSTALL;
				case ManifestType::Url:
					return BASE_URL STEAM_MANIFEST_RELEASE;
			}
		case SteamUniverse::PublicBeta:
			switch (type) {
				case ManifestType::Default:
					return STEAM_MANIFEST_BETA EXT_MANIFEST;
				case ManifestType::InstallList:
					return STEAM_MANIFEST_BETA EXT_INSTALL;
				case ManifestType::Url:
					return BASE_URL STEAM_MANIFEST_BETA;
			}
	}
}

void SteamLibUpdater::HexStringToBytes(AString str, unsigned char bytes[], size_t nBytes) {
	size_t len = str.length();
	for (size_t i = 0, j = 0; i < len && j < nBytes; i++) {
		unsigned char c = str[i];

		if (c >= '0' && c <= '9')
			c -= '0';
		else if (c >= 'A' && c <= 'F')
			c += 10 - 'A';
		else if (c >= 'a' && c <= 'f')
			c += 10 - 'a';

		if ((i & 0x1) == 0)
			bytes[j] = c << 4;
		else
			bytes[j++] += c;
	}
}

void SteamLibUpdater::AddToSortedInstallList(InstallEntry_t &entry,
											 LinkedList<InstallEntry_t> &list) {
	LinkedList<InstallEntry_t>::iterator it = list.begin();
	const char *addedName = entry.filename.chars();
	bool inserted = false;

	while (it != list.end()) {
		InstallEntry_t &e = (*it);
		if (strcasecmp(addedName, e.filename.chars()) < 0) {
			list.insertBefore(it, entry);
			inserted = true;
			break;
		}
		it++;
	}

	if (!inserted)
		list.append(entry);
}

int SteamLibUpdater::mkpath(const char *path, mode_t mode)
{
	char *tmpPath = strdup(path);
	char *ptr = tmpPath;
	size_t len = strlen(tmpPath);

	if (len == 0)
		return -1;

	// Strip trailing separator if it exists
	if (tmpPath[len - 1] == '/')
		tmpPath[len - 1] = '\0';

	while (*ptr) {
		if (*ptr == '/') {
			*ptr = '\0';

			// Create directory up to this point if it doesn't exist
			if (access(tmpPath, F_OK) == -1 && mkdir(tmpPath, mode) == -1) {
				free(tmpPath);
				return -1;
			}

			*ptr = '/';
		}

		ptr++;
	}

	free(tmpPath);

	// Create final directory if it doesn't exist
	if (access(path, F_OK) == -1)
		return mkdir(path, mode);

	return 0;
}
