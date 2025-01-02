@ECHO OFF
call ChangeLinks.bat hp2
python SetProperties.py --debug-exe Game.exe
echo #define GAME_NAME "Harry Potter Chamber of Secrets v433"> ..\gamename.h
