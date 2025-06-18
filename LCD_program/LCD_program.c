#include <stdio.h>        // 標準入出力関数 (printf など) を使うためにインクルード
#include <stdlib.h>       // 標準ライブラリ関数 (rand() など) を使うためにインクルード
#include "pico/stdlib.h"  // Pico SDK の標準関数を使うためにインクルード
#include "hardware/i2c.h" // I2C (Inter-Integrated Circuit) 通信に関連する関数を使うためにインクルード
#include "font8x8.h"      // 8x8 ドットフォントのデータを使うためにインクルード

/* 定義 (マクロ) */
#define I2C_SDA_PIN 6                                          // I2C の SDA (シリアルデータ) ピン：GPIO 6番を使用することを定義
#define I2C_SCL_PIN 7                                          // I2C の SCL (シリアルクロック) ピン：GPIO 7番を使用することを定義
#define I2C_SPEED 1000000                                      // I2C の通信速度を 1000000 Hz (1 MHz) に定義
#define SSD1327_ADDR 0x3D                                      // SSD1327 OLED ディスプレイの I2C アドレスを 0x3D に定義
#define DISPLAY_WIDTH 128                                      // OLED ディスプレイの幅を 128 ピクセルに定義
#define DISPLAY_HEIGHT 128                                     // OLED ディスプレイの高さを 128 ピクセルに定義
#define DISPLAY_DATA_SIZE (DISPLAY_WIDTH * DISPLAY_HEIGHT / 2) // ディスプレイに必要なデータ量を計算して定義。SSD1327 は 1 ピクセルあたり 4 ビットなので、バイト数は総ピクセル数の半分
#define font_HEART \x03                                        // ハートのフォントデータを定義。8x8 ドットフォントの一部として使用

/* グローバル変数 */
i2c_inst_t *i2c = i2c1;
// 使用する I2C インスタンスとして i2c1 を指定
uint8_t buffer[DISPLAY_DATA_SIZE]; // ディスプレイに表示するデータを一時的に格納するバッファ (配列)

/* プロトタイプ宣言 (関数の事前定義) */
static void i2c_init_pico();
static void ssd1327_send_command(uint8_t command);
static void ssd1327_send_data(uint8_t data);
static void ssd1327_set_window(uint16_t X_start, uint16_t Y_start, uint16_t X_end, uint16_t Y_end);
static void ssd1327_init();
static void ssd1327_set_display(uint8_t *data);

/* 関数 */

// I2C を初期化する関数
static void i2c_init_pico()
{
    i2c_init(i2c, I2C_SPEED);
    // 指定した I2C インスタンス (i2c1) と速度 (1MHz) で I2C を初期化
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C); // SDA ピン (GPIO 6) を I2C の機能として使用するように設定
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C); // SCL ピン (GPIO 7) を I2C の機能として使用するように設定
    gpio_pull_up(I2C_SDA_PIN);                     // SDA ピンにプルアップ抵抗を有効化
    gpio_pull_up(I2C_SCL_PIN);                     // SCL ピンにプルアップ抵抗を有効化
}

// SSD1327 にコマンドを送信する関数
static void ssd1327_send_command(uint8_t command)
{
    uint8_t buffer[2] = {0x00, command};
    // 送信するデータバッファを作成。最初のバイト 0x00 はコマンドであることを示す制御バイト
    i2c_write_blocking(i2c, SSD1327_ADDR, buffer, 2, false); // I2C デバイス (SSD1327_ADDR) にバッファの内容 (2バイト) を送信。false は STOP ビットを送信しないことを意味します
}

// SSD1327 にデータを送信する関数
static void ssd1327_send_data(uint8_t data)
{
    uint8_t buffer[2] = {0x40, data};
    // 送信するデータバッファを作成。最初のバイト 0x40 はデータであることを示す制御バイト
    i2c_write_blocking(i2c, SSD1327_ADDR, buffer, 2, false); // I2C デバイスにバッファの内容 (2バイト) を送信
}

// SSD1327 の描画範囲 (ウィンドウ) を設定する関数
static void ssd1327_set_window(uint16_t X_start, uint16_t Y_start, uint16_t X_end, uint16_t Y_end)
{
    // 引数として渡された座標がディスプレイの範囲を超えていないかチェック
    if (X_start >= DISPLAY_WIDTH || Y_start >= DISPLAY_HEIGHT ||
        X_end >= DISPLAY_WIDTH || Y_end >= DISPLAY_HEIGHT)
    {
        return; // 範囲外の場合は何もせずに関数を終了
    }

    // コラム (X軸) アドレスの設定
    ssd1327_send_command(0x15);
    // コラムアドレス設定コマンドを送信
    ssd1327_send_command(X_start / 2); // 開始 X 座標を 2 で割った値を送信 (SSD1327 は 2 ピクセル単位でアドレスを指定する)
    ssd1327_send_command(X_end / 2);
    // 終了 X 座標を 2 で割った値を送信

    // ロウ (Y軸) アドレスの設定
    ssd1327_send_command(0x75);
    // ロウアドレス設定コマンドを送信
    ssd1327_send_command(Y_start); // 開始 Y 座標を送信
    ssd1327_send_command(Y_end);
    // 終了 Y 座標を送信
}

// SSD1327 を初期化する関数
static void ssd1327_init()
{
    // SSD1327 の初期化シーケンス (データシートに記載されている初期設定)
    ssd1327_send_command(0xae); // ディスプレイをオフにする (スリープモード ON)

    // コラムアドレス設定
    ssd1327_send_command(0x15); // コラムアドレス設定コマンド
    ssd1327_send_command(0x00); // 開始コラムアドレス (0)
    ssd1327_send_command(0x7f); // 終了コラムアドレス (127)

    // ロウアドレス設定
    ssd1327_send_command(0x75); // ロウアドレス設定コマンド
    ssd1327_send_command(0x00); // 開始ロウアドレス (0)
    ssd1327_send_command(0x7f); // 終了ロウアドレス (127)

    // コントラスト設定
    ssd1327_send_command(0x81); // コントラスト設定コマンド
    ssd1327_send_command(0x80); // コントラスト値 (0x80 は中間的な明るさ)

    // セグメントリマップ (表示の左右反転)
    ssd1327_send_command(0xa0); // セグメントリマップ設定コマンド
    ssd1327_send_command(0x51); // リマップ設定 (0x51 で左右反転。必要に応じて変更)

    // スタートライン設定
    ssd1327_send_command(0xa1); // スタートライン設定コマンド
    ssd1327_send_command(0x00); // スタートライン (通常は 0)

    // 表示オフセット設定
    ssd1327_send_command(0xa2); // 表示オフセット設定コマンド
    ssd1327_send_command(0x00); // オフセット値 (通常は 0)

    // 全画面表示オン/オフ (反転表示)
    ssd1327_send_command(0xa4); // 全画面表示オフ (通常表示モード)

    // マルチプレックス比設定 (表示ライン数)
    ssd1327_send_command(0xa8); // マルチプレックス比設定コマンド
    ssd1327_send_command(0x7f); // 128 ライン (0x7F は 127 を意味し、0 から数えるので 128 ライン)

    // マスターコンフィグレーション
    ssd1327_send_command(0xad);
    ssd1327_send_command(0x02);

    // 電源制御
    ssd1327_send_command(0xb0);
    ssd1327_send_command(0x0b);

    // 位相長設定
    ssd1327_send_command(0xb1);
    ssd1327_send_command(0xf1);

    // 表示イネーブル (リセット)
    ssd1327_send_command(0xab);
    ssd1327_send_command(0x01);

    // プリチャージ電流設定
    ssd1327_send_command(0xbc);
    ssd1327_send_command(0x3f);

    // VCOMH レベル設定
    ssd1327_send_command(0xbe);
    ssd1327_send_command(0x0f);

    // クロック設定
    ssd1327_send_command(0xd5); // 表示クロック制御の設定コマンド
    ssd1327_send_command(0x62); // クロック設定値

    // コントラスト微調整
    ssd1327_send_command(0x87);
    ssd1327_send_command(0x0f);

    ssd1327_send_command(0xAF); // ディスプレイをオンにする (スリープモード OFF)
}

// バッファの内容を SSD1327 に送信して表示する関数
static void ssd1327_set_display(uint8_t *data)
{
    const int32_t data_length = DISPLAY_DATA_SIZE + 1; // 送信するデータ長は、表示データサイズに制御バイト (0x40) の 1 バイトを加えたもの
    uint8_t buffer[data_length];
    // 送信するデータを格納するローカルなバッファ
    buffer[0] = 0x40;
    // 最初のバイトはデータであることを示す制御バイト (0x40)
    for (int i = 0; i < DISPLAY_DATA_SIZE; i++)
    {
        buffer[i + 1] = data[i];
        // グローバルな表示バッファの内容を、送信用のバッファにコピー
    }
    ssd1327_set_window(0, 0, DISPLAY_WIDTH - 1, DISPLAY_HEIGHT - 1);   // ディスプレイ全体の書き込み範囲を設定
    i2c_write_blocking(i2c, SSD1327_ADDR, buffer, data_length, false); // I2C でディスプレイにデータを送信
}

// 指定した座標のピクセルの明るさを設定する関数 (4ビットグレースケール：0〜15 の値で明るさを指定)
void set_pixel(uint8_t *buffer, int x, int y, uint8_t brightness)
{
    int index = (y * DISPLAY_WIDTH + x) / 2; // 指定された x, y 座標に対応するバッファ内のインデックスを計算
                                             // SSD1327 は横方向に 2 ピクセルで 1 バイトを扱うため、インデックスを 2 で割る
    if (x % 2 == 0)
    {
        // x が偶数の場合、そのピクセルはバイトの上位 4 ビットに対応
        buffer[index] = (buffer[index] & 0x0F) | (brightness << 4); // 元の下位 4 ビットを保持し、上位 4 ビットを新しい明るさで更新
    }
    else
    {
        // x が奇数の場合、そのピクセルはバイトの下位 4 ビットに対応
        buffer[index] = (buffer[index] & 0xF0) | (brightness & 0x0F); // 元の上位 4 ビットを保持し、下位 4 ビットを新しい明るさで更新
    }
}

//文字を指定サイズに拡大して描画する関数
void draw_char_scaled(uint8_t *buffer, int x, int y, int col, int row, uint8_t brightness, int scale)
{
    for(int y_offset = 0; y_offset < scale; y_offset++)     // 縦方向の拡大
    
    {
        for(int x_offset = 0; x_offset < scale; x_offset++) // 横方向の拡大
        {   
            // 拡大された座標を計算して、指定されたピクセルの明るさを設定
            set_pixel(buffer, (x + col * scale )+ x_offset, (y + row * scale) + y_offset, brightness);  
        }
    }
}

// 指定した座標に 1 文字を描画する関数
void draw_char(uint8_t *buffer, char c, int x, int y, uint8_t brightness, int scale)
{
    int font_index = -1; // フォント配列のインデックスを初期化

    // 描画する文字が数字かアルファベットかを判定し、フォント配列の対応するインデックスを取得
    if (c >= '0' && c <= '9')  
    {
        font_index = c - '0'; // 数字をインデックス (0〜9) に変換
    }
    else if (c >= 'A' && c <= 'Z')
    {
        font_index = c - 'A' + 10; // 大文字アルファベットをインデックス (10〜35) に変換
    }
    else if (c == '\x03') 
    {
        font_index = 36; // ハートマークの文字コード (\x03) の場合、インデックスを 36 に設定
    }
    else
    {
        return; // サポートされていない文字の場合は、何も描画せずに関数を終了
    }

    // フォントデータから文字のパターンを取得して、ピクセル単位で描画
    for (int row = 0; row < 8; row++) // 8x8 フォントなので、縦に 8 行処理

    {
        uint8_t line = font_8x8[font_index][row]; // フォント配列から、指定された文字の指定された行のデータを取得 (1バイトで 8 ピクセルの情報)
        for (int col = 0; col < 8; col++)         // 横に 8 列処理 (1バイトの各ビットが 1 ピクセルに対応)
        {
            // 各ビットが 1 (点灯) か 0 (消灯) かを判定
            if (line & (1 << (7 - col))) // 左端から順にビットをチェック (ビット演算で判定)
            {
                // 点灯するピクセルの場合、指定された座標に拡大して描画
                draw_char_scaled(buffer, x, y, col, row, brightness, scale); 
            }
        }
    }
}

// 指定した座標に文字列を描画する関数
void draw_string(uint8_t *buffer, const char *str, int x, int y, uint8_t brightness)
{
    int scale = 2; // 文字を 2 倍に拡大するスケールを設定

    while (*str) // 文字列の終端 ('\0') まで繰り返す
    {
        draw_char(buffer, *str, x, y, brightness, scale); // 現在の文字を描画
        x += 8 * scale + 4;
        // 次の文字を描画する X 座標を 8×scale分、右に移動 (8x8 フォントの幅)
        // 文字間のスペースを4ピクセル分確保
        str++;
        // 文字列の次の文字を指すようにポインタをインクリメント
    }
}

void draw_countdown(char c)
{
   // 表示バッファを毎回クリア
    for (int i = 0; i < DISPLAY_DATA_SIZE; i++)
    {
        buffer[i] = 0; // バッファの各バイトを 0 でクリア
    }

    int scale = 8; //　文字を8倍に拡大
    draw_char(buffer, c, 32, 32, 15, scale); // 指定された文字をバッファに描画。指定座標 (32, 32) に、明るさ 15 で描画
    
}

int main()
{
    stdio_init_all(); // 標準入出力 (USB シリアルなど) を初期化。デバッグなどに使用可能

    // I2C の初期化
    i2c_init_pico(); // I2C 通信に必要な設定 (ピン、速度など) を行う

    // SSD1327 OLED ディスプレイの初期化
    ssd1327_init(); // SSD1327 に初期設定コマンドを送信し、使用できる状態にする

    int state = 1;

    while (true) // メインループ：無限に繰り返して、顔の表情をアニメーション表示する
    {
        if(state == 1)
        {
             // 画面表示バッファをクリア (黒で塗りつぶし)
            for (int i = 0; i < DISPLAY_DATA_SIZE; i++)
            {
                buffer[i] = 0; // バッファの各バイトを 0 でクリア (4 ビットグレースケールで 0 は最も暗い状態)
            }
                ssd1327_set_display(buffer); // クリアしたバッファの内容をディスプレイに送信し、画面を黒で初期化
                sleep_ms(1000);
        }
        else if(state == 2)
        {
            draw_countdown('3'); // カウントダウンの数字 '3' を表示
            ssd1327_set_display(buffer); // バッファの内容をディスプレイに送信
            sleep_ms(1000); // 1 秒待機
        }
        else if(state == 3)
        {
            draw_countdown('2');
            ssd1327_set_display(buffer); // バッファの内容をディスプレイに送信
            sleep_ms(1000); // 1 秒待機
        }
        else if(state == 4)
        {
            draw_countdown('1');
            ssd1327_set_display(buffer); // バッファの内容をディスプレイに送信
            sleep_ms(1000); // 1 秒待機
        }
        else if(state == 5 || state == 7 || state == 9)
        {
            // 画面表示バッファをクリア (黒で塗りつぶし)
            for (int i = 0; i < DISPLAY_DATA_SIZE; i++)
            {
                buffer[i] = 0; // バッファの各バイトを 0 でクリア
            }
    
            draw_string(buffer, "HAPPY", 14, 18, 15);
            draw_string(buffer, "BIRTH", 14, 56, 15);
            draw_string(buffer, "DAY\x03", 26 , 94, 15);

            ssd1327_set_display(buffer); // バッファの内容をディスプレイに送信
            sleep_ms(1000); // 1 秒待機
        }
        else if(state == 6 || state == 8 || state == 10)
        {
            // 画面表示バッファをクリア (黒で塗りつぶし)
            for (int i = 0; i < DISPLAY_DATA_SIZE; i++)
            {
                buffer[i] = 0; // バッファの各バイトを 0 でクリア
            }
    
            draw_string(buffer, "NAGOMI", 6, 56, 15);
            ssd1327_set_display(buffer); // バッファの内容をディスプレイに送信
            sleep_ms(1000); // 1 秒待機
        }

        if(state < 10)     // 状態が 10 未満の場合
        {
            state++;    // 状態を 1 つ進める
        }
        else
        {
            state = 1;   // 状態が 10 以上の場合は、状態を 1 にリセット
        }
    }
        
}