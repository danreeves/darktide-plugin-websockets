Write-Output "const char *script = R`"====(" | Out-File ".\src\script.lua.h" -Encoding UTF8;
Get-Content .\src\script.lua | Out-File ".\src\script.lua.h" -Append -Encoding UTF8;
Write-Output ")====`";" | Out-File ".\src\script.lua.h" -Encoding UTF8 -Append;
