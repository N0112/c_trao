#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/timer.h"
#include "hardware/adc.h"

// 読み取るADチャネルを定義
// 0: GP26 (ADC0) 照度センサ
#define ADC_CHANNEL 0

// GPIOピンの定義
#define BUTTON_PIN 3  // ボタンが接続されているGPIOピン番号
#define BUZZER_PIN 12 // ブザーが接続されているGPIOピン番号

// タイマー割り込みの周期 (ms)
#define TIMER_INTERVAL_MS 5

// ボタンの状態を格納するグローバル変数（volatileキーワードで最適化を防ぐ）
volatile bool button_pressed = false;

// BUZZERのデューティサイクルの定義
#define BUZZER_ON 0.7 // ブザーONのデューティサイクル（30%）
#define BUZZER_OFF 0  // ブザーOFFのデューティサイクル（0%）

// ラの音（A3）を再生する関数
// 引数: slice_num - PWMスライス番号(PWM機能を構成する独立した信号生成ユニット)
void play_note_a(uint slice_num)
{
    int freq = 220; // ラの音（A3）の周波数（220Hz）

    // pwm_set_clkdiv()はPWMのクロック分周比を設定する関数
    // 125.0fはPWMのクロック分周比（125MHz / 125 = 1MHz）を指定している
    // つまり、PWMのクロックは1MHzになり
    // 例えば、1MHzのクロックで、周期を1000に設定すると、1kHzの音が鳴る
    pwm_set_clkdiv(slice_num, 125.0f);

    // PWMの周期を設定
    // pwm_set_wrap()はPWMの周期を設定する関数
    pwm_set_wrap(slice_num, 125000 / freq);

    // pwm_set_chan_level()はPWMのデューティサイクルを設定する関数
    // 125000 / freqはPWMの周期に対するデューティサイクルの比率を計算している
    // 例えば、周波数が220Hzの場合、125000 / 220 = 568.18...となる
    // これをデューティサイクルの比率として使用することで、音の大きさを調整できる
    // 0.3はデューティサイクルの比率（30%）を指定している
    pwm_set_chan_level(slice_num, PWM_CHAN_A, (125000 / freq) * BUZZER_ON);
}

// タイマー割り込み関数
bool timer_callback(struct repeating_timer *rt)
{
    // ボタンの状態を読み取る（プルアップなので、押されていないときはHIGH、押されているときはLOW）
    if (gpio_get(BUTTON_PIN) == 0)
    {
        // ボタンが押されたときの処理
        button_pressed = true;
    }
    else
    {
        // ボタンが離されたときの処理
        button_pressed = false;
    }
    return true; // 継続してタイマーを動作させる
}

int main()
{
    // 標準入出力を初期化（デバッグ用）
    stdio_init_all();

        // ADC (アナログ-デジタル変換器) の初期化
    adc_init();

    // ADCを使用するGPIOピンの設定
    // PicoのGPIOは、様々な機能に使用できる。
    // ここでは、GP26, GP27, GP28をアナログ入力として設定する。
    //  - gpio_set_function(ピン番号, 機能): ピンの機能を設定する。
    //  - gpio_set_dir(ピン番号, 方向): ピンの方向を設定する (入力または出力)。
    gpio_set_function(26, GPIO_FUNC_SIO); // GP26をSIO (標準入出力) 機能に設定
    gpio_set_dir(26, GPIO_IN);            // GP26を入力に設定
    gpio_set_function(27, GPIO_FUNC_SIO); // GP27をSIO機能に設定
    gpio_set_dir(27, GPIO_IN);            // GP27を入力に設定
    gpio_set_function(28, GPIO_FUNC_SIO); // GP28をSIO機能に設定
    gpio_set_dir(28, GPIO_IN);            // GP28を入力に設定

    // 選択されたADチャネルを設定
    // ADCには複数の入力チャネルがあり、どれを読み取るかを指定する必要がある。
    if (ADC_CHANNEL == 0)
    {
        adc_select_input(0); // ADC0 (GP26) を選択
    }
    else if (ADC_CHANNEL == 1)
    {
        adc_select_input(1); // ADC1 (GP27) を選択
    }
    else if (ADC_CHANNEL == 2)
    {
        adc_select_input(2); // ADC2 (GP28) を選択
    }
    else
    {
        // 定義されていないADC_CHANNELが指定された場合は、エラーとしてプログラムを終了する。
        return 1;

    // ボタン用GPIOの初期化
    // gpio_init()はGPIOの初期化を行う関数
    // gpio_set_dir()はGPIOの入出力方向を設定する関数
    // gpio_pull_up()はGPIOのプルアップ抵抗を有効にする関数
    // プルアップ抵抗とは、ボタンが押されていないときにGPIOピンをHIGHに保つための抵抗
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

    // ブザー用GPIOの初期化
    // gpio_set_function()はGPIOの機能を設定する関数
    // pwm_gpio_to_slice_num()はGPIOピン番号からPWMスライス番号を取得する関数
    // PWMスライス番号はPWM機能を構成する独立した信号生成ユニット
    // 例えば、GPIO12はPWM0のスライス番号0に対応している
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);

    // PWMの設定
    // pwm_get_default_config()はPWMのデフォルト設定を取得する関数
    // pwm_init()はPWMを初期化する関数
    // pwm_set_chan_level()はPWMのデューティサイクルを設定する関数
    pwm_config config = pwm_get_default_config();
    pwm_init(slice_num, &config, true);
    pwm_set_chan_level(slice_num, PWM_CHAN_A, BUZZER_OFF); // 最初は音を鳴らさない

    // タイマーの設定
    struct repeating_timer timer; // タイマー構造体を宣言
    // 繰り返しタイマーを設定する関数
    // - 第1引数: 繰り返し間隔 (マイクロ秒)。負の値は、最初の実行も遅延させる
    // - 第2引数: コールバック関数 (タイマー割り込み時に実行される関数)
    // - 第3引数: コールバック関数に渡すユーザーデータ (ここではNULL)
    // - 第4引数: 設定するタイマー構造体へのポインタ
    add_repeating_timer_ms(TIMER_INTERVAL_MS, timer_callback, NULL, &timer);

    // メインループ
    while (true)
    {
        // ボタンが押されていたら音を鳴らす
        if (button_pressed)
        {
            play_note_a(slice_num);
        }
        else
        {
            pwm_set_chan_level(slice_num, PWM_CHAN_A, BUZZER_OFF); // ブザーをOFF
        }
    }

    return 0;
}
}