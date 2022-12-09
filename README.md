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
./ws-sqlite -d ../logger-huffman/db/logger-huffman.db -r ../lorawan-ws-angular/lorawan-ws-angular/dist/lorawan-ws-angular -i local -s "1-2-3" -l 1.log

./ws-sqlite -d ~/dist/logger-huffman.db -r ~/dist/html -p 5002 -i local -s "1-2-3"
```

Опция -l указывает записывать в файл журнала адреса загруженных из веб сервиса данных.

Если задан корневой каталог опцией -r, откройте в браузере адрес http://localhost:5002/

Если корневой каталог не задан, проверьте работу сервиса, открыв в браузере адрес http://localhost:5002/t?o=0&s=2

Можете также использовать wget:
```
wget http://localhost:5002/t?o=0&s=10
```

Запустите веб сервис, указав в опции -i JWT issuer name и в -s пароль:

```
./ws-sqlite -d ../logger-huffman/db/logger-huffman.db -r ../lorawan-ws-angular/lorawan-ws-angular/dist/lorawan-ws-angular -l 1.log -i "local" -s "1-2-3"
```

## Параметры командной строки

- -p, --port=<port>         port number. Default 5002
- -r, --root=<path>         web root path. Default './html'
- -d, --database=<file>     SQLite database file name. Default ../lorawan-network-server/db/logger-huffman.db
- -l, --log=<file>          log file
- -i, --issuer=<JWT-realm>      JWT issuer name
- -s, --secret=<JWT-secret>     JWT secret
- -c, --create-table        force create table in database
- -v, --verbosity           v- error, vv- warning, vvv, vvvv- debug
- -h, --help                Show this help

### Токен безопасности JWT

Если задан параметр -i, --issuer, веб сервис будет авторизовать запрос по токену безопасности JWT 
(при этом желательно задать пароль в опции -s, --secret).

Если -i не хадан, веб сервис не проверяет запросы.

Запросы должны передавать заголовок Authorization: Bearer <JWT>

Токен безопасности JWT в клиенте создается в другом месте (например, вызовом onSpecialPathHandler->handle()) 
алгоритмом HS256.

Параметр --issuer задает имя выпустившего JavaScript web token, параметр --secret - назначенный им пароль.

Эти два параметра нужны для проверки токена базопасности JWT на валидность.

Если токен просрочен, или подпись неверна, веб сервис возвращвет ошибку 401 вместе с заголовком WWW-Authenticate.

В таком случае клент должен сгененрировать новый токен безопасности и передать его в заголовке Authorization: Bearer <JWT>.

## Файлы

html: папка с веб контентом

В репозиторий не входит папка html с веб приложением. Скопируйте эту папку или поставьте линк на эту папку вручную.

Файлы могут быть сжаты GZip и иметь расширение файла '.gz'.

В этом случае веб сервер отдает сжатый файл с заголовком Content-Encoding: gzip по запросу имени файла (без расширения файла .gz).


## JSON API

Используется для получения данных с ошейников в веб интерфейсе методом GET.

Параметры передаются в виде:

``` 
path?parameter1=value1&parameter2=value2
```

где пути (path):

- /	если задан корневой каталог, будут отдаваться файлы из него (HTML файлы, Javascript, CSS, веб- шрифты)
- /raw полученные пакеты
- /raw-count количество полученных пакетов
- /raw-id Полученный пакет. Нужно указать в параметрах его id.
- /t температуры, измеренные косой
- /t-count количество записей температуры, измеренные косой
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

Например, запрос записи температур с идентификатором 4:

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

- -lt меньше
- -le меньше или равно
- -gt больше
- -ge больше или равно
- -like применяет оператор LIKE. Символ подчерка- любой символ. Символ процента (%)- любые символы

Без суффикса параметр задает сравнение на равенство.


## Сборка

Сборка с авторизацией по JWT токену

GNU Autoconf/Automake:
```shell
./autogen.sh
./configure --enable-jwt
make
```

CMake:
```shell
mkdir build
cd build
cmake -DENABLE_JWT=ON ..
make
```

Необходимые зависимости:

- libmicrohttpd-0.9.43 (http://ftp.gnu.org/gnu/libmicrohttpd/)
- sqlite3



Если включен enable-jwt:

- [jwt-cpp](https://github.com/Thalhammer/jwt-cpp) [репозиторий](https://github.com/Thalhammer/jwt-cpp.git)
- [PicoJSON](https://github.com/kazuho/picojson) [репозиторий](https://github.com/kazuho/picojson.git)

Эти header-only библиотеки находятся в папке third_party. 

Для jwt-cpp требуется установка одной из библиотек SSL:

- [OpenSSL](https://www.openssl.org/)
- LibreSSL
- wolfSSL
 
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

##  Флаги

MHD_USE_ERROR_LOG = 1,

Run in debug mode.  If this flag is used, the library should
print error messages and warnings to `stderr`.
MHD_USE_DEBUG = 1,

Run in HTTPS mode.  The modern protocol is called TLS.
MHD_USE_TLS = 2,

Run using one thread per connection.
Must be used only with #MHD_USE_INTERNAL_POLLING_THREAD.
MHD_USE_THREAD_PER_CONNECTION = 4,

Run using an internal thread (or thread pool) for sockets sending
and receiving and data processing. Without this flag MHD will not
run automatically in background thread(s).
If this flag is set, #MHD_run() and #MHD_run_from_select() couldn't
be used.
This flag is set explicitly by #MHD_USE_POLL_INTERNAL_THREAD and
by #MHD_USE_EPOLL_INTERNAL_THREAD.
When this flag is not set, MHD run in "external" polling mode.
MHD_USE_INTERNAL_POLLING_THREAD = 8,

Run using the IPv6 protocol (otherwise, MHD will just support
IPv4).  If you want MHD to support IPv4 and IPv6 using a single
socket, pass #MHD_USE_DUAL_STACK, otherwise, if you only pass
this option, MHD will try to bind to IPv6-only (resulting in
no IPv4 support).
MHD_USE_IPv6 = 16,

Be pedantic about the protocol (as opposed to as tolerant as
possible).  Specifically, at the moment, this flag causes MHD to
reject HTTP 1.1 connections without a "Host" header.  This is
required by the standard, but of course in violation of the "be
as liberal as possible in what you accept" norm.  It is
recommended to turn this ON if you are testing clients against
MHD, and OFF in production.
MHD_USE_PEDANTIC_CHECKS = 32,

Use `poll()` instead of `select()` for polling sockets.
This allows sockets with `fd >= FD_SETSIZE`.
This option is not compatible with an "external" polling mode
(as there is no API to get the file descriptors for the external
poll() from MHD) and must also not be used in combination
with #MHD_USE_EPOLL.
MHD_USE_POLL = 64,

Run using an internal thread (or thread pool) doing `poll()`.
MHD_USE_POLL_INTERNAL_THREAD = MHD_USE_POLL | MHD_USE_INTERNAL_POLLING_THREAD,

Suppress (automatically) adding the 'Date:' header to HTTP responses.
This option should ONLY be used on systems that do not have a clock
and that DO provide other mechanisms for cache control.  See also
RFC 2616, section 14.18 (exception 3).
MHD_USE_SUPPRESS_DATE_NO_CLOCK = 128,

Run without a listen socket.  This option only makes sense if
MHD_add_connection is to be used exclusively to connect HTTP
clients to the HTTP server.  This option is incompatible with
using a thread pool; if it is used, #MHD_OPTION_THREAD_POOL_SIZE
is ignored.
MHD_USE_NO_LISTEN_SOCKET = 256,

Use `epoll()` instead of `select()` or `poll()` for the event loop.
This option is only available on some systems; using the option on
systems without epoll will cause #MHD_start_daemon to fail.  Using
this option is not supported with #MHD_USE_THREAD_PER_CONNECTION.
MHD_USE_EPOLL = 512,

Run using an internal thread (or thread pool) doing `epoll` polling.
This option is only available on certain platforms; using the option on
platform without `epoll` support will cause #MHD_start_daemon to fail.
MHD_USE_EPOLL_INTERNAL_THREAD = MHD_USE_EPOLL  | MHD_USE_INTERNAL_POLLING_THREAD,

Use inter-thread communication channel.  #MHD_USE_ITC can be used with #MHD_USE_INTERNAL_POLLING_THREAD
and is ignored with any "external" sockets polling. It's required for use of #MHD_quiesce_daemon
or #MHD_add_connection. This option is enforced by #MHD_ALLOW_SUSPEND_RESUME or MHD_USE_NO_LISTEN_SOCKET. MHD_USE_ITC is always used automatically on platforms
where select()/poll()/other ignore shutdown of listen socket.
MHD_USE_ITC = 1024

Use a single socket for IPv4 and IPv6.
MHD_USE_DUAL_STACK = MHD_USE_IPv6 | 2048,

Enable `turbo`.  Disables certain calls to `shutdown()`, enables aggressive non-blocking optimistic reads and
other potentially unsafe optimizations. Most effects only happen with #MHD_USE_EPOLL.
MHD_USE_TURBO = 4096,

Enable suspend/resume functions, which also implies setting up ITC to signal resume.
MHD_ALLOW_SUSPEND_RESUME = 8192 | MHD_USE_ITC,

Enable TCP_FASTOPEN option.  This option is only available on Linux with a kernel >= 3.6.  On other systems, using this option cases #MHD_start_daemon to fail.
MHD_USE_TCP_FASTOPEN = 16384

You need to set this option if you want to use HTTP "Upgrade". "Upgrade" may require usage of additional internal resources,
which we do not want to use unless necessary.
MHD_ALLOW_UPGRADE = 32768

Automatically use best available polling function.
MHD_USE_AUTO = 65536

MHD_USE_AUTO_INTERNAL_THREAD = MHD_USE_AUTO | MHD_USE_INTERNAL_POLLING_THREAD,

Flag set to enable post-handshake client authentication only useful in combination with #MHD_USE_TLS
MHD_USE_POST_HANDSHAKE_AUTH_SUPPORT = 1U << 17

Flag set to enable TLS 1.3 early data.  This has
MHD_USE_INSECURE_TLS_EARLY_DATA = 1U << 18
