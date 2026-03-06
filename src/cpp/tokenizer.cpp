/*
 * Lab 3 — Токенизация.
 * Читает raw HTML из SQLite, удаляет HTML-теги,
 * разбивает текст на токены, собирает статистику.
 */

#include <iostream>
#include <string>
#include <vector>
#include <sqlite3.h>
#include <chrono>
#include <cstddef>

static size_t g_token_count   = 0;
static size_t g_total_len     = 0;
static size_t g_clean_bytes   = 0;
static size_t g_docs_done     = 0;

std::string strip_tags(const std::string& html) {
    std::string out;
    out.reserve(html.size());
    bool inside = false;
    for (size_t i = 0; i < html.size(); ++i) {
        char ch = html[i];
        if (ch == '<') {
            inside = true;
            out += ' ';
        } else if (ch == '>') {
            inside = false;
        } else if (!inside) {
            out += ch;
        }
    }
    return out;
}

static inline bool is_delimiter(unsigned char c) {
    if (c <= 32) return true;
    switch (c) {
        case '.': case ',': case '!': case '?':
        case ':': case ';': case '"': case '\'':
        case '(': case ')': case '[': case ']':
        case '{': case '}': case '<': case '>':
        case '/': case '\\': case '|': case '-':
        case '+': case '=': case '#': case '@':
        case '*': case '~': case '`': case '^':
            return true;
    }
    return false;
}

static inline char ascii_lower(char c) {
    if (c >= 'A' && c <= 'Z') return c + 32;
    return c;
}

std::vector<std::string> split_tokens(const std::string& text) {
    std::vector<std::string> result;
    std::string buf;
    for (size_t i = 0; i < text.size(); ++i) {
        unsigned char c = text[i];
        if (is_delimiter(c)) {
            if (!buf.empty()) {
                result.push_back(buf);
                buf.clear();
            }
        } else {
            buf += ascii_lower(static_cast<char>(c));
        }
    }
    if (!buf.empty()) result.push_back(buf);
    return result;
}

static int row_handler(void*, int, char** cols, char**) {
    if (!cols[0]) return 0;

    std::string html(cols[0]);
    std::string clean = strip_tags(html);
    g_clean_bytes += clean.size();

    std::vector<std::string> toks = split_tokens(clean);
    g_token_count += toks.size();
    for (const auto& t : toks)
        g_total_len += t.size();

    ++g_docs_done;
    if (g_docs_done % 2000 == 0)
        std::cout << "  обработано документов: " << g_docs_done << "\r" << std::flush;

    return 0;
}

int main() {
    sqlite3* db = nullptr;
    if (sqlite3_open("crawler_data.db", &db) != SQLITE_OK) {
        std::cerr << "Не удалось открыть БД: " << sqlite3_errmsg(db) << "\n";
        return 1;
    }

    std::cout << "Запуск токенизации...\n";
    auto t0 = std::chrono::steady_clock::now();

    char* err = nullptr;
    int rc = sqlite3_exec(db, "SELECT html_content FROM pages", row_handler, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL ошибка: " << (err ? err : "unknown") << "\n";
        sqlite3_free(err);
    }

    auto t1 = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(t1 - t0).count();

    sqlite3_close(db);

    double kb = g_clean_bytes / 1024.0;
    double avg_len = g_token_count ? (double)g_total_len / g_token_count : 0;

    std::cout << "\n\n===== РЕЗУЛЬТАТЫ ТОКЕНИЗАЦИИ =====\n";
    std::cout << "Документов обработано: " << g_docs_done << "\n";
    std::cout << "Размер чистого текста: " << kb << " КБ\n";
    std::cout << "Всего токенов:         " << g_token_count << "\n";
    std::cout << "Средняя длина токена:  " << avg_len << "\n";
    std::cout << "Время обработки:       " << elapsed << " сек\n";
    std::cout << "Скорость:              " << (elapsed > 0 ? kb / elapsed : 0) << " КБ/сек\n";
    std::cout << "==================================\n";

    return 0;
}
