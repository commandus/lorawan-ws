# lorawan - LoraWAN web service

![lorawan-ws](res/lorawan-ws.png)

Библиотека и утилита командной строки- встроенный веб сервер для работы с термокосами.

ws-sqlite - веб сервер HTTP/1.0 

Порт по умолчанию - 5002

## Первый запуск

У вас должно иметься приложение ws-sqlite.
Если приложение не собрано, перейдите к разделу Сборка..

Если у вас нет файла базы данных, создайте его, указав имя файла базы данных в опции -d.
Опция -c создает новую базу данных с пустыми таблицами.

```
./ws-sqlite -d ../logger-huffman/db/logger-huffman.db -c
```

Запустите веб сервис, указав в опции -r путь к каталогу, где размещены веб страницы.

```
./ws-sqlite -d ../logger-huffman/db/logger-huffman.db -r ../lorawan-ws-angular/lorawan-ws-angular/dist/lorawan-ws-angular -l 1.log
```

Опция -l указывает записывать в файл журнала адреса загруженных из веб сервиса данных.

Если задан корневой каталог опцией -r, откройте в браузере адрес http://localhost:5002/

Если корневой каталог не задан, проверьте работу сервиса, открыв в браузере адрес http://localhost:5002/t?o=0&s=2

Можете также использовать wget:
```
wget http://localhost:5002/t?o=0&s=10
```

## Параметры командной строки

- -p, --port=<port>         port number. Default 5002
- -r, --root=<path>         web root path. Default './html'
- -d, --database=<file>     SQLite database file name. Default ../lorawan-network-server/db/logger-huffman.db
- -l, --log=<file>          log file
- -c, --create-table        force create table in database
- -v, --verbosity           v- error, vv- warning, vvv, vvvv- debug
- -h, --help                Show this help

## Файлы

html: папка с веб контентом

В репозиторий не входит папка html с веб приложением. Скопируйте эту папку или поставьте линк на эту папку вручную.


## JSON API

Используется для получения данных с ошейников в веб интерфейсе методом GET.

Параметры передаются в виде:

``` 
path?parameter1=value1&parameter2=value2
```

где пути (path):

- /	если задан корневой каталог, будут отдаваться файлы из него (HTML файлы, Javascript, CSS, веб- шрифты)
- /raw полученные пакеты
- /t температуры, измеренные косой
- /raw-count количество полученных пакетов
- /t-count количество записей температуры, измеренные косой
- /raw-id Полученный пакет. Нужно указать в параметрах его id.
- /t-id Запись температуры, измеренные косой. Нужно указать в параметрах его id.


В запросах /t-count и /raw-count могут быть заданы условия для фильтрации записей, результатом будет
массив из одного элемента. 

Число записей возвращается в атрибуте cnt.

Например, запрос числа записей с косами 2020 года:

```
http://localhost:5002/t-count?year=20
[{"cnt": 2}]
```
вовращает значение 0.


В запросах /t-id и /raw-id нужно указать обязательный параметр id.

Ответом будет массив из одного элемента, если запись существует, или пустой массив, если записи
с таким идентификатором нет.

Например, запрос записи температур с идентифкатором 4:

```
http://localhost:5002/t-id?id=4
[{"id": 4, "kosa": 28, "year": 20, "no": 2, "measured": 1614438000, "parsed": 1648705346, "vcc": 0.0, "vbat": 0.0, "t": [-783.625,-1407.94,-1407.94,-1391.94,-1391.94,-1391.94], "raw": "4a00280002031c140038100f160216000000003981190002 4b1c02020006cfaa0101a8000201a8000301a9000401a900 4b1c02030501a900", "devname": "pak811-1", "loraaddr": "34313235", "received": "1648705349"}]
```
вернет массив с одной записью.

Параметры (используются не все в разных путях)

- o смещение первой записи (начиная с нуля)
- s число записей в выдаче
- id идентификатор записи
- start
- finish
- sensor
- kosa
- year
- name
- t
- vcc
- vbat
- devname
- loraaddr
- received

Для путей /raw, /t обязательны параметры o, s. Для путей /raw-count, /t-count параметры o, s нельзя указывать.

Для остальных путей параметры start, finish, sensor, kosa, year, name, t, vcc,
vbat, devname, loraaddr, received значение параметра фильтрует записи на равенство, например

```
kosa=42
```

фильтрует записи по косе 42.

Чтобы отфильтровать записи по косам от 11 до 22, нужно к параметру добавить суффиксы ge(больше или равно) 
и le(меньше или равно) :

```
kosa-ge=11&kosa-le=22
```

Все суффиксы:

-lt меньше
-le меньше или равно
-gt больше
-ge больше или равно
-like применяет оператор LIKE. Символ _- любой символ. Символ %- любые символы

Без суффикса параметр задает сравнение на равенство.


## Сборка

Необходимые зависимости:

- libmicrohttpd-0.9.43 (http://ftp.gnu.org/gnu/libmicrohttpd/)
- sqlite3

### Linux

Установите зависимости:

```
sudo apt install libmicrohttpd-dev
sudo apt install libsqlite3-dev
```

или из исходников:

```
tar xvfz libmicrohttpd-0.9.43.tgz
cd libmicrohttpd-0.9.43/
./configure
make
sudo make install
make clean
```

Для sqlite3 используйте амальгаму sqlite3.h sqlite3.c

Соберите библиотеку и утилиту командной строки (веб-сервер для проверки функциональности и тестирования html и javascript)
используя CMake или Automake.

CMake:

```
cd lorawan-ws
mkdir build
cd build
cmake ..
```

Automake:

```
./autogen.sh
./configure
make
sudo make install
make clean
```

### Windows

Если не установлен vcpkg, установите его:
```
git clone https://github.com/Microsoft/vcpkg.git
.\vcpkg\bootstrap-vcpkg.bat
```

Установите зависимости

Для 32 битной целевой платформы:
```
vcpkg install libmicrohttpd:x86-windows-static
vcpkg install sqlite3:x86-windows-static
```

Для 64 битной целевой платформы:
```
vcpkg install libmicrohttpd:x64-windows-static
vcpkg install sqlite3:x64-windows-static
```

Интегрируйте среду
```
vcpkg integrate install
```

vcpkg по умолчанию устанавливает зависимости для платформа x86.

Для целевой платформы x64, в отличие от x86, нужно вручную включить интеграцию проекта:

В файле проекта ws-sqlite.vcxproj:
```
<PropertyGroup Label="Globals">
    <VcpkgTriplet>x64-windows-static</VcpkgTriplet>
    <VcpkgEnabled>true</VcpkgEnabled>
```

Пакет microhttpd имеет неверное имя (с префиксом lib).
В папке D:\git\vcpkg\installed\x64-windows-static\lib\ 
переименуйте libmicrohttpd.lib  в microhttpd.lib 


Используйте CMake для создания решения Visual Studio:

```
cd lorawan-ws
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=D:/git/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static
```

Запустите созданное решение в Visual Studio, выберите конфигурацию Release и соберите проект ws-sqlite.
