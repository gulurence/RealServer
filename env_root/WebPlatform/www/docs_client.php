<?php
// =============================================================================
// 客户端架构文档 — 《灵魂之枪》(Soul of Gun)
//
// 技术文档模块 / 客户端。静态文档页，无动态数据。
// 覆盖: 整体架构分层、一级模块、二级模块、网络层、共享战斗核心、配置系统、关键流程。
// =============================================================================
require_once __DIR__ . '/_layout.php';

layout_header('客户端架构', 'docs_client');
?>
<div class="summary">
    <div class="stat-grid">
        <div class="stat"><div class="k">引擎</div><div class="v">Cocos2d-x<small>C++ / OpenGL</small></div></div>
        <div class="stat"><div class="k">游戏类型</div><div class="v">2D 射击<small>僵尸波次 PVE + PVP</small></div></div>
        <div class="stat"><div class="k">后端</div><div class="v">RealServer<small>Gate TCP + HTTP 登录</small></div></div>
        <div class="stat"><div class="k">战斗模型</div><div class="v">帧同步<small>确定性 + 服务端权威</small></div></div>
    </div>
    <p>
        客户端是基于 <b>Cocos2d-x</b>（C++）的 2D 横版僵尸射击游戏《灵魂之枪》(Soul of Gun)，
        工程代码位于 <code>client/soulofgun</code>（引擎整包），游戏本体代码在
        <code class="doc-path">samples/HelloCpp/Classes/</code>（项目沿用 HelloCpp 模板名，运行日志/崩溃日志以
        <code>sog_</code> 前缀标识）。客户端通过 Gate 的 TCP 长连接与服务端通信，账号密码登录走
        WebPlatform 的 HTTP 接口；战斗采用<b>确定性帧同步 + 服务端权威</b>模型，其战斗核心
        <code>battlecore</code> 与配置系统 <code>config</code> 位于 <code class="doc-path">E:\RealServer\shared</code>，为 C/S 共享代码。
    </p>
</div>

<!-- ================= 分层架构 ================= -->
<div class="card">
    <div class="card-header">整体架构分层</div>
    <div class="card-body">
        <pre class="doc-pre">┌────────────────────────────────────────────────────────────────────────┐
│  界面层 UI/场景   StageScene · HudLayer · UILobby / UILogin / UIStore…  │
├────────────────────────────────────────────────────────────────────────┤
│  玩法层 Gameplay  Hero · Bullet · EnemyWaveManager · Sp01 · WeaponMgr   │
├────────────────────────────────────────────────────────────────────────┤
│  共享战斗核心     shared/battlecore（确定性帧同步模拟，C/S 共享）        │
├────────────────────────────────────────────────────────────────────────┤
│  网络层 net/      NetClient + ProtoCodec + 帧协议（TCP → Gate）         │
├────────────────────────────────────────────────────────────────────────┤
│  引擎层 Engine    Cocos2d-x（渲染/场景/动作/物理/音频/资源）             │
├────────────────────────────────────────────────────────────────────────┤
│  平台层 Platform  Win32 / Android / iOS（CCEGLView / JNI）              │
└────────────────────────────────────────────────────────────────────────┘</pre>
        <div class="dim">
            依赖方向自上而下：UI 调用玩法与战斗核心，战斗核心与网络层交互；引擎层为底层支撑。
            战斗核心 / 配置系统从 <code>shared/</code> 源码编译进客户端，与服务端保证逐位一致的确定性。
        </div>
    </div>
</div>

<!-- ================= 一级模块 ================= -->
<div class="card">
    <div class="card-header">一级模块 <span class="dim">client/soulofgun 顶层目录</span></div>
    <div class="card-body">
        <div class="table-responsive"><table class="table table-vcenter doc-table">
            <thead><tr><th>一级模块</th><th>路径</th><th>职责说明</th></tr></thead>
            <tbody>
                <tr><td>引擎层 <code>cocos2dx/</code></td><td class="doc-path">client/soulofgun/cocos2dx</td><td>渲染 / 场景 / 动作 / 粒子 / 精灵 / 触摸 / 平台，第三方引擎，未修改</td></tr>
                <tr><td>引擎扩展 <code>extensions/</code></td><td class="doc-path">client/soulofgun/extensions</td><td>GUI 控件（EditBox/ScrollView）、network、CCBReader 场景读取</td></tr>
                <tr><td>物理引擎 <code>external/</code></td><td class="doc-path">client/soulofgun/external</td><td>Box2D、chipmunk 物理库</td></tr>
                <tr><td>音频 <code>CocosDenshion/</code></td><td class="doc-path">client/soulofgun/CocosDenshion</td><td>背景音乐 / 音效（SimpleAudioEngine）</td></tr>
                <tr><td>脚本 <code>scripting/</code></td><td class="doc-path">client/soulofgun/scripting</td><td>Lua / JavaScript 绑定（本作主逻辑为 C++，脚本层基本未用）</td></tr>
                <tr><td>游戏本体 <code>samples/HelloCpp/</code></td><td class="doc-path">client/soulofgun/samples/HelloCpp</td><td><b>游戏代码</b>（Classes/）+ 资源（Resources/）+ 平台工程（proj.*）</td></tr>
                <tr><td>共享代码 <code>shared/</code></td><td class="doc-path">E:\RealServer\shared</td><td>C/S 共享：battlecore 战斗核心 + config 配置系统</td></tr>
            </tbody>
        </table></div>
    </div>
</div>

<!-- ================= 二级模块 ================= -->
<div class="card">
    <div class="card-header">二级模块 <span class="dim">游戏代码 samples/HelloCpp/Classes/ 内部分层</span></div>
    <div class="card-body">
        <h3 class="doc-h3">网络层 <code>net/</code></h3>
        <div class="table-responsive"><table class="table table-vcenter doc-table">
            <thead><tr><th>文件</th><th>职责</th></tr></thead>
            <tbody>
                <tr><td>NetClient</td><td>TCP 连接 / 非阻塞收发 / 帧协议粘包拆包 / 按 (actorType&lt;&lt;16)|msgType 分发到 handler</td></tr>
                <tr><td>NetEnums</td><td>协议枚举集中定义：一级 Actor 类型、各模块消息 ID、错误码、连接/登录状态机</td></tr>
                <tr><td>ProtoCodec</td><td>手写 proto3 wire 编解码（varint / length-delimited），客户端不依赖 protobuf 运行时</td></tr>
                <tr><td>HttpClient</td><td>WinHTTP 同步 POST JSON，账号密码登录（连 Gate 前调用 api/login.php）</td></tr>
                <tr><td>ClientConfig</td><td>配置热更单例：进大厅拉取 bundle、hash 校验、版本变更重拉、战斗注入</td></tr>
                <tr><td>FlowLog</td><td>流程日志（sog_flow.log），崩溃/流程追踪</td></tr>
                <tr><td>AutoTest</td><td>无人值守自动测试辅助</td></tr>
            </tbody>
        </table></div>

        <h3 class="doc-h3">游戏自有扩展 <code>extension/</code></h3>
        <div class="table-responsive"><table class="table table-vcenter doc-table">
            <thead><tr><th>子模块</th><th>内容</th><th>说明</th></tr></thead>
            <tbody>
                <tr><td>advance/</td><td>SpriteButton · SlideBar · SlideLayer · CameraContainer · DarknessLayer · CCBlade · DiTranslation · LoadingProgressNode · UserRecord · XAction</td><td>自研高级 UI 组件与渲染特效</td></tr>
                <tr><td>animation/</td><td>MigAnimation · MigAnimationCache · MigAnimationLoader</td><td>自研骨骼/帧动画系统（.dat/.plist 精灵动画）</td></tr>
                <tr><td>rms/</td><td>RecordStore</td><td>本地存档存储</td></tr>
                <tr><td>xml/</td><td>tinyxml · XmlResolver</td><td>XML 配置解析（Charge.xml / Config.xml 等）</td></tr>
                <tr><td>utils/</td><td>base64 · sha1 · StringTools · MathHelper · ColorHelper · exMacros</td><td>通用工具（编码/加密/字符串/数学/宏）</td></tr>
                <tr><td>io/</td><td>UIFeiDiaoLayer</td><td>IO 相关 UI</td></tr>
            </tbody>
        </table></div>

        <h3 class="doc-h3">玩法层 <code>Classes/</code> 根目录</h3>
        <div class="table-responsive"><table class="table table-vcenter doc-table">
            <thead><tr><th>分组</th><th>关键类</th><th>说明</th></tr></thead>
            <tbody>
                <tr><td>角色 / 战斗</td><td>Hero · Bullet · Sp01 · Sp01Boss · Sp01HurtFS · Turret · Prop · EnemyWaveManager</td><td>玩家角色、子弹、僵尸/Boss、炮台、道具、敌人波次</td></tr>
                <tr><td>武器</td><td>WeaponManager · WuQiLayer · GunUpdateLayer · EquipGunDialog · SelectGunItem</td><td>武器管理、升级、装备、选择</td></tr>
                <tr><td>战斗场景</td><td>StageScene（主战场）· GameScene · HudLayer · UIBattleLayer · GamingDialogLayer</td><td>战斗主场景、HUD、战斗 UI、失败/暂停弹窗</td></tr>
                <tr><td>特效</td><td>CCBlade（刀光）· DarknessLayer（战争迷雾）· darklayerba · CameraContainer</td><td>战斗视觉特效与镜头</td></tr>
            </tbody>
        </table></div>

        <h3 class="doc-h3">UI 界面层 <code>Classes/</code> 根目录</h3>
        <div class="table-responsive"><table class="table table-vcenter doc-table">
            <thead><tr><th>分组</th><th>关键类</th><th>说明</th></tr></thead>
            <tbody>
                <tr><td>登录 / 大厅</td><td>UILoginLayer · UIMainMenuLayer · UILobbyLayer · UILoadingGameLayer · LoadingLayer · UIBeforeGameCGLayer</td><td>登录、主菜单、大厅、加载、开场 CG</td></tr>
                <tr><td>选关 / 选角</td><td>UIBigLevelLayer · UILittleLevelLayer · UIDifficultyChooseLayer · UICharacterSelectLayer · UIChoosePlayer</td><td>章节/关卡选择、难度、角色选择</td></tr>
                <tr><td>商城 / 充值</td><td>UIStoreLayer · UIStoreDialog · UINStoreDialog · SlotMachinesLayer · DialogLiBao · UIFirstPayDialog · BuyGunGiftDialog</td><td>商城、扭蛋（老虎机）、礼包、首充、付费（PayID / Charge.xml）</td></tr>
                <tr><td>任务 / 设置</td><td>UITaskLayer · GameTask · UISetDialog · UISetLayerb · GameRecord · UserRecord</td><td>任务、系统设置、玩家/游戏数据存档</td></tr>
                <tr><td>联网对战</td><td>UINetBattleLayer</td><td>联网（房间/战斗）入口 UI</td></tr>
                <tr><td>通用弹窗</td><td>GeneralDialog · Dialog · DialogLayer · UIOnlyShowDialog · HelpShieldLayer</td><td>通用对话框 / 新手引导遮挡层</td></tr>
            </tbody>
        </table></div>
    </div>
</div>

<!-- ================= 网络层 ================= -->
<div class="card">
    <div class="card-header">网络层详解 <span class="dim">客户端 ↔ RealServer 通信</span></div>
    <div class="card-body">
        <h3 class="doc-h3">帧协议（与 engine/xNet/CNetGateway.cpp 一致）</h3>
        <pre class="doc-pre">客户端 → Gate:  <span class="m">[uint32 小端 bodyLen][protobuf body]</span>
每条 protobuf body 前两字段固定为路由头:
  big_type(1)   → 一级 Actor 类型  (EActorType)
  small_type(2) → 模块内消息 ID   (ELoginMsgId / EBattleMsgId / …)</pre>
        <h3 class="doc-h3">一级 Actor 类型（BigType，镜像服务端 PbID.ActorType）</h3>
        <div class="table-responsive"><table class="table table-vcenter doc-table">
            <thead><tr><th>类型</th><th>值</th><th>承载进程</th><th>说明</th></tr></thead>
            <tbody>
                <tr><td>EGate</td><td>100</td><td>Gate</td><td>网关 Actor，客户端接入</td></tr>
                <tr><td>ELogin</td><td>101</td><td>Login</td><td>登录 / 选角 / 创角</td></tr>
                <tr><td>EFriend</td><td>102</td><td>Global</td><td>好友（单例）</td></tr>
                <tr><td>ERoom</td><td>103</td><td>Game</td><td>房间（单例）</td></tr>
                <tr><td>EChat</td><td>104</td><td>Global</td><td>聊天（单例）</td></tr>
                <tr><td>EConfig</td><td>105</td><td>Global</td><td>配置热更（单例）</td></tr>
                <tr><td>EPlayer</td><td>201</td><td>Game</td><td>玩家（多实例）</td></tr>
                <tr><td>EBattle</td><td>203</td><td>Battle</td><td>战斗（多实例）</td></tr>
                <tr><td>ESpectator</td><td>204</td><td>Battle</td><td>观战（多实例）</td></tr>
            </tbody>
        </table></div>
        <div class="dim">
            消息分发键为 <code>(actorType &lt;&lt; 16) | msgType</code>，业务层通过
            <code>NetClient::on()</code> 注册 handler、<code>off()</code> 注销（图层销毁时避免悬垂指针）。
            线程模型为<b>单线程</b>（Cocos2d-x 主循环），每帧 <code>poll()</code> 驱动收发与分发，handler 可直接操作 UI。
        </div>
    </div>
</div>

<!-- ================= 共享战斗核心 ================= -->
<div class="card">
    <div class="card-header">共享战斗核心 <span class="dim">shared/battlecore（C/S 共享，确定性）</span></div>
    <div class="card-body">
        <div class="table-responsive"><table class="table table-vcenter doc-table">
            <thead><tr><th>文件</th><th>职责</th></tr></thead>
            <tbody>
                <tr><td>BattleCore</td><td>战斗核心模拟入口（CBattleCore，客户端通过 ClientConfig::InjectInto 注入配置）</td></tr>
                <tr><td>BattleEntity</td><td>战斗实体（玩家 / 敌人 / 子弹的确定性状态）</td></tr>
                <tr><td>BattleInput</td><td>玩家输入定义（逐帧上报 EInputReq）</td></tr>
                <tr><td>BattleClock</td><td>固定点帧时钟（帧同步时间基准）</td></tr>
                <tr><td>BattleRng</td><td>确定性随机数（保证 C/S 同种子同结果）</td></tr>
                <tr><td>BattlePacing</td><td>帧率节奏控制</td></tr>
                <tr><td>BattlePrediction</td><td>客户端预测 / 回滚（本地先行，权威帧校准）</td></tr>
                <tr><td>BattleDeterminism</td><td>确定性校验（跨平台一致性）</td></tr>
                <tr><td>FixedPoint / FixedTrig</td><td>定点数运算（浮点在不同平台有差异，用定点保证逐位一致）</td></tr>
            </tbody>
        </table></div>
        <h3 class="doc-h3">战斗帧同步流程</h3>
        <pre class="doc-pre">客户端                         服务端(Game/Battle Actor)
   │  EInputReq (逐帧上报输入)         │
   ├──────────────────────────────────▶│ 权威模拟所有输入
   │  EFrameNtf (权威帧命令)           │
   │◀──────────────────────────────────┤ 下发权威帧
   │  ESnapshotNtf (全量快照)          │ 重连/校准
   │◀──────────────────────────────────┤
   │  EHashNtf  (一致性校验哈希)       │
   ├──────────────────────────────────▶│ 校验 C/S 状态一致</pre>
    </div>
</div>

<!-- ================= 配置系统 ================= -->
<div class="card">
    <div class="card-header">配置系统 <span class="dim">shared/config（C/S 共享）</span></div>
    <div class="card-body">
        <div class="table-responsive"><table class="table table-vcenter doc-table">
            <thead><tr><th>分组</th><th>文件</th><th>说明</th></tr></thead>
            <tbody>
                <tr><td>配置管理</td><td>CConfigMgr · CClientConfigMgr</td><td>配置版本 / contentHash 校验 / 原子替换（失败回滚旧配置）</td></tr>
                <tr><td>配置表</td><td>CConfigTable · CMapTable · CSp01Table · CTurretTable · CWaveTable</td><td>角色/炮台/波次/地图等配置表</td></tr>
                <tr><td>二进制配置</td><td>CBinReader · CBinWriter · ConfigFormat · ConfigExporter</td><td>配置二进制化读写 / 导出（源 XML/plist → .bin）</td></tr>
                <tr><td>关卡数据</td><td>LevelDataLoader · WavePlist · EnemyInfoPlist · TmxCollision</td><td>关卡加载、波次、敌人信息、地图碰撞</td></tr>
            </tbody>
        </table></div>
        <div class="dim">
            客户端进大厅时通过 <code>CONFIG_GET_REQ</code> 从服务端拉取配置 bundle，校验
            <code>contentHash</code> 后原子替换；服务端版本变更下发 <code>CONFIG_CHANGED_NTF</code> 触发重新拉取。
            战斗开始前把网络拉到的配置（内存 blob）注入战斗核心，无网时回退本地文件。
        </div>
    </div>
</div>

<!-- ================= 关键流程 ================= -->
<div class="card">
    <div class="card-header">关键流程</div>
    <div class="card-body">
        <h3 class="doc-h3">登录 → 进游戏流程</h3>
        <pre class="doc-pre">1. AppDelegate → UILoadingGameLayer → UILoginLayer
2. 账号密码 → HttpClient.httpPostJson() → WebPlatform api/login.php（bcrypt 校验，签发 session token）
3. NetClient.connect(host, port) 连 Gate（非阻塞，poll 推进）
4. ELoginReq（携带 session token）→ ELoginRsp（账号 + 角色列表）
5. ESelectRoleReq → ESelectRoleRsp（角色 + Player 信息）
6. 进入 UILobbyLayer，ClientConfig.pullOnEntry() 拉取配置</pre>
        <h3 class="doc-h3">战斗流程</h3>
        <pre class="doc-pre">1. 大厅建房/入房（ERoom）→ 房主 EStartReq 开始战斗
2. EBattle.EnterReq（校验 auth_key）→ EEnterRsp
3. EStartNtf → 进入帧同步：逐帧 EInputReq / EFrameNtf，本机 battlecore 确定性模拟
4. 战斗中 UIBattleLayer / HudLayer 渲染，StageScene 承载
5. EEndNtf（胜负）→ 结算；异常时 ESnapshotNtf 重连校准 / EHashNtf 一致性校验</pre>
    </div>
</div>

<?php layout_footer(); ?>
