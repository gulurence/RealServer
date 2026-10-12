<?php
// =============================================================================
// 服务器基础配置 — 集中编辑「服务器内部需手工配置」的基础配置并同步到 Redis
//
//   ① 链接规则 LinkRule → cluster:link:rules Hash (from_type → "to_type1,to_type2,...")
//     进程间「谁连谁」的有向拓扑规则 (每条有向边 = 一条 TCP), 由
//     CServiceDiscovery::GetLinkRules() 读取、CLinkManager::DiscoverAndConnect() 每 10s 用它连对端。
//   ② 全局配置          → cluster:config:cluster Hash (platform/log_level/...)
//
// server.xml 仅保留引导信息 (Redis seed / 进程身份 / auth secret), 运行时基础配置都在 Redis。
// =============================================================================
require_once __DIR__ . '/_layout.php';

const LINK_RULE_KEY = 'cluster:link:rules';

$r = redis();
$types = array_keys(PROCESS_TYPES);   // [100,101,102,201]

$notice = '';
$noticeOk = true;

// ---------- ① 链接规则 LinkRule 保存 ----------
if (isset($_POST['save_link'])) {
    if ($r === null) {
        $notice = '无法连接 Redis，链接规则未保存';
        $noticeOk = false;
    } else {
        $posted = $_POST['link'] ?? [];
        // 先算清「期望状态」: 有勾选的 from 写逗号串, 没勾选的 from 删除该行
        $intended = [];   // (string)from => "101,201"
        foreach ($types as $from) {
            $tos = [];
            foreach ($types as $to) {
                if ($to === $from) continue;
                if (!empty($posted[$from][$to])) $tos[] = $to;
            }
            if (!empty($tos)) {
                $intended[(string)$from] = implode(',', $tos);
            }
        }

        $failed = false;
        try {
            foreach ($types as $from) {
                $f = (string)$from;
                if (isset($intended[$f])) {
                    $r->hset(LINK_RULE_KEY, $f, $intended[$f]);
                } else {
                    $r->hdel(LINK_RULE_KEY, $f);
                }
            }
            // 读回校验: 若 Redis 静默写失败 (集群瞬断/路由异常), 此处能发现,
            // 而不是照样提示「已写入」→ 刷新后勾选丢失。
            $got = $r->hgetall(LINK_RULE_KEY);
            $gotNorm = [];
            if (is_array($got)) {
                foreach ($got as $f => $v) $gotNorm[(string)$f] = (string)$v;
            }
            if ($gotNorm != $intended) {
                $failed = true;
            }
        } catch (Throwable $e) {
            $failed = true;
        }

        if ($failed) {
            $notice = '链接规则保存失败（Redis 写入异常，请重试）';
            $noticeOk = false;
        } else {
            $notice = '链接规则已写入 ' . LINK_RULE_KEY;
        }
    }
}

// ---------- ② 全局配置 保存 ----------
$FIELDS = [
    'platform'            => ['label' => '平台标识', 'type' => 'text'],
    'log_level'           => ['label' => '日志级别', 'type' => 'select',
                               'opts' => ['trace', 'debug', 'info', 'warn', 'error']],
    'auto_scale_enabled'  => ['label' => '自动扩缩容', 'type' => 'select',
                               'opts' => ['0' => '关闭 (手动优先)', '1' => '开启 (二期 Leader)']],
    'controller_interval' => ['label' => '控制器评估间隔 (秒)', 'type' => 'number'],
];

if (isset($_POST['save_global'])) {
    if ($r === null) {
        $notice = '无法连接 Redis，全局配置未保存';
        $noticeOk = false;
    } else {
        $data = [];
        foreach (array_keys($FIELDS) as $f) {
            $data[$f] = post_str($f);
        }
        redis_hash_set($r, config_cluster_key(), $data);
        $notice = '全局配置已写入 ' . config_cluster_key();
    }
}

// ---------- ③ Actor 状态失效时长 保存 ----------
if (isset($_POST['save_actor_ttl'])) {
    if ($r === null) {
        $notice = '无法连接 Redis，Actor 状态失效时长未保存';
        $noticeOk = false;
    } else {
        foreach (array_keys(ACTOR_TYPES) as $t) {
            $v = post_str('ttl_' . $t);
            $iv = (int)$v;
            if ($v === '' || $iv <= 0) {
                // 留空/非法 → 删除字段, 服务端回退默认 86400
                $r->hdel(config_actor_state_ttl_key(), (string)$t);
            } else {
                $r->hset(config_actor_state_ttl_key(), (string)$t, (string)$iv);
            }
        }
        $notice = 'Actor 状态失效时长已写入 ' . config_actor_state_ttl_key();
    }
}

// ---------- 读取当前值 ----------
$linkRules = redis_hash_get($r, LINK_RULE_KEY);   // from_type => "101,201"
$linkSet = [];
foreach ($linkRules as $from => $csv) {
    $linkSet[(int)$from] = array_map('intval', array_filter(explode(',', (string)$csv), 'strlen'));
}
$cfg = redis_hash_get($r, config_cluster_key());
$ttlCfg = redis_hash_get($r, config_actor_state_ttl_key());

/** 判断矩阵某格是否勾选: 仅有显式有向规则才勾选 (无规则 = 不连, 不再默认全互联) */
function link_checked(array $linkSet, int $from, int $to): bool
{
    if (isset($linkSet[$from])) {
        return in_array($to, $linkSet[$from], true);
    }
    return false;
}

layout_header('服务器基础配置', 'settings_server');
?>
<?php if ($notice !== ''): ?>
    <div class="alert <?= $noticeOk ? 'alert-success' : 'alert-warning' ?>"><?= esc($notice) ?></div>
<?php endif; ?>

<?php if ($r === null): ?>
    <div class="empty">⚠️ 无法连接 Redis Cluster，无法读取/保存配置。</div>
<?php else: ?>

<!-- ① 链接规则 LinkRule -->
<div class="card">
    <div class="card-header">链接规则 LinkRule <span class="dim"><?= esc(LINK_RULE_KEY) ?></span></div>
    <div class="card-body">
        <form method="post" class="form">
            <div class="table-responsive">
            <table class="table table-vcenter">
                <thead>
                    <tr>
                        <th style="width:120px">从 \ 到</th>
                        <?php foreach ($types as $to): ?>
                            <th class="text-center"><?= esc(PROCESS_TYPES[$to] ?? $to) ?><div class="dim small"><?= $to ?></div></th>
                        <?php endforeach; ?>
                    </tr>
                </thead>
                <tbody>
                <?php foreach ($types as $from): ?>
                    <tr>
                        <th><?= esc(PROCESS_TYPES[$from] ?? $from) ?><div class="dim small"><?= $from ?></div></th>
                        <?php foreach ($types as $to): ?>
                            <td class="text-center">
                            <?php if ($to === $from): ?>
                                <span class="dim">—</span>
                            <?php else: ?>
                                <input type="checkbox" class="form-check-input"
                                       name="link[<?= $from ?>][<?= $to ?>]"
                                       <?= link_checked($linkSet, $from, $to) ? 'checked' : '' ?>>
                            <?php endif; ?>
                            </td>
                        <?php endforeach; ?>
                    </tr>
                <?php endforeach; ?>
                </tbody>
            </table>
            </div>
            <div class="actions">
                <button class="btn btn-primary" type="submit" name="save_link" value="1">保存链接规则</button>
                <span id="link-save-status" class="dim" style="align-self:center"></span>
            </div>
            <div class="dim" style="margin-top:14px">
                说明：<b>有向链接</b>——勾选 = 「from 进程」连接「to 进程」，每条边只建<b>一条 TCP</b>（保证时序）。
                有向性：勾选 <code>gate→login</code> 只产生 gate 连 login 这一条连接，<b>不会</b>自动产生
                <code>login→gate</code>；如需双向，请分别勾选两格（会得到两条独立 TCP）。
                未配置 = 不连接；移除某条边后，对应 TCP 连接会在下轮发现时被断开。
                写入后由 <code>CLinkManager::DiscoverAndConnect()</code> 每 10 秒重新发现时生效。
            </div>
        </form>
    </div>
</div>

<!-- ② 全局配置 -->
<div class="card" style="margin-top:16px">
    <div class="card-header">全局配置 <span class="dim"><?= esc(config_cluster_key()) ?></span></div>
    <div class="card-body">
        <form method="post" class="form">
            <?php foreach ($FIELDS as $f => $meta): ?>
            <div class="form-row">
                <label><span><?= esc($meta['label']) ?></span>
                <?php if ($meta['type'] === 'select'): ?>
                    <select class="form-select" name="<?= esc($f) ?>">
                    <?php foreach ($meta['opts'] as $ov => $ol): ?>
                        <option value="<?= esc((string)$ov) ?>" <?= (string)($cfg[$f] ?? '') === (string)$ov ? 'selected' : '' ?>><?= esc((string)$ol) ?></option>
                    <?php endforeach; ?>
                    </select>
                <?php else: ?>
                    <input type="<?= esc($meta['type']) ?>" name="<?= esc($f) ?>" value="<?= esc($cfg[$f] ?? '') ?>">
                <?php endif; ?>
                </label>
            </div>
            <?php endforeach; ?>
            <div class="actions">
                <button class="btn btn-primary" type="submit" name="save_global" value="1">保存全局配置</button>
            </div>
            <div class="dim" style="margin-top:14px">
                说明：这些值由各进程周期性拉取（配 pub/sub 变更通知 <code>cluster:config:notify</code>），
                写后数秒内生效；<code>server.xml</code> 仅保留引导信息（Redis seed / 进程身份 / auth secret）。
            </div>
        </form>
    </div>
</div>

<!-- ③ Actor 状态失效时长 -->
<div class="card" style="margin-top:16px">
    <div class="card-header">Actor 状态失效时长（秒） <span class="dim"><?= esc(config_actor_state_ttl_key()) ?></span></div>
    <div class="card-body">
        <form method="post" class="form">
            <?php foreach (ACTOR_TYPES as $t => $name): ?>
            <div class="form-row">
                <label><span><?= esc($name) ?> <span class="dim small"><?= $t ?></span></span>
                    <input type="number" name="ttl_<?= $t ?>" min="1"
                           value="<?= esc($ttlCfg[(string)$t] ?? '') ?>" placeholder="86400">
                </label>
            </div>
            <?php endforeach; ?>
            <div class="actions">
                <button class="btn btn-primary" type="submit" name="save_actor_ttl" value="1">保存失效时长</button>
            </div>
            <div class="dim" style="margin-top:14px">
                说明：<b>Actor 状态快照</b> <code>actor:state:{type}:{id}</code> 在 Actor 到 Remove 状态之后的
                Redis 失效时长（秒）。留空 = 用默认 <code>86400</code>（24h）。仅多实例 Actor
                （Player/Battle/Spectator，id≠0）落状态快照；单例（id=0）不持久化。
                写后由服务端下次心跳/卸载时生效。
            </div>
        </form>
    </div>
</div>

<?php endif; ?>

<?php if ($r !== null): ?>
<script>
// 勾选后即时自动保存 (有向链接规则): 每次勾选/取消都立即 POST 全量勾选状态到本页,
// 避免「只勾选、忘记点保存」导致刷新后勾选丢失。显式「保存链接规则」按钮仍可用。
(function () {
    var boxes = Array.prototype.slice.call(document.querySelectorAll('input[name^="link["]'));
    var status = document.getElementById('link-save-status');
    if (!boxes.length || !status) return;

    function currentParams() {
        var p = new URLSearchParams();
        p.set('save_link', '1');
        boxes.forEach(function (cb) { if (cb.checked) p.set(cb.name, 'on'); });
        return p;
    }

    boxes.forEach(function (cb) {
        cb.addEventListener('change', function () {
            status.textContent = '保存中…';
            status.className = 'dim';
            fetch(location.href, {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: currentParams().toString()
            }).then(function (r) { return r.text(); }).then(function (html) {
                // 解析返回页面里的 .alert 提示, 避免匹配到本脚本源码里的相同字符串
                var failed = false;
                try {
                    var doc = new DOMParser().parseFromString(html, 'text/html');
                    var alert = doc.querySelector('.alert');
                    failed = alert ? alert.textContent.indexOf('保存失败') !== -1 : false;
                } catch (e) {}
                status.textContent = failed ? '保存失败' : '已自动保存';
                status.className = failed ? 'text-danger' : 'dim';
            }).catch(function () {
                status.textContent = '保存失败';
                status.className = 'text-danger';
            });
        });
    });
})();
</script>
<?php endif; ?>

<?php layout_footer(); ?>
