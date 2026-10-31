/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: luacore_friend.i
      Purpose:   好友模块 Lua 绑定

      生成 %module luacore_friend，导出 PbFriend 编解码 si_pb_friend_add_rsp / apply_ntf / accept_rsp / list_rsp / del_rsp / online_ntf / invite_ntf
      好友列表以逗号分号串拼装 repeated friends 字段
  *****************************************************************/

// luacore_friend.i — 好友模块编解码绑定 (package PbFriend)
// Lua: luacore_friend.si_pb_friend_*
%module luacore_friend

%include <typemaps.i>
%include <std_string.i>
%include <stdint.i>

%{
#include "xBase/xDefine.h"
#include "msg/msg_id.pb.h"
#include "msg/friend.pb.h"
#include <string>
#include <vector>
#include <cstdint>
#include <cstdlib>
%}

%include "base/xDefine.i"
%include "pb_util.i"

// ================================================================
// 好友模块编解码 — si_pb_friend_* (package PbFriend)
// ================================================================
%inline %{
std::string si_pb_friend_add_rsp(int32 errorCode, int64 targetPlayerId) {
    PbFriend::PBFriendAddRsp rsp;
    rsp.set_big_type(PbID::AT_FRIEND);
    rsp.set_small_type(PbFriend::FRIEND_ADD_RSP);
    rsp.set_error_code((PbID::ErrorCode)errorCode);
    rsp.set_target_player_id(targetPlayerId);
    std::string out; rsp.SerializeToString(&out); return out;
}
std::string si_pb_friend_apply_ntf(int64 applicantId, const std::string& applicantName) {
    PbFriend::PBFriendApplyNtf ntf;
    ntf.set_big_type(PbID::AT_FRIEND);
    ntf.set_small_type(PbFriend::FRIEND_ADD_APPLY_NTF);
    ntf.set_applicant_id(applicantId);
    ntf.set_applicant_name(applicantName);
    std::string out; ntf.SerializeToString(&out); return out;
}
std::string si_pb_friend_accept_rsp(int32 errorCode, int64 friendId) {
    PbFriend::PBFriendAcceptRsp rsp;
    rsp.set_big_type(PbID::AT_FRIEND);
    rsp.set_small_type(PbFriend::FRIEND_ADD_ACCEPT_RSP);
    rsp.set_error_code((PbID::ErrorCode)errorCode);
    rsp.set_friend_id(friendId);
    std::string out; rsp.SerializeToString(&out); return out;
}
// listStr: "player_id,name,level,online;..." (好友列表)
std::string si_pb_friend_list_rsp(int32 errorCode, const std::string& listStr) {
    PbFriend::PBFriendListRsp rsp;
    rsp.set_big_type(PbID::AT_FRIEND);
    rsp.set_small_type(PbFriend::FRIEND_LIST_RSP);
    rsp.set_error_code((PbID::ErrorCode)errorCode);
    for (auto& rec : pb_split(listStr, ';')) {
        if (rec.empty()) continue;
        auto f = pb_split(rec, ',');
        if (f.size() < 4) continue;
        auto* info = rsp.add_friends();
        info->set_player_id(atoll(f[0].c_str()));
        info->set_name(f[1]);
        info->set_level(atoi(f[2].c_str()));
        info->set_online(f[3] == "1");
    }
    std::string out; rsp.SerializeToString(&out); return out;
}
std::string si_pb_friend_del_rsp(int32 errorCode, int64 friendId) {
    PbFriend::PBFriendDelRsp rsp;
    rsp.set_big_type(PbID::AT_FRIEND);
    rsp.set_small_type(PbFriend::FRIEND_DEL_RSP);
    rsp.set_error_code((PbID::ErrorCode)errorCode);
    rsp.set_friend_id(friendId);
    std::string out; rsp.SerializeToString(&out); return out;
}
std::string si_pb_friend_online_ntf(int64 friendId, bool online) {
    PbFriend::PBFriendOnlineNtf ntf;
    ntf.set_big_type(PbID::AT_FRIEND);
    ntf.set_small_type(PbFriend::FRIEND_ONLINE_NTF);
    ntf.set_friend_id(friendId);
    ntf.set_online(online);
    std::string out; ntf.SerializeToString(&out); return out;
}
std::string si_pb_friend_invite_ntf(int64 inviterId, int64 roomId) {
    PbFriend::PBFriendInviteNtf ntf;
    ntf.set_big_type(PbID::AT_FRIEND);
    ntf.set_small_type(PbFriend::FRIEND_INVITE_NTF);
    ntf.set_inviter_id(inviterId);
    ntf.set_room_id(roomId);
    std::string out; ntf.SerializeToString(&out); return out;
}
%}
