@ECHO OFF
call ChangeLinks.bat undying
python SetProperties.py --debug-exe Undying.exe --no-unicode
echo #define GAME_NAME "Clive Barkers Undying v1.1"> ..\gamename.h
