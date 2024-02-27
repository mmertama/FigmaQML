# FigmaQML #

FigmaQML is an application to generate [QML](https://doc.qt.io/qt-6/qtqml-index.html) sourcecode
from [Figma](https://www.figma.com) designs for [QtQuick](https://doc.qt.io/qt-6/qtquick-index.html) 
applications. FigmaQML will let designer to compose the UI without need of programmer to reproduce 
design using UI code. Omitting that tedious, redundant step, will dramatically decrease development effort.
FigmaQML will provide a QML code that is ready programmer to focus on implementing functionality. You can inject
tags in you Figma Design that enables seamlessly assingn functionality so that there is no need to implement any
QML UI code.

FigmaQML supports both Desktop and Qt for MCU development.

* Version 3.0.0
* License: [Mit license](https://en.wikipedia.org/wiki/MIT_License)

## Run ##

 * FigmaQML can be run either in command line or as a gui application.
    * Command line support for Qt for MCU is under construction.
    * Maybe I will drop off the commandline support entirely as it is good to see results before export. 
 * Gui application is interactive to see visuals and source code before saving.
 * Command line is for processing Figma documents to QML. 
 * Basic Command line format is <code> $figmaqml &lt;options&gt; TOKEN PROJECT QML_DIRECTORY </code>
    * For options see --help
    * For MacOSX the commandline refers inside of application bundle: i.e. <code>$ FigmaQML.app/Contents/MacOS/FigmaQML</code>
* FigmaQML can be run as a WebAssembly application.
   * There seems to be some occasional CORS issues, and I have no idea how to fix. If that is a problem, please use a native app! - [Start FigmaQML](https://mmertama.github.io/FigmaQML/FigmaQML.html)
   * Qt for MCU support is under construction.  

### Binaries ###

* For [Windows 10](https://github.com/mmertama/FigmaQML/releases) (update TODO - currently version 1.0.0)
* For [Mac OSX](https://github.com/mmertama/FigmaQML/releases) (update TODO - currently version 1.0.0)
* For Ubuntu, binaries release is not available, please compile from sources.

### Quick guide to FigmaQML UI ###

## File ##
* **Tokens**
  * Set your user [account token ](https://www.figma.com/developers/api#access-tokens) and project token. 
  * To get project token open your Figma Document on the browser and see the URL: The token after file.
  * For example: if the URL is "https://www.figma.com/file/bPWNMoKnXkXgf71S9cFX7G/…", the requested token is
  "bPWNMoKnXkXgf71S9cFX7G"  (without quotes)
* **Export Qt for Desktop**
    * Export appliaction QMLs     
* **Export Qt for MCU**
    * Open view to manage and export Qt for MCU content.
    * Available only for Linux (Qt for MCU requires Ubuntu 20.04)
* **Export all QMLs**
    * Generates QML code and related images into the given directory.
    * Do not use for Qt for MCU.
* **Edit Imports...** edit a QML "imports" statement.
* **Fonts...**
    * See and map how the current Figma design fonts are mapped with the local fonts. You don't have to install missing fonts, therefore you set an additional font file search folder.
    * Qt For MCU is partial, MCU application uses the font defined in its qmlproject file, and it may be different defined for FigmaQML. I.e. Fonts defined here does not impact to final fonts. (TODO) 
* **Store** 
    * Saves the current project. The big project can take time and Store-Restore would save your time by storing of 'snapshot' of the current content.
* **Restore**
    * Reads a project from a file.
    
## Tabs ##    
* **Source**
    * Generated source code. Mostly for debugging . You may use Ctrl+C to copy it to an external editor. 
* **Figma**
    * Figma produced interal content. Mostly for debugging
* **QtQuick**
    * Show QML rendering of the rendering - verify here that it looks ok. 

## Views ##

* **+/-**
    * Navigate pages and views.. 
    * Figma project consists of "page" (or canvas) and "views" on each page.
    * FigmaQML generes an UI for each view.

## Connect ##

**Update**
* Read Figma data and generate QML out from it.
* May take a while, When server is connected at first time, all the document data is retrieved, then trancoded to QML and rendered to UI
    
## Settings ##
  * **Break booleans**
    * Generate code for each child element that composites a Figma Boolean element are generated. By default only the composed shape Item is produced.
    * Avoid using.
  * **Embed images**
    * Creates stand-alone QML files that have images written in the QML code instead of generating and referring to image files.
    * Do not use with Qt for MCU
  * **Antialiaze shapes**
    * Whether "antialiazed: true" property is set on each shape.
    * Alternatively improve rendering quality (as FigmaQML does) by setting the global multisampling using the code snippet:
        
    ```cpp
    
    QSurfaceFormat format;
    format.setSamples(8);
    QSurfaceFormat::setDefaultFormat(format);
    
    ```

    * Do not use with Qt for MCU
    
 * **Qt for MCU**
    * The generated code is targeted for Qt for MCU.
    * Available only for Linux (Qt for MCU requires Ubuntu 20.04)
 * **Static QML**
    * Turn off all dynamic code creation.
 * **Cyan background**
    * Change FigmaQML background to an alternative color.
    * Does not affect to the exported code.
 * **No Gradients**
    * Qt for MCU current version (2.6) does not support gradients, this enforces to rendered with the gradient's average color.
 * **Loader placeholders**
    * Show dynamic loader placeholder instead of rendered components.
    * Does not affect to the exported code.
 * **Render placeholders** 
     * Show placeholder components as rendered images.
     * Only for debugging and verification. Figma server may have hiccups if there are too much to render.
     * Does not affect to the exported code.
 * **Render view** 
    * Set the view to be rendered on the Figma Server, and the generated UI is just an image. Handy to compare rendering results.
    * Only for debugging and verification. Figma server may have hiccups if there are too much to render. 

## Components ##
* Displayed only when the source code tab is activated.
    * Show a source code of the component or item selected.
* **Scaling**
    * Let the user to zoom in and out the current rendering. 

## Dynamic code ##

The FigmaQML generated code is exported into application code where it is used like any QML item. FigmaQmlUi Item takes charge of rendering the UI graphics.
You can inject the FigmaQML generated UI in your application just by  adding a FigmaQML item.

```js

FigmaQmlUi {
        id: figmaUi
        anchors.fill: parent
        }

```


That code renders the UI on the parent space, that is typically the full screen rectangle. 

Displaying is only part of the UI: Any useful applications would be able to change its content and interact with user.
Therefore, the new FigmaQML adds concept for dynamic functionality, and introduces few methods to act with FigmaQML generated code.

The special innovation is to use a special naming in the Figma document, i.e. tagging.
The naming is simple and practical – name targeted Figma element format is with “qml?name.command” - 
the name identifies the element in QML and command tells what to do:

*   If command is a property name, that value can be changed.
    * *FigmaQmlSingleton::applyValue* - Change UI strings, e.g., when tag is *qml?pressure_info.text*:
    
    ```js

    FigmaQmlSingleton.applyValue("pressure_info",  pressure + " kPa");

    ```

 * *onClick*
    * *FigmaQMLSingleton::onEvent* is a signal handler that receives touch events coming from the named element.
    * The signalled parameters are 'element' and 'click_event'.
    * Most common is to implement button actions, e.g. *qml?temp_unit.onClick*: 

    ```js
    FigmaQmlSingleton.onEventReceived: {
        if(event == 'click_event' && element == 'temp_unit') {
            bme680.currentUnitCelcius = !bme680.currentUnitCelcius;
        }
    ```        

* *asLoader*
    * *FigmaQMLSingleton::setSource* let replace part of Figma generated with the custom QML component.
    * Since all UI content just cannot be visualize beforehand, this powerful feature injects any QML content 
     into UI to replace the tagged element.
    * In Qt for MCU the QML file has to be injected in the FigmaQML module - therefore some steps are needed.
        * *FigmaQml_AddQml* cmake funtion let you inject QML and header files into UI codes.
        * in Desktop that is not needed. 
    
    ```cmake 
    # include helpers  
    include(${path_to_generated_source/FigmaQmlInterface}/FigmaQmlInterface.cmake)
    FigmaQml_AddQml(QMLS  ${CMAKE_SOURCE_DIR}/qml/Graphs.qml  ${CMAKE_SOURCE_DIR}/qml/TimeSeries.qml HEADERS ${mcukit_SOURCE_DIR}/utils/Utils.hpp)
    ```
    * Injecting component is straighforward, to replace qml?Temp.asLoader:
    
    ```js
    FigmaQmlSingleton.setSource("Temp", "Graphs.qml");
    ```
 

Besides of those methods you can include multiple views in your application, and navigate between them using 
*FigmaQMLSingleton::setView* that changes entire view.

```js
FigmaQmlSingleton.setView(1);
```

## Export Qt for Desktop

#### Execute
* Application UI built and executed
* For debug and verfication
* Available only for Linux

#### Save
Store generated files in your application folder.    

* *Included views*
    * By default a only a current view is exported, to add more views add them into list.
    * *FigmaQmlSingleton::setView* index refers to this view order so that 1st, default, is zero. 
* *Save as app*
    * For debugging, copies also project files to create an executable (virtually same as "Execute" is using).


## Export Qt for MCU ## 
(available only on Linux)

[Qt for MCU dialog](doc/qtformcu.png)

#### Execute

For UI verification "Execute" builds a dummy UI and execute it on the
connected MCU device. 

* *Qt Dir* 
    * Qt directory, e.g. "/home/user/Qt"
* *Qul Version*
    * Qt for MCU version, e.g, "2.6.0"
* *Qul Platform*
    * Target platform e.g. "STM32F769I-DISCOVERY-baremetal"
* *Qt License*
    * Qt for MCU requires Qt commerical license. 
    * License file, can be downloaded from your Qt account.
* *Platform Hardware*
    * Supported "Execute" platform - currently only STM32 is available.
* *Platform tools*
    * Path to MCU tooling e.g. "/home/user/STM32_MCU" 
    
#### Save

Store generated files in your application folder.    

* *Included views*
    * By default a only a current view is exported, to add more views add them into list.
    * *FigmaQmlSingleton::setView* index refers to this view order so that 1st, default, is zero. 
* *Save as app*
    * For debugging, copies also project files to create an executable (virtually same as "Execute" is using).

## Integrate for Desktop project
1. Add FigmaInterface into target_link_libraries
    ```cmake
    target_link_libraries(${PROJECT_NAME} PRIVATE` FigmaQmlInterface)
    ```
1. In your application QML file
    * Import FigmaQmlInterface and add FigmaQmlUi
    
    ```js
   import FigmaQmlInterface

   ...

    FigmaQmlUi {
        anchors.fill: parent
    }    
    ```

1. Register singleton
    
    Call `registerFigmaQmlSingleton` before laoding your UI.
    
    ```cpp
    
    #include <QtGui>
    #include <QtQml>
    
    #include "FigmaQmlInterface/FigmaQmlInterface.hpp"
    
    int main(int argc, char** argv) {
        QGuiApplication app(argc, argv);
        QQmlApplicationEngine engine;
        FigmaQmlInterface::registerFigmaQmlSingleton(engine);
        engine.load("qrc:/qml/main.qml");
        return app.exec();
    }
    
    ```

## Integrate for MCU project

1. CMake
    * See also injecting source for loader above
    * Add FigmaInterface into target_link_libraries
    ```cmake
    target_link_libraries(${PROJECT_NAME} PRIVATE` FigmaQmlInterface)
    ```
1. qmlproject file
    * Add import path to FigmaQmlInterface (the path is in the generated code, and depends Figma naming)
    ```json
    importPaths: ["path_to/FigmaQmlInterface"] 
    ```
    * Apply Spark [See Qt doc](https://doc.qt.io/QtForMCUs-2.6/qtul-fonts.html)    

1. In your application QML file
    * Import FigmaQmlInterface and add FigmaQmlUi
    
    ```js
   import FigmaQmlInterface

   ...

    FigmaQmlUi {
        anchors.fill: parent
    }    

    ```


## Other Info

### Build and deploy
* Qt6.2 or newer
* Python 3.8 (or later)
* CMake 3.20 (or later)
* For WebAssembly see [Qt 6.4 WebAssembly](https://doc-snapshots.qt.io/qt6-6.4/wasm.html)
    * Qt5 compatibility library    
* For Windows 10:
    * Qt5 compatibility library 
    * MSVC 19
    * MinGW TBC
    * OpenSSL binaries (prebuilt version within Qt)
* Mac OSX
    * Qt5 compatibility library
* Linux
    * Qt Serial port
    * Qt5 compatibility library


#### Testing
There are few scripts in the [test]() folder that are used for testing. Since the test data itself is under a personal Figma account, there are no automated tests provided along the FigmaQML sources. However if you have a good set of Figma documents available you can execute tests using next example:
 <pre>
 export IMAGE_COMPARE="python ../figmaQML/test/imagecomp.py"
 export IMAGE_THRESHOLD="0.90"
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
 * runtest_image let run additional image tests on data without further data retrive. (Figma service has data quota)
 * image test compares Figma rendered Canvas-view and FigmaQML rendered canvas view (see IMAGE_COMPARE above) and provides fuzzy match value between 0 and 1.
 * Here I have been using value 0.9, "90% same"), (see IMAGE_TRESHOLD above) to pass the test.
 * Note: You may have to install SSIM_PIL from https://github.com/mmertama/SSIM-PIL.git until my change is accepted in.
 
 #### Changes
 * 1.0.1 
    * Qt5 may not build anymore, Qt 6.3 had some issues and maintaining those also on Qt 5.15 was too much work. The support for Qt6 is a bit half hearted, as many Qt5 components are used - but some of those are not available yet (some coming Qt 6.5). 
    But more troublesome issue is that Graphic effects are on Qt6 only available on commercial version. Therefore Qt5Compat untill eternity or the issue changes.
    * On OSX Checkbox was not working correctly so I did a quick styling to fix it.
    * Some minor fixes to get rid of warnings.
    * Build script and README updates.   
 * 2.0.0
    * WebAssembly support
    * Lot of fixes and refactoring code for the WebAssembly 
* 3.0.0
    * Qt for MCU support
    * Dynamic code support
    * Fixes 
    * UI updates
