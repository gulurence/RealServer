@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d D:\code\RealServer\shared\battlecore\test
cl /nologo /EHsc /W3 /std:c++14 /utf-8 /Fe:test_bossai.exe test_bossai.cpp ..\BattleCore.cpp ..\BattleGame.cpp
cl /nologo /EHsc /W3 /std:c++14 /utf-8 /Fe:test_sp01move.exe test_sp01move.cpp ..\BattleCore.cpp ..\BattleGame.cpp
cl /nologo /EHsc /W3 /std:c++14 /utf-8 /Fe:test_heromove.exe test_heromove.cpp ..\BattleCore.cpp ..\BattleGame.cpp
cl /nologo /EHsc /W3 /std:c++14 /utf-8 /Fe:test_fixedpoint.exe test_fixedpoint.cpp
cl /nologo /EHsc /W3 /std:c++14 /utf-8 /Fe:test_battleentity.exe test_battleentity.cpp ..\BattleCore.cpp ..\BattleGame.cpp
