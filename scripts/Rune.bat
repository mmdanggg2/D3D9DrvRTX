@ECHO OFF
call ChangeLinks.bat rune107
python SetDebug.py Rune.exe
echo #define GAME_NAME "Rune v107"> ..\gamename.h
