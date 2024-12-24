#include "excel_refresh.h"
#include "../excel/excel_mgr.h"

namespace ws_relay{
excel_refresh::excel_refresh(float price)
  : waves_idx_(0)
  , shop_cost_(0)
  , price_increase_(price){    
}

excel_refresh::~excel_refresh() = default;

void excel_refresh::int_refresh(int32_t waves_idx){
  shop_cost_ = -1;
  
  const auto battle_levels = excel_mgr->get_battle_levels();
  
  const auto it_shop = battle_levels->shop_map().find(waves_idx);
  if (it_shop == battle_levels->shop_map().end()){
    return;
  }
  waves_idx_ = it_shop->second.id();
  shop_cost_ = it_shop->second.first_reroll_price();
}

int32_t excel_refresh::cur_shop_refresh() const{
  return shop_cost_;
}

int32_t excel_refresh::next_shop_refresh(){
  auto _add = static_cast<int32_t>(price_increase_ * static_cast<float>(waves_idx_));
  if(_add <1){
    _add = 1;
  }
  shop_cost_ += _add;
  return shop_cost_;
}

int32_t excel_refresh::next_waves(){
  int_refresh(++waves_idx_);
  return waves_idx_;
}
}



