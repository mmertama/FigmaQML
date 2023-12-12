# Readme.txt 

The FigmaQML can produce QT for MCU compliant QML code (later QUL to distinguish from the QML).

Here is another application to verify that generated QML content is MCU compliant

For verification and preview it is generated an application to show content.
The QUL code cannot load anything from to files, e.g. for the loader the QtQuick source code has to be pre-compiled.
Therefore for preview, each time the preview is generated, the generated QUL source is injected in the application
source and the preview application is built and started.


