<?php
// =============================================================================
// 服务器游戏配置 — 游戏玩法相关可调参数 (PlayerActor 上限 / 登录排队)
//
//   player_actor_cap → cluster:config:game Hash (集群 PlayerActor 数量上限, 0=不限)
//
// 与 script/actors/login/login.lua 的登录排队逻辑配合: Login 进程登录时读
// cluster:load:player_count (PlayerActor 总数, 由 CPlayerActorIndex 建/卸时 INCR/DECR)
// 与 player_actor_cap 比较, 满则入队 queue:login, 按平均等待时间估算排队时长。
// =============================================================================
require_once __DIR__ . '/_layout.php';

$r = redis();
$notice = '';
$noticeOk = true;

$GAME_FIELDS = [
    'player_actor_cap' => ['label' => 'PlayerActor 数量上限 (0=不限)', 'type' => 'number'],
];

if (isset($_POST['save_game'])) {
    if ($r === null) {
        $notice = '无法连接 Redis，游戏配置未保存';
        $noticeOk = false;
    } else {
        $data = [];
        foreach (array_keys($GAME_FIELDS) as $f) {
            $data[$f] = post_str($f);
        }
        redis_hash_set($r, config_game_key(), $data);
        $notice = '游戏配置已写入 ' . config_game_key();
    }
}

$cfg = redis_hash_get($r, config_game_key());

layout_header('服务器游戏配置', 'settings_game');
?>
<?php if ($notice !== ''): ?>
    <div class="alert <?= $noticeOk ? 'alert-success' : 'alert-warning' ?>"><?= esc($notice) ?></div>
<?php endif; ?>

<?php if ($r === null): ?>
    <div class="empty">⚠️ 无法连接 Redis Cluster，无法读取/保存配置。</div>
<?php else: ?>

<div class="card">
    <div class="card-header">登录排队 <span class="dim"><?= esc(config_game_key()) ?></span></div>
    <div class="card-body">
        <form method="post" class="form">
            <?php foreach ($GAME_FIELDS as $f => $meta): ?>
            <div class="form-row">
                <label><span><?= esc($meta['label']) ?></span>
                    <input type="<?= esc($meta['type']) ?>" name="<?= esc($f) ?>" min="0"
                           value="<?= esc($cfg[$f] ?? '') ?>" placeholder="0">
                </label>
            </div>
            <?php endforeach; ?>
            <div class="actions">
                <button class="btn btn-primary" type="submit" name="save_game" value="1">保存游戏配置</button>
            </div>
            <div class="dim" style="margin-top:14px">
                说明：<code>player_actor_cap</code> 为集群 <b>PlayerActor 总负载上限</b>，
                <code>0</code> 表示不限。登录时若 <code>cluster:load:player_count</code> 已达上限，
                客户端将进入<b>登录排队</b>，按「平均等待时间」动态估算排队时长并展示。
            </div>
        </form>
    </div>
</div>

<?php endif; ?>

<?php layout_footer(); ?>
