@ECHO OFF
call ChangeLinks.bat dx1112f
python SetProperties.py --debug-exe DeusEx.exe
echo #define GAME_NAME "DeusEx v1112fm"> ..\gamename.h
