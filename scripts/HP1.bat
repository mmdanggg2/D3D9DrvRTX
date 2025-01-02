@ECHO OFF
call ChangeLinks.bat hp1
python SetProperties.py --debug-exe HP.exe
echo #define GAME_NAME "Harry Potter Sourcerer's Stone v433"> ..\gamename.h
