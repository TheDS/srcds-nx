SrcDS NX
=========

This is currently a command line only program for running dedicated servers for various Source games on macOS. It wraps around the existing `dedicated` library that is included with most Source games. It also employs a number of hacks that install function detours and manipulate memory in order to get the game server to run properly.

It is a rewrite of and replacement for the [srcds_osx](https://github.com/TheDS/srcds_osx) project which aims to be more maintainable and hopefully expandable to other platforms in the future. Improvements include automatic updates and a Steam library downloader. Automatic updates occur on server startup to deliver faster fixes when things break due to game updates. 
The Steam library downloader makes it easier to set up servers using only a terminal; you no longer need the Steam client GUI to do so. Using this along with [steamcmd](https://developer.valvesoftware.com/wiki/SteamCMD) to install/update a game is all that is necessary.

Requirements
---
* macOS 10.9+
* One or more of the Source games listed below

Supported Games
---
* Counter-Strike: Global Offensive
* Counter-Strike: Source
* Day of Defeat: Source
* Day of Infamy
* Garry's Mod
* Half-Life Deathmatch: Source
* Half-Life 2: Deathmatch
* Insurgency
* Left 4 Dead
* Left 4 Dead 2
* Nuclear Dawn
* Source SDK Base 2013 Multiplayer Mods
* Team Fortress 2

Installation and Usage
---
For binary downloads and usage instructions, see: https://forums.alliedmods.net/showthread.php?t=158240
