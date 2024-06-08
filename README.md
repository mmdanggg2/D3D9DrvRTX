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
- Nerf Arena Blast (v300)

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

## Settings
Settings for the renderer are stored in `D3D9DrvRTX.ini`, they can be changed in this file and take effect after restarting the game.
You can also change the settings in game by running the `preferences` command in the console and navigating to `Rendering->Direct3D 9 RTX Optimised`

The RTX specific options are listed here:

- `EnableSkyBoxAnchors`: Enables the special mesh at the camera's position, generated for anchoring the skybox in remix.
- `EnableHashTextures`: Enables specially generated textures with a stable hash in place of procedurally generated ones.


	### Hash textures (Help, some textures are pink mush!)
	UE1 makes use of textures that are generated procedurally at runtime, which means that the hash for them that Remix sees is not always the same, this makes replacing them difficult. To get around this issue, when `EnableHashTextures` is on, we generate a unique static texture that is used in place of the procedural one.
	
	Textures can be individually excluded from this option by adding their name into the `D3D9DrvRTX_hash_tex_blacklist.txt` file.
	To help in detemining which textures are affected, when a texture is replaced this way, a message is logged in the game's log (eg. `UnrealTournament.log`) that will give you the full name of the texture.
