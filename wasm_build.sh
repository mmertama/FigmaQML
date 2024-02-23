export QT_DIR=~/Qt/6.6.2/wasm_singlethread
export QT_HOST_PATH=~/Qt/6.6.2/gcc_64

export CMAKE_PREFIX_PATH=${QT_DIR}/lib/cmake

mkdir -p build
pushd build

export EMSDK=/home/markus/Development/emsdk
export _QT_TOOLCHAIN_VARS_INITIALIZED=1
export _QT_TOOLCHAIN_QT_CHAINLOAD_TOOLCHAIN_FILE=${EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake
export _QT_TOOLCHAIN_QT_TOOLCHAIN_INCLUDE_FILE=
export _QT_TOOLCHAIN_QT_TOOLCHAIN_RELOCATABLE_CMAKE_DIR="bing2"
export _QT_TOOLCHAIN_QT_TOOLCHAIN_RELOCATABLE_PREFIX="bing3"
export _QT_TOOLCHAIN_QT_ADDITIONAL_PACKAGES_PREFIX_PATH="bing4"

CPPCOMPILER=/home/markus/Development/emsdk/upstream/emscripten/em++
CCOMPILER=/home/markus/Development/emsdk/upstream/emscripten/emcc
QMAKE=/home/markus/Qt/6.6.2/wasm_singlethread/bin/qmake
BEFORE=/home/markus/Development/build-FigmaQML-WebAssembly_Qt_6_6_2_single_threaded-Debug/.qtc/package-manager/auto-setup.cmake
TOOL_CHAIN=/home/markus/Qt/6.6.2/wasm_singlethread/lib/cmake/Qt6/qt.toolchain.cmake


cmake -S .. -B . -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PROJECT_INCLUDE_BEFORE=$BEFORE -DQT_QMAKE_EXECUTABLE=$QMAKE -DCMAKE_PREFIX_PATH=$QT_DIR -DCMAKE_C_COMPILER=$CCOMPILER -DCMAKE_CXX_COMPILER=$CPPCOMPILER -DCMAKE_TOOLCHAIN_FILE=$TOOL_CHAIN -DQT_HOST_PATH=$QT_HOST_PATH

cmake --build . --config Release

   
popd

