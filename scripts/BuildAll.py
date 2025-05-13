import argparse
import re
import os
from pathlib import Path
import zipfile
import subprocess

from termcolor import colored

SCRIPTS_DIR = Path(__file__).parent
VS_PROJECT_PATH = SCRIPTS_DIR.parent

os.chdir(SCRIPTS_DIR)

def get_version():
	# Extract the version number from the .rc file
	rc_file_path = VS_PROJECT_PATH / "D3D9DrvRTX.rc"
	with open(rc_file_path, "r", encoding="utf-16-le") as rc_file:
		rc_file_content = rc_file.read()
		# Skip the BOM if present
		if rc_file_content.startswith('\ufeff'):
			rc_file_content = rc_file_content[1:]
		version_major_match = re.search(r"#define\s+VERSION_MAJOR\s+(\d+)", rc_file_content)
		version_minor_match = re.search(r"#define\s+VERSION_MINOR\s+(\d+)", rc_file_content)
		version_patch_match = re.search(r"#define\s+VERSION_PATCH\s+(\d+)", rc_file_content)
		if version_major_match and version_minor_match and version_patch_match:
			version_parts = [
				version_major_match.group(1),
				version_minor_match.group(1),
				version_patch_match.group(1),
			]
			version_number = ".".join(version_parts)
			print(f"Version number extracted: {colored(version_number, 'green')}")
			return version_number
		raise RuntimeError("No version found!")

def switch_game(game_code: str):
	print(f"Switching to '{colored(game_code, 'cyan')}'", flush=True)
	subprocess.run([f"{game_code}.bat"], shell=True, check=True)

def build(version: str | None = None, x64: bool = False):
	solution_path = VS_PROJECT_PATH / "D3D9Drv.sln"
	configuration = "Release"
	platform = "x64" if x64 else "Win32"
	
	# Build the Visual Studio solution
	build_command = [
		"msbuild",
		str(solution_path),
		"-verbosity:minimal",
		"-target:Rebuild",
		"-m",
		f"-p:Configuration={configuration}",
		f"-p:Platform={platform}"
	]
	if version:
		# Eww stinky, just shove this in here since the rc has it anyway.
		with (VS_PROJECT_PATH / "gamename.h").open("a") as f:
			f.write(f'\n#define VERSION_EXTRA "{version}"\n')
	subprocess.run(build_command, shell=True, check=True)

def zip_build(build_name: str, version_path: Path, x64: bool = False):
	# Set the files to include in the ZIP file
	files_to_zip: 'list[Path | tuple[Path, str]]' = [
		VS_PROJECT_PATH / "install" / ("System64" if x64 else "System") / "D3D9DrvRTX.dll",
		VS_PROJECT_PATH / "Config" / "D3D9DrvRTX.ini",
		VS_PROJECT_PATH / "Config" / "D3D9DrvRTX.int",
		VS_PROJECT_PATH / "Config" / "D3D9DrvRTX_config_example.json",
		VS_PROJECT_PATH / "Config" / "D3D9DrvRTX_config_schema.json",
		VS_PROJECT_PATH / "Config" / "dxvk.conf",
		VS_PROJECT_PATH / "Config" / "rtx.conf",
		VS_PROJECT_PATH / "README.md",
	]
	if not x64:
		files_to_zip.append((VS_PROJECT_PATH / "Config" / "bridge.conf", ".trex/bridge.conf"))
	
	# Create the ZIP file
	zip_file_path = version_path / f"{build_name}.zip"
	with zipfile.ZipFile(zip_file_path, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as zip_file:
		for file in files_to_zip:
			if isinstance(file, tuple):
				file_path, arc_path = file
			else:
				file_path, arc_path = (file, file.name)
			
			zip_file.write(file_path, arcname=arc_path)
	
	print(f"Zipped at: {zip_file_path}")

parser = argparse.ArgumentParser()
parser.add_argument("--version-extra", required=False)
parser.add_argument("-o", "--overwrite", action="store_true", required=False)
args = parser.parse_args()

if args.version_extra:
	version = args.version_extra
else:
	version = get_version()

version_path = VS_PROJECT_PATH / "Releases" / version
version_path.mkdir(parents=True, exist_ok=args.overwrite)

games_codes = [
	"UT_469d",
	"UT_469e",
	"UT_436",
	"Unreal_226",
	"Unreal_227k_12",
	"DeusEx",
	"Nerf",
	"Rune",
	"HP1",
	"HP2",
	"Brother_Bear",
	"Klingon_Honor_Guard",
]

for game_code in games_codes:
	switch_game(game_code)
	build(args.version_extra)
	build_name = f"D3D9DrvRTX-{game_code}-{version}"
	zip_build(build_name, version_path)
	print(flush=True)
	if game_code == "Unreal_227k_12":
		build(args.version_extra, x64=True)
		build_name = f"D3D9DrvRTX-{game_code}-x64-{version}"
		zip_build(build_name, version_path, x64=True)
		print(flush=True)
