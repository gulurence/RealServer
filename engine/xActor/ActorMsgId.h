#pragma once

#include "xBase/xDefine.h"

// =============================================================================
// ActorMsgId — 消息 ID 约定 (C++ 侧权威定义, Lua 侧 common/msg_id.lua 镜像)
//
// 一条消息 ID 是 32 位无符号整数, 分成两段:
//   高 16 位 (bit31..16) : 消息类别 (category)
//       0   = 框架系统消息, 由 C++ 直接处理 (不进 Lua)
//       非0 = 业务消息,     由 Lua 的 ActorBase:on_msg 分发处理
//   低 16 位 (bit15..0)  : 类别内子编号
//
// 网络包头 NetPackageHeadST.u32MsgId 即此 32 位 ID, 也是
// CActor::PostMessage / ActorMessage.u32MsgId 携带的值。
//
// 分发规则 (CLuaActor::OnMessage):
//   IsSystemMsg(id) == true  → HandleSystemMessage (C++)
//   IsSystemMsg(id) == false → 转发 Lua delegate 的 on_msg
// =============================================================================

namespace xMsgId
{

// 类别位宽与掩码
constexpr uint32 CATEGORY_SHIFT = 16;
constexpr uint32 CATEGORY_MASK = 0xFFFF0000u; // 高 16 位
constexpr uint32 SUBID_MASK = 0x0000FFFFu;    // 低 16 位

// ---- 框架系统消息 (高 16 位 == 0, C++ 处理) ----
constexpr uint32 SYS_PING = 0x00000001u;       // 连接探测
constexpr uint32 SYS_PONG = 0x00000002u;       // 探测应答
constexpr uint32 SYS_DISCONNECT = 0x00000003u; // 客户端断开
// 服务器主动推送 (业务 Actor → Gate): RPC 服务线程投递此消息到 Gate Actor 的
// 消息队列, 由 Actor 调度线程串行执行 on_rpc_call("push", ...)。它虽走系统消息
// 通道 (高 16 位 == 0), 但会被 C++ 转发回 Lua —— 目的是让 Lua 始终在调度线程
// 内执行, 不与 on_msg 并发触碰进程唯一 lua_State。
constexpr uint32 SYS_RPC_PUSH = 0x00000004u;
// GM 命令 (CGmCommandMgr 投递): GM 控制面把命令投递到本 Actor 的消息队列,
// 由调度线程串行执行 on_gm_cmd(cmdline), 应答经 CGmCommandMgr::LogReply 回写。
// 同 SYS_RPC_PUSH: 高 16 位 == 0 但会被 C++ 转发回 Lua (串行在调度线程内)。
constexpr uint32 SYS_GM_CMD = 0x00000005u;

// ---- 业务消息 (高 16 位 != 0, Lua 处理) ----
// 具体业务消息 ID 由 Lua 侧 common/msg_id.lua 定义, 起点:
constexpr uint32 USER_BEGIN = 0x00010000u;

// 类别判断
constexpr bool IsSystemMsg(uint32 u32MsgId)
{
    return (u32MsgId & CATEGORY_MASK) == 0;
}
constexpr bool IsUserMsg(uint32 u32MsgId)
{
    return !IsSystemMsg(u32MsgId);
}

} // namespace xMsgId
