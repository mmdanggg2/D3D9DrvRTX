from pathlib import Path
import re
import argparse
import shutil

parse = argparse.ArgumentParser()
parse.add_argument("--debug-exe")
parse.add_argument("--wchar", action="store_true")

args = parse.parse_args()

exe_name = args.debug_exe

SCRIPTS_DIR = Path(__file__).parent
VS_PROJECT_PATH = SCRIPTS_DIR.parent

prop_path = VS_PROJECT_PATH / "PropertySheet.props"

props = prop_path.read_text()

def replace_tag(tag: str, new_value: str, xml: str):
	return re.sub(fr"(?<=<{tag}>)(.*)(?=</{tag}>)", new_value, xml)

props = replace_tag("UNREAL_EXE_NAME", exe_name, props)
props = replace_tag("TreatWChar_tAsBuiltInType", "true" if args.wchar else "false", props)

prop_path.write_text(props)

sln_path = VS_PROJECT_PATH / "D3D9Drv.sln"
# Poke the sln file, makes vs reload the config
sln_path.touch(exist_ok=True)

# Clear build so it doesn't try to use the old one
shutil.rmtree(VS_PROJECT_PATH / "Build", ignore_errors=True)
