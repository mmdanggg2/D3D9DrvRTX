@ECHO OFF
call ChangeLinks.bat ut469d
python SetProperties.py --debug-exe UnrealTournament.exe
echo #define GAME_NAME "Unreal Tournament v469d"> ..\gamename.h
