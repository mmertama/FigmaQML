# FigmaQML #

FigmaQML is an application to generate [QML](https://doc.qt.io/qt-6/qtqml-index.html) sourcecode
from [Figma](https://www.figma.com) designs for [QtQuick](https://doc.qt.io/qt-6/qtquick-index.html) 
applications. FigmaQML will let designer to compose the UI without need of programmer to reproduce 
design using UI code. Omitting that tedious, reduntant step, will dramatically decrease development effort.
FigmaQML will provide a QML code that is ready programmer to focus on implementing functionality.  

* Version 2.0.0
* License: [Mit license](https://en.wikipedia.org/wiki/MIT_License)

## Run ##

 * FigmaQML can be run as a Web Assembly application (There seems to be some occasional CORS issues, and I have no idea how to fix. If that is a problem, please use a native app!) - [Start FigmaQML](https://mmertama.github.io/FigmaQML/FigmaQML.html)
 * FigmaQML can be run either in command line or as a gui application.
 * Gui application is interactive to see visuals and source code before saving.
 * Command line is for processing Figma documents to QML. 
 * Basic Command line format is <code> $figmaqml <options> TOKEN PROJECT QML_DIRECTORY </code>
  * For options see --help
  * For MacOSX the commandline refers inside of application bundle: i.e. <code>$ FigmaQML.app/Contents/MacOS/FigmaQML</code> 

### Binaries ###

* For [Windows 10](https://github.com/mmertama/FigmaQML/releases) (update todo - currently version 1.0.0)
* For [Mac OSX](https://github.com/mmertama/FigmaQML/releases) (update todo - currently version 1.0.0)
* For Ubuntu, binaries release is not available, please compile from sources.

### Quick guide to FigmaQML UI ###

* **Tokens**
  * Set your user [account token ](https://www.figma.com/developers/api#access-tokens) and project token. 
  * To get project token open your Figma Document on the browser and see the URL: The token after file.
  * For example: if the URL is "https://www.figma.com/file/bPWNMoKnXkXgf71S9cFX7G/…", the requested token is
  "bPWNMoKnXkXgf71S9cFX7G"  (without quotes)
* **Export all QMLs**
    * Generates QML code and related images into the given directory.
* **Edit Imports...** edit a QML "imports" statement.
* **Fonts...** See and map how the current Figma design fonts are mapped with the local fonts. You don't have to install missing fonts, therefore you set an additional font file search folder. 
* **Store** saves the current project.
* **Restore** reads a project from a file.
* '**Source**', '**Figma**', '**QtQuick** show generated source, project JSON or draw the generated Frame.
* **+/-** a Figma project is having one or more pages. FigmaQML expects that each page contains one or more views when a view is typically a frame or a group Figma element. 
* **Connect** starts requesting project data periodically from the Figma Server. When server is connected at first time, all the document data is retrieved, then trancoded to QML and rendered to UI. After that FigmaQML polls the changes at the server. Therefore the modifications made in Figma application are reflected to the FigmaQML more or less instantly depending on design's size. 
* **Disconnect** stops polling from the server. You must disconnect before another project can be loaded.
* **Settings**:
  * Break booleans: Generate code for each child element that composites a Figma Boolean element are generated. By default only the composed shape Item is produced.
  * Embed images: Creates stand-alone QML files that have images written in the QML code instead of generating and referring to image files.
  * Render view: Set the view to be rendered on the Figma Server, and the generated UI is just an image. Handy to compare rendering results. 
  * Antialize shapes: Whether "antialized: true" property is set on each shape. Alternatively improve rendering quality (as FigmaQML does) by setting the global multisampling using the code snippet:
 <pre>
 QSurfaceFormat format;
 format.setSamples(8);
 QSurfaceFormat::setDefaultFormat(format);
 </pre>
* **Component** is displayed only when the source code tab is activated. If the currently rendered view contains QML, here user can select the view's QML source, or the source code of any of the components that are generated. 
* **Scaling**: let the user to zoom in and out the current rendering. 
* **Ctrl+C** copies the current text view to the clipboard. 

### Build and deploy ###
* Qt6
* Python 3.8 (or later)
* CMake 3.16 (or later)
* For WebAssembly see [Qt 6.4 WebAssembly](https://doc-snapshots.qt.io/qt6-6.4/wasm.html)
* For Windows 10: 
    * Git Bash
    * MSVC 19
    * OpenSSL binaries (prebuilt version within Qt)
    * msvc_build.bat in x64 Native Tools Command Prompt
    * If the batch files refuses to work, please look the following sequence:
      ```
      echo Expected to be executed in x64 Native Tools Command Prompt for VS 2019
      set QT_DIR=C:\Qt\6.3.1\msvc2019_64
      set CMAKE_PREFIX_PATH=%QT_DIR%\lib\cmake
      mkdir build
      pushd build
      cmake ..
      cmake --build . --config Release
      ```
* For Mac OSX
    * osx_build.sh
* For Linux
    * linux_build.sh

#### Testing
There are few scripts in the [test]() folder that are used for testing. Since the test data itself is under personal Figma account, there are no automated tests provided along the FigmaQML sources. However if you have a good set of Figma documents available you can execute tests using next example:
 <pre>
 export IMAGE_COMPARE="python ../figmaQML/test/imagecomp.py"
 export IMAGE_TRESHOLD="0.90"
 token=123456-aaaabbbbb-cccc-dddd-8888-1234567890
 echo "Test time: $(date)" >> test_log.txt 
 {
 echo Test_no: 1
 ../figmaQML/test/runtest.sh ../figmaQML/Release/FigmaQML $token Nku226IVrvZtsRc71QJyWx
 if [ $? -ne 0 ]; then echo fail; exit; fi
.....
 echo Test_no: 3 
 ../figmaQML/test/runtest.sh ../figmaQML/Release/FigmaQML $token OZcdWgROy0Czk0JASRF21v "2-10"
 if [ $? -ne 0 ]; then echo fail; exit; fi
 echo Test_no: 4
 ../figmaQML/test/runtest_image.sh ../figmaQML/Release/FigmaQML "2-3"
 if [ $? -ne 0 ]; then echo fail; exit; fi
 echo Test_no: 5 
 ../figmaQML/test/runtest_image.sh ../figmaQML/Release/FigmaQML "2-4"
 if [ $? -ne 0 ]; then echo fail; exit; fi
 echo Test_no: 6 
 ../figmaQML/test/runtest.sh ../figmaQML/Release/FigmaQML $token bZDWbBfInVrD1ijuIJZD88WG
 if [ $? -ne 0 ]; then echo fail; exit; fi
.... 
 echo Test_no: done
 } 2> test_errors.txt | tee -a test_log.txt 
 </pre>

 * $token is your Figma user token.
 * Parameter after that is a test project token.
 * Optional third parameter is a Canvas-view to be tested (default is 1-1)
 * runtest.sh fetch data from Figma server and runs basic QML generation test on that
 * runtest_image let run additional image tests on data without furher data retrive. (Figma service has data quota)
 * image test compares Figma rendered Canvas-view and FigmaQML rendered canvas view (see IMAGE_COMPARE above) and provides fuzzy match value between 0 and 1.
 * Here I have been using value 0.9, "90% same"), (see IMAGE_TRESHOLD above) to pass the test.
 * Note: You may have to install SSIM_PIL from https://github.com/mmertama/SSIM-PIL.git until my change is accepted in.
 
 #### Changes
 * 1.0.1 
    * Qt5 may not build anymore, Qt 6.3 had some issues and maintaing those also on Qt 5.15 was too much work. The support for Qt6 is a bit half hearted, as many Qt5 components are used - but some of those are not available yet (some coming Qt 6.5). 
    But more troublesome issue is that Graphic effects are on Qt6 only available on commercial version. Therefore Qt5Compat until eternity or the issue changes.
    * On OSX Checkbox was not working correctly so I did a quick styling to fix it.
    * Some minor fixes to get rid of warnings.
    * Build script and README updates.   
 * 2.0.0
    * WebAssembly support
    * Lot of fixes and refactoring code for the Web assembly
