-- verify.lua — 会话/登录校验 RPC (Login 侧)
-- 校验 Session 令牌 → 返回账号 id 与账户状态 (供 Gate 登录链路调用)
local LoginClass = xEngine.Login

--- on_rpc_verify(token) → 校验会话令牌
-- @param token string  客户端带上的 Session token
-- @return bool
function LoginClass:on_rpc_verify(token)
    local accountId = xEngine.Session.validate(token)
    if not accountId then
        xEngine.Log.warn("[Login] verify: invalid session token='%s'", tostring(token))
        return false
    end

    -- 更新账户状态: 已登录 (LoggedIn)
    xEngine.Account.setState(accountId, xEngine.AccountState.LOGGED_IN)
    xEngine.Log.info("[Login] verify: account=%d session ok", accountId)
    return true
end
