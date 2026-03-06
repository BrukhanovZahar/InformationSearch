"""
Поисковый робот (Labs 1-2).
Обходит Habr (мобильная разработка) и русскую Wikipedia (мобильное ПО),
сохраняет сырой HTML в SQLite. Поддерживает возобновление работы.
"""

import time
import sqlite3
import sys
import signal
import yaml
import requests
from urllib.parse import urljoin, urlparse, urlunparse
from bs4 import BeautifulSoup

SKIP_EXTENSIONS = (
    ".jpg", ".jpeg", ".png", ".gif", ".svg", ".webp",
    ".pdf", ".zip", ".tar", ".gz", ".mp3", ".mp4",
)

SKIP_PATTERNS = (
    "Special:", "Служебная:", "Обсуждение:", "Участник:",
    "Файл:", "Шаблон:", "Категория:Все_", "action=edit",
    "action=history", "oldid=", "diff=",
)

running = True


def handle_signal(signum, frame):
    global running
    print("\nОстановка краулера (завершаем текущую страницу)...")
    running = False


signal.signal(signal.SIGINT, handle_signal)


def load_config(path):
    with open(path, "r", encoding="utf-8") as fh:
        return yaml.safe_load(fh)


def normalize_url(raw_url):
    parsed = urlparse(raw_url)
    clean = urlunparse((
        parsed.scheme,
        parsed.netloc.lower(),
        parsed.path.rstrip("/"),
        "",  # params
        "",  # query (drop for wiki/habr)
        "",  # fragment
    ))
    return clean


def url_allowed(url, domains):
    host = urlparse(url).netloc.lower()
    return any(d in host for d in domains)


def url_rejected(url):
    low = url.lower()
    if any(low.endswith(ext) for ext in SKIP_EXTENSIONS):
        return True
    if any(pat in url for pat in SKIP_PATTERNS):
        return True
    return False


def detect_source(url):
    if "habr.com" in url:
        return "habr"
    if "wikipedia.org" in url:
        return "wikipedia"
    return "other"


# ---- База данных ----

def open_db(db_path):
    conn = sqlite3.connect(db_path, timeout=30)
    conn.execute("PRAGMA journal_mode=WAL")
    conn.execute(
        "CREATE TABLE IF NOT EXISTS pages ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  url TEXT UNIQUE,"
        "  source_name TEXT,"
        "  html_content BLOB,"
        "  fetched_at REAL"
        ")"
    )
    conn.execute(
        "CREATE TABLE IF NOT EXISTS crawl_queue ("
        "  url TEXT PRIMARY KEY,"
        "  state TEXT DEFAULT 'waiting'"
        ")"
    )
    conn.commit()
    return conn


def seed_queue(conn, seed_urls):
    cur = conn.cursor()
    cur.execute("SELECT count(*) FROM crawl_queue")
    if cur.fetchone()[0] == 0:
        for url in seed_urls:
            cur.execute(
                "INSERT OR IGNORE INTO crawl_queue (url, state) VALUES (?, 'waiting')",
                (url,),
            )
        conn.commit()


def next_url(conn):
    cur = conn.cursor()
    cur.execute("SELECT url FROM crawl_queue WHERE state='waiting' LIMIT 1")
    row = cur.fetchone()
    if row is None:
        return None
    url = row[0]
    cur.execute("UPDATE crawl_queue SET state='active' WHERE url=?", (url,))
    conn.commit()
    return url


def finish_url(conn, url):
    conn.execute("UPDATE crawl_queue SET state='done' WHERE url=?", (url,))
    conn.commit()


def save_page(conn, url, source, html_bytes):
    conn.execute(
        "INSERT OR IGNORE INTO pages (url, source_name, html_content, fetched_at) "
        "VALUES (?, ?, ?, ?)",
        (url, source, html_bytes, time.time()),
    )
    conn.commit()


def enqueue_links(conn, links, domains):
    cur = conn.cursor()
    for link in links:
        link = normalize_url(link)
        if url_allowed(link, domains) and not url_rejected(link):
            cur.execute(
                "INSERT OR IGNORE INTO crawl_queue (url, state) VALUES (?, 'waiting')",
                (link,),
            )
    conn.commit()


def pages_count(conn):
    cur = conn.cursor()
    cur.execute("SELECT count(*) FROM pages")
    return cur.fetchone()[0]


# ---- Обкачка ----

def fetch_page(url):
    headers = {
        "User-Agent": "MobileDevSearchBot/1.0 (university lab project)",
        "Accept-Language": "ru-RU,ru;q=0.9,en;q=0.5",
    }
    resp = requests.get(url, headers=headers, timeout=10)
    ctype = resp.headers.get("Content-Type", "")
    if resp.status_code == 200 and "text/html" in ctype:
        return resp.content
    return None


def extract_links(html_bytes, base_url):
    try:
        soup = BeautifulSoup(html_bytes, "html.parser")
    except Exception:
        return []
    found = []
    for tag in soup.find_all("a", href=True):
        href = tag["href"]
        full = urljoin(base_url, href).split("#")[0]
        found.append(full)
    return found


# ---- Статистика (Lab 1) ----

def corpus_stats(conn):
    cur = conn.cursor()
    cur.execute("SELECT count(*) FROM pages")
    total_docs = cur.fetchone()[0]
    if total_docs == 0:
        print("Корпус пуст.")
        return

    cur.execute("SELECT html_content FROM pages")
    total_raw = 0
    total_text = 0
    doc_count = 0

    for (html_blob,) in cur:
        if html_blob is None:
            continue
        raw_size = len(html_blob)
        total_raw += raw_size

        try:
            soup = BeautifulSoup(html_blob, "html.parser")
            for junk in soup(["script", "style", "nav", "footer", "header", "aside", "noscript"]):
                junk.decompose()
            clean = " ".join(soup.get_text(separator=" ").split())
            total_text += len(clean.encode("utf-8"))
        except Exception:
            pass

        doc_count += 1

    avg_raw = total_raw / doc_count if doc_count else 0
    avg_text = total_text / doc_count if doc_count else 0
    ratio = (total_text / total_raw * 100) if total_raw else 0

    print("\n===== СТАТИСТИКА КОРПУСА =====")
    print(f"Документов:                {doc_count}")
    print(f"Общий размер сырых данных: {total_raw / (1024*1024):.2f} МБ")
    print(f"Общий размер чистого текста: {total_text / (1024*1024):.2f} МБ")
    print(f"Средний размер документа (сырой): {avg_raw / 1024:.1f} КБ")
    print(f"Средний объём текста в документе: {avg_text / 1024:.1f} КБ")
    print(f"Доля полезного текста:     {ratio:.2f}%")
    print("==============================\n")


# ---- Главный цикл ----

def crawl(config_path):
    cfg = load_config(config_path)
    db_path = cfg["db"]["path"]
    delay = cfg["logic"]["request_delay"]
    max_docs = cfg["logic"]["max_docs"]
    domains = cfg["logic"]["allowed_domains"]
    seeds = cfg["logic"]["seed_urls"]

    conn = open_db(db_path)
    seed_queue(conn, seeds)

    saved = pages_count(conn)
    print(f"Краулер запущен. Уже в базе: {saved} документов. Цель: {max_docs}.")

    while running:
        if pages_count(conn) >= max_docs:
            print(f"Достигнут лимит {max_docs} документов.")
            break

        url = next_url(conn)
        if url is None:
            print("Очередь пуста — обход завершён.")
            break

        html = fetch_page(url)
        if html is not None:
            save_page(conn, url, detect_source(url), html)
            links = extract_links(html, url)
            enqueue_links(conn, links, domains)
            count = pages_count(conn)
            if count % 50 == 0:
                print(f"[{count}] {url[:90]}")
        else:
            pass  # страница не HTML или ошибка — пропускаем

        finish_url(conn, url)
        time.sleep(delay)

    corpus_stats(conn)
    conn.close()
    print("Краулер завершён.")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Использование: python3 crawler.py <путь_к_config.yaml>")
        sys.exit(1)
    crawl(sys.argv[1])
