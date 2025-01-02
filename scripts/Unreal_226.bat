@ECHO OFF
call ChangeLinks.bat unreal226
python SetProperties.py --debug-exe Unreal.exe
echo #define GAME_NAME "Unreal v226"> ..\gamename.h
