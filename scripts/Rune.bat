@ECHO OFF
call ChangeLinks.bat rune107
python SetProperties.py --debug-exe Rune.exe
echo #define GAME_NAME "Rune v107"> ..\gamename.h
