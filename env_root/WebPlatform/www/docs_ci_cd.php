<?php
// =============================================================================
// CI/CD 流水线文档 — RealServer
//
// 技术文档模块 / CI/CD。静态文档页，无动态数据。
// 覆盖: 本地 Jenkins + Gitea 搭建、AI 改码 → 人工确认提交 → 触发 CI/CD、
//       二进制变更走灰度更新(自动提升版本)、纯 Lua 变更走热更、WebPlatform 灰度界面。
// =============================================================================
require_once __DIR__ . '/_layout.php';

layout_header('CI/CD 流水线', 'docs_ci_cd');
?>
<div class="summary">
    <div class="stat-grid">
        <div class="stat"><div class="k">代码托管</div><div class="v">Gitea<small>本地 Git 服务</small></div></div>
        <div class="stat"><div class="k">构建编排</div><div class="v">Jenkins<small>2.580.1 war</small></div></div>
        <div class="stat"><div class="k">二进制变更</div><div class="v">灰度更新<small>时间戳+md5 版本</small></div></div>
        <div class="stat"><div class="k">Lua 变更</div><div class="v">热更<small>不停机</small></div></div>
    </div>
    <p>
        服务器改动统一走「<b>本地 Git (Gitea)</b> → <b>人工确认提交</b> → <b>Jenkins 触发 CI/CD</b>」。
        Jenkins 按<b>变更类型</b>分流：<b>进程二进制</b>变化走灰度更新（构建时自动生成「时间戳.md5」版本）；
        <b>纯 Lua</b>变化走 Lua 热更。全程由 WebPlatform 的「灰度更新 / 脚本热更」两个界面可视与下发。
    </p>
</div>

<!-- ================= 完整流程 ================= -->
<div class="card">
    <div class="card-header">完整流程 <span class="dim">七步 CI/CD 工作流</span></div>
    <div class="card-body">
        <pre class="doc-pre"><span class="m">开发机 (AI + 人工)</span>
  1. AI 改完代码 (C++/SWIG 或 Lua) → 生成改动集
  2. <span class="k">自动 git add/commit 到本地</span> → <span class="c">推送前必须经人工确认 (user confirm)</span>
        ↓
<span class="m">Gitea</span> <span class="c">(本地 Git 服务, 仓库唯一真源)</span>
  3. 人工确认后 push 到 Gitea → <span class="k">webhook</span> 通知 Jenkins
        ↓
<span class="m">Jenkins</span> <span class="c">(CI/CD 编排)</span>
  4. 触发方式二选一:  <span class="k">Gitea webhook 自动</span> / <span class="k">人工手动 Build</span> (由人决定)
  5. Pipeline: checkout → 变更分类 → 构建 → 测试 → 产物
     ├─ <span class="c">C++/SWIG 变更</span> → 二进制变化 → <span class="k">构建后计算 时间戳.md5 版本</span> → 打包 exe
     └─ <span class="c">仅 Lua 变更</span> → 生成 Lua 热更包 (MD5 差异文件)
        ↓
<span class="m">部署</span> <span class="c">(WebPlatform 下发)</span>
  6. 二进制 → Jenkins 构建完 <span class="k">POST api/build.php 登记版本+产物</span>（LPUSH cluster:build:versions）
             → 灰度更新面板选类型/版本 → 「更新」按钮 → api/gray_update.php 写灰度配置并调 gray_roll.ps1 分批滚动
     Lua    → <span class="k">脚本热更</span>: script_reload.php 打包 → Redis → 各进程内存热更 + 写盘
        ↓
<span class="m">WebPlatform</span> <span class="c">(全程可视)</span>
  7. 「灰度更新」页: 所有进程版本 / 灰度状态 / Actor 负载
     「脚本热更」页: 文件树 / 有变动 / 热更历史 / 回滚</pre>
    </div>
</div>

<!-- ================= 变更分类 ================= -->
<div class="card">
    <div class="card-header">变更分类 <span class="dim">决定走灰度还是热更</span></div>
    <div class="card-body">
        <div class="table-responsive"><table class="table table-vcenter doc-table">
            <thead><tr><th>改动范围</th><th>是否变二进制</th><th>流程</th><th>版本动作</th></tr></thead>
            <tbody>
                <tr><td><code>engine/**</code>、<code>shared/battlecore</code>、SWIG <code>.i</code></td><td><span class="badge bg-red">是</span></td><td>灰度更新</td><td>生成「时间戳.md5」版本</td></tr>
                <tr><td><code>source/proto-def/*.proto</code>（重跑 make.bat 生成 C++）</td><td><span class="badge bg-red">是</span></td><td>灰度更新</td><td>生成「时间戳.md5」版本</td></tr>
                <tr><td><code>script/**</code>（Lua 业务逻辑）</td><td><span class="badge bg-green">否</span></td><td>Lua 热更</td><td>热更版本 <code>cluster:hotfix:seq</code> 自增</td></tr>
                <tr><td>框架核心 Lua（class/include/hotfix/actor_mgr）</td><td><span class="badge bg-yellow">需重启</span></td><td>跟随灰度</td><td>生成「时间戳.md5」版本</td></tr>
            </tbody>
        </table></div>
        <div class="dim">
            判定规则：变更集里只要命中 <code>engine/**</code> / <code>shared/**</code> / <code>*.proto</code> / <code>*.i</code>
            任一即视为「二进制变更」，否则按「纯 Lua」处理。框架核心 Lua（class/include/hotfix/actor_mgr）不可热更，必须随二进制灰度。
        </div>
    </div>
</div>

<!-- ================= 基础设施 ================= -->
<div class="card">
    <div class="card-header">基础设施 <span class="dim">CI/CD 组件清单</span></div>
    <div class="card-body">
        <div class="table-responsive"><table class="table table-vcenter doc-table">
            <thead><tr><th>组件</th><th>版本 / 位置</th><th>职责</th><th>现状</th></tr></thead>
            <tbody>
                <tr><td>Gitea</td><td><code>env_root/tools/gitea-28.1.0-windows-amd64.exe</code></td><td>本地 Git 服务（仓库托管 / webhook）</td><td><span class="badge bg-green">已接线</span> http://127.0.0.1:3000 · 仓库 realserver/RealServer（私有）</td></tr>
                <tr><td>Jenkins</td><td><code>env_root/tools/jenkins-2.580.1.war</code> + JDK 21</td><td>CI/CD 编排（pull/构建/灰度下发）</td><td><span class="badge bg-green">已接线</span> http://127.0.0.1:8081 · job RealServer（零插件 Freestyle）</td></tr>
                <tr><td>Redis Cluster</td><td><code>env_root/RedisCluster/</code> (7000-7005)</td><td>灰度配置 / 热更包 / 服务发现</td><td><span class="badge bg-green">已运行</span></td></tr>
                <tr><td>MySQL</td><td><code>env_root/Mysql/</code> (3306)</td><td>持久化（xRecord 回源）</td><td><span class="badge bg-green">已运行</span></td></tr>
                <tr><td>WebPlatform</td><td><code>env_root/WebPlatform/</code> (8080)</td><td>控制台（灰度更新 / 脚本热更 UI）</td><td><span class="badge bg-green">已运行</span></td></tr>
                <tr><td>NodeManager</td><td><code>engine/node_manager/</code></td><td>Leader 读灰度配置、分批滚动重启子进程</td><td><span class="badge bg-green">已实现</span></td></tr>
                <tr><td>RealServer 进程</td><td>单 exe 多进程（Gate/Login/Global/Game）</td><td>业务运行时 + GmCommandMgr 订阅热更</td><td><span class="badge bg-green">已运行</span></td></tr>
            </tbody>
        </table></div>
        <div class="dim">
            Gitea / Jenkins 已在本机接线并运行（均只监听 127.0.0.1，仅本地可达）。依赖自包含在
            <code>env_root/tools/</code>（JDK 21 → <code>tools/jdk</code>、MinGit → <code>tools/git</code>），
            该目录已 gitignore。启动：<code>tools/start-gitea.ps1</code>、<code>tools/start-jenkins.ps1</code>。
            首个 push 需人工确认（见「落地清单」），确认后即可走通完整链路。
        </div>
    </div>
</div>

<!-- ================= 版本模型 ================= -->
<div class="card">
    <div class="card-header">版本模型 <span class="dim">进程二进制版本 = 时间戳.md5</span></div>
    <div class="card-body">
        <pre class="doc-pre"><span class="c">// 进程二进制版本格式: 年.月.日.时.分.秒.毫秒.二进制md5(前12位)</span>
<span class="c">// 例: 2026.10.11.22.07.01.1000.afgjladjfgl</span>

<span class="c">// md5 是构建产物哈希, 无法作为编译期常量烤进二进制(自引用), 故由构建侧计算:</span>
<span class="c">// build_ci.ps1 编译完成后:</span>
$ts  = Get-Date -Format 'yyyy.MM.dd.HH.mm.ss.fff'
$md5 = (Get-FileHash $exe -Algorithm MD5).Hash.Substring(0,12).ToLower()
$ver = "$ts.$md5"                         <span class="c">// 完整版本串</span>

<span class="c">// 运行时通过 --version=$ver 传入进程, 进程创建时写入 Redis 进程信息:</span>
<span class="c">// engine/xCluster/xProcessManager.cpp — CreateProcess</span>
m_stSelfInfo.version = cfg.process.version.empty() ? kProcessVersion : cfg.process.version;
<span class="c">// 每 5s 心跳 disc.UpdateVersion(m_stSelfInfo.version, contentVersion) → hset version</span></pre>
        <div class="table-responsive"><table class="table table-vcenter doc-table">
            <thead><tr><th>版本</th><th>载体</th><th>变更触发</th><th>消费方</th></tr></thead>
            <tbody>
                <tr><td>进程二进制版本</td><td><code>--version</code> 参数 → <code>cluster:process:{type}:{id}.version</code></td><td>每次构建（时间戳+md5 天然唯一）</td><td>灰度面板「最新/旧版本」判定 + NodeManager「上报 version ≠ 目标」比对</td></tr>
                <tr><td>构建版本索引</td><td><code>cluster:build:versions</code>（LIST）</td><td>Jenkins 构建完 api/build.php LPUSH</td><td>灰度面板「版本列表框」</td></tr>
                <tr><td>内容版本</td><td><code>contentVersion</code>（ProcessInfo）</td><td>内容灰度下发</td><td>脚本层按版本目录切流</td></tr>
                <tr><td>Lua 脚本版本</td><td><code>cluster:process:*:lua_version</code></td><td>Lua 热更成功</td><td>脚本热更界面 / 节点状态展示</td></tr>
            </tbody>
        </table></div>
        <div class="dim">
            无 <code>--version</code> 直跑 <code>RealServer.exe</code>（本地 Debug）时，回退到编译期
            <code>kProcessVersion</code>（git 短哈希+时间），行为不变。
        </div>
    </div>
</div>

<!-- ================= 灰度机制 ================= -->
<div class="card">
    <div class="card-header">关键代码 <span class="dim">① 灰度更新（NodeManager + Redis）</span></div>
    <div class="card-body">
        <h3 class="doc-h3">Redis 灰度配置键</h3>
        <div class="table-responsive"><table class="table table-vcenter doc-table">
            <thead><tr><th>键</th><th>类型</th><th>字段</th></tr></thead>
            <tbody>
                <tr><td><code>cluster:update:process:config</code></td><td>HASH</td><td>version / batch_size / status(idle|rolling|paused|done) / note</td></tr>
                <tr><td><code>cluster:update:process:log</code></td><td>LIST</td><td>滚动阶段日志</td></tr>
                <tr><td><code>cluster:build:versions</code></td><td>LIST</td><td>已构建版本索引（api/build.php LPUSH，最新在前）</td></tr>
                <tr><td><code>cluster:update:content:config</code></td><td>HASH</td><td>version / gray_percent / status / note</td></tr>
            </tbody>
        </table></div>
        <h3 class="doc-h3">NodeManager.cpp · Leader 滚动重启逻辑（摘录）</h3>
        <pre class="doc-pre"><span class="c">// 读 cluster:update:process:config</span>
<span class="c">// 收集「上报 version != 目标 version」的子进程</span>
<span class="c">// 按 batch_size 分批 graceful 重启 → 每批确认 Running 后再滚下一批</span>
for (auto &amp;child : children) {
    if (child.reportedVersion != targetVersion) {
        rolling.push_back(child);       <span class="c">// 待滚动替换的进程</span>
    }
}
<span class="c">// status=rolling 时按 batch 替换 exe → 拉起新版本 → 状态写回 log</span></pre>
        <div class="dim">
            dev 环境 <code>bin/windows/start_cluster.ps1</code> 直接 <code>Start-Process</code>（绕过 NodeManager），
            优先挑选 <code>releases/RealServer_&lt;version&gt;.exe</code> 最新一个并以 <code>--version</code> 拉起。
            dev 的滚动由 <code>bin/windows/gray_roll.ps1</code> 完成：按 <code>-Type</code> 过滤进程、
            按 batch_size 分批 <code>Stop-Process</code> 旧进程 → 用同 <code>--type/--name/--platform/--version</code>
            参数从版本化 exe 拉起（exe 文件名解析出 <code>--version</code>）。生产（NodeManager 托管）仍走
            Leader 的 <code>ProcessUpdateGray</code> 灰度比对。
        </div>
    </div>
</div>

<!-- ================= Jenkins 构建 ================= -->
<div class="card">
    <div class="card-header">关键代码 <span class="dim">② Jenkins 构建（零插件 Freestyle + build_ci.ps1）</span></div>
    <div class="card-body">
        <h3 class="doc-h3">job 配置 <span class="dim">env_root/tools/jenkins-home/jobs/RealServer/config.xml</span></h3>
        <pre class="doc-pre"><span class="c">&lt;!-- 零插件: NullSCM 不 checkout, 直接用 E:\RealServer 现有树 (规避 third_part/proto-cpp 未入库) --&gt;</span>
&lt;scm class="hudson.scm.NullSCM"/&gt;
&lt;authToken&gt;REALSERVER_CI_TOKEN&lt;/authToken&gt;      <span class="c">&lt;!-- 远程触发令牌 --&gt;</span>

<span class="c">&lt;!-- 构建步骤 BatchFile: 前置 git/jdk 到 PATH, 再跑 build_ci.ps1 --&gt;</span>
set PATH=E:\RealServer\env_root\tools\git\cmd;E:\RealServer\env_root\tools\jdk\bin;%PATH%
powershell -NoProfile -ExecutionPolicy Bypass -File E:\RealServer\build\build_ci.ps1</pre>

        <h3 class="doc-h3">build_ci.ps1 <span class="dim">build/build_ci.ps1（构建 + 登记，独立可跑）</span></h3>
        <pre class="doc-pre"><span class="c">// 1. 拉取 Gitea 最新代码</span>
git pull gitea master

<span class="c">// 2. 重新 configure（重读 git → 重新生成 xVersion.h 的 GIT_VERSION，仅作无 --version 时的回退）</span>
cmake -S E:\RealServer -B build_msvc -G "Visual Studio 17 2022" -A x64 -DVCPKG_INSTALLED_ROOT=E:/RealServer/third_part/_install

<span class="c">// 3. 编译 Release</span>
cmake --build build_msvc --config Release --target RealServer

<span class="c">// 4. 计算版本 = 构建时间戳 + 产物 md5(前12位)</span>
$ts  = Get-Date -Format 'yyyy.MM.dd.HH.mm.ss.fff'
$md5 = (Get-FileHash $exe -Algorithm MD5).Hash.Substring(0,12).ToLower()
$ver = "$ts.$md5"

<span class="c">// 5. 拷贝 exe → bin/windows/staging/RealServer.exe</span>

<span class="c">// 6. POST http://127.0.0.1:8080/api/build.php  {token, version, artifact}</span>
<span class="c">    (只登记: 拷 releases/RealServer_<version>.exe + LPUSH cluster:build:versions, 不滚动)</span></pre>

        <h3 class="doc-h3">触发方式（人工决定）</h3>
        <pre class="doc-pre">默认:  手动 Build Now（登录 Jenkins ci/ci → job RealServer → Build Now）
可选:  远程触发  POST http://127.0.0.1:8081/job/RealServer/build?token=REALSERVER_CI_TOKEN
       (Gitea webhook 可配此 URL, 事件=push, 实现 push 后自动触发)</pre>
        <div class="dim">
            按约定「push 之后是否自动触发 Jenkins、还是人手动 Build，由人决定」：本地 Jenkins 已开启安全（CSRF 开、需登录），
            故默认走人工 Build Now；需要自动触发时再挂 Gitea webhook 到上面的远程触发 URL。AI 只做本地 commit，
            push 到 Gitea 前由人确认。
        </div>
    </div>
</div>

<!-- ================= WebPlatform UI ================= -->
<div class="card">
    <div class="card-header">关键代码 <span class="dim">③ WebPlatform 灰度界面（gray_update.php 五部分）</span></div>
    <div class="card-body">
        <pre class="doc-pre"><span class="c">// 前端 fetch api/gray_update.php?type=&lt;type&gt;[&amp;target=&lt;ver&gt;] 取数据, 五部分渲染:</span>
<span class="c">// ① 统计区   进程总数 / 更新完成数 / 等待更新数 / 完成百分比 (对所选类型+所选版本)</span>
<span class="c">// ② 类型下拉 PROCESS_TYPES (100 Gate / 101 Login / 102 Global / 201 Game)</span>
<span class="c">// ③ 版本列表框 LRANGE cluster:build:versions (最新在前, 可选中目标版本)</span>
<span class="c">// ④ 进程列表框 smembers cluster:process:by_type:{type} → hgetall, 逐行显示</span>
<span class="c">//      当前版本 + 最新(绿)/旧版本(黄) 徽标 (version == latest 判最新)</span>
<span class="c">// ⑤ 更新按钮 POST api/gray_update.php {type, version, batch_size}</span>

<span class="c">// api/gray_update.php POST: 写灰度配置 → 同步调 gray_roll.ps1 分批滚动</span>
redis_hash_set($r, 'cluster:update:process:config', [
    'version'    =&gt; $version,
    'batch_size' =&gt; $batchSize,
    'status'     =&gt; 'rolling',
    'note'       =&gt; 'panel gray update ' . date('Y-m-d H:i:s'),
]);
exec('powershell -NoProfile -ExecutionPolicy Bypass -File gray_roll.ps1'
   . ' -Type ' . $type . ' -ExePath releases/RealServer_' . $version . '.exe'
   . ' -BatchSize ' . $batchSize, $output, $code);
<span class="c">// 成功 status=done / 失败 status=paused, RPUSH cluster:update:process:log</span></pre>
        <div class="dim">
            页面五部分：统计（进程总数/更新完成数/等待更新数/完成百分比）、进程类型下拉列、版本列表框
            （Jenkins 构建好的版本，最新在前）、进程列表框（当前版本 + 最新/旧版本说明）、更新按钮。
            类型/版本切换即 fetch 联动刷新。滚动为同步执行（batch_size=1 时每进程约 5s，按钮点击后阻塞数秒）。
            登记构建由 <code>api/build.php</code> 负责（拷 releases/ + LPUSH 版本索引），与滚动解耦。
        </div>
    </div>
</div>

<!-- ================= 落地清单 ================= -->
<div class="card">
    <div class="card-header">落地清单 <span class="dim">已完成 / 待接线</span></div>
    <div class="card-body">
        <div class="table-responsive"><table class="table table-vcenter doc-table">
            <thead><tr><th>步骤</th><th>内容</th><th>状态</th></tr></thead>
            <tbody>
                <tr><td>1</td><td>本地搭建 Jenkins + Git 服务</td><td><span class="badge bg-green">已接线</span> Gitea:3000 / Jenkins:8081（JDK+MinGit 自包含于 tools/）</td></tr>
                <tr><td>2</td><td>AI 改码 → 本地 commit → 人工确认 → push</td><td><span class="badge bg-green">脚本已接</span> build_ci.ps1 git pull gitea；首 push 待人工确认</td></tr>
                <tr><td>3</td><td>人工决定 Jenkins 自动触发 / 手动发起</td><td><span class="badge bg-green">手动 Build Now 已就绪</span>；webhook 自动触发可选（远程令牌）</td></tr>
                <tr><td>4</td><td>二进制变更 → 灰度更新 + 时间戳.md5 版本</td><td><span class="badge bg-green">灰度机制已实现（--version + 版本索引 + NodeManager）</span></td></tr>
                <tr><td>5</td><td>灰度更新界面：统计/类型/版本列表/进程列表/更新按钮</td><td><span class="badge bg-green">gray_update.php 五部分已落地</span></td></tr>
                <tr><td>6</td><td>纯 Lua 更新 → Lua 热更流程</td><td><span class="badge bg-green">已落地（见「Lua 热更」文档）</span></td></tr>
                <tr><td>7</td><td>「更新」模块重构为灰度更新 + 脚本热更</td><td><span class="badge bg-green">已完成</span></td></tr>
            </tbody>
        </table></div>
        <div class="dim">
            剩余主线：首次 <code>git push gitea master</code>（人工确认）→ 触发 Jenkins Build →
            走 build_ci.ps1（pull → configure → Release 编译 → 算 时间戳.md5 版本 → POST api/build.php 登记）→
            灰度面板选类型/版本 → 更新按钮 → api/gray_update.php 写灰度配置并调 gray_roll.ps1 分批滚动 →
            面板统计变为 100%。纯 Lua 变更仍走「Lua 热更」文档描述的独立热更链路。
        </div>
    </div>
</div>

<?php layout_footer(); ?>
