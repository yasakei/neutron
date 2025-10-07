# Copy MinGW DLLs to build directory
Copy-Item C:\msys64\mingw64\bin\libgcc_s_seh-1.dll build\ -Force
Copy-Item C:\msys64\mingw64\bin\libwinpthread-1.dll build\ -Force
Copy-Item C:\msys64\mingw64\bin\libstdc++-6.dll build\ -Force
Write-Host "MinGW runtime DLLs copied to build directory"
Write-Host "Testing neutron.exe..."
