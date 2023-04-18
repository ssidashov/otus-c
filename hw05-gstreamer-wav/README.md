# Плагин для GStreamer, воспроизводящий wav файлы.

Работает только с обычными, seekable файлами. Плагин представляет собой GStreamer source element, принимающий параметр location для указания имени wav файла. Плагин протестирован для чтения 16-битных little-endian несжатых моно wav-файлов, потенциально может работать с разным числом каналов и битностью аудио потока.
## Сборка
Приложение использует CMake для сборки. Для библиотек используется модули поиска PkgConfig.

    mkdir build
    cd build
    cmake ..
    make
Результат сборки (.so библиотека) создается в директории build относительно корня проекта.

## Использование
Протестировать плагин можно с помощью утилиты gst-launch:

    GST_PLUGIN_PATH=./build gst-launch-1.0 -v -m wavsrc location=test.wav ! audio/x-raw,format=S16LE,channels=1,rate=48000 ! autoaudiosink

также возможно использование без явного указания формата аудио потока, плагин будет пытаться выставить формат сам:

    GST_PLUGIN_PATH=./build gst-launch-1.0 -v -m wavsrc location=test.wav ! autoaudiosink
