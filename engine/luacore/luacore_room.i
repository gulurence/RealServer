/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: luacore_room.i
      Purpose:   房间模块 Lua 绑定

      生成 %module luacore_room，导出 PbRoom 编解码 si_pb_room_create/list/join/leave/ready/kick/start 等响应与 info/member 通知
      房间与成员列表以逗号分号串拼装 repeated 字段，开战响应含 battle_id/auth_key/host_ip/port
  *****************************************************************/

// luacore_room.i — 房间模块编解码绑定 (package PbRoom)
// Lua: luacore_room.si_pb_room_*
%module luacore_room

%include <typemaps.i>
%include <std_string.i>
%include <stdint.i>

%{
#include "xBase/xDefine.h"
#include "msg/msg_id.pb.h"
#include "msg/room.pb.h"
#include <string>
#include <vector>
#include <cstdint>
#include <cstdlib>
%}

%include "base/xDefine.i"
%include "pb_util.i"

// ================================================================
// 房间模块编解码 — si_pb_room_* (package PbRoom)
// ================================================================
%inline %{
// membersStr: "player_id,name,ready,is_owner;..."
std::string si_pb_room_create_rsp(int32 errorCode, int64 roomId, const std::string& membersStr) {
    PbRoom::PBRoomCreateRsp rsp;
    rsp.set_big_type(PbID::AT_ROOM);
    rsp.set_small_type(PbRoom::ROOM_CREATE_RSP);
    rsp.set_error_code((PbID::ErrorCode)errorCode);
    rsp.set_room_id(roomId);
    for (auto& rec : pb_split(membersStr, ';')) {
        if (rec.empty()) continue;
        auto f = pb_split(rec, ',');
        if (f.size() < 4) continue;
        auto* m = rsp.add_members();
        m->set_player_id(atoll(f[0].c_str()));
        m->set_name(f[1]);
        m->set_ready(f[2] == "1");
        m->set_is_owner(f[3] == "1");
    }
    std::string out; rsp.SerializeToString(&out); return out;
}
// roomsStr: "room_id,owner_id,mode,visibility,member_count,max_members,state,name;..."
std::string si_pb_room_list_rsp(int32 errorCode, const std::string& roomsStr) {
    PbRoom::PBRoomListRsp rsp;
    rsp.set_big_type(PbID::AT_ROOM);
    rsp.set_small_type(PbRoom::ROOM_LIST_RSP);
    rsp.set_error_code((PbID::ErrorCode)errorCode);
    for (auto& rec : pb_split(roomsStr, ';')) {
        if (rec.empty()) continue;
        auto f = pb_split(rec, ',');
        if (f.size() < 7) continue;
        auto* room = rsp.add_rooms();
        room->set_room_id(atoll(f[0].c_str()));
        room->set_owner_id(atoll(f[1].c_str()));
        room->set_mode((PbRoom::RoomMode)atoi(f[2].c_str()));
        room->set_visibility((PbRoom::RoomVisibility)atoi(f[3].c_str()));
        room->set_member_count(atoi(f[4].c_str()));
        room->set_max_members(atoi(f[5].c_str()));
        room->set_state(atoi(f[6].c_str()));
        if (f.size() >= 8) room->set_name(f[7]);
    }
    std::string out; rsp.SerializeToString(&out); return out;
}
std::string si_pb_room_join_rsp(int32 errorCode, int64 roomId, const std::string& membersStr) {
    PbRoom::PBRoomJoinRsp rsp;
    rsp.set_big_type(PbID::AT_ROOM);
    rsp.set_small_type(PbRoom::ROOM_JOIN_RSP);
    rsp.set_error_code((PbID::ErrorCode)errorCode);
    rsp.set_room_id(roomId);
    for (auto& rec : pb_split(membersStr, ';')) {
        if (rec.empty()) continue;
        auto f = pb_split(rec, ',');
        if (f.size() < 4) continue;
        auto* m = rsp.add_members();
        m->set_player_id(atoll(f[0].c_str()));
        m->set_name(f[1]);
        m->set_ready(f[2] == "1");
        m->set_is_owner(f[3] == "1");
    }
    std::string out; rsp.SerializeToString(&out); return out;
}
std::string si_pb_room_leave_rsp(int32 errorCode, int64 roomId) {
    PbRoom::PBRoomLeaveRsp rsp;
    rsp.set_big_type(PbID::AT_ROOM);
    rsp.set_small_type(PbRoom::ROOM_LEAVE_RSP);
    rsp.set_error_code((PbID::ErrorCode)errorCode);
    rsp.set_room_id(roomId);
    std::string out; rsp.SerializeToString(&out); return out;
}
std::string si_pb_room_ready_rsp(int32 errorCode, int64 roomId) {
    PbRoom::PBRoomReadyRsp rsp;
    rsp.set_big_type(PbID::AT_ROOM);
    rsp.set_small_type(PbRoom::ROOM_READY_RSP);
    rsp.set_error_code((PbID::ErrorCode)errorCode);
    rsp.set_room_id(roomId);
    std::string out; rsp.SerializeToString(&out); return out;
}
std::string si_pb_room_kick_rsp(int32 errorCode, int64 roomId) {
    PbRoom::PBRoomKickRsp rsp;
    rsp.set_big_type(PbID::AT_ROOM);
    rsp.set_small_type(PbRoom::ROOM_KICK_RSP);
    rsp.set_error_code((PbID::ErrorCode)errorCode);
    rsp.set_room_id(roomId);
    std::string out; rsp.SerializeToString(&out); return out;
}
std::string si_pb_room_start_rsp(int32 errorCode, int64 roomId, int64 battleId,
                                 const std::string& authKey, const std::string& hostIp = "",
                                 int32 port = 0) {
    PbRoom::PBRoomStartRsp rsp;
    rsp.set_big_type(PbID::AT_ROOM);
    rsp.set_small_type(PbRoom::ROOM_START_RSP);
    rsp.set_error_code((PbID::ErrorCode)errorCode);
    rsp.set_room_id(roomId);
    rsp.set_battle_id(battleId);
    rsp.set_auth_key(authKey);
    rsp.set_host_ip(hostIp);
    rsp.set_port(port);
    std::string out; rsp.SerializeToString(&out); return out;
}
std::string si_pb_room_info_ntf(int64 roomId, int64 ownerId, int32 mode, int32 visibility,
                                int32 memberCount, int32 maxMembers, int32 state,
                                const std::string& name) {
    PbRoom::PBRoomInfoNtf ntf;
    ntf.set_big_type(PbID::AT_ROOM);
    ntf.set_small_type(PbRoom::ROOM_INFO_NTF);
    auto* room = ntf.mutable_room();
    room->set_room_id(roomId);
    room->set_owner_id(ownerId);
    room->set_mode((PbRoom::RoomMode)mode);
    room->set_visibility((PbRoom::RoomVisibility)visibility);
    room->set_member_count(memberCount);
    room->set_max_members(maxMembers);
    room->set_state(state);
    room->set_name(name);
    std::string out; ntf.SerializeToString(&out); return out;
}
std::string si_pb_room_member_ntf(int64 roomId, const std::string& membersStr) {
    PbRoom::PBRoomMemberNtf ntf;
    ntf.set_big_type(PbID::AT_ROOM);
    ntf.set_small_type(PbRoom::ROOM_MEMBER_NTF);
    ntf.set_room_id(roomId);
    for (auto& rec : pb_split(membersStr, ';')) {
        if (rec.empty()) continue;
        auto f = pb_split(rec, ',');
        if (f.size() < 4) continue;
        auto* m = ntf.add_members();
        m->set_player_id(atoll(f[0].c_str()));
        m->set_name(f[1]);
        m->set_ready(f[2] == "1");
        m->set_is_owner(f[3] == "1");
    }
    std::string out; ntf.SerializeToString(&out); return out;
}
%}
