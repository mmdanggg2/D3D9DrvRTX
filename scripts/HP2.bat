@ECHO OFF
call ChangeLinks.bat hp2
python SetDebug.py Game.exe
echo #define GAME_NAME "Harry Potter Chamber of Secrets v433"> ..\gamename.h
