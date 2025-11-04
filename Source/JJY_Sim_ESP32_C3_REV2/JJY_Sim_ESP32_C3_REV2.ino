/*
 * JJY Simulator Rev.2
 * ESP32-C3-WROOM-02
 * 
 * ファイル/環境設定の追加のボードマネージャに
 * "https://dl.espressif.com/dl/package_esp32_index.json" を追加
 * 
 * [env:wemos_d1_mini32]
 * platform = espressif32
 * board = ESP32C3 Dev Module
 *
 * PWM出力   = A:IO10, B:IO4
 * OLED(I2C) = SCL:IO6, SDA:IO7, RSES:IO8 
 */
#include <WiFi.h>
#include <time.h>
#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include "wire_compat.h"
#include "SSD1306Wire.h" // legacy include: `#include "SSD1306.h"`
#include "OLEDDisplayUi.h"
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include "FS.h"
#include "SPIFFS.h"
#include <driver/ledc.h>

#define Wire1 Wire

#define JJY_TYPE          0       // 0:East(40kHz) / 1:West(60kHz)
#define WIFI_DEFAULT      1

#define FORMAT_SPIFFS_IF_FAILED true
#define FILE_NAME         "/JJY_TYPE.DAT"

#define OUTPUT_TIMES      3
#define JST       		    3600*9

#undef  BUILTIN_LED
#define BUILTIN_LED       0       // GPIO0
#define PIN_LED_WAVE      BUILTIN_LED
#define PIN_LED_WIFI      5       // GPIO5
#define PIN_CONFIG        9       // GPIO9
#define PIN_OLED_RES      8       // GPIO8
#define PIN_OLED_SCL      6       // GPIO6
#define PIN_OLED_SDA      7       // GPIO7
#define PIN_PWM_A         10      // GPIO10
#define PIN_PWM_B         4       // GPIO4

#define I2CADR            0x3c

#define LED_ON            0
#define LED_OFF           1
#define LED_BLINK         2

#define JJY_FREQ_EAST     40000
#define JJY_FREQ_WEST     60000
#define JJY_STR_EAST      "(E)"
#define JJY_STR_WEST      "(W)"
#define JJY_TYPE_STR(type)  (!type ? JJY_STR_EAST : JJY_STR_WEST)
#define JJY_TYPE_FREQ(type) (!type ? JJY_FREQ_EAST : JJY_FREQ_WEST)

#define CONFIG_WAIT_TIME  5000    // msec
#define CONFIG_WAIT_TICK  100     // msec

#define PWM_BITS          7
#define PWM_DUTY_MAX      ((1 << PWM_BITS) - 1)
#define PWM_DUTY_ON       (PWM_DUTY_MAX / 2)
#define PWM_CH_A          LEDC_CHANNEL_0
#define PWM_CH_B          LEDC_CHANNEL_1
#define PWM_DEAD          0

#define JJY_T_BIT0        800
#define JJY_T_BIT1        500
#define JJY_T_PM          200

#define JJY_BIT_ZERO      0
#define JJY_BIT_PMn       -1
#define JJY_BIT_PM0       -2
#define JJY_BIT_OFF       -3

// Wi-Fi設定保存ファイル
const char* WiFi_settings = "/wifi_settings.txt";
const char* Potal_pass = "password";
const char* Version_str = "Version 2.00.0";

String WiFi_ssid = "my-ssid";
String WiFi_pass = "my-pass";
String WiFi_time = "";
String WiFi_ip   = "";
char timeNowStr[64] = "";

SSD1306Wire display(I2CADR, PIN_OLED_SDA, PIN_OLED_SCL);  

WiFiManager wm; // global wm instance
WiFiManagerParameter custom_field; // global param ( for non blocking w params )

bool JJY_type = false;
int JJY_freq = JJY_FREQ_EAST;
String JJY_str = JJY_STR_EAST;
int config_wait_remain = 0;

/**
   初期化
*/
void setup() {

  bool conn_flag = false;

  led_blink(PIN_LED_WIFI, LED_ON);
  led_blink(PIN_LED_WAVE, LED_ON);
  delay(500);

  Serial.begin(115200);
  Serial.println();
  Serial.println("Start JJY Sim / ESP32-C3");

  if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) {
    go_reboot("SPIFFS Mount Failed");
  } else {
    JJY_type = get_JJY_type();
    if ( JJY_type ) {
      JJY_freq = JJY_FREQ_WEST;
      JJY_str = JJY_STR_WEST;
    }
  }
  Serial.println("JJY_FREQ = " + String(JJY_freq) + JJY_str ); 

  disp_screen(0);

  pinMode(PIN_CONFIG, INPUT_PULLUP);  
  led_blink(PIN_LED_WIFI, LED_OFF);
  led_blink(PIN_LED_WAVE, LED_OFF);

  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP  

  led_blink(PIN_LED_WAVE, LED_ON);

  for(int i = CONFIG_WAIT_TIME/CONFIG_WAIT_TICK; i; i-- ) {
    config_wait_remain = i / (1000/CONFIG_WAIT_TICK) + 1;
    disp_screen(-1);

    delay(CONFIG_WAIT_TICK);
    if(digitalRead(PIN_CONFIG) != LOW) continue;

    disp_screen(-2);
    led_blink(PIN_LED_WAVE, LED_BLINK);
    Serial.println("Enter to CONFIG mode");

    config_mode_setup();

    if (!wm.startConfigPortal()) {
      go_reboot("failed to connect / timeout");
    }
    conn_flag = true;
    break;
  }

  disp_screen(1);
  led_blink(PIN_LED_WAVE, LED_OFF);

  WiFi_ssid = wm.getWiFiSSID(!conn_flag);
  WiFi_pass = wm.getWiFiPass(!conn_flag);
  Serial.println(WiFi_ssid);
  Serial.println(WiFi_pass);
  if( !conn_flag ) {
    Serial.println("Connecting....");
    led_blink(PIN_LED_WIFI, LED_BLINK);

    if(!wm.autoConnect()) {
      go_reboot("Failed to connect");
    } 
  }

  if( WiFi.status() == WL_CONNECTED ){

    conn_flag = true;
    Serial.println("connected...");

//  WiFi_ssid = WiFi.SSID();
//  WiFi_pass = WiFi.psk();
    Serial.print  ("IP Address:");
    Serial.println(WiFi.localIP());
  
    led_blink(PIN_LED_WIFI, LED_ON);
//  digitalWrite(5,0);

    disp_screen(3);  

  }else
  { 
    go_reboot("Connect timeout.");
  }

  configTime( JST, 0, "ntp.nict.jp", "ntp.jst.mfeed.ad.jp");

//ledcAttach(PWM_PIN, JJY_freq, PWM_BITS);
  pwm_setup();
}

/*
 * 
 */
void loop() {
  get_ntp_time();
}

void go_reboot(String text)
{
  led_blink(PIN_LED_WIFI, LED_ON);
  led_blink(PIN_LED_WAVE, LED_ON);
  Serial.println(text);
  Serial.println("reboot!!");
  disp_screen(2);

  led_blink(PIN_LED_WIFI, LED_OFF);
  led_blink(PIN_LED_WAVE, LED_OFF);

  //reset and try again, or maybe put it to deep sleep
  ESP.restart();
  delay(5000);
}

void led_blink(uint8_t pin, uint8_t mode)
{
  static bool attached[16] = { 0 };

  if( mode == LED_BLINK ) {
    if( !attached[pin] ) ledcAttach(pin, 4, 14);
    attached[pin] = true;
    ledcWrite(pin, 8192);
  } else {
    if( attached[pin] ) ledcDetach(pin);
    attached[pin] = false;
    pinMode(pin, OUTPUT);  
    digitalWrite(pin, mode == LED_OFF);
  }
}

//
void disp_screen( int mode )
{
  if(mode == 0) {
    pinMode(PIN_OLED_RES, OUTPUT); 
    digitalWrite(PIN_OLED_RES, 0);
    delay(10);
    digitalWrite(PIN_OLED_RES, 1);
    delay(10);
  
    // Initialising the UI will init the display too.
    display.init();
    display.flipScreenVertically();
  }

  display.clear();
  display.setFont(ArialMT_Plain_10);
  String title = "ESP32 JJY Simulator  " + JJY_str;
  display.drawString( 0,  0, title );

  if( mode == -1 ){
    display.drawString( 0, 15, Version_str );    
    display.drawString( 0, 25, "Waiting for CONFIG" );
    display.drawString( 0, 35, String(config_wait_remain) + " sec left" );
  }
  if( mode == -2 ){  
    display.drawString( 0, 15, "Enter to CONFIG mode" );
  }

  if( mode > 0 ) {
    display.drawString( 0, 15, "SSID : " + WiFi_ssid );
  }
  if( mode == 1 ) {
    display.drawString( 0, 25, "Connecting..." );
  } else
  if( mode == 2 ) {
      display.drawString( 0, 25, "Connect timeout");
      display.drawString( 0, 35, "Reboot now!!" );
  } else
  if( mode > 2 ) {
      display.drawString( 0, 25, "Connect : " + WiFi.localIP().toString());
  }
  if( mode == 5 ){
    display.drawString( 0, 35, "Waiting for just min" );
  } else
  if( mode > 5 ) {
    display.drawString( 0, 35, "Outputting the wave!!" );
  }
  if( mode == 4 ) {
    display.drawString( 0, 35, "Waiting to get time");
  } else
  if( mode > 4 ) {
    display.setFont(ArialMT_Plain_16);
    display.drawString( 0, 45, timeNowStr);
  }
  display.display();
}

struct tm *get_time_now()
{
  static time_t time_last = 0;

  time_t now = time(NULL);
  if( time_last == now )  return NULL;
  time_last = now;

  struct tm *tm = localtime( &time_last );
  if( tm == NULL ) return NULL;

  if( tm->tm_year < 100 )
  {
      timeNowStr[0] = '\0';
      return tm;
  }
  tm->tm_year -= 100;
  sprintf(timeNowStr, "%02d/%02d/%02d,%02d:%02d:%02d",
    tm->tm_year, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);

  return tm;
}

/*
 * 
 */
 void get_ntp_time()
{
  static int radio_output_stat = 0;
  static int last_min = 99;

  struct tm *tm;
  tm = get_time_now();
  if ( tm == NULL ) return; 

  if ( timeNowStr[0] == '\0' ) {
      Serial.printf(".");
      disp_screen(4);
      return;
  }

  disp_screen(5 + radio_output_stat);

  if( tm->tm_sec != 0 ) return;
  if( tm->tm_min == last_min ) return;
  last_min = tm->tm_min;

  Serial.println();
  Serial.printf("JST=%s > ", timeNowStr);

  radio_output_stat = 1;

  jjy_output( tm );

  led_blink(PIN_LED_WAVE, LED_ON);
}


void pwm_setup()
{
  pinMode(PIN_PWM_A, OUTPUT);                 
  ledcAttachChannel(PIN_PWM_A, JJY_freq, PWM_BITS, PWM_CH_A);

  pinMode(PIN_PWM_B, OUTPUT);                 
  ledcAttachChannel(PIN_PWM_B, JJY_freq, PWM_BITS, PWM_CH_B);

  pwm_stop();
}

// PWM出力
void pwm_on()
{
  ledc_set_duty_with_hpoint(LEDC_LOW_SPEED_MODE, PWM_CH_A, PWM_DUTY_ON-PWM_DEAD, PWM_DEAD); 
  ledc_set_duty_with_hpoint(LEDC_LOW_SPEED_MODE, PWM_CH_B, PWM_DUTY_ON-PWM_DEAD, PWM_DUTY_ON+PWM_DEAD); 

  ledc_update_duty( LEDC_LOW_SPEED_MODE, PWM_CH_A );
  ledc_update_duty( LEDC_LOW_SPEED_MODE, PWM_CH_B );
}

// 出力をブレーキするとき（Hブリッジブレーキ）
void pwm_off()
{
  ledcWriteChannel(PWM_CH_A, PWM_DUTY_MAX);
  ledcWriteChannel(PWM_CH_B, PWM_DUTY_MAX);
}

// 出力を止るとき（Hブリッジ休止）
void pwm_stop()
{
  ledcWriteChannel(PWM_CH_A, 0);
  ledcWriteChannel(PWM_CH_B, 0);
}

/*
 * 
 */
void jjy_put_bit( int flag )
{
  int t_on;
  char ch;

  if( flag == JJY_BIT_OFF ){
    t_on = 0;
    ch = '.';
  }else
  if( flag == JJY_BIT_PMn ){
    t_on = JJY_T_PM;
    ch = '-';
  }else
  if( flag == JJY_BIT_PM0 ){
    t_on = JJY_T_PM;
    ch = '-';
  }else
  if( flag == JJY_BIT_ZERO )
  {
    t_on = JJY_T_BIT0;
    ch = '0';
  }else
  {
    t_on = JJY_T_BIT1;
    ch = '1';
  }
  Serial.write( ch );

  if( t_on ) {
    pwm_on();
  }
  digitalWrite(BUILTIN_LED, LED_ON);
  delay( 3 );   
  digitalWrite(BUILTIN_LED, LED_OFF);
  if( t_on ) {
    delay( t_on - 3 );
    pwm_off();
  }

  if( flag == JJY_BIT_PM0 ) {
    return;
  }
  while ( get_time_now() == NULL ) delay(1);
  disp_screen(6);
}

/*
 * 
 */
int get_int_to_bcd( int n )
{
  int res = n % 10;
  res += ((n/ 10)%10)<<4;
  res += ((n/100)%10)<<8;
  return res;
}

/*
 * 
 */
int get_even_parity( int n )
{
  n ^= n >> 8;
  n ^= n >> 4;
  n ^= n >> 2;
  n ^= n >> 1;
  return n & 1;
}

/*
 * 
 */
void jjy_output( struct tm *tm )
{
const int totalDaysOfMonth[] = {0,31,59,90,120,151,181,212,243,273,304,334 };
  int totalDays = totalDaysOfMonth[tm->tm_mon];
  totalDays += tm->tm_mday;
  if(((tm->tm_year & 0x03)==0) && (tm->tm_mon > 2)) totalDays++;

  int mm = get_int_to_bcd( tm->tm_min );
  int hh = get_int_to_bcd( tm->tm_hour );
  int dd = get_int_to_bcd( totalDays );
  int yy = get_int_to_bcd( tm->tm_year % 100 );
  int pa1= get_even_parity( hh );
  int pa2= get_even_parity( mm );
  int ww = tm->tm_wday;

  jjy_put_bit( JJY_BIT_PMn );   // :00 M
  jjy_put_bit( mm & 0x40 );     // :01
  jjy_put_bit( mm & 0x20 );     // :02
  jjy_put_bit( mm & 0x10 );     // :03
  jjy_put_bit( 0 );             // :04
  jjy_put_bit( mm & 0x08 );     // :05
  jjy_put_bit( mm & 0x04 );     // :06
  jjy_put_bit( mm & 0x02 );     // :07
  jjy_put_bit( mm & 0x01 );     // :08
  jjy_put_bit( JJY_BIT_PMn );   // :09 P1
  jjy_put_bit( 0 );             // :10
  jjy_put_bit( 0 );             // :11
  jjy_put_bit( hh & 0x20 );     // :12
  jjy_put_bit( hh & 0x10 );     // :13
  jjy_put_bit( 0 );             // :14
  jjy_put_bit( hh & 0x08 );     // :15
  jjy_put_bit( hh & 0x04 );     // :16
  jjy_put_bit( hh & 0x02 );     // :17
  jjy_put_bit( hh & 0x01 );     // :18
  jjy_put_bit( JJY_BIT_PMn );   // :19 P2
  jjy_put_bit( 0 );             // :20
  jjy_put_bit( 0 );             // :21
  jjy_put_bit( dd & 0x200 );    // :22
  jjy_put_bit( dd & 0x100 );    // :23
  jjy_put_bit( 0 );             // :24
  jjy_put_bit( dd & 0x80 );     // :25
  jjy_put_bit( dd & 0x40 );     // :26
  jjy_put_bit( dd & 0x20 );     // :27
  jjy_put_bit( dd & 0x10 );     // :28
  jjy_put_bit( JJY_BIT_PMn );   // :29 P3
  jjy_put_bit( dd & 0x08 );     // :30
  jjy_put_bit( dd & 0x04 );     // :31
  jjy_put_bit( dd & 0x02 );     // :32
  jjy_put_bit( dd & 0x01 );     // :33
  jjy_put_bit( 0 );             // :34
  jjy_put_bit( 0 );             // :35
  jjy_put_bit( pa1 );           // :36 PA1
  jjy_put_bit( pa2 );           // :37 PA2
  jjy_put_bit( 0 );             // :38 SU1
  jjy_put_bit( JJY_BIT_PMn );   // :39 P4
  
  if (tm->tm_min == 15 || tm->tm_min == 45) {
    jjy_put_bit( JJY_BIT_OFF );   // :40  
    jjy_put_bit( JJY_BIT_OFF );   // :41  
    jjy_put_bit( JJY_BIT_OFF );   // :42  
    jjy_put_bit( JJY_BIT_OFF );   // :43  
    jjy_put_bit( JJY_BIT_OFF );   // :44  
    jjy_put_bit( JJY_BIT_OFF );   // :45  
    jjy_put_bit( JJY_BIT_OFF );   // :46  
    jjy_put_bit( JJY_BIT_OFF );   // :47  
    jjy_put_bit( JJY_BIT_OFF );   // :48  
    ww = 0;
 }
 else {
    jjy_put_bit( 0 );             // :40 SU2
    jjy_put_bit( yy & 0x80 );     // :41
    jjy_put_bit( yy & 0x40 );     // :42
    jjy_put_bit( yy & 0x20 );     // :43
    jjy_put_bit( yy & 0x10 );     // :44
    jjy_put_bit( yy & 0x08 );     // :45
    jjy_put_bit( yy & 0x04 );     // :46
    jjy_put_bit( yy & 0x02 );     // :47
    jjy_put_bit( yy & 0x01 );     // :48
  }
  jjy_put_bit( JJY_BIT_PMn );   // :49 P5
  jjy_put_bit( ww & 0x04 );     // :50
  jjy_put_bit( ww & 0x02 );     // :51
  jjy_put_bit( ww & 0x01 );     // :52
  jjy_put_bit( 0 );             // :53 LS1
  jjy_put_bit( 0 );             // :54 LS2
  jjy_put_bit( 0 );             // :55
  jjy_put_bit( 0 );             // :56
  jjy_put_bit( 0 );             // :57
  jjy_put_bit( 0 );             // :58
  jjy_put_bit( JJY_BIT_PM0 );   // :59 P0
}

uint8_t get_JJY_type()
{
  uint8_t type = 0;
  File fp = SPIFFS.open(FILE_NAME, FILE_READ);
  if( !fp || fp.isDirectory() ) {
    Serial.println("- failed to open file for reading");
    return type;
  }
  if ( fp.available() ) {
    type = (fp.readString()).toInt();
  }
  fp.close();
  return type;
}

void put_JJY_type(String type)
{
  File fp = SPIFFS.open(FILE_NAME, FILE_WRITE);
  if( !fp || fp.isDirectory() ) {
    Serial.println("- failed to open file for writting");
    return;
  }
  fp.print( type );
  fp.close();
}

void config_mode_setup()
{
  static String custom_radio_str = "<br/><label for='radiofreq'>Radio Freq</label><br><input type='radio' name='radiofreq' value='0' ";
  if (!JJY_type)  custom_radio_str += "checked";
  custom_radio_str += "> 40kHz (East)<br><input type='radio' name='radiofreq' value='1' ";
  if (JJY_type)   custom_radio_str += "checked";
  custom_radio_str += "> 60kHz (West)";
  
  new (&custom_field) WiFiManagerParameter(custom_radio_str.c_str()); // custom html input
  wm.addParameter(&custom_field);
  wm.setSaveParamsCallback(saveParamCallback);
  std::vector<const char *> menu = {"wifi","info","param","sep","restart","exit"};
  wm.setMenu(menu);
  // set dark theme
  wm.setClass("invert");

  wm.setConfigPortalTimeout(120);
}

String getParam(String name)
{
  //read parameter from server, for customhmtl input
  String value;
  if(wm.server->hasArg(name)) {
    value = wm.server->arg(name);
  }
  return value;
}

void saveParamCallback(){
  Serial.println("[CALLBACK] saveParamCallback fired");
  String para = getParam("radiofreq");
  Serial.println("PARAM radiofreq = " + para);
  put_JJY_type(para);
}