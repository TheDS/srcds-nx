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

#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <spawn.h>
#include <execinfo.h>
#include "IServerFixer.h"
#include "ServerAPI.h"
#include "GameDetector.h"
#include "GameLib.h"
#include "GameShared.h"
#include "SteamLibUpdater.h"
#include "am-string.h"
#include "cocoa_helpers.h"
#include "stringutil.h"

#import <Cocoa/Cocoa.h>

#if defined(PLATFORM_X64)
#define INSTR_PTR __rip
#else
#define INSTR_PTR __eip
#endif

void crash_handler(int sig, siginfo_t *info, void *context)
{
	void *stack[128];
	ucontext_t *cx = (ucontext_t *)context;
	void *eip = (void *)cx->uc_mcontext->__ss.INSTR_PTR;

	fprintf(stderr, "Caught signal %d (%s). Invalid memory access of %p from %p.\n", sig, strsignal(sig), info->si_addr, eip);

	int nframes = backtrace(stack, sizeof(stack) / sizeof(void *)) - 1;

	stack[2] = eip;

	char **frames = backtrace_symbols(stack, nframes);

	if (!frames) {
		fprintf(stderr, "Failed to generate backtrace!\n");
		exit(sig);
	}

	fprintf(stderr, "Backtrace:\n");
	for (int i = 0; i < nframes; i++)
		fprintf(stderr, "#%s\n", frames[i]);

	free(frames);

	exit(sig);
}

// Version of execvp that respects the current architecture of the running binary
int respawn(char * const argv[]) {
	extern char **environ;
	size_t ocount;
	posix_spawnattr_t attr;
	cpu_type_t cpu_types[1];

#if defined(PLATFORM_X86)
	cpu_types[0] = CPU_TYPE_X86;
#elif defined(PLATFORM_X64)
	cpu_types[0] = CPU_TYPE_X86_64;
#else
#error "Unsupported platform."
#endif

	posix_spawnattr_init(&attr);
	posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETEXEC);
	posix_spawnattr_setbinpref_np(&attr, 1, cpu_types, &ocount);
	return posix_spawn(nullptr, argv[0], nullptr, &attr, argv, environ);
}

int main(int argc, char **argv) {
	using DedicatedMainFn = int (*)(int, char **);
	using GameFixerFn = IServerFixer *(*)(void);
	DedicatedMainFn DedicatedMain;
	GameFixerFn GetGameFixer;

	bool shouldHandleCrash = false;

	for (int i = 0; i < argc; i++) {
		if (strcmp(argv[i], "-nobreakpad") == 0) {
			shouldHandleCrash = true;
			break;
		}
	}

	if (shouldHandleCrash)
	{
		stack_t sigstack;
		sigstack.ss_sp = malloc(SIGSTKSZ);
		sigstack.ss_size = SIGSTKSZ;
		sigstack.ss_flags = SS_ONSTACK;
		sigaltstack(&sigstack, nullptr);

		struct sigaction sa;
		sa.sa_sigaction = crash_handler;
		sa.sa_flags = SA_ONSTACK | SA_SIGINFO;
		sigemptyset(&sa.sa_mask);

		sigaction(SIGILL, &sa, nullptr);
		sigaction(SIGBUS, &sa, nullptr);
		sigaction(SIGSEGV, &sa, nullptr);
	}

	// Add game binary paths and Steam library path to environment and respawn process
	char *oldPath = getenv("DYLD_LIBRARY_PATH");
	ke::AString steamPath = GetSteamPath();
	if (!oldPath || !strstr(oldPath, steamPath.chars())) {
		ke::AString execPath = GameShared::GetExecutablePath();
		ke::AString libPath =  execPath + "/bin:" + execPath + ":";
#if defined(PLATFORM_X64)
		libPath += execPath + "/bin/osx64:";
#endif
		libPath += steamPath;

		if (oldPath) {
			libPath.append(":");
			libPath.append(oldPath);
		}

		setenv("DYLD_LIBRARY_PATH", libPath.chars(), 1);

		if (int res = respawn(argv); res != 0) {
			printf("Failed to respawn process: %d\n", res);
			return res;
		}
	}

	{
		SteamLibUpdater updater;
		updater.Update(SteamUniverse::Steam_Public);
	}

	GameDetector detector = GameDetector::GetInstance(argc, argv);
	const AString& game = detector.GetGameName();
	if (game.chars() == nullptr || game.compare("") == 0)
	{
		printf("Fatal: Could not detect game name. Did you forget to add the -game option?\n");
		return -1;
	}

	if (detector.GetEngineBranch() == Engine_Unknown) {
		printf("Fatal: Failed to detect game engine\n");
		return -2;
	}

	ke::AString supportLib("libsrcds-");
	supportLib.append(detector.GetEngineString());
	supportLib.append(".dylib");

	void *fixLib = dlopen(supportLib.chars(), RTLD_LAZY);
	GetGameFixer = (GameFixerFn)dlsym(fixLib, "GetGameFixer");
	IServerFixer *fixer = GetGameFixer();

	GameLib dedicated("dedicated");
	if (!dedicated.IsLoaded()) {
		printf("Fatal: Failed to load dedicated library\n");
		dlclose(fixLib);
		return -3;
	}

	DedicatedMain = dedicated.ResolveSymbol<DedicatedMainFn>("DedicatedMain");
	if (!DedicatedMain) {
		printf("Fatal: Failed to find symbol in dedicated library, DedicatedMain\n");
		dlclose(fixLib);
		return -4;
	}

	// Prevent problem where files can't be found when executable path contains spaces by putting
	// quotation marks around it. This can be a problem when running the server under certain
	// versions of GDB. It can also occur under LLDB if only the app bundle path is given as the
	// executable to debug.
	char *execPath = nullptr;
	if (argv[0][0] != '"' && strstr(argv[0], " "))
	{
		// Allocate space for path + two quotation marks
		size_t len = strlen(argv[0]) + 3;
		execPath = new char[len];

		// Copy original path
		strcpy(&execPath[1], argv[0]);

		// Add quotation marks and null terminate
		execPath[0] = execPath[len - 2] = '"';
		execPath[len - 1] = '\0';

		argv[0] = execPath;
	}

	ServerAPI api(argc, argv);
	GameShared &gameShared = GameShared::GetInstance();

	if (!gameShared.Init(&api, fixer))
		return -5;

	if (!fixer->Init(&api))
		return -6;

	@autoreleasepool
	{
		NSString *bundlePath = [[NSBundle mainBundle] bundlePath];
		NSString *parentPath = [bundlePath stringByDeletingLastPathComponent];

		// Ensure that the current working directory is the path in which the app bundle is located
		chdir([parentPath UTF8String]);
	}

	// Since the program is running in console mode, ensure that it is considered a background
	// application. This prevents the Steam libraries from inadvertantly causing an icon to appear
	// on the macOS dock.
	ProcessSerialNumber psn = { 0, kCurrentProcess };
	TransformProcessType(&psn, kProcessTransformToBackgroundApplication);

	int result = DedicatedMain(argc, argv);

	fixer->Shutdown();
	gameShared.Shutdown();

	dlclose(fixLib);

	return result;
}
