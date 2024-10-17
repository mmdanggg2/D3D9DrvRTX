@ECHO OFF
call ChangeLinks.bat unreal226
python SetDebug.py Unreal.exe
echo #define GAME_NAME "Unreal v226"> ..\gamename.h
