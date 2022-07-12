if [ -z ${QT_DIR} ]; then
    echo "Oops: QT_DIR is not set"
    echo "e.g. export QT_DIR=\$HOME/Qt/6.2.4/macos/lib/cmake/Qt6"
    exit -1
fi
export CMAKE_PREFIX_PATH=${QT_DIR}/lib/cmake
cmake CMakeLists.txt -DCMAKE_BUILD_TYPE=Release -Bbuild
pushd build
make
if [ -f ${QT_DIR}/bin/macdeployqt ]; then
    MACDEPLOY=${QT_DIR}/bin/macdeployqt
else
    MACDEPLOY=${QT_DIR}/../../../bin/macdeployqt
fi
$MACDEPLOY FigmaQML.app -qmldir=../qml
popd

