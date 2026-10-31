/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: luacore_chat.i
      Purpose:   聊天模块 Lua 绑定

      生成 %module luacore_chat，导出 PbChat 编解码 si_pb_chat_send_rsp / si_pb_chat_ntf / si_pb_chat_history_rsp
      历史消息以逗号分号串拼装 repeated messages，避免 std::vector 跨越 SWIG 边界
  *****************************************************************/

// luacore_chat.i — 聊天模块编解码绑定 (package PbChat)
// Lua: luacore_chat.si_pb_chat_*
%module luacore_chat

%include <typemaps.i>
%include <std_string.i>
%include <stdint.i>

%{
#include "xBase/xDefine.h"
#include "msg/msg_id.pb.h"
#include "msg/chat.pb.h"
#include <string>
#include <vector>
#include <cstdint>
#include <cstdlib>
%}

%include "base/xDefine.i"
%include "pb_util.i"

// ================================================================
// 聊天模块编解码 — si_pb_chat_* (package PbChat)
// ================================================================
%inline %{
std::string si_pb_chat_send_rsp(int32 errorCode) {
    PbChat::PBChatSendRsp rsp;
    rsp.set_big_type(PbID::AT_CHAT);
    rsp.set_small_type(PbChat::CHAT_SEND_RSP);
    rsp.set_error_code((PbID::ErrorCode)errorCode);
    std::string out; rsp.SerializeToString(&out); return out;
}
std::string si_pb_chat_ntf(int32 channelType, int64 channelId, int64 senderId,
                           const std::string& senderName, const std::string& content) {
    PbChat::PBChatNtf ntf;
    ntf.set_big_type(PbID::AT_CHAT);
    ntf.set_small_type(PbChat::CHAT_NTF);
    ntf.set_channel_type((PbChat::ChatChannelType)channelType);
    ntf.set_channel_id(channelId);
    ntf.set_sender_id(senderId);
    ntf.set_sender_name(senderName);
    ntf.set_content(content);
    std::string out; ntf.SerializeToString(&out); return out;
}
// msgsStr: "sender_id,sender_name,content,timestamp;..."
std::string si_pb_chat_history_rsp(int32 errorCode, int32 channelType, int64 channelId,
                                   const std::string& msgsStr) {
    PbChat::PBChatHistoryRsp rsp;
    rsp.set_big_type(PbID::AT_CHAT);
    rsp.set_small_type(PbChat::CHAT_HISTORY_RSP);
    rsp.set_error_code((PbID::ErrorCode)errorCode);
    rsp.set_channel_type((PbChat::ChatChannelType)channelType);
    rsp.set_channel_id(channelId);
    for (auto& rec : pb_split(msgsStr, ';')) {
        if (rec.empty()) continue;
        auto f = pb_split(rec, ',');
        if (f.size() < 4) continue;
        auto* m = rsp.add_messages();
        m->set_sender_id(atoll(f[0].c_str()));
        m->set_sender_name(f[1]);
        m->set_content(f[2]);
        m->set_timestamp(atoll(f[3].c_str()));
    }
    std::string out; rsp.SerializeToString(&out); return out;
}
%}
