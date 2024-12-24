#include "excel_mgr.h"

#include "log4cxx.h"

#include <google/protobuf/text_format.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <fstream>
#include <random>


using namespace google::protobuf;

std::shared_ptr<excel_manager> excel_mgr = std::make_shared<excel_manager>();


excel_manager::excel_manager()
    : price_increase_(0.5f)
    , selling_prise_(0.25f)
    , shop_price_(0.08f)
    //, shop_price2_(1.5f)
    , lucky_constant_(1.5f)
    , starting_coin_(30)
    , def_stats_id(0) {
}

bool excel_manager::init_excel_manager(const std::string& path) {
    std::ifstream svnfs(path + "/../Version.txt");
    if (svnfs.is_open()) {
        std::string line;
        if (getline(svnfs, line)) {
            logger->info("当前配置SVN版本：{}", line);
        }
        svnfs.close();
    }
    excel_mgr->hero_exp_map_.clear();
    excel_mgr->characters_excels_ = std::make_shared<TX::battle_characters_excels>();
    std::ifstream cf(path + "/battle_characters_excel.dat", std::ios::in | std::ios::binary);
    if (cf.is_open()) {
        if (!excel_mgr->characters_excels_->ParseFromIstream(&cf)) {
            logger->error("Parse characters_excels_ error!");
            return false;
        }
    } else {
        logger->error("Load characters_excels_ file error!");
        return false;
    }

    excel_mgr->extras_excels_ = std::make_shared<TX::battle_extras_excels>();
    std::ifstream ef(path + "/battle_extras_excel.dat", std::ios::in | std::ios::binary);
    if (ef.is_open()) {
        if (!excel_mgr->extras_excels_->ParseFromIstream(&ef)) {
            logger->error("Parse extras_excels_ error!");
            return false;
        }
    } else {
        logger->error("Load extras_excels_ file error!");
        return false;
    }

    excel_mgr->items_excels_ = std::make_shared<TX::battle_items_excels>();
    std::ifstream ifs(path + "/battle_items_excel.dat", std::ios::in | std::ios::binary);
    if (ifs.is_open()) {
        if (!excel_mgr->items_excels_->ParseFromIstream(&ifs)) {
            logger->error("Parse items_excels_ error!");
            return false;
        }
    } else {
        logger->error("Load items_excels_ file error!");
        return false;
    }

    excel_mgr->levels_excels_ = std::make_shared<TX::battle_levels_excels>();
    std::ifstream lf(path + "/battle_levels_excel.dat", std::ios::in | std::ios::binary);
    if (lf.is_open()) {
        if (!excel_mgr->levels_excels_->ParseFromIstream(&lf)) {
            logger->error("Parse levels_excels_ error!");
            return false;
        }
    } else {
        logger->error("Load levels_excels_ file error!");
        return false;
    }

    excel_mgr->skills_excels_ = std::make_shared<TX::battle_skills_excels>();
    std::ifstream sf(path + "/battle_skills_excel.dat", std::ios::in | std::ios::binary);
    if (sf.is_open()) {
        if (!excel_mgr->skills_excels_->ParseFromIstream(&sf)) {
            logger->error("Parse skills_excels_ error!");
            return false;
        }
    } else {
        logger->error("Load skills_excels_ file error!");
        return false;
    }

    excel_mgr->scenes_excels_ = std::make_shared<TX::battle_scene_excels>();
    std::ifstream sf1(path + "/battle_scene_excel.dat", std::ios::in | std::ios::binary);
    if (sf1.is_open()) {
        if (!excel_mgr->scenes_excels_->ParseFromIstream(&sf1)) {
            logger->error("Parse battle_scene_excel error!");
            return false;
        }
    } else {
        logger->error("Load scenes_excels_ file error!");
        return false;
    }

    excel_mgr->tx_excels_ = std::make_shared<TX::tx_excels>();
    std::ifstream tf(path + "/tx_excel.dat", std::ios::in | std::ios::binary);
    if (tf.is_open()) {
        if (!excel_mgr->tx_excels_->ParseFromIstream(&tf)) {
            logger->error("Parse tx_excel error!");
            return false;
        }
    } else {
        logger->error("Load tx_excel file error!");
        return false;
    }

    excel_mgr->island_player_excels_ = std::make_shared<TX::player_excels>();
    std::ifstream pf1(path + "/player_excel.dat", std::ios::in | std::ios::binary);
    if (pf1.is_open()) {
        if (!excel_mgr->island_player_excels_->ParseFromIstream(&pf1)) {
            logger->error("Parse island_player_excels_ error!");
            return false;
        }
    } else {
        logger->error("Load island_player_excels_ file error!");
        return false;
    }

    excel_mgr->update_excel_manager();

    return true;
}

void excel_manager::update_excel_manager() {
    for (const auto& exp : characters_excels_->battle_exp_map()) {
        if (exp.second.exp() > 0) {
            hero_exp_map_[exp.second.exp()] = exp.first;
        }
    }

    for (const auto& stats : items_excels_->stats_map()) {
        if (stats.second.activation() == 0) {
            continue;
        }
        def_stats_id = stats.first;
        auto it = random_stats_maps_.find(stats.second.quality());
        if (it != random_stats_maps_.end()) {
            it->second.push_back(stats.first);
        } else {
            random_stats_maps_[stats.second.quality()] = std::vector{ stats.first };
        }
    }

    auto cont_maps = levels_excels_->shop_constant_map();
    auto it_price = cont_maps.find("price_increase");
    if (it_price != cont_maps.end() && it_price->second.f_value() > 0) {
        price_increase_ = it_price->second.f_value();
    }

    it_price = cont_maps.find("selling_prise");
    if (it_price != cont_maps.end() && it_price->second.f_value() > 0) {
        selling_prise_ = it_price->second.f_value();
    }

    it_price = cont_maps.find("shop_price");
    if (it_price != cont_maps.end() && it_price->second.f_value() > 0) {
        shop_price_ = it_price->second.f_value();
    }

    //  it_price = cont_maps.find("shop_price2");
    //  if (it_price != cont_maps.end() && it_price->second.f_value() > 0){
    //    shop_price2_ = it_price->second.f_value();
    //  }

    it_price = cont_maps.find("lucky_constant");
    if (it_price != cont_maps.end() && it_price->second.f_value() > 0) {
        lucky_constant_ = it_price->second.f_value();
    }

    it_price = cont_maps.find("starting_materials");
    if (it_price != cont_maps.end() && it_price->second.i_value() > 0) {
        starting_coin_ = it_price->second.i_value();
    }

    for (const auto& info : items_excels_->battlebox_map()) {
        max_maps tem_maps;
        for (const auto& items : info.second.dropitems()) {
            for (const int32_t item_id : items.vlist()) {
                auto it = items_excels_->battleitems_index_map().find(item_id);
                if (it != items_excels_->battleitems_index_map().end()) {
                    if (it->second.has_starrange()) {
                        for (int32_t bq = it->second.starrange().x(); bq <= it->second.starrange().y(); ++bq) {
                            tem_maps.max_ += items.key();
                            tem_maps.maps_[tem_maps.max_] = item_id * 10 + bq;
                        }
                    }
                }
            }
        }
        random_box_maps_[info.first] = tem_maps;
    }

    for (const auto& item : items_excels_->battleitems_index_map()) {
        if (!item.second.has_starrange())
            continue;
        if (item.second.activation() != 1)
            continue;
        for (int32_t bq = item.second.starrange().x(); bq <= item.second.starrange().y(); ++bq) {
            int32_t high_quality = item.second.id() * 10 + bq + 1;
            if (bq == item.second.starrange().y()) {
                high_quality = 0;
            }
            int32_t _price = 99;
            if (item.second.baseprice_size() > bq - 1 && bq - 1 >= 0) {
                _price = item.second.baseprice(bq - 1);
            } else {
                logger->error("excel_manager::update_excel_manager base price error:{}, {}", item.second.id(), bq);
            }
            if (item.second.battleitemtype() == 1) {
                weapon_maps_[item.second.id() * 10 + bq] = { item.second.id() * 10 + bq, bq, high_quality, _price };
                auto it = random_weapon_maps_.find(bq);
                if (it != random_weapon_maps_.end()) {
                    it->second.emplace_back(item.second.id() * 10 + bq, bq, high_quality, _price);
                } else {
                    random_weapon_maps_[bq] = std::vector<item_info>{
                      item_info(item.second.id() * 10 + bq, bq, high_quality, _price)
                    };
                }
            }
            auto i_it = random_item_maps_.find(bq);
            if (i_it != random_item_maps_.end()) {
                i_it->second.emplace_back(item.second.id() * 10 + bq, bq, high_quality, _price);
            } else {
                random_item_maps_[bq] = std::vector<item_info>{
                  item_info(item.second.id() * 10 + bq, bq, high_quality, _price)
                };
            }
            item_base_prices_[item.second.id() * 10 + bq] = _price;
        }
    }
}

int32_t excel_manager::random_battle_stats(int32_t _waves_idx, int32_t _luck, const std::vector<int32_t>& _has_list) {
    const auto it = levels_excels_->shop_map().find(_waves_idx);
    if (it == levels_excels_->shop_map().end()) {
        return {};
    }
    if (it->second.drop_q4() > 0) {
        const auto stats_id = random_stats_maps_quality(it->second.drop_q4(), 4, _luck, _has_list);
        if (stats_id > 0)
            return stats_id;
    }
    if (it->second.drop_q3() > 0) {
        const auto stats_id = random_stats_maps_quality(it->second.drop_q3(), 3, _luck, _has_list);
        if (stats_id > 0)
            return stats_id;
    }
    if (it->second.drop_q2() > 0) {
        const auto stats_id = random_stats_maps_quality(it->second.drop_q2(), 2, _luck, _has_list);
        if (stats_id > 0)
            return stats_id;
    }
    return random_stats_maps_quality(it->second.drop_q1(), 1, _luck, _has_list);
}

int32_t excel_manager::get_item_coin(int32_t coin, int32_t waves_idx) {
    return static_cast<int32_t>(coin * pow(1.0f + excel_mgr->get_shop_price(), waves_idx));
}

int32_t excel_manager::get_sell_coin(int32_t item_id, int32_t waves_idx) const {
    const auto iit = item_base_prices_.find(item_id);
    if (iit != item_base_prices_.end()) {
        const int32 cur_coin = get_item_coin(iit->second, waves_idx);
        return static_cast<int32_t>(static_cast<float>(cur_coin) * selling_prise_);
        //return static_cast<int32_t>(static_cast<float>(iit->second) * selling_prise_);
    }
    return 0;
}

int32_t excel_manager::get_high_quality(int32_t _weapon_id) const {
    const auto it = weapon_maps_.find(_weapon_id);
    if (it != weapon_maps_.end()) {
        return it->second.high_quality_;
    }
    return -1;
}

bool excel_manager::is_weapon_id(int32_t item_id) const {
    if (weapon_maps_.find(item_id) != weapon_maps_.end()) {
        return true;
    }
    return false;
}

bool excel_manager::check_has_list(const std::vector<int32_t>& _has_list, int32_t item_id) {
    return std::any_of(_has_list.begin(), _has_list.end(), [&](int32_t id) {
        return id / 10 == item_id / 10;
        });
}

// 获取 event template
//const TX::event_template_item* excel_manager::get_scene_event_item(int32_t id) {
//	auto mapData = scenes_excels_->event_template_map();
//	auto it = mapData.find(id);
//	if (it != mapData.end()) {
//		return &it->second;
//	}
//	return nullptr;
//}
// 
int excel_manager::get_battle_param_a() {
    auto it = scenes_excels_->global_config_map().find(1);
    if (it != scenes_excels_->global_config_map().end()) {
        return it->second.int_value();
    }
    return 0;
}




