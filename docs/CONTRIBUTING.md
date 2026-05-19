# Contributing to DatabaseMCPServer

Спасибо за интерес к нашему проекту! Мы рады вашему участию.

## Как начать

1. Форкните репозиторий
2. Создайте ветку для ваших изменений (`git checkout -b feature/amazing-feature`)
3. Закоммитьте изменения (`git commit -m 'Add amazing feature'`)
4. Запушите ветку (`git push origin feature/amazing-feature`)
5. Откройте Pull Request

## Установка

```bash
# Клонируем себе репозиторий
git clone https://github.com/username/DatabaseMcp.git
cd DatabaseMcp

# Создаём виртуальное окружение для тестов
python -m venv .venv
source .venv/bin/activate  # Linux/Mac
.venv\Scripts\activate     # Windows
pip install -r requirements-dev.txt
```

## Стандарты кода

- Соблюдайте стандарт оформления кода: `clang-format`
- Запускаем линтер: `clang-tidy`
- Покрытие тестами: >80%

```bash
# Форматирование кода
clang-format -i include/*.h src/*.cpp

# Линтинг
clang-tidy src/*.cpp -- -std=c++17
```

## Тесты

```bash
# Запускаем все тесты
ctest --test-dir build --output-on-failure

# Конкретный тест
./build/tests/unit/test_query
```

## Соглашения

- Используйте английский язык для кода и комментариев
- Соблюдайте принципы SOLID
- Добавляйте тесты для новых функций
- Обновляйте документацию при изменении API