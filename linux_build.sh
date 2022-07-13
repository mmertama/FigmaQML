if [ -z ${QT_DIR} ]; then
    echo "Oops: QT_DIR is not set"
    echo "e.g. export QT_DIR= /opt/Qt/6.3.1/gcc64"
    exit -1
fi

if [ ! -z $1 ]; then
   LINUX_DEPLOY_QT=realpath $1
fi

if [ -z ${LINUX_DEPLOY_QT} ]; then
   LINUX_DEPLOY_QT=$(which linuxdeployqt)
fi

if [ -z ${LINUX_DEPLOY_QT} ]; then
    echo "Oops: linuxdeployqt is not found"
    echo "Either should find from path, or pass as a parameter"
fi

export CMAKE_PREFIX_PATH=${QT_DIR}/lib/cmake
mkdir -p build
pushd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
if [ ! -z ${LINUX_DEPLOY_QT} ]; then
    ${LINUX_DEPLOY_QT} FigmaQML -qmldir=../qml
fi    
popd

