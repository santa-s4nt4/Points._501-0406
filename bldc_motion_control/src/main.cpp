#include "unit_rolleri2c.hpp" // ユーザー指定のヘッダー
#include <FastLED.h>
#include <M5AtomS3.h>

// --- LED設定 (ここを実際の配線に合わせて変更してください) ---
#define LED_PIN 5      // LEDテープのDINを接続するピン番号 (G2, G1はI2Cで使用中)
#define NUM_LEDS 9     // LEDの数
#define BRIGHTNESS 255 // 明るさ(0-255)

// --- インスタンス作成 ---
UnitRollerI2C roller;
CRGB leds[NUM_LEDS];

// --- グローバル変数 ---
int32_t target_position = 0; // 目標位置
unsigned long last_print_time = 0;
bool is_free_mode = false; // フリーモード状態管理フラグ

// デフォルトパラメータ
const int32_t DEFAULT_MAX_CURRENT = 100000;
const int32_t DEFAULT_SPEED_LIMIT = 240000;

// ===============================================================
// 関数プロトタイプ宣言
// ===============================================================
uint8_t scanI2CAddress();
void performHoming();
void performCalibration();
void processSerialInput();
void executeMotorCommand(String cmd);

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
// シリアル入力振分処理 (LED vs モーター)
// ---------------------------------------------------------------
void processSerialInput() {
  if (Serial.available() > 0) {
    // 最初の1バイトを覗き見して判断する
    char header = Serial.peek();

    if (header == 'S') {
      // --- LEDデータ処理 ---
      // ヘッダー('S') + データ(LED数*3バイト) が揃うまで待つ
      // ※データ不揃いの場合は処理せず次のループへ (ブロッキング回避)
      if (Serial.available() >= (1 + NUM_LEDS * 3)) {
        Serial.read(); // ヘッダー 'S' を読み捨てる
        Serial.readBytes((char *)leds, NUM_LEDS * 3); // LEDデータを一括読み込み
        FastLED.show();                               // LED更新
      }
    } else {
      // --- モーターコマンド処理 ---
      // 改行まで読み込む
      String cmd = Serial.readStringUntil('\n');
      cmd.trim();
      if (cmd.length() > 0) {
        executeMotorCommand(cmd);
      }
    }
  }
}

// ---------------------------------------------------------------
// モーターコマンド実行
// ---------------------------------------------------------------
void executeMotorCommand(String cmd) {
  cmd.toUpperCase();

  // --- C: キャリブレーション ---
  if (cmd.startsWith("C")) {
    performCalibration();
    return;
  }

  // --- F: フリーモード ---
  if (cmd.startsWith("F")) {
    roller.setOutput(0);
    roller.setMode(ROLLER_MODE_ENCODER);
    is_free_mode = true;
    Serial.println("Motor Free (Encoder Mode, Output 0)");
    M5.Lcd.fillScreen(TFT_YELLOW);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.print("FREE MODE");
    return;
  }

  // --- L: ロックモード (制御復帰) ---
  if (cmd.startsWith("L")) {
    roller.setMode(ROLLER_MODE_POSITION);
    roller.setSpeed(DEFAULT_SPEED_LIMIT);
    roller.setSpeedMaxCurrent(DEFAULT_MAX_CURRENT);
    target_position = roller.getPos();
    roller.setPos(target_position);
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
    if (!is_free_mode) {
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

  // --- LED初期化 ---
  FastLED.addLeds<SK6812, LED_PIN, GRB>(leds, NUM_LEDS); // SK6812MINIの場合
  FastLED.setBrightness(BRIGHTNESS);
  // 起動確認: 青点灯
  fill_solid(leds, NUM_LEDS, CRGB::Blue);
  FastLED.show();
  delay(500);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  // --- I2C / Motor初期化 ---
  delay(1000);
  Wire.begin(2, 1, 400000UL); // AtomS3 Grove Port
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

  // シリアル入力処理 (LED or Motor)
  processSerialInput();

  // 定期テレメトリ送信
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