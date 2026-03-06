"""
Lab 8 — Веб-интерфейс для булева поиска.
HTTP-сервер на порту 8080, вызывает ./searcher через subprocess.
"""

import http.server
import socketserver
import subprocess
import urllib.parse
import html as html_mod

PORT = 8080

MAIN_PAGE = """<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Mobile Dev Search</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{
  min-height:100vh;
  font-family:'Segoe UI',system-ui,sans-serif;
  background:#f0f4f8;
  display:flex;align-items:center;justify-content:center;
  padding:24px;
}
.wrap{
  width:100%;max-width:720px;
  background:#fff;
  border-radius:16px;
  box-shadow:0 8px 30px rgba(0,0,0,.08);
  padding:40px 36px 32px;
}
h1{font-size:28px;color:#1e293b;margin-bottom:6px}
.desc{color:#64748b;font-size:14px;margin-bottom:24px}
form{display:flex;gap:10px}
input[type=text]{
  flex:1;padding:12px 16px;
  border:2px solid #e2e8f0;border-radius:10px;
  font-size:15px;color:#334155;outline:none;
  transition:border-color .2s;
}
input[type=text]:focus{border-color:#6366f1}
input[type=text]::placeholder{color:#94a3b8}
button{
  padding:12px 24px;border:none;border-radius:10px;
  background:#6366f1;color:#fff;font-size:15px;
  font-weight:600;cursor:pointer;
  transition:background .15s;
}
button:hover{background:#4f46e5}
.hint{
  margin-top:16px;font-size:12px;color:#94a3b8;
  display:flex;gap:16px;flex-wrap:wrap;
}
.hint code{
  background:#f1f5f9;padding:2px 7px;border-radius:4px;
  font-size:11px;color:#475569;
}
</style>
</head>
<body>
<div class="wrap">
  <h1>Mobile Dev Search</h1>
  <p class="desc">Булев поиск по корпусу Habr + Wikipedia (мобильная разработка)</p>
  <form action="/search" method="get">
    <input type="text" name="q" placeholder="android && kotlin" required autofocus>
    <button type="submit">Найти</button>
  </form>
  <div class="hint">
    <span><code>&&</code> И</span>
    <span><code>||</code> ИЛИ</span>
    <span><code>!</code> НЕ</span>
    <span>Пример: <code>flutter || react !native</code></span>
  </div>
</div>
</body>
</html>"""


def results_page(query, lines):
    items_html = ""
    count = 0
    for line in lines:
        line = line.strip()
        if not line:
            continue
        count += 1
        if "\t" in line:
            title, url = line.split("\t", 1)
        else:
            title, url = line, line
        title = html_mod.escape(title)
        url_safe = html_mod.escape(url)
        items_html += (
            f'<div class="item">'
            f'<a href="{url_safe}" target="_blank">{title}</a>'
            f'<span class="link">{url_safe}</span>'
            f'</div>\n'
        )

    q_esc = html_mod.escape(query)
    return f"""<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Результаты: {q_esc}</title>
<style>
*{{box-sizing:border-box;margin:0;padding:0}}
body{{
  min-height:100vh;
  font-family:'Segoe UI',system-ui,sans-serif;
  background:#f0f4f8;
  padding:32px 24px;
}}
.wrap{{
  max-width:780px;margin:0 auto;
  background:#fff;border-radius:16px;
  box-shadow:0 8px 30px rgba(0,0,0,.08);
  padding:32px 36px 28px;
}}
.top{{display:flex;justify-content:space-between;align-items:center;margin-bottom:8px}}
h2{{font-size:22px;color:#1e293b}}
.back{{
  text-decoration:none;color:#6366f1;font-size:13px;
  padding:6px 14px;border:1px solid #e2e8f0;border-radius:8px;
  transition:background .15s;
}}
.back:hover{{background:#f1f5f9}}
.query{{
  font-family:monospace;font-size:13px;
  background:#f1f5f9;padding:4px 10px;border-radius:6px;
  color:#475569;display:inline-block;margin-bottom:10px;
}}
.stats{{font-size:13px;color:#94a3b8;margin-bottom:18px}}
.items{{border-top:1px solid #e2e8f0;padding-top:14px;max-height:65vh;overflow-y:auto}}
.item{{padding:10px 0;border-bottom:1px solid #f1f5f9}}
.item:last-child{{border-bottom:none}}
.item a{{font-size:15px;color:#4f46e5;text-decoration:none}}
.item a:hover{{text-decoration:underline}}
.link{{display:block;font-size:12px;color:#22c55e;margin-top:2px;word-break:break-all}}
</style>
</head>
<body>
<div class="wrap">
  <div class="top">
    <h2>Результаты поиска</h2>
    <a href="/" class="back">Новый запрос</a>
  </div>
  <div class="query">{q_esc}</div>
  <div class="stats">Найдено документов: {count}</div>
  <div class="items">
    {items_html}
  </div>
</div>
</body>
</html>"""


class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        params = urllib.parse.parse_qs(parsed.query)

        if parsed.path == "/":
            self._reply(200, MAIN_PAGE)
        elif parsed.path == "/search":
            query = params.get("q", [""])[0]
            try:
                proc = subprocess.run(
                    ["./searcher", query],
                    capture_output=True, text=True, timeout=30,
                )
                lines = proc.stdout.split("\n")
            except Exception as exc:
                lines = [f"Ошибка: {exc}"]
            self._reply(200, results_page(query, lines))
        else:
            self._reply(404, "<h1>404</h1>")

    def _reply(self, code, body):
        self.send_response(code)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.end_headers()
        self.wfile.write(body.encode("utf-8"))

    def log_message(self, fmt, *args):
        print(f"  [{self.client_address[0]}] {args[0]}")


def main():
    with socketserver.TCPServer(("", PORT), Handler) as srv:
        print(f"Сервер запущен: http://localhost:{PORT}")
        srv.serve_forever()


if __name__ == "__main__":
    main()
