/*
 * Lab 8 — Булев поиск.
 * Загружает documents.bin и inverted.bin, выполняет булевы запросы.
 * Поддерживает && (AND), || (OR), ! (NOT).
 * Режимы: CLI (аргументы) и интерактивный (stdin).
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>
#include <map>
#include <sstream>

static std::map<std::string, std::vector<int>> inverted_idx;
static std::map<int, std::string> id_to_url;
static std::map<int, std::string> id_to_title;
static std::vector<int> every_doc_id;

// ---- Стемминг (идентичный indexer) ----

static bool str_ends_with(const std::string& w, const char* suf, size_t slen) {
    if (w.size() < slen) return false;
    return w.compare(w.size() - slen, slen, suf) == 0;
}

std::string apply_stemmer(const std::string& word) {
    if (word.size() <= 4) return word;

    struct SuffixRule { const char* suf; size_t len; };

    static const SuffixRule rules[] = {
        {"ization", 7}, {"fulness", 7}, {"iveness", 7},
        {"ement",   5}, {"ation",  5},  {"ously",  5},
        {"ness",    4}, {"ment",   4},  {"able",   4},  {"ible",  4},
        {"ting",    3}, {"ing",    3},  {"ous",    3},
        {"ful",     3}, {"ity",    3},
        {"ed",      2}, {"ly",     2},  {"er",     2},  {"es",    2},
        {"al",      2}, {"en",     2},
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

// ---- Загрузка индекса ----

void load_all() {
    std::ifstream df("documents.bin", std::ios::binary);
    if (df) {
        while (df.peek() != EOF) {
            int id;
            size_t url_len, title_len;
            df.read(reinterpret_cast<char*>(&id), sizeof(int));
            if (df.eof()) break;
            df.read(reinterpret_cast<char*>(&url_len), sizeof(size_t));
            std::string url(url_len, '\0');
            df.read(&url[0], url_len);
            df.read(reinterpret_cast<char*>(&title_len), sizeof(size_t));
            std::string title(title_len, '\0');
            df.read(&title[0], title_len);

            id_to_url[id] = url;
            id_to_title[id] = title;
            every_doc_id.push_back(id);
        }
    }
    std::sort(every_doc_id.begin(), every_doc_id.end());

    std::ifstream idx("inverted.bin", std::ios::binary);
    if (idx) {
        while (idx.peek() != EOF) {
            int wlen;
            idx.read(reinterpret_cast<char*>(&wlen), sizeof(int));
            if (idx.eof()) break;
            std::string term(wlen, '\0');
            idx.read(&term[0], wlen);
            int cnt;
            idx.read(reinterpret_cast<char*>(&cnt), sizeof(int));
            std::vector<int> postings(cnt);
            for (int i = 0; i < cnt; ++i)
                idx.read(reinterpret_cast<char*>(&postings[i]), sizeof(int));
            std::sort(postings.begin(), postings.end());
            inverted_idx[term] = postings;
        }
    }
}

// ---- Операции над множествами ----

std::vector<int> intersect(const std::vector<int>& a, const std::vector<int>& b) {
    std::vector<int> r;
    std::set_intersection(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(r));
    return r;
}

std::vector<int> unite(const std::vector<int>& a, const std::vector<int>& b) {
    std::vector<int> r;
    std::set_union(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(r));
    return r;
}

std::vector<int> complement(const std::vector<int>& excluded) {
    std::vector<int> r;
    std::set_difference(every_doc_id.begin(), every_doc_id.end(),
                        excluded.begin(), excluded.end(), std::back_inserter(r));
    return r;
}

// ---- Поиск постинга для терма ----

std::vector<int> lookup_term(const std::string& raw) {
    std::string clean;
    for (char ch : raw) {
        if (ch == '(' || ch == ')' || ch == '!' || ch == '&' || ch == '|' || ch == ' ')
            continue;
        clean += (ch >= 'A' && ch <= 'Z') ? (ch + 32) : ch;
    }
    clean = apply_stemmer(clean);
    auto it = inverted_idx.find(clean);
    if (it != inverted_idx.end()) return it->second;
    return {};
}

// ---- Разбор запроса ----

std::vector<int> run_query(const std::string& query) {
    if (query.find(' ') == std::string::npos &&
        query.find('&') == std::string::npos &&
        query.find('|') == std::string::npos) {
        if (!query.empty() && query[0] == '!') {
            return complement(lookup_term(query.substr(1)));
        }
        return lookup_term(query);
    }

    std::istringstream ss(query);
    std::string tok;
    std::vector<std::string> parts;
    while (ss >> tok) parts.push_back(tok);

    if (parts.empty()) return {};

    std::vector<int> acc;
    if (!parts[0].empty() && parts[0][0] == '!') {
        acc = complement(lookup_term(parts[0].substr(1)));
    } else {
        acc = lookup_term(parts[0]);
    }

    for (size_t i = 1; i < parts.size(); ++i) {
        const std::string& p = parts[i];

        if (p == "&&" || p == "AND") {
            if (i + 1 < parts.size()) {
                auto rhs = lookup_term(parts[++i]);
                acc = intersect(acc, rhs);
            }
        } else if (p == "||" || p == "OR") {
            if (i + 1 < parts.size()) {
                auto rhs = lookup_term(parts[++i]);
                acc = unite(acc, rhs);
            }
        } else if (!p.empty() && p[0] == '!') {
            auto rhs = lookup_term(p.substr(1));
            acc = intersect(acc, complement(rhs));
        } else {
            auto rhs = lookup_term(p);
            acc = intersect(acc, rhs);
        }
    }
    return acc;
}

// ---- Main ----

int main(int argc, char* argv[]) {
    load_all();

    if (argc > 1) {
        std::string q;
        for (int i = 1; i < argc; ++i) {
            if (i > 1) q += " ";
            q += argv[i];
        }
        auto hits = run_query(q);
        for (int id : hits)
            std::cout << id_to_title[id] << "\t" << id_to_url[id] << "\n";
        return 0;
    }

    std::cout << "Индекс загружен. Термов: " << inverted_idx.size()
              << ". Документов: " << id_to_url.size() << "\n";

    std::string line;
    while (true) {
        std::cout << "\nЗапрос (exit для выхода): ";
        if (!std::getline(std::cin, line) || line == "exit") break;

        auto hits = run_query(line);
        std::cout << "Найдено: " << hits.size() << " документов\n";
        int shown = 0;
        for (int id : hits) {
            std::cout << "  [" << id << "] " << id_to_title[id]
                      << "  (" << id_to_url[id] << ")\n";
            if (++shown >= 15) {
                std::cout << "  ... ещё " << (hits.size() - shown) << " результатов\n";
                break;
            }
        }
    }
    return 0;
}
