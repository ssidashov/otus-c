# Приложение, запрашивающее и показывающее информацию о погоде на сегодня по заданной локации.

В качестве локации может быть указано имя города, координаты, название аэропорта и т.д. (см. типы расположений на https://wttr.in/:help)
Использует библиотеку libcurl для выполнения http запроса к серверу wttr.in. Для разбора полученной информации используется библиотека сJSON. Из полученного с сервера JSON c прогнозом погоды выбирается темература, текстовое описание и скорость и направление ветра, и выводится в виде строки вместе с описанием найденной по запросу локации.
## Сборка
Приложение использует cmake для сборки. Для libcurl используется find_package команда. Для cJSON используется метод с включением дерева исходников вместе с CMakeLists.txt и включением его в проект через add_subdirectory. Сами исходники включены в проект через git submodule, поэтому потребуется выполнить

    git submodule init
    git submodule update

После этого собрать проект можно командами

    mkdir build
    cd build
    cmake ..
    make
Результат сборки (исполняемый файл) создается в директории build относительно корня проекта.

## Использование
    ./curlweather <location>

### Пример:

    ./curlweather St.Petersburg
Выводит:

    Weather at Saint Petersburg, Saint Petersburg City, Russia: 2C(from -1C to 5C), Ясно, wind: 3 m/s ESE
