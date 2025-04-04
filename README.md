Unreal Engine 1 RTX Remix Render Device
=======================================

# Overview
This is a custom render device for the Unreal Engine, optimized for use with NVIDIA's RTX Remix. Built off of [Chris Dohnal's](https://www.cwdohnal.com/utglr/) D3D9Drv renderer and heavily modified to better support Remix.

## Currently Supported Games
- Unreal Tournament ([OldUnreal v469e, v469d](https://github.com/OldUnreal/UnrealTournamentPatches/releases) and v436)
- Unreal ([OldUnreal v227k_12](https://github.com/OldUnreal/Unreal-testing/releases) and v226)
- Deus Ex (v1112fm)
- Nerf Arena Blast (v300)
- Rune (v107)
- Harry Potter Philosopher's Stone
- Harry Potter Chamber of Secrets

## Requirements
- One of the supported games
- [RTX Remix](https://github.com/NVIDIAGameWorks/rtx-remix) Runtime v0.6+ (Installed in `System` folder)

# Installation
- [Installation Tutorial video](https://youtu.be/XEe-pyZ3J9g)
1. Download the appropriate zip file [from releases](https://github.com/mmdanggg2/D3D9DrvRTX/releases).
2. Navigate to the game's installation directory.
3. Extract the files into the `System` folder. (the RTX Remix `d3d9.dll` and `.trex` should be here too)
4. Configure the game to use the `Direct3D 9 RTX Optimised` render device in the menus.
	
	Or update the game's .ini configuration file (eg. `UnrealTournament.ini`)  by changing these lines under the `[Engine.Engine]` section:
	
	```ini
	[Engine.Engine]
	GameRenderDevice=D3D9DrvRTX.D3D9RenderDevice
	...
	Render=D3D9DrvRTX.D3D9Render
	```

## Harry Potter 1/2 installation
For the Harry Potter games, the .ini files are located in the documents folder and they must be manually edited as described above because there is no render device selection menu.
- `%USERPROFILE%\Documents\Harry Potter\HP.ini` for Philosophers Stone
- `%USERPROFILE%\Documents\Harry Potter II\Game.ini` for Chamber of Secrets

## Unreal 227k_12 64 bit installation
Unreal v227k_12 comes with a 64 bit version of the game that is within `System64`. Installation of D3D9DrvRTX is much the same except you need to extract the files from the `Unreal_227k_12-x64` zip into the `System64` directory.

However, installation of RTX Remix is different for 64-bit games. Instead of extracting the zip directly into the folder you must extract **only the contents of `.trex`** directly into `System64`.

# Usage
After installation, start the game, and it should now be using the RTX Remix optimized render device.

# Settings
Settings for the renderer are stored in `D3D9DrvRTX.ini`, they can be changed in this file and take effect after restarting the game.
You can also change the settings in game by running the `preferences` command in the console and navigating to `Rendering->Direct3D 9 RTX Optimised`

The RTX specific options are listed here:

- `LightMultiplier`: Global light brightness multiplier.
- `LightRadiusDivisor`: The LightRadius is divided by this value before being exponentiated.
- `LightRadiusExponent`: LightRadius is raised to the power of this value before being multiplied with the brightness.
- `NonSolidTranslucentHack`: Makes non-solid level geometry render as translucent to allow lights to shine through it.

- `EnableSkyBoxRendering`: Enables rendering of the skybox zone (if one is found) before the main view.
- `EnableSkyBoxAnchors`: Enables the special mesh at the camera's position, generated for anchoring the skybox in remix.
- `EnableHashTextures`: Enables specially generated textures with a stable hash in place of procedurally generated ones.

### Hash textures
UE1 makes use of textures that are generated procedurally at runtime, which means that the hash for them that Remix sees is not always the same, this makes replacing them difficult. To get around this issue, when `EnableHashTextures` is on, we generate a unique static texture that is used in place of the procedural one.

Textures can be individually excluded from this option by adding their name into the `hash_tex_blacklist` array in the `D3D9DrvRTX_config.json` file.
To help in detemining which textures are affected, when a texture is replaced this way, a message is logged in the game's log (eg. `UnrealTournament.log`) that will give you the full name of the texture.

### Light formula
The brightness of lights are calculated as follows:

`light.brightness * pow((light.radius / LightRadiusDivisor), LightRadiusExponent) * LightMultiplier * brightnessSetting`

This is to approximate the light radius cutoff effect of the regular renderer, which is not possible to achive with physically based lights. `brightnessSetting` is the value of the in game brightness slider * 2, where the slider's middle value (0.5) multiples light brighnesses by 1.0


# Advanced Config
There are some additional features that can be configured with the `D3D9DrvRTX_config.json` file. There is an included `D3D9DrvRTX_config_example.json` file with some examples of how the config should look along with comments. There is also a `D3D9DrvRTX_config_schema.json` file which should help you write and validate your config.
 
## Level Properties
Levels can be given a certain set of properties/settings that are applied when the level is loaded in. Each level can have its properties defined in the `level_properties` object. Each key in this object represents a level name (case-insensitive), and the corresponding value is an object containing that level’s properties.

If the current level isn't found, the properties will fall back to the top-level `level_properties_default` object, if it exists. This is useful, for example, to set config variables back to default in levels that have not been tuned.

- ### Anchors
	An "anchor" is a small generated mesh that will be rendered in the scene that can be replaced/attached to in RTX Remix. Anchors are defined in a level’s `anchors` array. Each anchor can have some animation applied to it, simple linear movement and constant speed rotation is currently supported.
	
	An anchor must always have `name`, `anim_type` and `start_loc` defined.
	- `name` (string): Can be anything. The name given is used to generate the mesh for the anchor, so 2 anchors with the same name will have the same hash in remix
	- `start_loc` (array[x, y, z]): This is where the anchor will be placed to start in the level (in unreal units)
	- `anim_type` (string): Specifies the type of animation this anchor will have
		#### Anim types:
		- `linear`: moves from start to end and jumps back to start
		- `ping-pong`: moves from start to end and then reverses to move back to start
		- `static`: does not move, will only rotate
		
		If the `anim_type` is set to `linear` or `ping-pong`, then the following are also required:
		- `end_loc` (array[x, y, z]): The destination location for the anchor to move to
		- `speed` (number): How fast the anchor will move from start to end in unreal units per second
	
	Optionally you can specify `start_rot`, `rotation_rate` or `scale`
	- `start_rot` (array[x, y, z]): The starting rotation of the anchor in degrees
	- `rotation_rate` (array[x, y, z]): How much the anchor rotates in degrees per second
	- `scale` (array[x, y, z]): What scale to apply to the anchor
	

- ### RTX Configuration Variables
	A set of Remix configuration variables can be set per level too. These go into the level's `config_vars` object, where each key is the name of the config variable and the value (string) is what value to set it to.
	
	For these to work, you require the Remix API to be enabled on the Remix bridge. To enable this, set `exposeRemixApi = True` in the `.trex/bridge.conf` file. On x64 builds this is not necessary as there is no bridge and the API is always available.
	
	⚠️ Note that on remix-1.0.0, this will cause it to crash so you need to use a later version where this is fixed.


# Developer setup
You will need to have Visual Studio installed with the Windows SDK, as well as the Direct3D 9 SDK.

For each game, copy an installation of that game into the `installs` directory. UT 469d for example would go in `installs/install_ut469d/`, where the game code matches the names in `sdks`.

To set the current game, simply run the corresponding .bat file in `scripts`. This creates `sdk` and `install` directory symlinks to the appropriate folders in `sdks` and `installs`. The vs project is setup to use the `sdk` folder for headers and libs, and save the built dll into `install/System`

The `BuildAll.py` scripts executes each bat script in turn, then builds and packages the result into the released .zip files.
