#include "esp32-hal.h"
#include "unit_rolleri2c.hpp" // ユーザー指定のヘッダー
#include <M5AtomS3.h>

// --- インスタンス作成 ---
UnitRollerI2C roller;

// --- グローバル変数 ---
int32_t target_position = 0; // 目標位置
unsigned long last_print_time = 0;
bool is_free_mode = false; // フリーモード状態管理フラグ

// デフォルトパラメータ
const int32_t DEFAULT_MAX_CURRENT = 100000;
const int32_t DEFAULT_SPEED_LIMIT = 240000;

// 定数定義 (ライブラリ側で定義済みのため削除)
// const roller_mode_t ROLLER_MODE_ENCODER = (roller_mode_t)4;

// ===============================================================
// 関数プロトタイプ宣言
// ===============================================================
uint8_t scanI2CAddress();
void performHoming();
void performCalibration();
void parseSerialCommand();

// ===============================================================
// 関数定義
// ===============================================================

// ---------------------------------------------------------------
// I2Cアドレススキャン関数
// ---------------------------------------------------------------
uint8_t scanI2CAddress() {
  Serial.println("Scanning I2C bus...");
  M5.Lcd.fillScreen(TFT_ORANGE);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.print("Scanning...");

  byte error, address;
  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.printf("I2C Device found at 0x%02X\n", address);
      M5.Lcd.setCursor(0, 20);
      M5.Lcd.printf("Addr: 0x%02X", address);
      delay(500);
      return address;
    }
  }
  Serial.println("No I2C devices found");
  return 0;
}

// ---------------------------------------------------------------
// キャリブレーション関数
// ---------------------------------------------------------------
void performCalibration() {
  Serial.println("Start encoder calibration");
  M5.Lcd.fillScreen(TFT_YELLOW);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.print("Calibrating...");

  // 公式手順に基づくキャリブレーション
  roller.setOutput(0);
  delay(100);
  roller.startAngleCal();
  delay(100);

  Serial.println("Calibrating...");
  while (roller.getCalBusyStatus()) {
    delay(100);
    Serial.print(".");
  }
  Serial.println();

  roller.updateAngleCal();
  Serial.println("Encoder calibration done");

  delay(500);

  // 復帰処理
  roller.setOutput(0);
  roller.setMode(ROLLER_MODE_POSITION);
  roller.setSpeed(DEFAULT_SPEED_LIMIT);
  roller.setSpeedMaxCurrent(DEFAULT_MAX_CURRENT);
  roller.setOutput(1);

  target_position = roller.getPos();

  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.print("Calib Done");
  delay(1000);
  M5.Lcd.fillScreen(TFT_BLACK);
}

// ---------------------------------------------------------------
// ホーミング関数 (突き当て原点出し)
// ---------------------------------------------------------------
void performHoming() {
  Serial.println(">>> Homing Start...");
  M5.Lcd.fillScreen(TFT_MAGENTA);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.print("Homing...");

  // 1. 速度制御モードに切り替え
  roller.setMode(ROLLER_MODE_SPEED);
  roller.setSpeedMaxCurrent(DEFAULT_MAX_CURRENT);
  roller.setOutput(1);

  // 2. ゆっくり逆回転 (マイナス方向へ)
  roller.setSpeed(-200);

  unsigned long start_time = millis();
  int32_t last_pos = roller.getPos();
  int same_pos_count = 0;

  // 3. 突き当て検知ループ
  while (true) {
    delay(50);
    int32_t curr_pos = roller.getPos();

    if (abs(curr_pos - last_pos) < 5) {
      same_pos_count++;
    } else {
      same_pos_count = 0;
    }
    last_pos = curr_pos;

    if (same_pos_count > 10) {
      Serial.println("Hit detected!");
      break;
    }
    if (millis() - start_time > 10000) {
      Serial.println("Homing Timeout!");
      break;
    }
  }

  // 4. 停止 & 原点設定
  roller.setSpeed(0);
  delay(500);

  // 位置制御モードに戻す
  roller.setMode(ROLLER_MODE_POSITION);
  roller.setSpeed(DEFAULT_SPEED_LIMIT);
  roller.setSpeedMaxCurrent(DEFAULT_MAX_CURRENT);

  target_position = roller.getPos();

  is_free_mode = false;

  M5.Lcd.fillScreen(TFT_BLACK);
  Serial.printf("Homing Done. Current Pos: %d\n", target_position);
}

// ---------------------------------------------------------------
// シリアルコマンド解析
// ---------------------------------------------------------------
void parseSerialCommand() {
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();

    // --- C: キャリブレーション ---
    if (cmd.startsWith("C")) {
      performCalibration();
      return;
    }

    // --- F: フリーモード ---
    if (cmd.startsWith("F")) {
      // 1. 出力をOFFにして脱力
      roller.setOutput(0);

      // 2. Encoder Mode (Mode 4) に設定
      // ライブラリで定義されている定数を使用 (キャスト不要)
      roller.setMode(ROLLER_MODE_ENCODER);

      // 3. 出力はOFFのまま維持
      // Output 0でもエンコーダー値の更新が続くことを期待

      is_free_mode = true;
      Serial.println("Motor Free (Encoder Mode, Output 0)");
      M5.Lcd.fillScreen(TFT_YELLOW);
      M5.Lcd.setCursor(0, 0);
      M5.Lcd.print("FREE MODE");
      return;
    }

    // --- L: ロックモード (制御復帰) ---
    if (cmd.startsWith("L")) {
      // 1. 位置制御モードに戻す
      roller.setMode(ROLLER_MODE_POSITION);

      // 2. パラメータ再設定
      roller.setSpeed(DEFAULT_SPEED_LIMIT);
      roller.setSpeedMaxCurrent(DEFAULT_MAX_CURRENT);

      // 3. 現在地をターゲットにしてロック
      target_position = roller.getPos();
      roller.setPos(target_position);

      // 4. 出力再開
      roller.setOutput(1);

      is_free_mode = false;
      Serial.println("Motor Locked");
      M5.Lcd.fillScreen(TFT_BLACK);
      return;
    }

    // --- H: ホーミング ---
    if (cmd.startsWith("H")) {
      performHoming();
      return;
    }

    // --- P: 位置指令 ---
    int p_index = cmd.indexOf('P');
    if (p_index != -1) {
      String pos_str = cmd.substring(p_index + 1);
      target_position = pos_str.toInt();
      roller.setPos(target_position);
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

  M5.Display.fillScreen(TFT_BLUE);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(0, 0);
  M5.Display.print("Init...");

  delay(1000);
  Wire.begin(2, 1, 400000UL); // AtomS3 Grove
  delay(200);

  uint8_t found_addr = scanI2CAddress();
  if (found_addr == 0)
    found_addr = 0x64; // Default

  Serial.printf("Connecting to Roller at 0x%02X...\n", found_addr);

  if (!roller.begin(&Wire, found_addr, 2, 1, 400000)) {
    Serial.println("Connect Failed!");
    M5.Lcd.fillScreen(TFT_RED);
    M5.Lcd.print("Conn Fail");
    while (1)
      delay(100);
  } else {
    Serial.println("Connected.");
    M5.Lcd.fillScreen(TFT_GREEN);
    M5.Lcd.print("Connected");

    // 初期化シーケンス
    roller.setOutput(0);
    delay(100);

    roller.setRGBBrightness(0);
    // 1. Start with Calibration
    performCalibration();

    // 2. Wait 3 seconds (Locked state from calibration)
    M5.Lcd.fillScreen(TFT_BLUE);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.print("Wait 1s...");
    Serial.println("Calibration Done. Wait 1s...");
    delay(1000);

    // 3. Enter Free Mode
    Serial.println("Enter Free Mode");
    roller.setMode(ROLLER_MODE_ENCODER);

    // Reset Position
    roller.setDialCounter(0);
    roller.setPos(0);
    target_position = 0;

    // Output OFF (Free)
    roller.setOutput(0);
    is_free_mode = true;

    M5.Lcd.fillScreen(TFT_YELLOW);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.print("Free Mode");
  }

  delay(500);
  M5.Lcd.fillScreen(TFT_BLACK);
}

void loop() {
  M5.update();
  unsigned long current_time = millis();

  parseSerialCommand();

  if (current_time - last_print_time >= 30) {
    last_print_time = current_time;

    int32_t real_pos = roller.getPosReadback();

    // Free Mode時はエラー値(16777216)であっても強制的に送信する
    if (is_free_mode || real_pos != 16777216) {
      if (is_free_mode) {
        Serial.printf("FreePos:%d\n", real_pos);
      } else {
        Serial.printf("Pos:%d\n", real_pos);
      }

      M5.Lcd.setCursor(0, 0);
      M5.Lcd.printf("Tgt:%d   \n", target_position);
      M5.Lcd.printf("Now:%d   \n", real_pos);
    }
  }
}