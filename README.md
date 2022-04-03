# ws-sqlite


Библиотека и утилита командной строки- встроенный веб сервер для работы с термокосами.

ws-sqlite - веб сервер HTTP/1.0 

Порт по умолчанию - 5002

## Первый запуск

У вас должно иметься приложение ws-sqlite.
Если приложение не собрано, перейдите к разделу Сборка..

Если у вас нет файла базы данных, создайте его, указав имя файла базы данных в опции -d.
Опция -c создает новую базу данных с пустыми таблицами.

```
ws-sqlite -d lora.db -c
```

Откройте в браузере http://localhost:5002/t?o=0&s=2

Можете также использовать wget:
```
wget http://localhost:5002/t?o=0s=10
```


## Параметры командной строки

## Файлы


html: папка с веб контентом

В репозиторий не входит папка html с веб приложением. Скопируйте эту папку или поставьте линк на эту папку вручную.


## JSON API

Используется для получения данных с ошейников в веб интерфейсе.

Команды (пути) и параметры передаюьтся методом GET в виде:

``` 
path?parameter1=value1&parameter2=value2
```

Команды (пути): 	

- /	если задан корневой каталог, будут отдаваться файлы из него (HTML файлы, Javascript, CSS, веб- шрифты)
- /raw полученные пакеты
- /t температуры, измеренные косой

Параметры (используются не все в разных путях)

- o
- s
- id
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


## Сборка

Необходимые зависимости:

- libmicrohttpd-0.9.43 (http://ftp.gnu.org/gnu/libmicrohttpd/)
- sqlite3

### Linux

Установите зависимости:

```
sudo apt install libmicrohttpd
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

