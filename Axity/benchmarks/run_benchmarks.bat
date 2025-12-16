@echo off
setlocal enabledelayedexpansion
set SCRIPT_DIR=%~dp0
set PY_DIR=%SCRIPT_DIR%python
set AX_DIR=%SCRIPT_DIR%axity
if not defined REPEAT set REPEAT=1
set RESULTS_CSV=%SCRIPT_DIR%results.csv
set AX_EXE_DEBUG=%SCRIPT_DIR%..\target\debug\axity.exe
set AX_EXE_RELEASE=%SCRIPT_DIR%..\target\release\axity.exe
set AX_EXE=
if exist "%AX_EXE_RELEASE%" set AX_EXE=%AX_EXE_RELEASE%
if not defined AX_EXE (
  if exist "%AX_EXE_DEBUG%" set AX_EXE=%AX_EXE_DEBUG%
)

set PYTHON_CMD=
where python3 >nul 2>&1 && set PYTHON_CMD=python3
if not defined PYTHON_CMD (
  where python >nul 2>&1 && set PYTHON_CMD=python
)
if not defined PYTHON_CMD (
  set PYTHON_CMD=py -3
)

if not defined AX_EXE (
  echo Axity executable not found in target\debug or target\release
  echo Build it with: cargo build
)

if not exist "%RESULTS_CSV%" (
  echo name,python_avg,python_min,python_max,axity_avg,axity_min,axity_max>"%RESULTS_CSV%"
)

call :run ackermann
call :run dict_ops
call :run fibonacci
call :run list_ops
call :run loops
call :run math
call :run matrix
call :run nested_loops
call :run primes
call :run recursion
call :run sorting
call :run string_ops
call :run strings

exit /b 0

:run
set NAME=%~1
echo ============================================
echo %NAME%
echo --- Python ---
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$times=@();" ^
  "$sw=[System.Diagnostics.Stopwatch]::StartNew();" ^
  " & $env:PYTHON_CMD \"$env:PY_DIR\\$env:NAME.py\";" ^
  " $sw.Stop();" ^
  "$times+= $sw.Elapsed.TotalSeconds;" ^
  " for($i=1;$i -lt [int]$env:REPEAT;$i++){ $sw=[System.Diagnostics.Stopwatch]::StartNew(); & $env:PYTHON_CMD \"$env:PY_DIR\\$env:NAME.py\" | Out-Null; $sw.Stop(); $times+= $sw.Elapsed.TotalSeconds; }" ^
  " $avg=[Math]::Round((($times | Measure-Object -Average).Average),3);" ^
  " $min=[Math]::Round((($times | Measure-Object -Minimum).Minimum),3);" ^
  " $max=[Math]::Round((($times | Measure-Object -Maximum).Maximum),3);" ^
  " Write-Host ('Time: {0:N3}s (avg {1:N3}s, min {2:N3}s, max {3:N3}s)' -f $times[-1], $avg, $min, $max);" ^
  " Set-Content -Path \"$env:SCRIPT_DIR\\py.tmp\" -Value ($avg.ToString('0.000')+','+$min.ToString('0.000')+','+$max.ToString('0.000'))"
echo --- Axity ---
if defined AX_EXE (
  powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "$times=@();" ^
    "$sw=[System.Diagnostics.Stopwatch]::StartNew();" ^
    " & \"$env:AX_EXE\" \"$env:AX_DIR\\$env:NAME.ax\";" ^
    " $sw.Stop();" ^
    "$times+= $sw.Elapsed.TotalSeconds;" ^
    " for($i=1;$i -lt [int]$env:REPEAT;$i++){ $sw=[System.Diagnostics.Stopwatch]::StartNew(); & \"$env:AX_EXE\" \"$env:AX_DIR\\$env:NAME.ax\" | Out-Null; $sw.Stop(); $times+= $sw.Elapsed.TotalSeconds; }" ^
    " $avg=[Math]::Round((($times | Measure-Object -Average).Average),3);" ^
    " $min=[Math]::Round((($times | Measure-Object -Minimum).Minimum),3);" ^
    " $max=[Math]::Round((($times | Measure-Object -Maximum).Maximum),3);" ^
    " Write-Host ('Time: {0:N3}s (avg {1:N3}s, min {2:N3}s, max {3:N3}s)' -f $times[-1], $avg, $min, $max);" ^
    " Set-Content -Path \"$env:SCRIPT_DIR\\ax.tmp\" -Value ($avg.ToString('0.000')+','+$min.ToString('0.000')+','+$max.ToString('0.000'))"
  set PY_STATS=
  set AX_STATS=
  for /f "usebackq delims=" %%A in ("%SCRIPT_DIR%py.tmp") do set PY_STATS=%%A
  for /f "usebackq delims=" %%A in ("%SCRIPT_DIR%ax.tmp") do set AX_STATS=%%A
  del "%SCRIPT_DIR%py.tmp" >nul 2>&1
  del "%SCRIPT_DIR%ax.tmp" >nul 2>&1
  echo %NAME%,!PY_STATS!,!AX_STATS!>>"%RESULTS_CSV%"
) else (
  echo Skipping Axity for %NAME% (axity.exe not found)
)
echo.
exit /b 0
