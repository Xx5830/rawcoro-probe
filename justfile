set shell := ["sh", "-c"]
set windows-shell := ["cmd.exe", "/c"]

#Help
default:
    @just --list --unsorted

#Очистка build
clear:
    cmake -E rm -rf build

#Конфигурация cmake
setup preset="release":
    cmake --preset {{preset}}

#Сборка проекта
build preset="release": (setup {{preset}})
    cmake --build --preset {{preset}}

#Запуск тестов
test preset="debug": (build preset)
    ctest --preset {{preset}}

#Группируем проект в файл
token :
    npx repomix