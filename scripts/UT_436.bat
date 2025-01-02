@ECHO OFF
call ChangeLinks.bat ut436
python SetProperties.py --debug-exe UnrealTournament.exe
echo #define GAME_NAME "Unreal Tournament v436"> ..\gamename.h
