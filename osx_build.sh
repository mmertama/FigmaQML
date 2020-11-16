export CMAKE_PREFIX_PATH=${QT_DIR}/lib/cmake
cmake CMakeLists.txt -DCMAKE_BUILD_TYPE=Release -Bbuild
pushd build
make
${QT_DIR}/bin/macdeployqt FigmaQML.app -qmldir=../qml
popd

