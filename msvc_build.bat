
echo Expected to be executed in x64 Native Tools Command Prompt for VS 2019
set QT_DIR=C:\Qt\6.3.1\msvc2019_64
set CMAKE_PREFIX_PATH=%QT_DIR%\lib\cmake
mkdir build
pushd build
cmake .. -DCMAKE_BUILD_TYPE=Release
msbuild FigmaQML.sln /property:Configuration=Release
%QT_DIR%\bin\windeployqt --qmldir ../qml Release
copy ..\OpenSSL_win\*.dll Release
popd



