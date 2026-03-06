/*
 * Lab 5 — Закон Ципфа (подсчёт частот термов).
 * Считает частоту каждого терма в корпусе, сортирует по убыванию,
 * записывает результат в freq_data.csv.
 */

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <algorithm>
#include <sqlite3.h>

static std::map<std::string, int> freq_table;

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

static int count_row(void*, int, char** cols, char**) {
    if (!cols[0]) return 0;
    std::string text = strip_tags(std::string(cols[0]));

    std::string buf;
    for (size_t i = 0; i < text.size(); ++i) {
        unsigned char c = text[i];
        if (is_delimiter(c)) {
            if (!buf.empty()) {
                freq_table[buf]++;
                buf.clear();
            }
        } else {
            buf += ascii_lower(static_cast<char>(c));
        }
    }
    if (!buf.empty()) freq_table[buf]++;
    return 0;
}

int main() {
    sqlite3* db = nullptr;
    if (sqlite3_open("crawler_data.db", &db) != SQLITE_OK) {
        std::cerr << "Ошибка открытия БД\n";
        return 1;
    }

    std::cout << "Подсчёт частот термов...\n";

    char* err = nullptr;
    sqlite3_exec(db, "SELECT html_content FROM pages LIMIT 10000", count_row, nullptr, &err);
    if (err) { std::cerr << err << "\n"; sqlite3_free(err); }
    sqlite3_close(db);

    std::vector<std::pair<std::string, int>> sorted_terms(freq_table.begin(), freq_table.end());
    std::sort(sorted_terms.begin(), sorted_terms.end(),
        [](const std::pair<std::string, int>& a, const std::pair<std::string, int>& b) {
            return a.second > b.second;
        }
    );

    std::ofstream csv("freq_data.csv");
    csv << "rank,frequency,term\n";
    for (size_t r = 0; r < sorted_terms.size(); ++r) {
        csv << (r + 1) << "," << sorted_terms[r].second << "," << sorted_terms[r].first << "\n";
    }
    csv.close();

    std::cout << "Готово. Уникальных термов: " << sorted_terms.size() << "\n";
    if (!sorted_terms.empty()) {
        std::cout << "Самый частый: \"" << sorted_terms[0].first
                  << "\" (" << sorted_terms[0].second << " раз)\n";
    }
    std::cout << "Результат сохранён в freq_data.csv\n";

    return 0;
}
