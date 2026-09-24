$env:PATH = "D:\app\qt-c++\Tools\mingw1310_64\bin;D:\app\qt-c++\6.11.2\mingw_64\bin;$env:PATH"
$env:CC  = "D:\app\qt-c++\Tools\mingw1310_64\bin\x86_64-w64-mingw32-gcc.exe"
$env:CXX = "D:\app\qt-c++\Tools\mingw1310_64\bin\x86_64-w64-mingw32-g++.exe"

cmake -B build -G Ninja `
  -DCMAKE_PREFIX_PATH="D:\app\qt-c++\6.11.2\mingw_64" `
  -DQT_NO_PACKAGE_VERSION_CHECK=ON `
  -DQT_NO_PACKAGE_VERSION_INCOMPATIBLE_WARNING=TRUE

cmake --build build --target game --parallel 1