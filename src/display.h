#pragma once
#include "screen_data.h"

// Owns the black/white ePaper panel.
//   full    = flashing full refresh (~2 s), removes ghosting
//   partial = fast refresh (~0.5 s) without flashing
class EpdDisplay {
 public:
  void begin();
  void render(const ScreenData& d, bool full);
};
