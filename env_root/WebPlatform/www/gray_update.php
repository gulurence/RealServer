<?php
// =============================================================================
// 灰度更新 — 五部分面板
//
//   1. 统计区: 进程总数 / 更新完成数 / 等待更新数 / 完成百分比
//   2. 进程类型下拉列 (100 Gate / 101 Login / 102 Global / 201 Game)
//   3. 版本列表框 (所选类型可用的 Jenkins 构建版本, 最新在前)
//   4. 进程列表框 (所选类型全部进程 + 当前版本 + 最新/旧版本说明)
//   5. 更新按钮 (点击后按所选类型/版本/batch_size 触发灰度滚动)
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

<!-- ===== 第二部分: 进程类型下拉列 ===== -->
<div class="card">
    <div class="card-header">进程类型</div>
    <div class="card-body">
        <div class="d-flex align-items-center gap-3">
            <select id="typeSelect" class="form-select" style="max-width:280px">
                <?php foreach (PROCESS_TYPES as $t => $n): ?>
                    <option value="<?= (int)$t ?>"><?= (int)$t ?> — <?= esc($n) ?></option>
                <?php endforeach; ?>
            </select>
            <span class="dim">选择进程类型后，下方版本列表 / 进程列表 / 统计联动刷新。</span>
        </div>
    </div>
</div>

<!-- ===== 第三部分: 版本列表框 ===== -->
<div class="card">
    <div class="card-header">可用版本 <span class="dim">Jenkins 编译好的构建（最新在前）</span></div>
    <div class="card-body">
        <select id="versionSelect" size="6" class="form-select" style="max-width:640px"></select>
        <div class="dim" style="margin-top:8px">选择目标版本后，统计与「最新/旧版本」判定随之更新。</div>
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
        <div class="dim" style="margin-top:10px">
            点击后对所选类型进程，按批大小分批滚动重启到所选版本（同步执行，期间页面会阻塞数秒）。
        </div>
    </div>
</div>

<script>
(function () {
    var typeSel = document.getElementById('typeSelect');
    var verSel  = document.getElementById('versionSelect');
    var state = { type: typeSel.value, target: '' };

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

        // 版本列表框
        var prev = state.target || d.latest;
        verSel.innerHTML = '';
        if (!d.versions.length) {
            var o = document.createElement('option');
            o.value = ''; o.textContent = '（暂无已构建版本）';
            verSel.appendChild(o);
        } else {
            d.versions.forEach(function (v) {
                var o = document.createElement('option');
                o.value = v; o.textContent = v;
                if (v === prev) o.selected = true;
                verSel.appendChild(o);
            });
        }
        state.target = verSel.value || d.latest;

        // 进程列表框
        var box = document.getElementById('processList');
        if (!d.processes.length) {
            box.className = 'empty';
            box.textContent = '暂无进程注册（cluster:process:*:*）。';
            return;
        }
        box.className = 'table-responsive';
        var rows = d.processes.map(function (p) {
            var badge = p.is_latest
                ? '<span class="badge text-bg-success">最新</span>'
                : '<span class="badge text-bg-warning">旧版本</span>';
            var online = p.online
                ? '<span class="badge text-bg-success">在线</span>'
                : '<span class="badge text-bg-secondary">离线</span>';
            return '<tr class="' + (p.online ? 'online' : 'offline') + '">'
                + '<td>' + escHtml(p.name) + '</td>'
                + '<td class="mono">' + escHtml(p.id) + '</td>'
                + '<td class="mono">' + (p.version ? escHtml(p.version) : '—') + '</td>'
                + '<td>' + badge + '</td>'
                + '<td>' + online + '</td>'
                + '</tr>';
        }).join('');
        box.innerHTML = '<table class="table table-vcenter">'
            + '<thead><tr><th>名称</th><th>ID</th><th>当前版本</th><th>版本说明</th><th>状态</th></tr></thead>'
            + '<tbody>' + rows + '</tbody></table>';
    }

    typeSel.addEventListener('change', function () {
        state.type = typeSel.value;
        state.target = '';
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
        if (!confirm('确认对类型 ' + typeSel.value + ' 的进程滚动更新到 ' + ver + ' ？')) return;

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
                msg.textContent = d.ok ? '更新完成' : ('更新失败：' + (d.error || ''));
                load();
            })
            .catch(function () { msg.textContent = '请求失败'; });
    });

    load();
})();
</script>

<?php layout_footer(); ?>
