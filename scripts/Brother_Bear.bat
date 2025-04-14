@ECHO OFF
call ChangeLinks.bat brotherbear
python SetProperties.py --debug-exe Game.exe
echo #define GAME_NAME "Brother Bear"> ..\gamename.h
