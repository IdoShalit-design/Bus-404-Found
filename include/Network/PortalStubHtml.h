#ifndef PORTAL_STUB_HTML_H
#define PORTAL_STUB_HTML_H

static const char kPortalStubHtml[] = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Bus-404-Found Portal</title>
  <style>
    :root {
      color-scheme: light;
      --bg1: #f5f7ff;
      --bg2: #e8f3ff;
      --card: #ffffff;
      --text: #1a2333;
      --muted: #5d6b82;
      --accent: #0b6cff;
      --border: #d9e4ff;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      min-height: 100vh;
      font-family: "Segoe UI", "Noto Sans", sans-serif;
      color: var(--text);
      background: radial-gradient(circle at top right, var(--bg2), var(--bg1));
      display: grid;
      place-items: center;
      padding: 16px;
    }
    .card {
      width: min(560px, 100%);
      background: var(--card);
      border: 1px solid var(--border);
      border-radius: 18px;
      padding: 24px;
      box-shadow: 0 12px 32px rgba(11, 108, 255, 0.08);
    }
    h1 {
      margin: 0 0 10px;
      font-size: 1.8rem;
      line-height: 1.25;
    }
    p {
      margin: 0;
      color: var(--muted);
      line-height: 1.6;
    }
    .badge {
      display: inline-block;
      margin-top: 16px;
      background: rgba(11, 108, 255, 0.1);
      color: var(--accent);
      border: 1px solid rgba(11, 108, 255, 0.2);
      padding: 8px 12px;
      border-radius: 999px;
      font-size: 0.9rem;
    }
  </style>
</head>
<body>
  <main class="card">
    <h1>Bus-404-Found Captive Portal</h1>
    <p>This is a temporary stub page served by PortalManager. You can replace this content later with your configuration UI.</p>
    <div class="badge">Portal Active</div>
  </main>
</body>
</html>
)HTML";

#endif // PORTAL_STUB_HTML_H
