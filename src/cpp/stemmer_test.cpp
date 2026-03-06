/*
 * Автотест стеммера.
 * Проверяет корректность отсечения суффиксов на наборе слов.
 */

#include <iostream>
#include <string>
#include <vector>
#include <cstring>

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

struct TestCase {
    std::string input;
    std::string expected;
};

int main() {
    std::vector<TestCase> cases = {
        // EN
        {"development",  "develop"},
        {"programming",  "programm"},
        {"connected",    "connect"},
        {"actively",     "active"},
        {"movement",     "move"},
        {"testing",      "test"},
        {"comfortable",  "comfort"},
        {"organization", "organ"},
        {"carefully",    "careful"},
        {"darkness",     "dark"},
        // RU
        {"разработки",   "разработк"},
        {"приложений",   "приложен"},
        {"телефонами",   "телефон"},
        {"интерфейсный", "интерфейсн"},
        {"красивая",     "красив"},
        {"программирование", "программиров"},
        {"серверного",   "сервер"},
        {"системами",    "систем"},
        // короткие слова — не меняются
        {"app",          "app"},
        {"код",          "код"},
    };

    int passed = 0;
    int failed = 0;

    std::cout << "=== Автотест стеммера ===" << std::endl << std::endl;

    for (const auto& tc : cases) {
        std::string result = apply_stemmer(tc.input);
        bool ok = (result == tc.expected);
        if (ok) {
            std::cout << "  OK: \"" << tc.input << "\" -> \"" << result << "\"" << std::endl;
            passed++;
        } else {
            std::cout << "FAIL: \"" << tc.input << "\" -> \""
                      << result << "\" (ожидалось \"" << tc.expected << "\")" << std::endl;
            failed++;
        }
    }

    std::cout << std::endl << "Результат: " << passed << "/" << (passed + failed)
              << " тестов пройдено" << std::endl;

    if (failed > 0) {
        std::cout << "Есть ошибки!" << std::endl;
        return 1;
    }
    std::cout << "Все тесты пройдены." << std::endl;
    return 0;
}
