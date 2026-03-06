/*
 * Labs 4+7 — Стемминг + Булев индекс.
 * Строит обратный индекс из HTML-документов в SQLite.
 * Хеш-таблица FNV-1a с цепочками коллизий.
 * Выходные файлы: documents.bin (прямой индекс), inverted.bin (обратный).
 */

#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <fstream>
#include <algorithm>
#include <sqlite3.h>

// ---- Хеш-таблица ----

const int TABLE_SLOTS = 65537;

struct PostingNode {
    int doc_id;
    PostingNode* next;
};

struct TermEntry {
    char* word;
    int posting_count;
    PostingNode* first;
    PostingNode* last;
    TermEntry* chain;
};

static TermEntry* hash_slots[TABLE_SLOTS];

static unsigned long fnv1a(const char* s) {
    unsigned long h = 2166136261u;
    while (*s) {
        h ^= (unsigned char)(*s);
        h *= 16777619u;
        ++s;
    }
    return h % TABLE_SLOTS;
}

// ---- Стемминг (Lab 4) ----

static bool str_ends_with(const std::string& w, const char* suf, size_t slen) {
    if (w.size() < slen) return false;
    return w.compare(w.size() - slen, slen, suf) == 0;
}

std::string apply_stemmer(const std::string& word) {
    if (word.size() <= 4) return word;

    struct SuffixRule { const char* suf; size_t len; };

    static const SuffixRule rules[] = {
        // EN (длинные первыми)
        {"ization", 7}, {"fulness", 7}, {"iveness", 7},
        {"ement",   5}, {"ation",  5},  {"ously",  5},
        {"ness",    4}, {"ment",   4},  {"able",   4},  {"ible",  4},
        {"ting",    3}, {"ing",    3},  {"ous",    3},
        {"ful",     3}, {"ity",    3},
        {"ed",      2}, {"ly",     2},  {"er",     2},  {"es",    2},
        {"al",      2}, {"en",     2},
        // RU (UTF-8: кириллические символы = 2 байта каждый)
        {"ость",    8}, {"ение",   8}, {"ание",   8}, {"ками",  8},
        {"ться",    8}, {"ного",   8},
        {"ого",     6}, {"ему",    6}, {"ями",    6}, {"ами",   6},
        {"ных",     6}, {"ной",    6}, {"ным",    6}, {"ную",   6},
        {"ает",     6}, {"ить",    6}, {"ать",    6}, {"ует",   6},
        {"ая",      4}, {"ый",     4}, {"ий",     4}, {"ое",    4},
        {"ые",      4}, {"ей",     4}, {"ов",     4}, {"ом",    4},
        {"ая",      4},
        {"а",       2}, {"о",      2}, {"ы",      2},
        {"и",       2}, {"е",      2}, {"у",      2},
    };

    for (const auto& r : rules) {
        if (str_ends_with(word, r.suf, r.len)) {
            size_t stem_len = word.size() - r.len;
            if (stem_len >= 4)
                return word.substr(0, stem_len);
        }
    }
    return word;
}

// ---- Извлечение заголовка ----

std::string grab_title(const std::string& html) {
    std::string lo;
    lo.reserve(html.size());
    for (char c : html)
        lo += (c >= 'A' && c <= 'Z') ? (c + 32) : c;

    size_t a = lo.find("<title");
    if (a == std::string::npos) return "";
    size_t b = lo.find('>', a);
    if (b == std::string::npos) return "";
    size_t c = lo.find("</title", b);
    if (c == std::string::npos) return "";

    std::string title = html.substr(b + 1, c - b - 1);
    while (!title.empty() && (unsigned char)title.front() <= 32) title.erase(title.begin());
    while (!title.empty() && (unsigned char)title.back() <= 32) title.pop_back();
    return title;
}

// ---- Индексация ----

static inline bool is_sep(unsigned char c) {
    if (c <= 32) return true;
    return std::strchr(".,!?:;\"'()[]{}/<>\\|-+=@#*~`^", c) != nullptr;
}

void index_document(int doc_id, const std::string& html) {
    std::string token;
    bool in_tag = false;

    for (size_t i = 0; i < html.size(); ++i) {
        char c = html[i];
        if (c == '<') { in_tag = true; continue; }
        if (c == '>') { in_tag = false; continue; }
        if (in_tag) continue;

        if (is_sep((unsigned char)c)) {
            if (!token.empty()) {
                std::string stemmed = apply_stemmer(token);
                const char* key = stemmed.c_str();
                unsigned long slot = fnv1a(key);

                TermEntry* te = hash_slots[slot];
                TermEntry* found = nullptr;
                while (te) {
                    if (std::strcmp(te->word, key) == 0) { found = te; break; }
                    te = te->chain;
                }
                if (!found) {
                    found = new TermEntry;
                    found->word = strdup(key);
                    found->posting_count = 0;
                    found->first = nullptr;
                    found->last  = nullptr;
                    found->chain = hash_slots[slot];
                    hash_slots[slot] = found;
                }
                if (!found->last || found->last->doc_id != doc_id) {
                    PostingNode* pn = new PostingNode;
                    pn->doc_id = doc_id;
                    pn->next = nullptr;
                    if (!found->first) found->first = pn;
                    else found->last->next = pn;
                    found->last = pn;
                    found->posting_count++;
                }
                token.clear();
            }
        } else {
            token += (c >= 'A' && c <= 'Z') ? (c + 32) : c;
        }
    }
}

// ---- Callback SQLite ----

static int on_doc(void* fp, int, char** cols, char**) {
    if (!cols[0] || !cols[1] || !cols[2]) return 0;

    int id = std::stoi(cols[0]);
    std::string url(cols[1]);
    std::string raw(cols[2]);

    auto* out = reinterpret_cast<std::ofstream*>(fp);
    size_t url_len = url.size();
    out->write(reinterpret_cast<char*>(&id), sizeof(int));
    out->write(reinterpret_cast<char*>(&url_len), sizeof(size_t));
    out->write(url.data(), url_len);

    std::string title = grab_title(raw);
    if (title.empty()) title = url;
    size_t title_len = title.size();
    out->write(reinterpret_cast<char*>(&title_len), sizeof(size_t));
    out->write(title.data(), title_len);

    index_document(id, raw);

    if (id % 1000 == 0)
        std::cout << "  индексация: doc " << id << "\r" << std::flush;

    return 0;
}

// ---- Main ----

int main() {
    for (int i = 0; i < TABLE_SLOTS; ++i) hash_slots[i] = nullptr;

    std::cout << "=== Построение индекса ===\n";

    std::ofstream docs_out("documents.bin", std::ios::binary);
    if (!docs_out) { std::cerr << "Ошибка создания documents.bin\n"; return 1; }

    sqlite3* db = nullptr;
    if (sqlite3_open("crawler_data.db", &db) != SQLITE_OK) {
        std::cerr << "Ошибка открытия БД\n"; return 1;
    }

    char* err = nullptr;
    sqlite3_exec(db, "SELECT id, url, html_content FROM pages", on_doc, &docs_out, &err);
    if (err) { std::cerr << err << "\n"; sqlite3_free(err); }

    sqlite3_close(db);
    docs_out.close();

    std::cout << "\nЗапись обратного индекса...\n";
    std::ofstream idx_out("inverted.bin", std::ios::binary);
    int unique_terms = 0;

    for (int s = 0; s < TABLE_SLOTS; ++s) {
        TermEntry* te = hash_slots[s];
        while (te) {
            int wlen = std::strlen(te->word);
            idx_out.write(reinterpret_cast<char*>(&wlen), sizeof(int));
            idx_out.write(te->word, wlen);
            idx_out.write(reinterpret_cast<char*>(&te->posting_count), sizeof(int));

            PostingNode* pn = te->first;
            while (pn) {
                idx_out.write(reinterpret_cast<char*>(&pn->doc_id), sizeof(int));
                pn = pn->next;
            }
            unique_terms++;
            te = te->chain;
        }
    }
    idx_out.close();

    std::cout << "=== Готово ===\n";
    std::cout << "Уникальных термов: " << unique_terms << "\n";
    std::cout << "Файлы: documents.bin, inverted.bin\n";

    return 0;
}
