# InformationSearch

Поисковая система с булевым индексом по корпусу статей о мобильной разработке.
Проект для курса «Информационный поиск», МАИ.

Корпус собирается из двух источников — Habr (хабы по Android, iOS, мобильной разработке)
и русская Wikipedia (категории по мобильному ПО).

## Быстрый старт

```bash
# 1. Установка зависимостей
pip3 install -r requirements.txt

# 2. Сбор корпуса
python3 src/python/crawler.py config.yaml

# 3. Сборка C++ компонентов
g++ src/cpp/tokenizer.cpp -o tokenizer -lsqlite3
g++ src/cpp/freq_counter.cpp -o freq_counter -lsqlite3
g++ src/cpp/indexer.cpp -o indexer -lsqlite3
g++ src/cpp/searcher.cpp -o searcher

# 4. Токенизация (статистика)
./tokenizer

# 5. Закон Ципфа
./freq_counter
python3 src/python/zipf_plot.py

# 6. Построение индекса
./indexer

# 7. Автотест стеммера
g++ src/cpp/stemmer_test.cpp -o stemmer_test
./stemmer_test

# 8. Поиск через CLI
./searcher "android && kotlin"
./searcher "flutter || swift"

# 9. Веб-интерфейс
python3 src/python/web_search.py
# Откройте http://localhost:8080
```

## Структура

- `src/cpp/` — C++ компоненты (токенизатор, счётчик частот, индексатор, поисковик)
- `src/python/` — Python компоненты (краулер, построение графиков, веб-сервер)
- `config.yaml` — конфигурация краулера

## Синтаксис запросов

- `&&` — И (пересечение)
- `||` — ИЛИ (объединение)
- `!` — НЕ (исключение)

Примеры: `android && kotlin`, `ios || swift`, `flutter && dart`
