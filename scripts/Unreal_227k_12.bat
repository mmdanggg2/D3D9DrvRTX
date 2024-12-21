@ECHO OFF
call ChangeLinks.bat unreal227k_12
python SetDebug.py Unreal.exe
echo #define GAME_NAME "Unreal v227k_12"> ..\gamename.h
