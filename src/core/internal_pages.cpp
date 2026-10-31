#include "internal_pages.hpp"

#include "fs_utils.hpp"

#include <cstdio>

namespace litebrowser {

namespace {

std::string commonPageHead(const std::string& title) {
    return "<!DOCTYPE html>\n<html lang=\"ar\" dir=\"rtl\">\n<head>\n"
           "<meta charset=\"UTF-8\"/>\n"
           "<meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\"/>\n"
           "<title>" + fs::htmlEscape(title) + "</title>\n"
           "<style>\n"
           "* { box-sizing: border-box; }\n"
           "body { margin: 0; font-family: Tahoma, 'Segoe UI', 'Noto Sans Arabic', Arial, sans-serif; "
           "background: #f1f5f9; color: #0f172a; line-height: 1.65; }\n"
           ".top-nav { background: #0f172a; color: #fff; padding: 12px 22px; display: flex; flex-wrap: wrap; "
           "align-items: center; justify-content: space-between; gap: 10px; }\n"
           ".brand { font-size: 18px; font-weight: bold; color: #38bdf8; text-decoration: none; }\n"
           ".nav-links a { color: #cbd5e1; text-decoration: none; margin-left: 14px; font-size: 13px; padding: 5px 9px; border-radius: 5px; }\n"
           ".nav-links a:hover { background: #1e293b; color: #fff; }\n"
           ".wrap { max-width: 980px; margin: 24px auto; padding: 0 16px 40px 16px; }\n"
           ".hero { background: #ffffff; border-radius: 12px; padding: 28px 24px; text-align: center; "
           "box-shadow: 0 2px 10px rgba(15,23,42,0.06); margin-bottom: 20px; border: 1px solid #e2e8f0; }\n"
           ".hero h1 { margin: 0 0 8px 0; font-size: 24px; color: #0f172a; }\n"
           ".hero p { margin: 0 0 18px 0; color: #475569; font-size: 14px; }\n"
           ".search-row { display: flex; max-width: 660px; margin: 0 auto 12px auto; gap: 8px; flex-wrap: wrap; }\n"
           ".search-inp { flex: 1; min-width: 220px; padding: 11px 14px; font-size: 15px; border: 2px solid #cbd5e1; "
           "border-radius: 8px; font-family: inherit; outline: none; }\n"
           ".search-inp:focus { border-color: #0284c7; }\n"
           ".btn { background: #0284c7; color: #fff; border: none; border-radius: 8px; padding: 10px 18px; "
           "font-size: 14px; font-weight: bold; cursor: pointer; text-decoration: none; display: inline-block; font-family: inherit; }\n"
           ".btn:hover { background: #0369a1; }\n"
           ".btn-sec { background: #334155; }\n"
           ".btn-sec:hover { background: #1e293b; }\n"
           ".btn-green { background: #059669; }\n"
           ".btn-green:hover { background: #047857; }\n"
           ".btn-amber { background: #d97706; }\n"
           ".grid { display: flex; flex-wrap: wrap; gap: 14px; margin-top: 16px; }\n"
           ".card { background: #fff; border: 1px solid #e2e8f0; border-radius: 10px; padding: 18px; "
           "width: calc(33.333% - 10px); min-width: 240px; flex-grow: 1; box-shadow: 0 1px 4px rgba(15,23,42,0.04); }\n"
           ".card h3 { margin: 0 0 8px 0; font-size: 16px; color: #0f172a; }\n"
           ".card p { margin: 0 0 12px 0; font-size: 13px; color: #475569; }\n"
           ".quick-links a { display: inline-block; margin: 3px 0 3px 6px; padding: 5px 10px; background: #f1f5f9; "
           "color: #0f172a; border-radius: 6px; text-decoration: none; font-size: 12px; border: 1px solid #cbd5e1; }\n"
           ".quick-links a:hover { background: #e0f2fe; border-color: #7dd3fc; }\n"
           ".status-strip { display: flex; flex-wrap: wrap; justify-content: space-between; align-items: center; "
           "background: #e0f2fe; border: 1px solid #bae6fd; border-radius: 8px; padding: 10px 16px; margin-bottom: 18px; font-size: 13px; }\n"
           "table.list-tbl { width: 100%; border-collapse: collapse; background: #fff; border-radius: 8px; overflow: hidden; border: 1px solid #e2e8f0; }\n"
           "table.list-tbl th, table.list-tbl td { padding: 10px 12px; border-bottom: 1px solid #e2e8f0; text-align: right; font-size: 13px; }\n"
           "table.list-tbl th { background: #f8fafc; color: #334155; }\n"
           ".prog-bg { width: 140px; height: 10px; background: #e2e8f0; border-radius: 5px; overflow: hidden; display: inline-block; vertical-align: middle; }\n"
           ".prog-fill { height: 100%; background: #0284c7; }\n"
           "</style>\n</head>\n<body>\n"
           "<div class=\"top-nav\">\n"
           "  <a class=\"brand\" href=\"lite://home\">متصفح النور الخفيف (LiteBrowser)</a>\n"
           "  <div class=\"nav-links\">\n"
           "    <a href=\"lite://home\">الرئيسية</a>\n"
           "    <a href=\"lite://video\">الفيديو</a>\n"
           "    <a href=\"lite://games\">الألعاب</a>\n"
           "    <a href=\"lite-action://open-file\">فتح PDF / Word</a>\n"
           "    <a href=\"lite://downloads\">التحميلات</a>\n"
           "    <a href=\"lite://bookmarks\">المفضلة</a>\n"
           "    <a href=\"lite://history\">السجل</a>\n"
           "  </div>\n"
           "</div>\n";
}

std::string buildHomeHtml(StorageManager& storage) {
    BrowserSettings s = storage.getSettings();
    auto bookmarks = storage.getBookmarks();

    std::string html = commonPageHead("الصفحة الرئيسية - متصفح خفيف");
    html += "<div class=\"wrap\">\n";

    html += "<div class=\"status-strip\">\n"
            "  <div><strong>حالة الأداء:</strong> " +
            std::string(s.liteMode ? "الوضع الخفيف مفعّل (مثالي لـ Windows XP / 7 والأجهزة الضعيفة)" : "الوضع القياسي الكامل (فيديو HD + ألعاب WebGL)") +
            " &nbsp;|&nbsp; <strong>مانع الإعلانات:</strong> " +
            std::string(s.adBlockEnabled ? "نشط" : "متوقف") +
            "</div>\n"
            "  <div>\n"
            "    <a class=\"btn btn-sec\" style=\"padding:5px 11px;font-size:12px;\" href=\"lite-action://toggle-lite\">" +
            std::string(s.liteMode ? "التبديل للوضع الكامل" : "تفعيل الوضع الخفيف") + "</a>\n"
            "    <a class=\"btn btn-sec\" style=\"padding:5px 11px;font-size:12px;\" href=\"lite-action://toggle-adblock\">" +
            std::string(s.adBlockEnabled ? "إيقاف مانع الإعلانات" : "تفعيل مانع الإعلانات") + "</a>\n"
            "  </div>\n"
            "</div>\n";

    html += "<div class=\"hero\">\n"
            "  <h1>ابحث وتصفح بسرعة وخفة على جميع الأنظمة</h1>\n"
            "  <p>متصفح C++ أصلي خفيف يدعم البحث، مشاهدة الفيديوهات، تشغيل الألعاب، تحميل البرامج، وقراءة ملفات PDF و Word مباشرة</p>\n"
            "  <form onsubmit=\"return doWebSearch();\" class=\"search-row\">\n"
            "    <input id=\"mainQ\" class=\"search-inp\" type=\"text\" placeholder=\"ابحث في الإنترنت أو اكتب رابط موقع...\" autofocus/>\n"
            "    <button type=\"submit\" class=\"btn\">بحث الآن</button>\n"
            "    <button type=\"button\" class=\"btn btn-sec\" onclick=\"doLiteSearch()\">بحث خفيف سريع</button>\n"
            "    <button type=\"button\" class=\"btn btn-amber\" onclick=\"doVideoSearch()\">بحث فيديو</button>\n"
            "  </form>\n"
            "  <div class=\"quick-links\">\n";
    for (const auto& bm : bookmarks) {
        html += "    <a href=\"" + fs::htmlEscape(bm.url) + "\">" + fs::htmlEscape(bm.title) + "</a>\n";
    }
    html += "  </div>\n</div>\n";

    html += "<div class=\"grid\">\n"
            "  <div class=\"card\">\n"
            "    <h3>قارئ ملفات PDF و Word المدمج</h3>\n"
            "    <p>افتح واقرأ ملفات <code>.pdf</code> و <code>.docx</code> و <code>.doc</code> و <code>.rtf</code> مباشرة داخل تبويب المتصفح بدون برامج إضافية.</p>\n"
            "    <a class=\"btn btn-green\" href=\"lite-action://open-file\">اختيار ملف من الجهاز</a>\n"
            "  </div>\n"
            "  <div class=\"card\">\n"
            "    <h3>مشاهدة الفيديوهات</h3>\n"
            "    <p>شاهد يوتيوب ومنصات الفيديو، أو استخدم المشغّل المدمج الخفيف لتشغيل روابط الفيديو المباشرة بأقل استهلاك للذاكرة.</p>\n"
            "    <div class=\"quick-links\">\n"
            "      <a href=\"https://www.youtube.com\">YouTube</a>\n"
            "      <a href=\"https://www.dailymotion.com\">Dailymotion</a>\n"
            "      <a href=\"https://archive.org/details/movies\">Archive Video</a>\n"
            "      <a href=\"lite://video\">المشغّل الخفيف</a>\n"
            "    </div>\n"
            "  </div>\n"
            "  <div class=\"card\">\n"
            "    <h3>تشغيل الألعاب</h3>\n"
            "    <p>العب 3 ألعاب مدمجة تعمل فوراً حتى بدون إنترنت، أو افتح منصات ألعاب الويب الخفيفة HTML5 و WebGL.</p>\n"
            "    <a class=\"btn\" href=\"lite://games\">فتح مركز الألعاب</a>\n"
            "  </div>\n"
            "  <div class=\"card\">\n"
            "    <h3>تحميل البرامج والملفات</h3>\n"
            "    <p>مدير تحميلات مدمج لتحميل البرامج (<code>.exe</code>, <code>.zip</code>, <code>.deb</code>) والمستندات مع متابعة التقدم وفتح المجلد.</p>\n"
            "    <a class=\"btn btn-sec\" href=\"lite://downloads\">مدير التحميلات</a>\n"
            "  </div>\n"
            "  <div class=\"card\">\n"
            "    <h3>البحث السريع والخفيف</h3>\n"
            "    <p>محركات بحث متعددة تشمل البحث القياسي والبحث النصي فائق السرعة للأجهزة القديمة والاتصال الضعيف.</p>\n"
            "    <div class=\"quick-links\">\n"
            "      <a href=\"https://www.google.com\">Google</a>\n"
            "      <a href=\"https://html.duckduckgo.com/html/\">DuckDuckGo Lite</a>\n"
            "      <a href=\"https://www.bing.com\">Bing</a>\n"
            "    </div>\n"
            "  </div>\n"
            "  <div class=\"card\">\n"
            "    <h3>المفضلة وسجل التصفح</h3>\n"
            "    <p>احفظ مواقعك المفضلة للوصول السريع وتحكّم بسجل التصفح بسهولة تامة.</p>\n"
            "    <div class=\"quick-links\">\n"
            "      <a href=\"lite://bookmarks\">إدارة المفضلة</a>\n"
            "      <a href=\"lite://history\">سجل التصفح</a>\n"
            "    </div>\n"
            "  </div>\n"
            "</div>\n</div>\n"
            "<script>\n"
            "function doWebSearch() {\n"
            "  var q = document.getElementById('mainQ').value;\n"
            "  if (!q) return false;\n"
            "  if (q.indexOf('http://') === 0 || q.indexOf('https://') === 0) { window.location.href = q; return false; }\n"
            "  window.location.href = " +
            std::string(s.liteMode ? "'https://html.duckduckgo.com/html/?q=' + encodeURIComponent(q)"
                                   : "'https://www.google.com/search?q=' + encodeURIComponent(q)") + ";\n"
            "  return false;\n"
            "}\n"
            "function doLiteSearch() {\n"
            "  var q = document.getElementById('mainQ').value;\n"
            "  if (!q) return;\n"
            "  window.location.href = 'https://html.duckduckgo.com/html/?q=' + encodeURIComponent(q);\n"
            "}\n"
            "function doVideoSearch() {\n"
            "  var q = document.getElementById('mainQ').value;\n"
            "  if (!q) { window.location.href = 'https://www.youtube.com'; return; }\n"
            "  window.location.href = 'https://www.youtube.com/results?search_query=' + encodeURIComponent(q);\n"
            "}\n"
            "</script>\n</body>\n</html>\n";
    return html;
}

std::string buildGamesHtml() {
    std::string html = commonPageHead("مركز الألعاب - متصفح خفيف");
    html += "<div class=\"wrap\">\n"
            "<div class=\"hero\" style=\"padding:20px;\">\n"
            "  <h1>مركز الألعاب الخفيفة وألعاب الويب</h1>\n"
            "  <p>اختر لعبة مدمجة للعب الفوري (تعمل على كافة الأنظمة حتى بدون إنترنت)، أو انتقل إلى منصات ألعاب HTML5 العالمية</p>\n"
            "  <div class=\"quick-links\">\n"
            "    <a href=\"https://poki.com\">Poki ألعاب أونلاين</a>\n"
            "    <a href=\"https://www.crazygames.com\">CrazyGames</a>\n"
            "    <a href=\"https://js13kgames.com\">JS13k ألعاب فائقة الخفة</a>\n"
            "    <a href=\"https://itch.io/games/html5\">itch.io HTML5</a>\n"
            "  </div>\n"
            "</div>\n"
            "<div style=\"background:#fff;border-radius:12px;padding:20px;border:1px solid #e2e8f0;text-align:center;\">\n"
            "  <div style=\"margin-bottom:14px;\">\n"
            "    <button class=\"btn\" onclick=\"startGame('snake')\">لعبة الثعبان (Snake)</button>\n"
            "    <button class=\"btn btn-green\" onclick=\"startGame('bricks')\">كاسر الطوب (Breakout)</button>\n"
            "    <button class=\"btn btn-amber\" onclick=\"startGame('2048')\">لعبة الذكاء 2048</button>\n"
            "  </div>\n"
            "  <div id=\"scoreBar\" style=\"font-weight:bold;font-size:16px;margin-bottom:10px;color:#0284c7;\">النتيجة: 0</div>\n"
            "  <canvas id=\"gameCanvas\" width=\"480\" height=\"360\" style=\"background:#0f172a;border-radius:8px;max-width:100%;border:2px solid #334155;\"></canvas>\n"
            "  <div style=\"margin-top:12px;\">\n"
            "    <button class=\"btn btn-sec\" style=\"padding:6px 14px;\" onclick=\"sendDir(0,-1)\">&#9650; أعلى</button><br/>\n"
            "    <button class=\"btn btn-sec\" style=\"padding:6px 14px;margin:4px;\" onclick=\"sendDir(1,0)\">&#9654; يمين</button>\n"
            "    <button class=\"btn btn-sec\" style=\"padding:6px 14px;margin:4px;\" onclick=\"sendDir(0,1)\">&#9660; أسفل</button>\n"
            "    <button class=\"btn btn-sec\" style=\"padding:6px 14px;margin:4px;\" onclick=\"sendDir(-1,0)\">&#9664; يسار</button>\n"
            "  </div>\n"
            "  <p style=\"font-size:12px;color:#64748b;margin-top:8px;\">استخدم أسهم لوحة المفاتيح (أو الأزرار أعلاه) للتحكم باللعبة</p>\n"
            "</div>\n</div>\n"
            "<script>\n"
            "var cv = document.getElementById('gameCanvas');\n"
            "var ctx = cv && cv.getContext ? cv.getContext('2d') : null;\n"
            "var timerId = null;\n"
            "var mode = 'snake';\n"
            "var score = 0;\n"
            "var snake = [], sdx = 1, sdy = 0, food = {x:10,y:8};\n"
            "var padX = 190, ball = {x:240,y:200,vx:3,vy:-3}, bricks = [];\n"
            "var board2048 = [];\n"
            "function setScore(v) { score = v; document.getElementById('scoreBar').innerHTML = 'النتيجة: ' + score; }\n"
            "function stopLoop() { if (timerId) { clearInterval(timerId); timerId = null; } }\n"
            "function startGame(m) {\n"
            "  if (!ctx) return;\n"
            "  stopLoop(); mode = m; setScore(0);\n"
            "  if (m === 'snake') {\n"
            "    snake = [{x:6,y:9},{x:5,y:9},{x:4,y:9}]; sdx = 1; sdy = 0;\n"
            "    food = {x:14,y:9};\n"
            "    timerId = setInterval(stepSnake, 110);\n"
            "  } else if (m === 'bricks') {\n"
            "    padX = 195; ball = {x:240,y:290,vx:3.5,vy:-3.5}; bricks = [];\n"
            "    for (var r=0;r<4;r++) for (var c=0;c<7;c++) bricks.push({x:22+c*63,y:28+r*26,alive:true});\n"
            "    timerId = setInterval(stepBricks, 20);\n"
            "  } else {\n"
            "    board2048 = [[0,0,0,0],[0,0,0,0],[0,0,0,0],[0,0,0,0]];\n"
            "    spawn2048(); spawn2048(); draw2048();\n"
            "  }\n"
            "}\n"
            "function stepSnake() {\n"
            "  var head = {x: (snake[0].x + sdx + 24) % 24, y: (snake[0].y + sdy + 18) % 18};\n"
            "  for (var i=0;i<snake.length;i++) {\n"
            "    if (snake[i].x === head.x && snake[i].y === head.y) { startGame('snake'); return; }\n"
            "  }\n"
            "  snake.unshift(head);\n"
            "  if (head.x === food.x && head.y === food.y) {\n"
            "    setScore(score + 10);\n"
            "    food = {x: Math.floor(Math.random()*24), y: Math.floor(Math.random()*18)};\n"
            "  } else { snake.pop(); }\n"
            "  ctx.fillStyle = '#0f172a'; ctx.fillRect(0,0,480,360);\n"
            "  ctx.fillStyle = '#ef4444'; ctx.fillRect(food.x*20+2, food.y*20+2, 16, 16);\n"
            "  for (var j=0;j<snake.length;j++) {\n"
            "    ctx.fillStyle = (j===0) ? '#38bdf8' : '#22c55e';\n"
            "    ctx.fillRect(snake[j].x*20+1, snake[j].y*20+1, 18, 18);\n"
            "  }\n"
            "}\n"
            "function stepBricks() {\n"
            "  ball.x += ball.vx; ball.y += ball.vy;\n"
            "  if (ball.x < 8 || ball.x > 472) ball.vx = -ball.vx;\n"
            "  if (ball.y < 8) ball.vy = -ball.vy;\n"
            "  if (ball.y > 330 && ball.y < 345 && ball.x >= padX && ball.x <= padX + 90) {\n"
            "    ball.vy = -Math.abs(ball.vy);\n"
            "  }\n"
            "  if (ball.y > 360) { startGame('bricks'); return; }\n"
            "  var rem = 0;\n"
            "  for (var i=0;i<bricks.length;i++) {\n"
            "    var b = bricks[i]; if (!b.alive) continue; rem++;\n"
            "    if (ball.x > b.x && ball.x < b.x+56 && ball.y > b.y && ball.y < b.y+18) {\n"
            "      b.alive = false; ball.vy = -ball.vy; setScore(score + 15);\n"
            "    }\n"
            "  }\n"
            "  if (rem === 0) startGame('bricks');\n"
            "  ctx.fillStyle = '#0f172a'; ctx.fillRect(0,0,480,360);\n"
            "  ctx.fillStyle = '#38bdf8'; ctx.fillRect(padX, 336, 90, 12);\n"
            "  ctx.fillStyle = '#facc15'; ctx.beginPath(); ctx.arc(ball.x, ball.y, 7, 0, 6.28); ctx.fill();\n"
            "  for (var k=0;k<bricks.length;k++) {\n"
            "    if (bricks[k].alive) { ctx.fillStyle = '#10b981'; ctx.fillRect(bricks[k].x, bricks[k].y, 56, 18); }\n"
            "  }\n"
            "}\n"
            "function spawn2048() {\n"
            "  var emp = [];\n"
            "  for (var r=0;r<4;r++) for (var c=0;c<4;c++) if (board2048[r][c]===0) emp.push({r:r,c:c});\n"
            "  if (!emp.length) return;\n"
            "  var p = emp[Math.floor(Math.random()*emp.length)];\n"
            "  board2048[p.r][p.c] = (Math.random() < 0.85) ? 2 : 4;\n"
            "}\n"
            "function draw2048() {\n"
            "  ctx.fillStyle = '#0f172a'; ctx.fillRect(0,0,480,360);\n"
            "  ctx.textAlign = 'center'; ctx.font = 'bold 22px Tahoma';\n"
            "  for (var r=0;r<4;r++) for (var c=0;c<4;c++) {\n"
            "    var v = board2048[r][c];\n"
            "    ctx.fillStyle = v ? '#0284c7' : '#1e293b';\n"
            "    ctx.fillRect(88 + c*78, 28 + r*78, 70, 70);\n"
            "    if (v) { ctx.fillStyle = '#fff'; ctx.fillText(String(v), 123 + c*78, 71 + r*78); }\n"
            "  }\n"
            "}\n"
            "function slideLine(arr) {\n"
            "  var f = []; for (var i=0;i<4;i++) if (arr[i]) f.push(arr[i]);\n"
            "  for (var j=0;j<f.length-1;j++) if (f[j]===f[j+1]) { f[j]*=2; setScore(score+f[j]); f.splice(j+1,1); }\n"
            "  while (f.length<4) f.push(0);\n"
            "  return f;\n"
            "}\n"
            "function move2048(dx, dy) {\n"
            "  var before = JSON.stringify(board2048);\n"
            "  for (var i=0;i<4;i++) {\n"
            "    var line = [];\n"
            "    for (var j=0;j<4;j++) {\n"
            "      var r = dy ? (dy>0 ? 3-j : j) : i;\n"
            "      var c = dx ? (dx>0 ? 3-j : j) : i;\n"
            "      line.push(board2048[r][c]);\n"
            "    }\n"
            "    line = slideLine(line);\n"
            "    for (var k=0;k<4;k++) {\n"
            "      var rr = dy ? (dy>0 ? 3-k : k) : i;\n"
            "      var cc = dx ? (dx>0 ? 3-k : k) : i;\n"
            "      board2048[rr][cc] = line[k];\n"
            "    }\n"
            "  }\n"
            "  if (JSON.stringify(board2048) !== before) spawn2048();\n"
            "  draw2048();\n"
            "}\n"
            "function sendDir(dx, dy) {\n"
            "  if (mode === 'snake') {\n"
            "    if (dx !== -sdx || dy !== -sdy) { sdx = dx; sdy = dy; }\n"
            "  } else if (mode === 'bricks') {\n"
            "    padX = Math.max(0, Math.min(390, padX + dx * 32));\n"
            "  } else {\n"
            "    move2048(dx, dy);\n"
            "  }\n"
            "}\n"
            "document.onkeydown = function(e) {\n"
            "  e = e || window.event; var k = e.keyCode;\n"
            "  if (k === 37) { sendDir(-1,0); return false; }\n"
            "  if (k === 38) { sendDir(0,-1); return false; }\n"
            "  if (k === 39) { sendDir(1,0); return false; }\n"
            "  if (k === 40) { sendDir(0,1); return false; }\n"
            "};\n"
            "startGame('snake');\n"
            "</script>\n</body>\n</html>\n";
    return html;
}

std::string buildVideoHtml() {
    std::string html = commonPageHead("مشغّل الفيديو والوسائط - متصفح خفيف");
    html += "<div class=\"wrap\">\n"
            "<div class=\"hero\">\n"
            "  <h1>مشاهدة الفيديوهات والمشغّل المدمج الخفيف</h1>\n"
            "  <p>ابحث في يوتيوب أو شغّل أي رابط فيديو مباشر (<code>.mp4</code>, <code>.webm</code>, <code>.ogg</code>) بأقل استهلاك للمعالج والذاكرة</p>\n"
            "  <form onsubmit=\"return searchYt();\" class=\"search-row\">\n"
            "    <input id=\"ytQ\" class=\"search-inp\" type=\"text\" placeholder=\"ابحث عن فيديو في يوتيوب...\"/>\n"
            "    <button type=\"submit\" class=\"btn\">بحث يوتيوب</button>\n"
            "  </form>\n"
            "  <div class=\"quick-links\">\n"
            "    <a href=\"https://www.youtube.com\">فتح YouTube</a>\n"
            "    <a href=\"https://m.youtube.com\">YouTube النسخة الخفيفة</a>\n"
            "    <a href=\"https://archive.org/details/movies\">أرشيف الأفلام المفتوحة</a>\n"
            "  </div>\n"
            "</div>\n"
            "<div style=\"background:#fff;border-radius:12px;padding:22px;border:1px solid #e2e8f0;\">\n"
            "  <h3 style=\"margin-top:0;\">مشغّل الفيديو المباشر (HTML5 خفيف)</h3>\n"
            "  <div class=\"search-row\" style=\"max-width:100%;margin-bottom:16px;\">\n"
            "    <input id=\"vUrl\" class=\"search-inp\" type=\"text\" placeholder=\"الصق رابط ملف فيديو مباشر (.mp4 / .webm)...\"/>\n"
            "    <button class=\"btn btn-green\" onclick=\"playDirect()\">تشغيل الفيديو</button>\n"
            "  </div>\n"
            "  <div style=\"text-align:center;background:#000;border-radius:8px;padding:8px;\">\n"
            "    <video id=\"vPlayer\" controls style=\"width:100%;max-height:460px;\" preload=\"metadata\"></video>\n"
            "  </div>\n"
            "</div>\n</div>\n"
            "<script>\n"
            "function searchYt() {\n"
            "  var q = document.getElementById('ytQ').value;\n"
            "  if (!q) return false;\n"
            "  window.location.href = 'https://www.youtube.com/results?search_query=' + encodeURIComponent(q);\n"
            "  return false;\n"
            "}\n"
            "function playDirect() {\n"
            "  var u = document.getElementById('vUrl').value;\n"
            "  if (!u) return;\n"
            "  var p = document.getElementById('vPlayer');\n"
            "  p.src = u; p.play();\n"
            "}\n"
            "</script>\n</body>\n</html>\n";
    return html;
}

std::string buildDownloadsHtml(StorageManager& storage) {
    auto downloads = storage.getDownloads();
    std::string html = commonPageHead("مدير التحميلات وتحميل البرامج");
    html += "<div class=\"wrap\">\n"
            "<div class=\"hero\" style=\"padding:22px;\">\n"
            "  <h1>مدير التحميلات وتنزيل البرامج</h1>\n"
            "  <p>مجلد الحفظ الافتراضي: <code>" + fs::htmlEscape(fs::getDownloadsDir()) + "</code></p>\n"
            "  <form onsubmit=\"return startDl();\" class=\"search-row\">\n"
            "    <input id=\"dlUrl\" class=\"search-inp\" type=\"text\" placeholder=\"الصق رابط تحميل مباشر لبرنامج أو ملف (.exe, .zip, .deb, .pdf)...\"/>\n"
            "    <button type=\"submit\" class=\"btn btn-green\">بدء التحميل الآن</button>\n"
            "    <a class=\"btn btn-sec\" href=\"lite-action://open-downloads-dir\">فتح مجلد التحميلات</a>\n"
            "    <a class=\"btn\" href=\"lite://downloads\">تحديث الحالة</a>\n"
            "  </form>\n"
            "  <div class=\"quick-links\">\n"
            "    <a href=\"https://www.7-zip.org\">7-Zip (ضغط الملفات)</a>\n"
            "    <a href=\"https://www.videolan.org/vlc/\">VLC (مشغل الوسائط)</a>\n"
            "    <a href=\"https://ar.libreoffice.org\">LibreOffice</a>\n"
            "    <a href=\"https://sourceforge.net\">SourceForge</a>\n"
            "  </div>\n"
            "</div>\n";

    html += "<table class=\"list-tbl\">\n"
            "<thead><tr><th>الملف</th><th>التقدم والحجم</th><th>الحالة</th><th>إجراءات</th></tr></thead>\n<tbody>\n";
    if (downloads.empty()) {
        html += "<tr><td colspan=\"4\" style=\"text-align:center;color:#64748b;padding:24px;\">"
                "لا توجد تحميلات حالياً. اضغط على أي رابط تحميل في الويب أو الصق رابطاً أعلاه للبدء.</td></tr>\n";
    } else {
        for (auto it = downloads.rbegin(); it != downloads.rend(); ++it) {
            int pct = 0;
            if (it->totalBytes > 0) {
                pct = static_cast<int>((it->receivedBytes * 100ULL) / it->totalBytes);
                if (pct > 100) pct = 100;
            } else if (it->status == DownloadStatus::Completed) {
                pct = 100;
            }
            std::string statusLabel;
            if (it->status == DownloadStatus::InProgress) statusLabel = "قيد التحميل (" + std::to_string(pct) + "%)";
            else if (it->status == DownloadStatus::Completed) statusLabel = "مكتمل بنجاح";
            else if (it->status == DownloadStatus::Cancelled) statusLabel = "ملغى";
            else statusLabel = "فشل التحميل";

            std::string sizeInfo = fs::formatBytes(it->receivedBytes);
            if (it->totalBytes > 0) sizeInfo += " / " + fs::formatBytes(it->totalBytes);

            html += "<tr><td><strong>" + fs::htmlEscape(it->fileName) + "</strong><br/><span style=\"font-size:11px;color:#64748b;\">" +
                    fs::htmlEscape(it->savePath) + "</span></td>"
                    "<td><div class=\"prog-bg\"><div class=\"prog-fill\" style=\"width:" + std::to_string(pct) + "%;\"></div></div>"
                    " <span style=\"font-size:12px;\">" + sizeInfo + "</span></td>"
                    "<td>" + fs::htmlEscape(statusLabel) + "</td>"
                    "<td><a class=\"btn\" style=\"padding:4px 10px;font-size:12px;\" href=\"lite-action://open-downloaded?id=" +
                    std::to_string(it->id) + "\">فتح الملف</a></td></tr>\n";
        }
    }
    html += "</tbody></table>\n</div>\n"
            "<script>\n"
            "function startDl() {\n"
            "  var u = document.getElementById('dlUrl').value;\n"
            "  if (!u) return false;\n"
            "  window.location.href = 'lite-action://download?url=' + encodeURIComponent(u);\n"
            "  return false;\n"
            "}\n"
            "</script>\n</body>\n</html>\n";
    return html;
}

std::string buildBookmarksHtml(StorageManager& storage) {
    auto bms = storage.getBookmarks();
    std::string html = commonPageHead("المفضلة - متصفح خفيف");
    html += "<div class=\"wrap\"><h2>المواقع المحفوظة في المفضلة</h2>\n"
            "<table class=\"list-tbl\"><thead><tr><th>العنوان</th><th>الرابط</th><th>حذف</th></tr></thead><tbody>\n";
    if (bms.empty()) {
        html += "<tr><td colspan=\"3\" style=\"text-align:center;padding:20px;\">لا توجد مواقع محفوظة بعد. اضغط على زر النجمة في شريط الأدوات لإضافة أي موقع.</td></tr>\n";
    } else {
        for (size_t i = 0; i < bms.size(); ++i) {
            html += "<tr><td><strong>" + fs::htmlEscape(bms[i].title) + "</strong></td>"
                    "<td><a href=\"" + fs::htmlEscape(bms[i].url) + "\">" + fs::htmlEscape(bms[i].url) + "</a></td>"
                    "<td><a style=\"color:#dc2626;\" href=\"lite-action://remove-bookmark?idx=" + std::to_string(i) + "\">حذف</a></td></tr>\n";
        }
    }
    html += "</tbody></table></div></body></html>\n";
    return html;
}

std::string buildHistoryHtml(StorageManager& storage) {
    auto hist = storage.getHistory();
    std::string html = commonPageHead("سجل التصفح - متصفح خفيف");
    html += "<div class=\"wrap\"><div style=\"display:flex;justify-content:space-between;align-items:center;margin-bottom:12px;\">"
            "<h2 style=\"margin:0;\">سجل التصفح</h2>"
            "<a class=\"btn btn-sec\" href=\"lite-action://clear-history\">مسح السجل بالكامل</a></div>\n"
            "<table class=\"list-tbl\"><thead><tr><th>الوقت</th><th>الصفحة</th><th>الرابط</th></tr></thead><tbody>\n";
    if (hist.empty()) {
        html += "<tr><td colspan=\"3\" style=\"text-align:center;padding:20px;\">سجل التصفح فارغ.</td></tr>\n";
    } else {
        for (auto it = hist.rbegin(); it != hist.rend(); ++it) {
            html += "<tr><td>" + fs::htmlEscape(it->timestamp) + "</td>"
                    "<td>" + fs::htmlEscape(it->title) + "</td>"
                    "<td><a href=\"" + fs::htmlEscape(it->url) + "\">" + fs::htmlEscape(it->url) + "</a></td></tr>\n";
        }
    }
    html += "</tbody></table></div></body></html>\n";
    return html;
}

}  // namespace

InternalPageOutput renderInternalPage(const std::string& liteUri, StorageManager& storage, int slotId) {
    InternalPageOutput out;
    std::string u = fs::toLowerAscii(fs::trim(liteUri));
    if (u == "lite://games") {
        out.title = "مركز الألعاب";
        out.html = buildGamesHtml();
    } else if (u == "lite://video") {
        out.title = "مشغّل الفيديو";
        out.html = buildVideoHtml();
    } else if (u == "lite://downloads") {
        out.title = "مدير التحميلات";
        out.html = buildDownloadsHtml(storage);
    } else if (u == "lite://bookmarks") {
        out.title = "المفضلة";
        out.html = buildBookmarksHtml(storage);
    } else if (u == "lite://history") {
        out.title = "سجل التصفح";
        out.html = buildHistoryHtml(storage);
    } else {
        out.title = "الرئيسية";
        out.html = buildHomeHtml(storage);
    }

    char nameBuf[64];
    std::snprintf(nameBuf, sizeof(nameBuf), "internal_page_%d.html", slotId);
    out.tempHtmlPath = fs::joinPath(fs::getTempSessionDir(), nameBuf);
    fs::writeFileText(out.tempHtmlPath, out.html);
    out.tempFileUrl = fs::pathToFileUrl(out.tempHtmlPath);
    return out;
}

}  // namespace litebrowser

