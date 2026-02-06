# MinGW toolchain file for MakineAI
# Uses Qt's MinGW distribution

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Compiler paths
set(CMAKE_C_COMPILER "C:/Qt/Tools/mingw1310_64/bin/gcc.exe")
set(CMAKE_CXX_COMPILER "C:/Qt/Tools/mingw1310_64/bin/g++.exe")
set(CMAKE_RC_COMPILER "C:/Qt/Tools/mingw1310_64/bin/windres.exe")
set(CMAKE_AR "C:/Qt/Tools/mingw1310_64/bin/ar.exe")
set(CMAKE_RANLIB "C:/Qt/Tools/mingw1310_64/bin/ranlib.exe")

# Make command
set(CMAKE_MAKE_PROGRAM "C:/Qt/Tools/Ninja/ninja.exe")

# Find root path
set(CMAKE_FIND_ROOT_PATH "C:/Qt/Tools/mingw1310_64")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
