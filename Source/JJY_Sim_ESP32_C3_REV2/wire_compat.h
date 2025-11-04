#pragma once
//
// wire_compat.h - ESP32-C3 用の I2C バス互換ヘッダ
//
// 一部のライブラリ（例：SSD1306Wire）は Wire1 を使用するが、
// ESP32-C3 には Wire1 が存在しないため、Wire にマッピングする。
// 最新の ESP32 ボードマネージャでは未定義のことがあるため、
// 旧バージョンとのバッティングを避けて #ifndef で保護する。
// 
// もし古いボードマネージャを使用しており、Wire1 が既に定義されていた場合、
// このヘッダーはスルーされる。
// 

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_IDF_TARGET_ESP32C3)
  #ifndef Wire1
    #define Wire1 Wire
  #endif
#endif
