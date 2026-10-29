<?php
// =============================================================================
// 灰度更新 — 面板
//
//   1. 统计区: 进程总数 / 更新完成数 / 等待更新数 / 完成百分比
//   2. 进程类型下拉列 (100 Gate / 101 Login / 102 Global / 201 Game / 202 Battle;
//      「0 所有进程」列表含 Gate, 但滚动不含 Gate, 由后端 -ExcludeType 100 保证)
//   3. 版本列表框 (Jenkins 构建版本, 最新在前)
//   4. 进程列表框 (所选类型全部进程 + 当前版本 + 最新/旧版本说明)
//   5. 更新按钮 (点击后按所选类型/版本/batch_size 触发灰度滚动)
//   6. Gate 灰度更新 (页面末尾独立按钮: Gate 正常情况下不更新, 滚动时客户端直接断链)
//
// 数据全部来自 api/gray_update.php (session 鉴权), 前端 fetch 联动刷新。
// =============================================================================
require_once __DIR__ . '/_layout.php';

layout_header('灰度更新', 'gray_update');
?>

<!-- ===== 第一部分: 统计 ===== -->
<div class="stat-grid" id="stats">
    <div class="stat"><div class="k">进程总数</div><div class="v" id="st-total">—</div></div>
    <div class="stat"><div class="k">更新完成数</div><div class="v" id="st-done" style="color:var(--online)">—</div></div>
    <div class="stat"><div class="k">等待更新数</div><div class="v" id="st-waiting" style="color:#f59f00">—</div></div>
    <div class="stat"><div class="k">完成百分比</div><div class="v" id="st-percent">—</div></div>
</div>

<!-- ===== 第二部分: 进程类型下拉列 (不含 Gate) ===== -->
<div class="card">
    <div class="card-header">进程类型</div>
    <div class="card-body">
        <div class="d-flex align-items-center gap-3">
            <select id="typeSelect" class="form-select" style="max-width:280px">
                <option value="0">0 — 所有进程</option>
                <?php foreach (PROCESS_TYPES as $t => $n): ?>
                    <option value="<?= (int)$t ?>"><?= (int)$t ?> — <?= esc($n) ?></option>
                <?php endforeach; ?>
            </select>
            <span class="dim">选择进程类型后，下方版本列表 / 进程列表 / 统计联动刷新。进程列表含 Gate；「所有进程」滚动不含 Gate（用下方独立按钮）。</span>
        </div>
    </div>
</div>

<!-- ===== 第三部分: 版本列表框 ===== -->
<div class="card">
    <div class="card-header">可用版本 <span class="dim">Jenkins 编译好的构建（最新在前）</span></div>
    <div class="card-body">
        <select id="versionSelect" size="6" class="form-select" style="max-width:640px"></select>
        <div id="verPager" class="actions" style="justify-content:space-between; align-items:center; flex-wrap:wrap; margin-top:8px;"></div>
        <div class="dim" style="margin-top:8px">选择目标版本后，统计与「最新/旧版本」判定随之更新。每页显示 6 个版本，最新在前。</div>
    </div>
</div>

<!-- ===== 第四部分: 进程列表框 ===== -->
<div class="card">
    <div class="card-header">进程列表 <span class="dim">当前版本 / 最新·旧版本说明</span></div>
    <div class="card-body">
        <div id="processList" class="empty">加载中…</div>
    </div>
</div>

<!-- ===== 第五部分: 更新按钮 ===== -->
<div class="card">
    <div class="card-header">开始灰度更新</div>
    <div class="card-body">
        <div class="d-flex align-items-center gap-3">
            <label class="mb-0 d-flex align-items-center gap-2">
                <span>批大小</span>
                <input id="batchSize" type="number" class="form-control" value="1" min="1" style="width:110px">
            </label>
            <button id="updateBtn" class="btn btn-primary">开始灰度更新</button>
            <span id="updateMsg" class="dim"></span>
        </div>
        <div class="d-flex align-items-center gap-3" style="margin-top:10px">
            <label class="mb-0 d-flex align-items-center gap-2">
                <input id="autoRefresh" type="checkbox" checked>
                <span>自动刷新（2 秒）</span>
            </label>
            <span id="updateStatus" class="badge text-bg-secondary">空闲</span>
            <span id="lastRefresh" class="dim"></span>
        </div>
        <div class="dim" style="margin-top:10px">
            点击后对所选类型进程按批大小分批滚动重启到所选版本。异步触发，页面不阻塞；滚动过程中页面自动刷新，统计/进程列表版本逐批翻转即完成。
        </div>
    </div>
</div>

<!-- ===== 第六部分: Gate 灰度更新 (独立按钮, 客户端直接断链) ===== -->
<div class="card" style="border-left:3px solid #f59f00">
    <div class="card-header">Gate 灰度更新 <span class="dim">特殊操作 — 客户端直接断链</span></div>
    <div class="card-body">
        <div class="d-flex align-items-center gap-3">
            <label class="mb-0 d-flex align-items-center gap-2">
                <span>目标版本</span>
                <select id="gateVersionSelect" class="form-select" style="width:220px"></select>
            </label>
            <label class="mb-0 d-flex align-items-center gap-2">
                <span>批大小</span>
                <input id="gateBatchSize" type="number" class="form-control" value="1" min="1" style="width:110px">
            </label>
            <button id="gateUpdateBtn" class="btn btn-warning">Gate 灰度更新</button>
            <span id="gateUpdateMsg" class="dim"></span>
        </div>
        <div class="dim" style="margin-top:10px">
            Gate 正常情况下不更新。滚动 Gate 时客户端会直接断链（需重新登录），请谨慎操作。
        </div>
    </div>
</div>

<script>
(function () {
    var typeSel = document.getElementById('typeSelect');
    var verSel  = document.getElementById('versionSelect');
    var state = { type: typeSel.value, target: '', processPage: 1, verPage: 1 };
    var PAGE_SIZE = 8;            // 进程列表每页条数
    var VERSION_PAGE_SIZE = 6;    // 可用版本列表每页条数

    function escHtml(s) {
        var d = document.createElement('div');
        d.textContent = s == null ? '' : String(s);
        return d.innerHTML;
    }

    function load() {
        var url = 'api/gray_update.php?type=' + encodeURIComponent(state.type);
        if (state.target) url += '&target=' + encodeURIComponent(state.target);
        fetch(url)
            .then(function (r) { return r.json(); })
            .then(function (d) {
                if (!d.ok) { showError(d.error || 'unknown'); return; }
                render(d);
            })
            .catch(function () { showError('请求失败'); });
    }

    function showError(msg) {
        document.getElementById('processList').className = 'empty';
        document.getElementById('processList').textContent = '加载失败：' + msg;
    }

    function render(d) {
        // 统计
        document.getElementById('st-total').textContent   = d.stats.total;
        document.getElementById('st-done').textContent    = d.stats.done;
        document.getElementById('st-waiting').textContent = d.stats.waiting;
        document.getElementById('st-percent').textContent = d.stats.percent + '%';

        // 灰度滚动状态徽标 + 最近刷新时间
        var st = document.getElementById('updateStatus');
        if (d.updating) {
            st.textContent = '滚动中…';
            st.className = 'badge text-bg-warning';
        } else {
            st.textContent = '空闲';
            st.className = 'badge text-bg-secondary';
        }
        if (d.now) {
            var t = new Date(d.now * 1000);
            var pad = function (n) { return (n < 10 ? '0' : '') + n; };
            document.getElementById('lastRefresh').textContent = '最近刷新 ' +
                pad(t.getHours()) + ':' + pad(t.getMinutes()) + ':' + pad(t.getSeconds());
        }

        // 可用版本列表框 (分页, 每页 VERSION_PAGE_SIZE 条, 最新在前)
        if (!state.target) state.target = d.latest || '';
        var verTotal = d.versions.length;
        var verPages = Math.max(1, Math.ceil(verTotal / VERSION_PAGE_SIZE));
        if (state.verPage > verPages) state.verPage = verPages;
        if (state.verPage < 1) state.verPage = 1;
        var verStart = (state.verPage - 1) * VERSION_PAGE_SIZE;
        var verPageItems = d.versions.slice(verStart, verStart + VERSION_PAGE_SIZE);

        verSel.innerHTML = '';
        if (!verTotal) {
            var vo = document.createElement('option');
            vo.value = ''; vo.textContent = '（暂无已构建版本）';
            verSel.appendChild(vo);
            state.target = '';
        } else {
            // 已选版本不在本页时钉在首项并标注, 保证选择不丢失
            var pinned = state.target && verPageItems.indexOf(state.target) === -1;
            var verOptions = pinned ? [state.target].concat(verPageItems) : verPageItems;
            verOptions.forEach(function (v) {
                var o = document.createElement('option');
                o.value = v;
                o.textContent = (pinned && v === state.target) ? v + '（已选）' : v;
                if (v === state.target) o.selected = true;
                verSel.appendChild(o);
            });
        }

        // 版本分页控件
        var verPager = document.getElementById('verPager');
        if (verPager) {
            if (!verTotal) {
                verPager.innerHTML = '';
            } else {
                verPager.innerHTML = '<div class="dim">第 ' + state.verPage + ' / ' + verPages + ' 页 · 共 ' + verTotal + ' 个版本</div>'
                    + '<div class="actions">'
                    + (state.verPage > 1 ? '<button class="btn btn-sm btn-outline-secondary" id="verPrev">上一页</button>' : '')
                    + (state.verPage < verPages ? '<button class="btn btn-sm btn-outline-secondary" id="verNext">下一页</button>' : '')
                    + '</div>';
                var vp = document.getElementById('verPrev');
                var vn = document.getElementById('verNext');
                if (vp) vp.addEventListener('click', function () { state.verPage--; load(); });
                if (vn) vn.addEventListener('click', function () { state.verPage++; load(); });
            }
        }

        // Gate 独立按钮的版本选择器 (同一份全局构建版本列表)
        var gateSel = document.getElementById('gateVersionSelect');
        var gatePrev = gateSel.value || d.latest;
        gateSel.innerHTML = '';
        if (!d.versions.length) {
            var go = document.createElement('option');
            go.value = ''; go.textContent = '（暂无已构建版本）';
            gateSel.appendChild(go);
        } else {
            d.versions.forEach(function (v) {
                var go = document.createElement('option');
                go.value = v; go.textContent = v;
                if (v === gatePrev) go.selected = true;
                gateSel.appendChild(go);
            });
        }

        // 进程列表框 (分页, 每页 PAGE_SIZE 条)
        var box = document.getElementById('processList');
        if (!d.processes.length) {
            box.className = 'empty';
            box.textContent = '暂无进程注册（cluster:process:*:*）。';
            return;
        }
        var total = d.processes.length;
        var pages = Math.max(1, Math.ceil(total / PAGE_SIZE));
        if (state.processPage > pages) state.processPage = pages;
        if (state.processPage < 1) state.processPage = 1;
        var start = (state.processPage - 1) * PAGE_SIZE;
        var pageItems = d.processes.slice(start, start + PAGE_SIZE);

        box.className = 'table-responsive';
        var rows = pageItems.map(function (p) {
            var badge = p.is_latest
                ? '<span class="badge text-bg-success">最新</span>'
                : '<span class="badge text-bg-warning">旧版本</span>';
            var online = p.online
                ? '<span class="badge text-bg-success">在线</span>'
                : '<span class="badge text-bg-secondary">离线</span>';
            return '<tr class="' + (p.online ? 'online' : 'offline') + '">'
                + '<td>' + escHtml(p.name) + '</td>'
                + '<td class="mono">' + escHtml(p.id) + '</td>'
                + '<td class="mono">' + escHtml(p.type) + ' — ' + escHtml(p.type_name || '') + '</td>'
                + '<td class="mono">' + (p.version ? escHtml(p.version) : '—') + '</td>'
                + '<td>' + badge + '</td>'
                + '<td>' + online + '</td>'
                + '</tr>';
        }).join('');
        var html = '<table class="table table-vcenter">'
            + '<thead><tr><th>名称</th><th>ID</th><th>类型</th><th>当前版本</th><th>版本说明</th><th>状态</th></tr></thead>'
            + '<tbody>' + rows + '</tbody></table>';

        if (pages > 1) {
            html += '<div class="actions" style="justify-content:space-between; align-items:center; flex-wrap:wrap; margin-top:10px;">'
                + '<div class="dim">第 ' + state.processPage + ' / ' + pages + ' 页 · 共 ' + total + ' 个进程</div>'
                + '<div class="actions">'
                + (state.processPage > 1 ? '<button class="btn btn-sm btn-outline-secondary" id="procPrev">上一页</button>' : '')
                + (state.processPage < pages ? '<button class="btn btn-sm btn-outline-secondary" id="procNext">下一页</button>' : '')
                + '</div></div>';
        }
        box.innerHTML = html;

        // innerHTML 重建后需重新绑定分页按钮
        var prevBtn = document.getElementById('procPrev');
        var nextBtn = document.getElementById('procNext');
        if (prevBtn) prevBtn.addEventListener('click', function () { state.processPage--; load(); });
        if (nextBtn) nextBtn.addEventListener('click', function () { state.processPage++; load(); });
    }

    typeSel.addEventListener('change', function () {
        state.type = typeSel.value;
        state.target = '';
        state.processPage = 1;   // 切换类型后回到第一页
        load();
    });
    verSel.addEventListener('change', function () {
        state.target = verSel.value;
        load();
    });

    document.getElementById('updateBtn').addEventListener('click', function () {
        var ver = verSel.value;
        if (!ver) { alert('请先在版本列表框中选择目标版本'); return; }
        var batch = document.getElementById('batchSize').value;
        if (!batch || parseInt(batch, 10) < 1) { alert('批大小必须 ≥ 1'); return; }
        var gateWarn = typeSel.value === '100' ? '\n⚠️ Gate 滚动期间客户端会直接断链（需重新登录）。' : '';
        if (!confirm('确认对类型 ' + typeSel.value + ' 的进程滚动更新到 ' + ver + ' ？' + gateWarn)) return;

        var msg = document.getElementById('updateMsg');
        msg.textContent = '滚动中…';
        var body = new URLSearchParams();
        body.set('type', typeSel.value);
        body.set('version', ver);
        body.set('batch_size', batch);
        fetch('api/gray_update.php', {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: body
        })
            .then(function (r) { return r.json(); })
            .then(function (d) {
                msg.textContent = d.ok ? '已触发，后台滚动中…' : ('更新失败：' + (d.error || ''));
                load();
            })
            .catch(function () { msg.textContent = '请求失败'; });
    });

    // Gate 独立更新按钮: 固定走 type=100, 与常规下拉滚动隔离 (客户端直接断链)。
    document.getElementById('gateUpdateBtn').addEventListener('click', function () {
        var ver = document.getElementById('gateVersionSelect').value;
        if (!ver) { alert('请先选择 Gate 目标版本'); return; }
        var batch = document.getElementById('gateBatchSize').value;
        if (!batch || parseInt(batch, 10) < 1) { alert('批大小必须 ≥ 1'); return; }
        if (!confirm('确认将 Gate 滚动更新到 ' + ver + ' ？\nGate 滚动期间客户端会直接断链（需重新登录）。')) return;

        var msg = document.getElementById('gateUpdateMsg');
        msg.textContent = '滚动中…';
        var body = new URLSearchParams();
        body.set('type', '100');
        body.set('version', ver);
        body.set('batch_size', batch);
        fetch('api/gray_update.php', {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: body
        })
            .then(function (r) { return r.json(); })
            .then(function (d) {
                msg.textContent = d.ok ? '已触发，后台滚动中…' : ('更新失败：' + (d.error || ''));
                load();
            })
            .catch(function () { msg.textContent = '请求失败'; });
    });

    // 自动刷新: 滚动过程中页面每 2s 轮询一次, 统计/进程列表/状态徽标随之更新。
    var timer = null;
    function startAuto() {
        if (timer) return;
        timer = setInterval(load, 2000);
    }
    function stopAuto() {
        if (timer) { clearInterval(timer); timer = null; }
    }
    document.getElementById('autoRefresh').addEventListener('change', function () {
        if (this.checked) { startAuto(); load(); } else { stopAuto(); }
    });

    load();
    startAuto();   // 默认开启自动刷新
})();
</script>

<?php layout_footer(); ?>
