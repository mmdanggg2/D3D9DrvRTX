# Unreal Engine 1 RTX Remix Render Device

## Overview
This is a custom render device for the Unreal Engine, optimized for use with NVIDIA's RTX Remix. Built off of [Chris Dohnal's](https://www.cwdohnal.com/utglr/) D3D9Drv renderer and heavily modified to better support Remix.

## Requirements
- One of the supported games
- RTX Remix Runtime v0.4+ (Installed in `System` folder)

## Currently Supported Games
- Unreal Tournament ([OldUnreal v469d](https://github.com/OldUnreal/UnrealTournamentPatches/releases) and v436)
- Unreal (v226)
- Deus Ex (v1112fm)

## Installation
1. Download the appropriate zip file [from releases](https://github.com/mmdanggg2/D3D9DrvRTX/releases).
2. Navigate to the game's installation directory.
3. Extract the files into the `System` folder.
4. Update the game's .ini configuration file (eg. `UnrealTournament.ini`) to use the new render device by changing these lines under the `[Engine.Engine]` section:
```
GameRenderDevice=D3D9DrvRTX.D3D9RenderDevice
Render=D3D9DrvRTX.D3D9Render
```

## Usage
After installation, start the game, and it should now be using the RTX Remix optimized render device.
