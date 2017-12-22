# SrcDS NX Changelog

### 1.0.0 (22 December 2017)
- Initial release
- Notable changes and improvements over [srcds_osx](https://github.com/TheDS/srcds_osx):
  * Automatic updating of srcds binaries on server startup. This can be disabled by adding
    the `-noupdate` option to the command line. Please note that you still have to keep
    the game up to date using SteamCMD or the Steam client GUI.
  * Automatic downloading and updating of Steam libraries. This means the Steam client GUI
    is no longer required to set up a server; SteamCMD will suffice. This can be disabled
    by adding the `-nosteamupdate` option to the command line.
  * Added `-steambeta` command line option to switch to the beta branch for Steam
    libraries.
  * Added support for the 64-bit version of Day of Infamy. This version is run by default
    unless you add the `-32` option to the command line.
  * Fixed support for Garry's Mod. The game update on December 18th broke things.
  * Fixed issue where the status command on CS:GO, Insurgency, and Day of Infamy did not
    display the current map.
