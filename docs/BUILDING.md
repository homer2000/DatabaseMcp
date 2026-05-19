# Building

## Requirements

- CMake 3.16+
- Qt 5.15.2+ или Qt 6.2+
- C++17 компилятор

## Build Options

| Опция | Описание | По умолчанию |
|-------|----------|--------------|
| `BUILD_TESTS` | Собирать тесты | ON |
| `BUILD_DOCS` | Собирать документацию | ON |
| `ENABLE_DEBUG` | Включить отладку | OFF |

## Linux/macOS

```bash
# С конфигурацией
cmake -B build -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH=/opt/qt6/lib/cmake \
    -DBUILD_TESTS=ON

cmake --build build -j$(nproc)

# Запуск
./build/DatabaseMCPServer
```

## Windows (Visual Studio)

```powershell
# Генерация решения
cmake -B build -S . -G "Visual Studio 17 2022" -A x64

# Сборка
cmake --build build --config Release

# Запуск
.\build\Release\DatabaseMCPServer.exe
```

## macOS (Homebrew Qt)

```bash
# Установка Qt через Homebrew
brew install qt@6

# Сборка
cmake -B build -S . -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6)
cmake --build build
```

## Docker

```dockerfile
FROM qtgitlab/qt6:latest

WORKDIR /app
COPY . .
RUN cmake -B build && cmake --build build

EXPOSE 8080
ENTRYPOINT ["./build/DatabaseMCPServer"]
```