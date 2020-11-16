export CMAKE_PREFIX_PATH=${QT_DIR}/lib/cmake
cmake CMakeLists.txt -DCMAKE_BUILD_TYPE=Release -Bbuild
pushd build
make
${LINUX_DEPLOY_QT}/linuxdeployqt FigmaQML.app -qmldir=../qml
popd

