cd ../server/ws-server-scene/pb
for /f "usebackq tokens=*" %%d in (`dir /b *.json`) do (
	json2lua "%%d" "../script/config/%%~nd.lua"
)
pause