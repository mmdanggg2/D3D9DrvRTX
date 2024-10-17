@ECHO OFF
call ChangeLinks.bat dx1112f
python SetDebug.py DeusEx.exe
echo #define GAME_NAME "DeusEx v1112fm"> ..\gamename.h
