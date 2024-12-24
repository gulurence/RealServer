#pragma once

#include <memory>
#include <string>
#include "tx_base_excel.pb.h"
#include "tx_excel.pb.h"


struct item_info{
  int32_t item_id_;
  int32_t quality_;
  int32_t high_quality_;
  int32_t base_price_;

  item_info(){
    item_id_ = quality_ = high_quality_ = base_price_ = 0;
  }

  item_info(int32_t weapon, int32_t quality, int32_t high_quality, int32_t price){
    item_id_ = weapon;
    quality_ = quality;
    high_quality_ = high_quality;
    base_price_ = price;
  }
};

class excel_manager{
public:
  excel_manager();
  virtual ~excel_manager() = default;

public:
  void fill_hero_items(int32_t hero_id, std::vector<int32_t>& list) const;
  int32_t get_level_wave_coin(int32_t int32, int32_t waves_idx) const;
  static int32_t get_level_wave_refresh();

  bool static init_excel_manager(const std::string& path);
  void update_excel_manager();


  int32_t random_battle_stats(int32_t _waves_idx, int32_t _luck, const std::vector<int32_t>& _has_list);

  static int32_t get_item_coin(int32_t coin, int32_t waves_idx);
  int32_t get_sell_coin(int32_t item_id, int32_t waves_idx) const;
  int32_t get_high_quality(int32_t _weapon_id) const;
  bool is_weapon_id(int32_t item_id) const;

  float get_price_increase() const{ return price_increase_; }
  float get_shop_price() const{ return shop_price_; }
  //float get_shop_price2() const { return shop_price2_;}
  float get_lucky_constant() const {return lucky_constant_;}
  int32_t get_starting_coin() const{ return starting_coin_; }

private:
  static bool check_has_list(const std::vector<int32_t>& _has_list, int32_t item_id);

public:
    int get_battle_param_a();
    int get_cri_param();

private:
  std::string excel_path_;

  std::map<int64_t, int32_t> hero_exp_map_;
  //std::vector<int32_t> battle_stats_vec;

  float price_increase_;
  float selling_prise_;
  float shop_price_;
  //float shop_price2_;
  float lucky_constant_;
  int32_t starting_coin_;

  //Random Box Maps
  struct max_maps{
    uint32_t max_;
    std::map<uint32_t, int32_t> maps_;

    max_maps(){
      max_ = 0;
      maps_.clear();
    }
  };

  std::map<int32_t, max_maps> random_box_maps_;

  int32_t def_stats_id;
  std::map<int32_t, std::vector<int32_t>> random_stats_maps_;
  std::map<int32_t, std::vector<item_info>> random_weapon_maps_;
  std::map<int32_t, std::vector<item_info>> random_item_maps_;
  std::map<int32_t, item_info> weapon_maps_;
  std::map<int32_t, int32_t> item_base_prices_;

  std::shared_ptr<TX::tx_excels> tx_excels_;

};

extern std::shared_ptr<excel_manager> excel_mgr;



