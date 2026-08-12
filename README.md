# Iec104Client
Репозиторий основан на проекте
https://github.com/riclolsen/qtester104
и представляет собой доработанный интерфейс для удобной визуализации подключения с поддержкой подключения к двум серверам МЭК-104

Для сборки как в статической так и в динамической сборке на одном ПК необходимо:
Добавить в переменные среды пользователя переменную 
QT_ROOT_STATIC
и 
QT_ROOT_DYNAMIC
ведущие к корневому каталогу с QT
Пример: 
QT_ROOT_STATIC "C:\Qt\Qt-5.15.18-static"
QT_ROOT_DYNAMIC "C:\Qt\Qt-5.15.18-dynamic"

Сборка поддерживается как на VS вплоть до VS2026 так и на Ninja

1. Через Visual Studio 2019-2026
Открыть папку/CMakeFile в VS и собрать.

2. Через командную строку из каталога проекта
mkdir build
cd build
cmake ..
cmake --build . --config Release 
или
cmake --build . --config Debug