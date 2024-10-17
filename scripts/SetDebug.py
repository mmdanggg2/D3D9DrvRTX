from pathlib import Path
import re
import sys

if len(sys.argv) < 2:
	raise ValueError("Not enough arguments!")
exe_name = sys.argv[-1]

SCRIPTS_DIR = Path(__file__).parent
VS_PROJECT_PATH = SCRIPTS_DIR.parent

prop_path = VS_PROJECT_PATH / "PropertySheet.props"

props = prop_path.read_text()
props = re.sub(r"<UNREAL_EXE_NAME>([.\w]+)</UNREAL_EXE_NAME>", f"<UNREAL_EXE_NAME>{exe_name}</UNREAL_EXE_NAME>", props)
prop_path.write_text(props)

sln_path = VS_PROJECT_PATH / "D3D9Drv.sln"
sln_path.touch(exist_ok=True)
