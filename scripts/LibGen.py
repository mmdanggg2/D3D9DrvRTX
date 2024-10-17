from pathlib import Path
import subprocess
import re

SCRIPTS_DIR = Path(__file__).parent
VS_PROJECT_PATH = SCRIPTS_DIR.parent

system_path = VS_PROJECT_PATH / "install" / "System"
lib_path = SCRIPTS_DIR / "generated_libs"

dlls: list[Path] = []
for path in system_path.iterdir():
	if path.name.endswith(".dll"):
		dlls.append(path)

def convert_dumpbin_to_libdef(input_text: str) -> str:
	# Find the start of the export list
	start_index = input_text.find("ordinal hint RVA      name")
	if start_index == -1:
		return "Error: Could not find export list in the input."

	# Extract the export list
	export_list = input_text[start_index:].split('\n')[1:]

	# Process each line
	exports = []
	for line in export_list:
		# Use regex to extract the function name
		match = re.search(r'\s+\d+\s+\w+\s+\w+\s+(\S+)', line)
		if match:
			exports.append(match.group(1))
	
	# Generate the output
	output = "EXPORTS\n" + "\n".join(exports)
	return output

def dll_to_lib(dll_path: Path, lib_path: Path):
	lib_path.parent.mkdir(parents=True, exist_ok=True)
	dump_proc = subprocess.run(["dumpbin", "/EXPORTS", str(dll_path)], capture_output=True, text=True)
	def_path = lib_path.with_suffix(".def")
	def_path.write_text(convert_dumpbin_to_libdef(dump_proc.stdout))
	lib_proc = subprocess.run(["lib", f"/DEF:{def_path}", "/MACHINE:X86", f"/OUT:{lib_path}"])
	if lib_proc.returncode:
		raise RuntimeError(lib_proc.stdout)
	def_path.unlink()
	lib_path.with_suffix(".exp").unlink()

for dll in dlls:
	dll_to_lib(dll, lib_path / dll.stem / "Lib" / dll.with_suffix(".lib").name)
