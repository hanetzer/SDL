set(CMAKE_SYSTEM_NAME Windows)
set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)

# cross compilers to use for C, C++ and Fortran
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_Fortran_COMPILER x86_64-w64-mingw32-gfortran)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(PKG_CONFIG_EXECUTABLE x86_64-w64-mingw32-pkg-config)

# target environment on the build host system
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)

# modify default behavior of FIND_XXX() commands
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
