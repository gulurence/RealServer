<?php
// =============================================================================
// Lua 脚本热更文档 — RealServer
//
// 技术文档模块 / Lua 热更。静态文档页，无动态数据。
// 覆盖: 完整热更链路 (WebPlatform → Redis → 各进程)、二进制包格式、关键代码、
//       进程内托管 Actor 过滤、预检查、状态上报。
// =============================================================================
require_once __DIR__ . '/_layout.php';

layout_header('Lua 热更', 'docs_lua_hotfix');
?>
<div class="summary">
    <div class="stat-grid">
        <div class="stat"><div class="k">分发</div><div class="v">Redis<small>pub/sub 广播</small></div></div>
        <div class="stat"><div class="k">内容</div><div class="v">二进制包<small>内存 load 热更</small></div></div>
        <div class="stat"><div class="k">预检查</div><div class="v">语法校验<small>load "t" 不执行</small></div></div>
        <div class="stat"><div class="k">状态</div><div class="v">进程信息<small>lua_version 上报</small></div></div>
    </div>
    <p>
        Lua 热更是一条「<b>数据驱动</b>」链路：WebPlatform 把 <b>MD5 发生变化的脚本文件</b>打包成二进制，
        经 Redis 发布到全部进程；每个进程解包后<b>只热更自己托管的 Actor 类型逻辑</b>（先预检查语法、再按内存内容热更、
        成功后覆盖本地文件），并把结果（lua 版本 / 成功失败 / 报错）写回进程信息供界面展示。
    </p>
</div>

<!-- ================= 完整流程 ================= -->
<div class="card">
    <div class="card-header">完整流程 <span class="dim">从界面点「热更」到进程应用并上报</span></div>
    <div class="card-body">
        <pre class="doc-pre"><span class="m">WebPlatform</span> <span class="c">(script_reload.php)</span>
  1. 勾选文件 → 逐文件算 md5 → <span class="k">只打包「MD5 相对基线变化」的文件</span>
  2. build_hotfix_package():  <span class="k">"HFX1"</span> + version + count + 每文件[pathLen+path+contentLen+content]
  3. <span class="k">SET</span> cluster:hotfix:pkg:&lt;ver&gt;   <span class="c">(二进制包)</span>
  4. <span class="k">PUBLISH</span> cluster:gm:all  "hotfix &lt;ver&gt;"
        ↓
<span class="m">Redis</span> <span class="c">(集群, pub/sub 全局广播到 4 进程)</span>
        ↓
<span class="m">各进程</span> <span class="c">(GmCommandMgr subscriber 线程)</span>
  5. <span class="k">GET</span> cluster:hotfix:pkg:&lt;ver&gt; → <span class="m">UnpackHotfixPackage</span> 解包
  6. <span class="m">IsHostedActorFile</span> 过滤：只留本进程托管的 actors/&lt;dir&gt; 文件
  7. FNV-1a hash 门控：内容与上次成功应用一致 → 跳过 (noop)
  8. DispatchSync → <span class="m">LuaVM::HotReloadPrecheck</span>   <span class="c">(load "t" 语法预检查, 不执行)</span>
  9. DispatchSync → <span class="m">LuaVM::HotReloadFiles</span>       <span class="c">(内存 load "bt" 热更)</span>
 10. 成功后 <span class="m">WriteScriptFile</span> 覆盖本地文件 + 记录已应用 hash
 11. <span class="m">WriteHotfixStatus</span> 写回进程信息 (lua_version / hotfix_status / error / versions)
        ↓
<span class="m">Redis</span> cluster:process:{type}:{id}  <span class="c">(WebPlatform 节点状态/脚本热更页读取展示)</span></pre>
    </div>
</div>

<!-- ================= Redis 键 ================= -->
<div class="card">
    <div class="card-header">Redis 键约定</div>
    <div class="card-body">
        <div class="table-responsive"><table class="table table-vcenter doc-table">
            <thead><tr><th>键</th><th>类型</th><th>说明</th></tr></thead>
            <tbody>
                <tr><td>cluster:hotfix:pkg:&lt;ver&gt;</td><td>STRING</td><td>二进制包（HFX1 + 版本 + 文件内容）</td></tr>
                <tr><td>cluster:hotfix:baseline</td><td>HASH</td><td>relpath → md5 基线（界面「有变动」判定）</td></tr>
                <tr><td>cluster:hotfix:files</td><td>HASH</td><td>relpath → 最近发布内容（回滚取 before）</td></tr>
                <tr><td>cluster:hotfix:history</td><td>LIST</td><td>热更历史 JSON（before/after base64）</td></tr>
                <tr><td>cluster:hotfix:seq</td><td>INCR</td><td>单调版本号（打包/发布用）</td></tr>
                <tr><td>cluster:gm:all</td><td>pub/sub</td><td>命令通道，消息为 <code>hotfix &lt;ver&gt;</code></td></tr>
            </tbody>
        </table></div>
        <h3 class="doc-h3">进程信息字段（cluster:process:{type}:{id}，由 WriteHotfixStatus 写入）</h3>
        <div class="table-responsive"><table class="table table-vcenter doc-table">
            <thead><tr><th>字段</th><th>说明</th></tr></thead>
            <tbody>
                <tr><td>lua_version</td><td>当前脚本版本（最近一次整体成功的版本）</td></tr>
                <tr><td>hotfix_versions</td><td>已成功热更的版本列表（逗号分隔）</td></tr>
                <tr><td>hotfix_status</td><td>ok / partial / precheck_fail / noop</td></tr>
                <tr><td>hotfix_error</td><td>报错明细（预检查/应用/写盘失败的文件+原因）</td></tr>
                <tr><td>hotfix_last_ver / hotfix_ts</td><td>上次尝试版本 / 时间戳</td></tr>
            </tbody>
        </table></div>
    </div>
</div>

<!-- ================= 关键代码: 打包 ================= -->
<div class="card">
    <div class="card-header">关键代码 <span class="dim">① WebPlatform 打包（PHP）</span></div>
    <div class="card-body">
        <h3 class="doc-h3">二进制包格式</h3>
        <pre class="doc-pre"><span class="k">"HFX1"</span> | version(u32 LE) | count(u32 LE) | 每文件 [ pathLen(u32 LE) + path + contentLen(u32 LE) + content ] ...</pre>
        <h3 class="doc-h3">script_reload.php · build_hotfix_package()</h3>
        <pre class="doc-pre">function build_hotfix_package(int $ver, array $files): string
{
    $bin = 'HFX1' . pack('V', $ver) . pack('V', count($files));
    foreach ($files as $f) {
        $bin .= pack('V', strlen($f[0])) . $f[0];   <span class="c">// pathLen + path</span>
        $bin .= pack('V', strlen($f[1])) . $f[1];   <span class="c">// contentLen + content</span>
    }
    return $bin;
}

<span class="c">// 只打包 MD5 变化的文件; 随后 SET pkg + PUBLISH</span>
$base = $r-&gt;hget(HF_BASELINE_KEY, $rel);
if ($base !== false &amp;&amp; (string)$base === md5($content)) continue;  <span class="c">// 未变化跳过</span>
$packFiles[] = [$rel, $content];
$r-&gt;set(HF_PKG_KEY . $ver, build_hotfix_package((int)$ver, $packFiles));
$r-&gt;publish(HF_CHANNEL, 'hotfix ' . $ver);</pre>
    </div>
</div>

<!-- ================= 关键代码: 解包/过滤 ================= -->
<div class="card">
    <div class="card-header">关键代码 <span class="dim">② 进程侧解包 + 托管过滤（C++）</span></div>
    <div class="card-body">
        <h3 class="doc-h3">GmCommandMgr.cpp · UnpackHotfixPackage()</h3>
        <pre class="doc-pre">bool UnpackHotfixPackage(const std::string &amp;blob,
                         std::vector&lt;std::pair&lt;std::string, std::string&gt;&gt; &amp;files)
{
    if (blob.size() &lt; 4 || blob.compare(0, 4, "HFX1") != 0) return false;
    size_t off = 4;
    auto rd32 = [&amp;](uint32 &amp;v) -&gt; bool {           <span class="c">// 读 4 字节小端</span>
        if (off + 4 &gt; blob.size()) return false;
        v = (uint32)(uint8)blob[off] | ((uint32)(uint8)blob[off+1] &lt;&lt; 8)
          | ((uint32)(uint8)blob[off+2] &lt;&lt; 16) | ((uint32)(uint8)blob[off+3] &lt;&lt; 24);
        off += 4; return true;
    };
    uint32 ver = 0, count = 0;
    if (!rd32(ver) || !rd32(count)) return false;
    for (uint32 i = 0; i &lt; count; ++i) {
        uint32 plen = 0, clen = 0;
        if (!rd32(plen)) return false;
        std::string path(blob.data() + off, plen); off += plen;
        if (!rd32(clen)) return false;
        std::string content(blob.data() + off, clen); off += clen;
        files.emplace_back(std::move(path), std::move(content));
    }
    return true;
}</pre>
        <h3 class="doc-h3">GmCommandMgr.cpp · IsHostedActorFile() —— 只热更自己托管的 Actor</h3>
        <pre class="doc-pre">bool IsHostedActorFile(const std::string &amp;rel, const std::vector&lt;EActorType&gt; &amp;hostedTypes)
{
    if (rel.rfind("actors/", 0) != 0) return true;   <span class="c">// common/ 等公共模块所有进程都加载</span>
    std::string dir = rel.substr(7);                 <span class="c">// 去掉 "actors/"</span>
    dir = dir.substr(0, dir.find_first_of("/\\"));
    EActorType t = ActorTypeFromDirName(dir);        <span class="c">// "chat" → EActorType_AChat</span>
    if (t == EActorType_None) return true;
    for (EActorType h : hostedTypes) if (h == t) return true;
    return false;                                   <span class="c">// 非本进程托管 → 丢弃, 不预检/不热更/不写盘</span>
}</pre>
    </div>
</div>

<!-- ================= 关键代码: 热更主流程 ================= -->
<div class="card">
    <div class="card-header">关键代码 <span class="dim">③ 热更主流程（GmCommandMgr.cpp · hotfix 分支）</span></div>
    <div class="card-body">
        <pre class="doc-pre"><span class="c">// 5. 解包</span>
std::vector&lt;std::pair&lt;std::string, std::string&gt;&gt; files;
UnpackHotfixPackage(blob, files);

<span class="c">// 6. 托管过滤</span>
std::vector&lt;EActorType&gt; hosted = GetHostedActorTypes(m_selfType);
for (auto &amp;f : files) if (IsHostedActorFile(f.first, hosted)) relevant.push_back(std::move(f));

<span class="c">// 7. FNV-1a hash 门控</span>
for (auto &amp;f : relevant) {
    std::string hash = Fnv1a64Hex(f.second);
    if (m_hotfixAppliedHash[f.first] == hash) { skipped++; continue; }
    changed.push_back(std::move(f));
}

<span class="c">// 8. 预检查 (调度线程, load "t" 只编译)</span>
auto precheck = DispatchSync([&amp;]{ return LuaVM::instance()-&gt;HotReloadPrecheck(changed); });
WriteHotfixStatus(cli, selfType, selfPid, version,
                  precheck.first ? "precheck_ok" : "precheck_fail", precheck.second, false);
if (!precheck.first) return "hotfix PRECHECK FAILED ...";

<span class="c">// 9. 热更 (调度线程, 内存 load "bt" 原地打补丁)</span>
std::vector&lt;std::string&gt; okPaths =
    DispatchSync([&amp;]{ return LuaVM::instance()-&gt;HotReloadFiles(changed); });

<span class="c">// 10. 成功才写盘 + 记录 hash (失败文件不落盘, 便于重试)</span>
for (const std::string &amp;p : okPaths) if (WriteScriptFile(root, p, content))
    m_hotfixAppliedHash[p] = hash;

<span class="c">// 11. 状态上报 (ok / partial)</span>
WriteHotfixStatus(cli, selfType, selfPid, version, fullOk ? "ok" : "partial", error, fullOk);</pre>
        <div class="dim">
            线程模型：<code>hotfix</code> 命令在 subscriber 线程执行 Redis 拉包/解包/写盘；触碰唯一 lua_State 的
            预检查与热更经 <code>CActorScheduler::DispatchSync</code> 投递到调度线程串行执行（进程唯一 lua_State 约束）。
        </div>
    </div>
</div>

<!-- ================= 关键代码: Lua 核心 ================= -->
<div class="card">
    <div class="card-header">关键代码 <span class="dim">④ Lua 热更核心（script/common/hotfix.lua，源自 LuaHotFix）</span></div>
    <div class="card-body">
        <h3 class="doc-h3">内存热更（不读盘）</h3>
        <pre class="doc-pre">function HotFix:BuildChunkFromSource(chunkname, source, isNotRetFlag)
    HotFix.fakeEnv = fakeEnv:New()
    local chunk, err = load(source, chunkname, "bt", HotFix.fakeEnv)  <span class="c">-- 隔离 env 内存 load</span>
    if not chunk then ERROR("BuildChunkFromSource failed: %s", err) return nil end
    ...
end

function HotFix:UpdateModuleFromSource(modname, source)
    local oldChunk = package.loaded[modname]
    HotFix:BuildChunkFromSource("@" .. modname, source, type(oldChunk) == "boolean")
    HotFix:CheckChunkType(oldChunk, ckOrBool)  <span class="c">-- upvalue join 原位打补丁</span>
end</pre>
        <h3 class="doc-h3">数据驱动入口（C++ 调用）</h3>
        <pre class="doc-pre">function HotFix.UpdateFilesData(entries)          <span class="c">-- 返回 { [relpath] = boolean }</span>
    for _, e in ipairs(entries) do
        local rel, source = tostring(e[1]), e[2]
        local mod = rel:gsub("%.lua$", ""):gsub("[/\\]", ".")  <span class="c">-- relpath → 模块名</span>
        if package.loaded[mod] == nil then results[rel] = true  <span class="c">-- 本进程未加载则跳过</span>
        else results[rel] = pcall(UpdateModuleFromSource, mod, source) end
    end
    return results
end

function HotFix.PrecheckFilesData(entries)        <span class="c">-- 预检查: 只编译不执行</span>
    for _, e in ipairs(entries) do
        local chunk, err = load(e[2], "=" .. e[1], "t")
        if not chunk then errors[e[1]] = tostring(err) end
    end
    return { ok = next(errors) == nil, errors = errors }
end</pre>
        <div class="dim">
            关键点：actor 模块是「布尔模块」（无 return，<code>package.loaded[...]==true</code>），LuaHotFix 的 upvalue 保活对其
            <b>不生效</b>；真正热更效果来自 <code>class.lua define()</code> 原地改类表 + <code>actor_mgr</code> 幂等注册，
            单例态必须放 <code>xEngine.*_state</code>（跨热更保留）。框架核心文件（class/include/hotfix/actor_mgr）标灰需重启。
        </div>
    </div>
</div>

<?php layout_footer(); ?>
