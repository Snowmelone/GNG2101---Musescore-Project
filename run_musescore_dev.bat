@echo off
rem === Adjust Qt base path if you ever change Qt version ===
set "QT_BASE=C:\Qt\6.9.3\msvc2022_64"

rem Put Qt bin first on PATH so all Qt6 dlls match the QML plugins
set "PATH=%QT_BASE%\bin;%PATH%"

rem QML import paths (Qt modules + Qt5Compat)
set "QML_IMPORT_PATH=%QT_BASE%\qml;%QT_BASE%\qml\Qt5Compat"
set "QML2_IMPORT_PATH=%QML_IMPORT_PATH%"

rem (optional but harmless) Qt plugin path
set "QT_PLUGIN_PATH=%QT_BASE%\plugins"

cd /d C:\Users\Kian\GNG2101---Musescore-Project\build-msvc

MuseScore4.exe
