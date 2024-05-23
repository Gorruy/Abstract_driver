# Abstract_driver
Абстрактный драйвер представляет из себя загружаемый модуль ядра, реализующий интерфейс в виде системных вызовов read, write, lseek, mmap и sysfs для виртуальной памяти из пространства ядра, для каждого устройства выделяется 4096 байт памяти (страница), помимо системных вызовов, работающих стандартным образом, доступ к памяти устройства осуществляется через атрибуты sysfs: abs_value и abs_address, в начале работы оба значения проинициализированы нулями, для смены адреса записи в память необходимо записать значение меньше 4096 в abs_address, запись значения (размером в один байт) в abs_value приведет к его записи в виртуальную память устройства по адресу abs_address

## Build
следующая команда в корне проекта создаст в папке src два объектных файла: abs.ko и abs_test_devices.ko
```sh
cd src & make
```
## Install

В папке src необходимо ввести следующие команды в любом порядке:
```sh
sudo insmod abs.ko
sudo insmod abs_test_devices.ko
```
Данные команды загрузят в ядро модуль драйвера и 4 тестовых устройства
## Remove
Для удаления устройств и драйвера необходимо ввести следующие команды:
```sh
sudo rmmod abs.ko
sudo rmmod abs_test_devices.ko
```
Нельзя удалять и заного загружать драйвер без удаления устройств
## Usage
С помощью следующих команд можно быстро проверит работу abs_value
```sh
sudo echo 10 | sudo tee -a /sys/devices/platform/abs_platform_device.0/abs_value
sudo cat /sys/devices/platform/abs_platform_device.0/abs_value
sudo echo 10 | sudo tee -a /sys/devices/platform/abs_platform_device.0/abs_address
sudo cat /sys/devices/platform/abs_platform_device.0/abs_value
```
Первая запишет в нулевой адрес значение 10, вторая его прочитает, третья перепишет значение адреса и четвертая прочитает из него.

Проверить системные вызовы можно с помощью файла test.c в папке tests:
```sh
make & sudo ./test
```
Данная программа проверяет работоспособность devfs записывая в файл, читая из него, меняя смещение в файле и отображая память

Системные вызовы (open, write, read, mmap, lseek) применяются к файлам устройств в devfs: /dev/abs_dev-0, /dev/abs_dev-1, /dev/abs_dev-2 и /dev/abs_dev-3

