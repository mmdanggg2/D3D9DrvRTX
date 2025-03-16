@ECHO OFF
call ChangeLinks.bat ut469e
python SetProperties.py --debug-exe UnrealTournament.exe
echo #define GAME_NAME "Unreal Tournament v469e"> ..\gamename.h
