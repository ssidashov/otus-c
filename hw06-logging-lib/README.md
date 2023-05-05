# Библиотека для логгирования.

Предназначена для логгирования сообщений с заданным уровнем важности (один из debug, info, warning, error) в файл или на консоль. Возможно задать несколько appender'ов (настроек для логгирования в файл или консоль). Каждый appender поддерживает следующие настройки:
- Формат сообщения
- Минимальный уровень важности
- Тип

Есть 2 типа appender'ов:
- file - вывод в файл
- stdout - вывод в stdout

Appender file также имеет обязательную опцию filename - имя файла для логгирования. Stdout имеет необязательную опция error_to_to_stderr - выводить ли ошибки ERROR в STDERR (по умолчанию в stdout).

Логгирование осуществляется функциями:
- LOG4C_DEBUG
- LOG4C_INFO
- LOG4C_WARNING
- LOG4C_ERROR

Для использования бибилиотеки необходимо ее проинициализировать и настроть. Это делается либо передачей аргументов командной строки в функцию

    int log4c_setup_cmdline(int argc, char **argv);
либо программным заполнением структур с конфигурацией appender'ов и вызовом функции

    int log4c_setup_appenders(log4c_appeder_cfg_struct *appender_list[], size_t appender_list_size);
если не вызвать инициализацию, то будет создан appender по умолчанию (stdout с форматом по умолчанию).
Для освобождения ресурсов и закрытия файлов перед окончанием работы приложения необходимо вызвать функцию

    int log4c_finalize(void);
Если этого не сделать, сработает освобождение ресурсов по обработчику завершения main (atexit).

## Сборка
Приложение использует CMake для сборки. Для библиотек используется модули поиска PkgConfig.

    mkdir build
    cd build
    cmake ..
    make
Результат сборки (.a библиоткеа) создается в директории build относительно корня проекта.

## Использование

Создано тестовое приложения для тестирования библиотеки. Оно представляет собой процесс, который инициализирует библиотеку из аргументов командной строки, запускает N потоков, которые периодически пишут сообщения в лог с различным уровнем. Для остановки приложения нужно послать SIGINT (Ctrl+C).

Сборка приложения (аналогично сборке библиотеки):

    mkdir build
    cd build
    cmake ..
    make

Запуск с аппендером по умолчанию:

    ./demoapp

Запуск с двумя логгерами и различным параметрами конфигурации:

    ./demoapp -Dlog4c.appender1.type=file -Dlog4c.appender1.filename=./log4c.log -Dlog4c.appender1.format="%t %l14 %p %f35 %c25 | %m" -Dlog4c.appender2.type=stdout -Dlog4c.appender2.format="%t %l14 %m" -Dlog4c.appender2.threshold=INFO -Dlog4c.appender2.error_to_stderr=true

Пример вывода лога:

    2023-05-05 13:44:27.869481 WARN  00007fba37fff6c0 /home/sergey/projects/otus-c/hw06-logging-lib/demoapp/demoapp.c:28 thread_func               Warning condition triggered, 1843993368
    2023-05-05 13:44:27.869517 DEBUG 00007fba3dffb6c0 /home/sergey/projects/otus-c/hw06-logging-lib/demoapp/demoapp.c:23 thread_func               Spinning...
    2023-05-05 13:44:27.869876 DEBUG 00007fba3effd6c0 /home/sergey/projects/otus-c/hw06-logging-lib/demoapp/demoapp.c:23 thread_func               Spinning...
    2023-05-05 13:44:27.869951 ERROR 00007fba3effd6c0 /home/sergey/projects/otus-c/hw06-logging-lib/demoapp/demoapp.c:31 thread_func               Error condition triggered, 1956297539
    #1  0x000056419d5fd4dc sp=0x00007fba3effce90 thread_func + 0x193
    #2  0x00007fba7717fbb5 sp=0x00007fba3effcee0 pthread_condattr_setpshared + 0x4e5
    #3  0x00007fba77201d90 sp=0x00007fba3effcf80 __clone + 0x120
    2023-05-05 13:44:27.896565 INFO  00007fba770c4740 /home/sergey/p asdfrojects/otus-c/hw06-logging-lib/demoapp/demoapp.c:44 int_handler          TRIGGERED SHUTDOWN...
