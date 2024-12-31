@echo off
	path = %path%;../Tools;
@echo on

rd /s /q "Out"
md "Out/c_go"
md "Out/c_cpp"
md "Out/c_pb"
md "Out/c_proto"
md "Out/c_cs"

cd ./Tools/

xlsx2dat.exe -I=./../Excels/battle_book/battle_book.xlsx -F=battle_book_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true

xlsx2dat.exe -I=./../Excels/battle_characters/battle_exp.xlsx -F=battle_exp_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/battle_characters/battle_player.xlsx -F=battle_player_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/battle_characters/heros.xlsx -F=heros_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/battle_characters/monsters.xlsx -F=monsters_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/battle_characters/battle_robot.xlsx -F=battle_robot_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true

xlsx2dat.exe -I=./../Excels/battle_extras/battle_attributes.xlsx -F=battle_attributes_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/battle_extras/battle_resource.xlsx -F=battle_resource_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/battle_extras/battle_ui_effect.xlsx -F=battle_ui_effect_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/battle_extras/default_setting.xlsx -F=default_setting_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/battle_extras/battle_guide.xlsx -F=battle_guide_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true

xlsx2dat.exe -I=./../Excels/battle_items/battlebox.xlsx -F=battlebox_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/battle_items/battleItems_index.xlsx -F=battleItems_index_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/battle_items/drops.xlsx -F=drops_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/battle_items/equips_enhance.xlsx -F=equips_enhance_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/battle_items/projectiles.xlsx -F=projectiles_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/battle_items/stats.xlsx -F=stats_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/battle_items/weapons.xlsx -F=weapons_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/battle_items/weapons_enhance.xlsx -F=weapons_enhance_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/battle_items/weapons_tag.xlsx -F=weapons_tag_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/battle_items/battle_summoned.xlsx -F=battle_summoned_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/battle_items/gene_index.xlsx -F=gene_index_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/battle_items/gene_levels.xlsx -F=gene_levels_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/battle_items/gene_enhance.xlsx -F=gene_enhance_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true

xlsx2dat.exe -I=./../Excels/battle_levels/levels.xlsx -F=levels_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/battle_levels/maps.xlsx -F=maps_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/battle_levels/shop.xlsx -F=shop_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/battle_levels/shop_constant.xlsx -F=shop_constant_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/battle_levels/spawns.xlsx -F=spawns_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/battle_levels/waves.xlsx -F=waves_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true

xlsx2dat.exe -I=./../Excels/battle_skills/ais.xlsx -F=ais_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/battle_skills/buffs.xlsx -F=buffs_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/battle_skills/skills.xlsx -F=skills_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/battle_skills/battle_passive_skill.xlsx -F=battle_passive_skill_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/battle_skills/skill_cfg.xlsx -F=skill_cfg_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/battle_skills/skill_index.xlsx -F=skill_index_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/battle_skills/skill_lvup.xlsx -F=skill_lvup_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true


xlsx2dat.exe -I=./../Excels/battle_scene/monster.xlsx -F=monster_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/battle_scene/buff_template.xlsx -F=buff_template_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true

REM xlsx2dat.exe -I=./../Excels/items/currency.xlsx -F=currency_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/items/item.xlsx -F=item_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/items/item_SkillDropControl.xlsx -F=item_SkillDropControl_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/items/item_storage.xlsx -F=item_storage_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true

xlsx2dat.exe -I=./../Excels/map/map_constant.xlsx -F=map_constant_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/map/map_dropPos.xlsx -F=map_dropPos_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/map/map_entity.xlsx -F=map_entity_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/map/map_farmLand.xlsx -F=map_farmLand_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/map/map_tags.xlsx -F=map_tags_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/map/map_unlock.xlsx -F=map_unlock_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true

xlsx2dat.exe -I=./../Excels/player/player.xlsx -F=player_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/player/player_constant.xlsx -F=player_constant_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/player/player_indicatDes.xlsx -F=player_indicatDes_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/player/player_islandeffect.xlsx -F=player_islandeffect_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/player/player_soundeffect.xlsx -F=player_soundeffect_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/player/player_onlinenotify.xlsx -F=player_onlinenotify_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/player/player_damage.xlsx -F=player_damage_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/player/player_task.xlsx -F=player_task_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/player/player_talk.xlsx -F=player_talk_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/player/player_event.xlsx -F=player_event_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true

xlsx2dat.exe -I=./../Excels/island/island_pickscore.xlsx -F=island_pickscore_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/island/island_region.xlsx -F=island_region_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true

xlsx2dat.exe -I=./../Excels/season/season.xlsx -F=season_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/season/levels_rewards.xlsx -F=levels_rewards_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/season/ranking_rewards.xlsx -F=ranking_rewards_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true

xlsx2dat.exe -I=./../Excels/skill/cook_index.xlsx -F=cook_index_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
REM xlsx2dat.exe -I=./../Excels/skill/skill_battleItems.xlsx -F=skill_battleItems_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/skill/skill_book.xlsx -F=skill_book_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/skill/skill_box.xlsx -F=skill_box_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/skill/skill_boxcreate.xlsx -F=skill_boxcreate_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/skill/skill_levelbox.xlsx -F=skill_levelbox_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/skill/skill_cook.xlsx -F=skill_cook_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/skill/skill_upgrade.xlsx -F=skill_upgrade_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/skill/skill_permit.xlsx -F=skill_permit_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/skill/gene_upgrade.xlsx -F=gene_upgrade_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/skill/gene_shop.xlsx -F=gene_shop_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/skill/chicken_shop.xlsx -F=chicken_shop_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true

REM xlsx2dat.exe -I=./../Excels/condition.xlsx -F=condition_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
REM xlsx2dat.exe -I=./../Excels/constant.xlsx -F=constant_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/constant_c.xlsx -F=constant_c_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/errorcode.xlsx -F=errorcode_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/effect_cfg.xlsx -F=effect_cfg_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true
xlsx2dat.exe -I=./../Excels/scenes_cfg.xlsx -F=scenes_cfg_excel -P=./../Out/c_proto/ -D=./../Out/c_pb/ -N=TX -S=c -L=true

for %%i in (..\Out\c_proto\*.proto) do (
   echo gen %%i...
   protoc_simple_client.exe --proto_path=..\Out\c_proto --csharp_out=..\Out\c_cs  %%i)

del /s /f /q .\..\..\client\SurvivaAndlKill\Assets\Script\Generate\Configs\*.cs
del /s /f /q .\..\..\client\SurvivaAndlKill\Assets\GameAssets\Config\*.dat

copy /Y .\..\out\c_pb\map_entity_excel.json .\..\..\client\SurvivaAndlKill\Assets\GameAssets\Config\json\
copy /y .\..\Out\c_pb\*.dat .\..\..\client\SurvivaAndlKill\Assets\GameAssets\Config\
copy /y .\..\Out\c_cs\*.cs  .\..\..\client\SurvivaAndlKill\Assets\Script\Generate\Configs\

rd /s /q ".\..\Out"


@echo Generate Done!

pause
