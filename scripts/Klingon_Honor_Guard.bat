@ECHO OFF
call ChangeLinks.bat klingon_honor_guard
python SetProperties.py --debug-exe khg.exe --no-unicode
echo #define GAME_NAME "Klingon Honor Guard v1.1"> ..\gamename.h
