# ws-sqlite


Библиотека и утилита командной строки- встроенный веб сервер для работы с термокосами.

ws-sqlite - веб сервер HTTP/1.0 

Порт по умолчанию - 5002


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

Примеры:

- wget http://localhost:5002/t?o=0s=10


## Параметры командной строки

## Файлы


html: папка с веб контентом

В репозиторий не входит папка html с веб приложением. Скопируйте эту папку или поставьте линк на эту папку вручную.

## Установка

### Сборка

Необходимые зависимости:

- curl-7.48.0 
- libmicrohttpd-0.9.43 
- sqlite3

Установите libmicrohttpd-0.9.43.tgz (http://ftp.gnu.org/gnu/libmicrohttpd/)

```
tar xvfz libmicrohttpd-0.9.43.tgz
cd libmicrohttpd-0.9.43/
./configure;make;sudo make install;make clean
```

Соберите библиотеку и утилиту командной строки (веб-сервер для проверки функциональности и тестирования html и javascript)
```
./configure;make;sudo make install;make clean
```

SQLite внедрен как header only в дистрибутив.

OpenSSl 1.0.x. Не рекомендуется использовать 1.1.x. 

CURL4

```
sudo apt-get install libcurl4-openssl-dev
```

#### Linux


```
./configure
make
sudo make install
```

#### Windows

Используйте CMake

