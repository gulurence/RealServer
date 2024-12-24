#pragma once

#include <cstdint>

class excel_refresh{
public:
  explicit excel_refresh(float price);

  virtual ~excel_refresh();

  void set_price_increase(float price){
    price_increase_ = price;
  }
  
  void int_refresh(int32_t waves_idx);
  
  int32_t cur_shop_refresh() const;
  int32_t next_shop_refresh();
  int32_t next_waves();
  
protected:
  int32_t waves_idx_;
  int32_t shop_cost_;
  //int32_t refresh_count_;

  float price_increase_;
};  //excel_refresh



