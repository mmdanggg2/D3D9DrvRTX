@ECHO OFF
call ChangeLinks.bat unreal227k_12
python SetProperties.py --debug-exe Unreal.exe --wchar
echo #define GAME_NAME "Unreal v227k_12"> ..\gamename.h
