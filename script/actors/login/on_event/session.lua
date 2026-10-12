-- session.lua — 会话生命周期事件 (Login 侧)
-- 处理连接建立/断开时的会话状态流转
local LoginClass = xEngine.Login

function LoginClass:on_session_event(event_enum)
    xEngine.Log.info("[Login] on_session_event: %d", event_enum)
    -- 会话建立/断开时按需更新账户状态 (Phase 5 登录链路里接入)
end
