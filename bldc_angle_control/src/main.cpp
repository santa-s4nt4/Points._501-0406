#include "unit_rolleri2c.hpp"
#include <FastLED.h>
#include <M5AtomS3.h>

// --- LED設定 ---
#define LED_PIN 5
#define NUM_LEDS 9
#define BRIGHTNESS 255

// --- インスタンス作成 ---
UnitRollerI2C roller;
CRGB leds[NUM_LEDS];

// --- グローバル変数 ---
int32_t target_position = 0;
unsigned long last_print_time = 0;
bool is_free_mode = false;
int32_t origin_offset = 0; // TD用の0基準オフセット

const int32_t DEFAULT_MAX_CURRENT = 100000;
const int32_t DEFAULT_SPEED_LIMIT = 240000;

// ===============================================================
// 関数定義
// ===============================================================

uint8_t scanI2CAddress() {
  byte error, address;
  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) {
      return address;
    }
  }
  return 0x64; // 見つからない場合はデフォルトアドレスを返す
}

void performHoming() {
  roller.setMode(ROLLER_MODE_SPEED);
  roller.setSpeedMaxCurrent(DEFAULT_MAX_CURRENT);
  roller.setOutput(1);
  roller.setSpeed(-200);

  unsigned long start_time = millis();
  int32_t last_pos = roller.getPos();
  int same_pos_count = 0;

  while (true) {
    delay(50);
    int32_t curr_pos = roller.getPos();

    if (abs(curr_pos - last_pos) < 5) {
      same_pos_count++;
    } else {
      same_pos_count = 0;
    }
    last_pos = curr_pos;

    if (same_pos_count > 10 || millis() - start_time > 10000) {
      break;
    }
  }

  roller.setSpeed(0);
  delay(500);

  roller.setMode(ROLLER_MODE_POSITION);
  roller.setSpeed(DEFAULT_SPEED_LIMIT);
  roller.setSpeedMaxCurrent(DEFAULT_MAX_CURRENT);

  target_position = roller.getPos();
  is_free_mode = false;
}

void executeMotorCommand(String cmd) {
  cmd.toUpperCase();

  // --- F: フリーモード ---
  if (cmd.startsWith("F")) {
    roller.setOutput(0);
    roller.setMode(ROLLER_MODE_ENCODER);
    is_free_mode = true;
    return;
  }

  // --- L: ロックモード (現在地で静かに固定) ---
  if (cmd.startsWith("L")) {
    int32_t real_pos = roller.getPosReadback();
    if (real_pos == 16777216) {
      real_pos = roller.getPos();
    }

    roller.setMode(ROLLER_MODE_POSITION);
    roller.setSpeed(DEFAULT_SPEED_LIMIT);
    roller.setSpeedMaxCurrent(DEFAULT_MAX_CURRENT);

    target_position = real_pos;
    roller.setPos(target_position);
    roller.setOutput(1);

    is_free_mode = false;
    return;
  }

  // --- H: ホーミング ---
  if (cmd.startsWith("H")) {
    performHoming();
    return;
  }

  // --- P: 位置指令 (TDの0基準の値を絶対座標に変換して指示) ---
  int p_index = cmd.indexOf('P');
  if (p_index != -1) {
    String pos_str = cmd.substring(p_index + 1);
    target_position = pos_str.toInt() + origin_offset;
    if (!is_free_mode) {
      roller.setPos(target_position);
    }
  }
}

void processSerialInput() {
  if (Serial.available() > 0) {
    char header = Serial.peek();

    if (header == 'S') {
      if (Serial.available() >= (1 + NUM_LEDS * 3)) {
        Serial.read();
        Serial.readBytes((char *)leds, NUM_LEDS * 3);
        FastLED.show();
      }
    } else {
      String cmd = Serial.readStringUntil('\n');
      cmd.trim();
      if (cmd.length() > 0) {
        executeMotorCommand(cmd);
      }
    }
  }
}

// ===============================================================
// Setup & Loop
// ===============================================================

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  M5.begin(cfg);

  // --- LED初期化 ---
  FastLED.addLeds<SK6812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  fill_solid(leds, NUM_LEDS, CRGB::Blue);
  FastLED.show();
  delay(500);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  // --- I2C / Motor初期化 ---
  delay(1000);
  Wire.begin(2, 1, 400000UL);
  delay(200);

  uint8_t found_addr = scanI2CAddress();

  if (roller.begin(&Wire, found_addr, 2, 1, 400000)) {
    // 1. 位置制御モードでトルクON (ここで少し飛ぶのは仕様)
    roller.setMode(ROLLER_MODE_POSITION);
    roller.setSpeed(DEFAULT_SPEED_LIMIT);
    roller.setSpeedMaxCurrent(DEFAULT_MAX_CURRENT);
    roller.setPos(0);
    roller.setOutput(1);
    delay(300);

    // 2. -2000posに回して磁極を完全に馴染ませる
    roller.setPos(-2000);
    delay(500);

    // 3. 安定した位置をTD通信用の「原点オフセット」として記憶
    origin_offset = roller.getPosReadback();
    target_position = origin_offset;

    // 4. トルクOFFにしてフリーモードで待機
    roller.setMode(ROLLER_MODE_ENCODER);
    roller.setOutput(0);
    is_free_mode = true;
  }
}

void loop() {
  M5.update();
  unsigned long current_time = millis();

  processSerialInput();

  // 定期テレメトリ送信 (30ms毎)
  if (current_time - last_print_time >= 30) {
    last_print_time = current_time;

    int32_t real_pos = roller.getPosReadback();

    if (is_free_mode || real_pos != 16777216) {
      // TD用にオフセットを引いて「0基準の座標」として送信
      int32_t td_pos = real_pos - origin_offset;

      if (is_free_mode) {
        Serial.printf("FreePos:%d\n", td_pos);
      } else {
        Serial.printf("Pos:%d\n", td_pos);
      }
    }
  }
}