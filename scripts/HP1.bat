@ECHO OFF
call ChangeLinks.bat hp1
python SetDebug.py HP.exe
echo #define GAME_NAME "Harry Potter Sourcerer's Stone v433"> ..\gamename.h
