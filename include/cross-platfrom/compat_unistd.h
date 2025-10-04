#pragma once

#ifdef _WIN32
  #include <io.h>       // for _access
  #include <process.h>  // for _getpid
  #include <windows.h>  // for Sleep
  #include <direct.h>   // for _chdir, _mkdir
  #include <stdlib.h>

  #define access _access
  #define getpid _getpid
  #define sleep(x) Sleep((x) * 1000)
  #define usleep(x) Sleep((x) / 1000)
  #define F_OK 0
  #define R_OK 4
  #define W_OK 2
  #define X_OK 1
#else
  #include <unistd.h>
#endif