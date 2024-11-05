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
		version_match = re.search(r"FILEVERSION\s+(\d+),(\d+),(\d+)", rc_file_content)
		if version_match:
			version_parts = version_match.groups()
			version_number = ".".join(version_parts)
			print(f"Version number extracted: {colored(version_number, 'green')}")
			return version_number
		raise RuntimeError("No version found!")

def switch_game(game_code):
	print(f"Switching to '{colored(game_code, 'cyan')}'")
	subprocess.run([f"{game_code}.bat"], shell=True, check=True)

def build():
	solution_path = VS_PROJECT_PATH / "D3D9Drv.sln"
	configuration = "Release"

	# Build the Visual Studio solution
	build_command = [
		"msbuild",
		str(solution_path),
		"-verbosity:minimal",
		"-target:Rebuild",
		f"-p:Configuration={configuration}",
	]
	subprocess.run(build_command, shell=True, check=True)

def zip_build(build_name, version_path):
	# Set the files to include in the ZIP file
	files_to_zip = [
		VS_PROJECT_PATH / "install" / "System" / "D3D9DrvRTX.dll",
		VS_PROJECT_PATH / "Config" / "D3D9DrvRTX.ini",
		VS_PROJECT_PATH / "Config" / "D3D9DrvRTX.int",
		VS_PROJECT_PATH / "Config" / "D3D9DrvRTX_hash_tex_blacklist.txt",
		VS_PROJECT_PATH / "Config" / "dxvk.conf",
		VS_PROJECT_PATH / "Config" / "rtx.conf",
		VS_PROJECT_PATH / "README.md",
	]
	
	# Create the ZIP file
	zip_file_path = version_path / f"{build_name}.zip"
	with zipfile.ZipFile(zip_file_path, "w") as zip_file:
		for file_path in files_to_zip:
			zip_file.write(file_path, arcname=file_path.name, compress_type=zipfile.ZIP_DEFLATED)

	print(f"Zipped at: {zip_file_path}")

version = get_version()

version_path = VS_PROJECT_PATH / "Releases" / version
version_path.mkdir(parents=True, exist_ok=False)

games_codes = [
	"UT_469d",
	"UT_436",
	"Unreal_226",
	"DeusEx",
	"Nerf",
	"Rune",
	"HP1",
	"HP2",
]

for game_code in games_codes:
	switch_game(game_code)
	build()
	build_name = f"D3D9DrvRTX-{game_code}-{version}"
	zip_build(build_name, version_path)
	print()

