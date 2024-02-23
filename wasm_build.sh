if [ -z $1 ]; then
    if [ ! -z $QT_ROOT_DIR ]; then
        QT_ROOT=$QT_ROOT_DIR/..
    else
        QT_ROOT=~/Qt/6.6.2
    fi
else
    QT_ROOT=$1
fi


if [ ! -d $QT_ROOT ]; then
    echo "invalid QT root '$QT_ROOT'";
    exit 1
fi

if [ -z $2 ]; then
    if [ ! -z $EMSDK ]; then
        EMSDK_ROOT=$EMSDK
    else
        EMSDK_ROOT=~/Development/emsdk
    fi
else
    EMSDK_ROOT=$2
fi


if [ ! -d $EMSDK_ROOT ]; then
    echo "invalid EMSDK root '$EMSDK_ROOT'";
    exit 2
fi


export QT_DIR=$QT_ROOT/wasm_singlethread
export QT_HOST_PATH=$QT_ROOT/gcc_64

export CMAKE_PREFIX_PATH=${QT_DIR}/lib/cmake

mkdir -p build
pushd build

export EMSDK=$EMSDK_ROOT
export _QT_TOOLCHAIN_VARS_INITIALIZED=1
export _QT_TOOLCHAIN_QT_CHAINLOAD_TOOLCHAIN_FILE=${EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake
export _QT_TOOLCHAIN_QT_TOOLCHAIN_INCLUDE_FILE=
export _QT_TOOLCHAIN_QT_TOOLCHAIN_RELOCATABLE_CMAKE_DIR="bing2"
export _QT_TOOLCHAIN_QT_TOOLCHAIN_RELOCATABLE_PREFIX="bing3"
export _QT_TOOLCHAIN_QT_ADDITIONAL_PACKAGES_PREFIX_PATH="bing4"

CPPCOMPILER=$EMSDK/upstream/emscripten/em++
CCOMPILER=$EMSDK/upstream/emscripten/emcc
QMAKE=$QT_ROOT/wasm_singlethread/bin/qmake
BEFORE=$(pwd)/../setup/auto-setup.cmake
TOOL_CHAIN=$QT_ROOT/wasm_singlethread/lib/cmake/Qt6/qt.toolchain.cmake


cmake -S .. -B . -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PROJECT_INCLUDE_BEFORE=$BEFORE -DQT_QMAKE_EXECUTABLE=$QMAKE -DCMAKE_PREFIX_PATH=$QT_DIR -DCMAKE_C_COMPILER=$CCOMPILER -DCMAKE_CXX_COMPILER=$CPPCOMPILER -DCMAKE_TOOLCHAIN_FILE=$TOOL_CHAIN -DQT_HOST_PATH=$QT_HOST_PATH

cmake --build . --config Release

   
popd

