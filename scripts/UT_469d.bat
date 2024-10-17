@ECHO OFF
call ChangeLinks.bat ut469d
python SetDebug.py UnrealTournament.exe
echo #define GAME_NAME "Unreal Tournament v469d"> ..\gamename.h
