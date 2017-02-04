/*
 * Sonoff and ElectroDragon by Theo Arends
 *
 * ====================================================
 * Prerequisites:
 *   - Change libraries/PubSubClient/src/PubSubClient.h
 *       #define MQTT_MAX_PACKET_SIZE 400
 *
 *   - Select IDE Tools - Flash size: "1M (64K SPIFFS)"
 * ====================================================
*/

#define VERSION                0x03020200   // 3.2.2

#define SONOFF                 1            // Sonoff, Sonoff RF, Sonoff SV, Sonoff Dual, Sonoff TH, S20 Smart Socket, 4 Channel
#define SONOFF_POW             9            // Sonoff Pow
#define SONOFF_2               10           // Sonoff Touch, Sonoff 4CH
#define MOTOR_CAC              11           // iTead Motor Clockwise/Anticlockwise
#define ELECTRO_DRAGON         12           // Electro Dragon Wifi IoT Relay Board Based on ESP8266

#define DHT11                  11
#define DHT21                  21
#define DHT22                  22
#define AM2301                 21
#define AM2302                 22
#define AM2321                 22

enum log_t   {LOG_LEVEL_NONE, LOG_LEVEL_ERROR, LOG_LEVEL_INFO, LOG_LEVEL_DEBUG, LOG_LEVEL_DEBUG_MORE, LOG_LEVEL_ALL};
enum week_t  {Last, First, Second, Third, Fourth};
enum dow_t   {Sun=1, Mon, Tue, Wed, Thu, Fri, Sat};
enum month_t {Jan=1, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec};
enum wifi_t  {WIFI_RESTART, WIFI_SMARTCONFIG, WIFI_MANAGER, WIFI_WPSCONFIG, WIFI_RETRY, MAX_WIFI_OPTION};
enum msgf_t  {LEGACY, JSON, MAX_FORMAT};
enum swtch_t {TOGGLE, FOLLOW, FOLLOW_INV, PUSHBUTTON, PUSHBUTTON_INV, MAX_SWITCH_OPTION};
enum led_t   {LED_OFF, LED_POWER, LED_MQTTSUB, LED_POWER_MQTTSUB, LED_MQTTPUB, LED_POWER_MQTTPUB, LED_MQTT, LED_POWER_MQTT, MAX_LED_OPTION};

#include "user_config.h"
#include "user_config_override.h"

/*********************************************************************************************\
 * Enable feature by removing leading // or disable feature by adding leading //
\*********************************************************************************************/

//#define USE_SPIFFS                          // Switch persistent configuration from flash to spiffs (+24k code, +0.6k mem)

/*********************************************************************************************\
 * Not released yet
\*********************************************************************************************/

#if MODULE == SONOFF_POW
//  #define FEATURE_POWER_LIMIT
#endif
#define MAX_POWER_HOLD         10           // Time in SECONDS to allow max agreed power
#define MAX_POWER_WINDOW       30           // Time in SECONDS to disable allow max agreed power
#define SAFE_POWER_HOLD        10           // Time in SECONDS to allow max unit safe power
#define SAFE_POWER_WINDOW      30           // Time in MINUTES to disable allow max unit safe power
#define MAX_POWER_RETRY        5            // Retry count allowing agreed power limit overflow

/*********************************************************************************************\
 * No user configurable items below
\*********************************************************************************************/

#define SONOFF_DUAL            2            // (iTEAD PSB)
#define CHANNEL_4              3            // iTEAD PSB (Stopped manufacturing)
#define SONOFF_4CH             4            // ESP8285 with four buttons/relays on GPIOs

#ifndef SWITCH_MODE
#define SWITCH_MODE            TOGGLE       // TOGGLE, FOLLOW or FOLLOW_INV (the wall switch state)
#endif

#ifndef MQTT_FINGERPRINT
#define MQTT_FINGERPRINT       "A5 02 FF 13 99 9F 8B 39 8E F1 83 4F 11 23 65 0B 32 36 FC 07"
#endif

#define DEF_WIFI_HOSTNAME      "%s-%04d"    // Expands to <MQTT_TOPIC>-<last 4 decimal chars of MAC address>

#define HLW_PREF_PULSE         12530        // was 4975us = 201Hz = 1000W
#define HLW_UREF_PULSE         1950         // was 1666us = 600Hz = 220V
#define HLW_IREF_PULSE         3500         // was 1666us = 600Hz = 4.545A

#define MQTT_UNITS             0            // Default do not show value units (Hr, Sec, V, A, W etc.)
#define MQTT_SUBTOPIC          "POWER"      // Default MQTT subtopic (POWER or LIGHT)
#define MQTT_RETRY_SECS        10           // Seconds to retry MQTT connection
#define APP_POWER              0            // Default saved power state Off
#define MAX_DEVICE             1            // Max number of devices

#define STATES                 10           // loops per second
#define SYSLOG_TIMER           600          // Seconds to restore syslog_level
#define OTA_ATTEMPTS           5            // Number of times to try fetching the new firmware

#define INPUT_BUFFER_SIZE      100          // Max number of characters in serial buffer
#define TOPSZ                  60           // Max number of characters in topic string
#define MESSZ                  240          // Max number of characters in JSON message string
#define LOGSZ                  128          // Max number of characters in log string
#ifdef USE_MQTT_TLS
  #define MAX_LOG_LINES        10           // Max number of lines in weblog
#else
  #define MAX_LOG_LINES        70           // Max number of lines in weblog
#endif

#define APP_BAUDRATE           115200       // Default serial baudrate

#ifdef USE_POWERMONITOR
  #define MAX_STATUS           9
#else
  #define MAX_STATUS           7
#endif

enum butt_t {PRESSED, NOT_PRESSED};

#include "support.h"                        // Global support
#include <Ticker.h>                         // RTC
#include <ESP8266WiFi.h>                    // MQTT, Ota, WifiManager
#include <ESP8266HTTPClient.h>              // MQTT, Ota
#include <ESP8266httpUpdate.h>              // Ota
#ifdef USE_MQTT
  #include <PubSubClient.h>                 // MQTT
#endif  // USE_MQTT
#ifdef USE_WEBSERVER
  #include <ESP8266WebServer.h>             // WifiManager, Webserver
  #include <DNSServer.h>                    // WifiManager
#endif  // USE_WEBSERVER
#ifdef USE_DISCOVERY
  #include <ESP8266mDNS.h>                  // MQTT, Webserver
#endif
#ifdef USE_SPIFFS
  #include <FS.h>                           // Config
#endif
#ifdef SEND_TELEMETRY_I2C
  #include <Wire.h>                         // I2C support library
#endif // SEND_TELEMETRY_I2C

typedef void (*rtcCallback)();

extern "C" uint32_t _SPIFFS_start;
extern "C" uint32_t _SPIFFS_end;

#define MAX_BUTTON_COMMANDS    5            // Max number of button commands supported

const char commands[MAX_BUTTON_COMMANDS][14] PROGMEM = {
  {"wificonfig 1"},   // Press button three times
  {"wificonfig 2"},   // Press button four times
  {"wificonfig 3"},   // Press button five times
  {"restart 1"},      // Press button six times
  {"upgrade 1"}};     // Press button seven times

const char wificfg[5][12] PROGMEM = { "Restart", "Smartconfig", "Wifimanager", "WPSconfig", "Retry" };

struct SYSCFG2 {      // Version 2.x (old)
  unsigned long cfg_holder;
  unsigned long saveFlag;
  unsigned long version;
  byte          seriallog_level;
  byte          syslog_level;
  char          syslog_host[32];
  char          sta_ssid1[32];
  char          sta_pwd1[64];
  char          otaUrl[80];
  char          mqtt_host[32];
  char          mqtt_grptopic[32];
  char          mqtt_topic[32];
  char          mqtt_topic2[32];
  char          mqtt_subtopic[32];
  int8_t        timezone;
  uint8_t       power;
  uint8_t       ledstate;
  uint16_t      mqtt_port;
  char          mqtt_client[33];
  char          mqtt_user[33];
  char          mqtt_pwd[33];
  uint8_t       webserver;
  unsigned long bootcount;
  char          hostname[33];
  uint16_t      syslog_port;
  byte          weblog_level;
  uint16_t      tele_period;
  uint8_t       sta_config;
  int16_t       savedata;
  byte          model;
  byte          mqtt_retain;
  byte          savestate;
  unsigned long hlw_pcal;
  unsigned long hlw_ucal;
  unsigned long hlw_ical;
  unsigned long hlw_kWhyesterday;
  byte          mqtt_units;
  uint16_t      hlw_pmin;
  uint16_t      hlw_pmax;
  uint16_t      hlw_umin;
  uint16_t      hlw_umax;
  uint16_t      hlw_imin;
  uint16_t      hlw_imax;
  uint16_t      hlw_mpl;    // MaxPowerLimit
  uint16_t      hlw_mplh;   // MaxPowerLimitHold
  uint16_t      hlw_mplw;   // MaxPowerLimitWindow
  uint16_t      hlw_mspl;   // MaxSafePowerLimit
  uint16_t      hlw_msplh;  // MaxSafePowerLimitHold
  uint16_t      hlw_msplw;  // MaxSafePowerLimitWindow
  uint16_t      hlw_mkwh;   // MaxEnergy
  uint16_t      hlw_mkwhs;  // MaxEnergyStart
  char          domoticz_in_topic[33];
  char          domoticz_out_topic[33];
  uint16_t      domoticz_update_timer;
  unsigned long domoticz_relay_idx[4];
  unsigned long domoticz_key_idx[4];
  byte          message_format;
  unsigned long hlw_kWhtoday;
  uint16_t      hlw_kWhdoy;
  uint8_t       switchmode;
  char          mqtt_fingerprint[60];
  byte          sta_active;
  char          sta_ssid2[33];
  char          sta_pwd2[65];

} sysCfg2;

struct SYSCFG {
  unsigned long cfg_holder;
  unsigned long saveFlag;
  unsigned long version;
  unsigned long bootcount;
  byte          migflag;
  int16_t       savedata;
  byte          savestate;
  byte          model;
  int8_t        timezone;
  char          otaUrl[101];
  char          friendlyname[33];

  byte          serial_enable;
  byte          seriallog_level;
  uint8_t       sta_config;
  byte          sta_active;
  char          sta_ssid[2][33];
  char          sta_pwd[2][65];
  char          hostname[33];
  char          syslog_host[33];
  uint16_t      syslog_port;
  byte          syslog_level;
  uint8_t       webserver;
  byte          weblog_level;

  char          mqtt_fingerprint[60];
  char          mqtt_host[33];
  uint16_t      mqtt_port;
  char          mqtt_client[33];
  char          mqtt_user[33];
  char          mqtt_pwd[33];
  char          mqtt_topic[33];
  char          mqtt_topic2[33];
  char          mqtt_grptopic[33];
  char          mqtt_subtopic[33];
  byte          mqtt_button_retain;
  byte          mqtt_power_retain;
  byte          mqtt_units;
  byte          message_format;
  uint16_t      tele_period;

  uint8_t       power;
  uint8_t       ledstate;
  uint8_t       switchmode;

  char          domoticz_in_topic[33];
  char          domoticz_out_topic[33];
  uint16_t      domoticz_update_timer;
  unsigned long domoticz_relay_idx[4];
  unsigned long domoticz_key_idx[4];

  unsigned long hlw_pcal;
  unsigned long hlw_ucal;
  unsigned long hlw_ical;
  unsigned long hlw_kWhtoday;
  unsigned long hlw_kWhyesterday;
  uint16_t      hlw_kWhdoy;
  uint16_t      hlw_pmin;
  uint16_t      hlw_pmax;
  uint16_t      hlw_umin;
  uint16_t      hlw_umax;
  uint16_t      hlw_imin;
  uint16_t      hlw_imax;
  uint16_t      hlw_mpl;    // MaxPowerLimit
  uint16_t      hlw_mplh;   // MaxPowerLimitHold
  uint16_t      hlw_mplw;   // MaxPowerLimitWindow
  uint16_t      hlw_mspl;   // MaxSafePowerLimit
  uint16_t      hlw_msplh;  // MaxSafePowerLimitHold
  uint16_t      hlw_msplw;  // MaxSafePowerLimitWindow
  uint16_t      hlw_mkwh;   // MaxEnergy
  uint16_t      hlw_mkwhs;  // MaxEnergyStart

  uint16_t      pulsetime;
  uint8_t       poweronstate;
  uint16_t      blinktime;
  uint16_t      blinkcount;

} sysCfg;

struct TIME_T {
  uint8_t       Second;
  uint8_t       Minute;
  uint8_t       Hour;
  uint8_t       Wday;      // day of week, sunday is day 1
  uint8_t       Day;
  uint8_t       Month;
  char          MonthName[4];
  uint16_t      DayOfYear;
  uint16_t      Year;
  unsigned long Valid;
} rtcTime;

struct TimeChangeRule
{
  uint8_t       week;      // 1=First, 2=Second, 3=Third, 4=Fourth, or 0=Last week of the month
  uint8_t       dow;       // day of week, 1=Sun, 2=Mon, ... 7=Sat
  uint8_t       month;     // 1=Jan, 2=Feb, ... 12=Dec
  uint8_t       hour;      // 0-23
  int           offset;    // offset from UTC in minutes
};

TimeChangeRule myDST = { TIME_DST };  // Daylight Saving Time
TimeChangeRule mySTD = { TIME_STD };  // Standard Time

int Baudrate = APP_BAUDRATE;          // Serial interface baud rate
byte SerialInByte;                    // Received byte
int SerialInByteCounter = 0;          // Index in receive buffer
char serialInBuf[INPUT_BUFFER_SIZE + 2];  // Receive buffer
byte Hexcode = 0;                     // Sonoff dual input flag
uint16_t ButtonCode = 0;              // Sonoff dual received code
int16_t savedatacounter;              // Counter and flag for config save to Flash or Spiffs
char Version[16];                     // Version string from VERSION define
char Hostname[33];                    // Composed Wifi hostname
char MQTTClient[33];                  // Composed MQTT Clientname
uint8_t mqttcounter = 0;              // MQTT connection retry counter
unsigned long timerxs = 0;            // State loop timer
int state = 0;                        // State per second flag
int mqttflag = 2;                     // MQTT connection messages flag
int otaflag = 0;                      // OTA state flag
int otaok = 0;                        // OTA result
int restartflag = 0;                  // Sonoff restart flag
int wificheckflag = WIFI_RESTART;     // Wifi state flag
int uptime = 0;                       // Current uptime in hours
int tele_period = 0;                  // Tele period timer
String Log[MAX_LOG_LINES];            // Web log buffer
byte logidx = 0;                      // Index in Web log buffer
byte Maxdevice = MAX_DEVICE;          // Max number of devices supported
int status_update_timer = 0;          // Refresh initial status
uint16_t pulse_timer = 0;             // Power off timer
uint16_t blink_timer = 0;             // Power cycle timer
uint16_t blink_counter = 0;           // Number of blink cycles
uint8_t blink_power;                  // Blink power state
uint8_t blink_mask = 0;               // Blink relay active mask
uint8_t blink_powersave;              // Blink start power save state
uint16_t mqtt_cmnd_publish = 0;       // ignore flag for publish command

#ifdef USE_MQTT_TLS
  WiFiClientSecure espClient;         // Wifi Secure Client
#else
  WiFiClient espClient;               // Wifi Client
#endif
#ifdef USE_MQTT
  PubSubClient mqttClient(espClient); // MQTT Client
#endif  // USE_MQTT
WiFiUDP portUDP;                      // UDP Syslog and Alexa

uint8_t power;                        // Current copy of sysCfg.power
byte syslog_level;                    // Current copy of sysCfg.syslog_level
uint16_t syslog_timer = 0;            // Timer to re-enable syslog_level

int blinks = 201;                     // Number of LED blinks
uint8_t blinkstate = 0;               // LED state

uint8_t lastbutton = NOT_PRESSED;     // Last button state
uint8_t holdcount = 0;                // Timer recording button hold
uint8_t multiwindow = 0;              // Max time between button presses to record press count
uint8_t multipress = 0;               // Number of button presses within multiwindow
uint8_t lastbutton2 = NOT_PRESSED;    // Last button 2 state
uint8_t lastbutton3 = NOT_PRESSED;    // Last button 3 state
uint8_t lastbutton4 = NOT_PRESSED;    // Last button 4 state

boolean mDNSbegun = false;
boolean udpConnected = false;
#ifdef USE_WEMO_EMULATION
  #define WEMO_BUFFER_SIZE 200        // Max UDP buffer size needed for M-SEARCH message

  char packetBuffer[WEMO_BUFFER_SIZE]; // buffer to hold incoming UDP packet
  IPAddress ipMulticast(239, 255, 255, 250); // Simple Service Discovery Protocol (SSDP)
  uint32_t portMulticast = 1900;      // Multicast address and port
#endif  // USE_WEMO_EMULATION

#ifdef USE_WALL_SWITCH
  uint8_t lastwallswitch;             // Last wall switch state
#endif  // USE_WALL_SWITCH

#ifdef USE_POWERMONITOR
  byte hlw_pminflg = 0;
  byte hlw_pmaxflg = 0;
  byte hlw_uminflg = 0;
  byte hlw_umaxflg = 0;
  byte hlw_iminflg = 0;
  byte hlw_imaxflg = 0;
  byte power_steady_cntr;
#ifdef FEATURE_POWER_LIMIT
  byte hlw_mkwh_state = 0;
  byte hlw_mplr_counter = 0;
  uint16_t hlw_mplh_counter = 0;
  uint16_t hlw_mplw_counter = 0;
#endif  // FEATURE_POWER_LIMIT
#endif  // USE_POWERMONITOR

#ifdef USE_MQTT
#ifdef USE_DOMOTICZ
  int domoticz_update_timer = 0;
  byte domoticz_update_flag = 1;
#endif  // USE_DOMOTICZ
#endif  // USE_MQTT

/********************************************************************************************/

void CFG_DefaultSet()
{
  memset(&sysCfg, 0x00, sizeof(SYSCFG));

  sysCfg.cfg_holder = CFG_HOLDER;
  sysCfg.saveFlag = 0;
  sysCfg.version = VERSION;
  sysCfg.bootcount = 0;
  sysCfg.migflag = 0;
  sysCfg.savedata = SAVE_DATA;
  sysCfg.savestate = SAVE_STATE;
  sysCfg.model = 0;
  sysCfg.timezone = APP_TIMEZONE;
  strlcpy(sysCfg.otaUrl, OTA_URL, sizeof(sysCfg.otaUrl));
  strlcpy(sysCfg.friendlyname, FRIENDLY_NAME, sizeof(sysCfg.friendlyname));

  sysCfg.seriallog_level = SERIAL_LOG_LEVEL;
  sysCfg.sta_active = 0;
  strlcpy(sysCfg.sta_ssid[0], STA_SSID1, sizeof(sysCfg.sta_ssid[0]));
  strlcpy(sysCfg.sta_pwd[0], STA_PASS1, sizeof(sysCfg.sta_pwd[0]));
  strlcpy(sysCfg.sta_ssid[1], STA_SSID2, sizeof(sysCfg.sta_ssid[1]));
  strlcpy(sysCfg.sta_pwd[1], STA_PASS2, sizeof(sysCfg.sta_pwd[1]));
  strlcpy(sysCfg.hostname, WIFI_HOSTNAME, sizeof(sysCfg.hostname));
  sysCfg.sta_config = WIFI_CONFIG_TOOL;
  strlcpy(sysCfg.syslog_host, SYS_LOG_HOST, sizeof(sysCfg.syslog_host));
  sysCfg.syslog_port = SYS_LOG_PORT;
  sysCfg.syslog_level = SYS_LOG_LEVEL;
  sysCfg.webserver = WEB_SERVER;
  sysCfg.weblog_level = WEB_LOG_LEVEL;

  strlcpy(sysCfg.mqtt_fingerprint, MQTT_FINGERPRINT, sizeof(sysCfg.mqtt_fingerprint));
  strlcpy(sysCfg.mqtt_host, MQTT_HOST, sizeof(sysCfg.mqtt_host));
  sysCfg.mqtt_port = MQTT_PORT;
  strlcpy(sysCfg.mqtt_client, MQTT_CLIENT_ID, sizeof(sysCfg.mqtt_client));
  strlcpy(sysCfg.mqtt_user, MQTT_USER, sizeof(sysCfg.mqtt_user));
  strlcpy(sysCfg.mqtt_pwd, MQTT_PASS, sizeof(sysCfg.mqtt_pwd));
  strlcpy(sysCfg.mqtt_topic, MQTT_TOPIC, sizeof(sysCfg.mqtt_topic));
  strlcpy(sysCfg.mqtt_topic2, "0", sizeof(sysCfg.mqtt_topic2));
  strlcpy(sysCfg.mqtt_grptopic, MQTT_GRPTOPIC, sizeof(sysCfg.mqtt_grptopic));
  strlcpy(sysCfg.mqtt_subtopic, MQTT_SUBTOPIC, sizeof(sysCfg.mqtt_subtopic));
  sysCfg.mqtt_button_retain = MQTT_BUTTON_RETAIN;
  sysCfg.mqtt_power_retain = MQTT_POWER_RETAIN;
  sysCfg.mqtt_units = MQTT_UNITS;
  sysCfg.message_format = MESSAGE_FORMAT;
  sysCfg.tele_period = TELE_PERIOD;

  sysCfg.power = APP_POWER;
  sysCfg.poweronstate = APP_POWERON_STATE;
  sysCfg.pulsetime = APP_PULSETIME;
  sysCfg.ledstate = APP_LEDSTATE;
  sysCfg.switchmode = SWITCH_MODE;
  sysCfg.blinktime = APP_BLINKTIME;
  sysCfg.blinkcount = APP_BLINKCOUNT;

  strlcpy(sysCfg.domoticz_in_topic, DOMOTICZ_IN_TOPIC, sizeof(sysCfg.domoticz_in_topic));
  strlcpy(sysCfg.domoticz_out_topic, DOMOTICZ_OUT_TOPIC, sizeof(sysCfg.domoticz_out_topic));
  sysCfg.domoticz_update_timer = DOMOTICZ_UPDATE_TIMER;
  sysCfg.domoticz_relay_idx[0] = DOMOTICZ_RELAY_IDX1;
  sysCfg.domoticz_relay_idx[1] = DOMOTICZ_RELAY_IDX2;
  sysCfg.domoticz_relay_idx[2] = DOMOTICZ_RELAY_IDX3;
  sysCfg.domoticz_relay_idx[3] = DOMOTICZ_RELAY_IDX4;
  sysCfg.domoticz_key_idx[0] = DOMOTICZ_KEY_IDX1;
  sysCfg.domoticz_key_idx[1] = DOMOTICZ_KEY_IDX2;
  sysCfg.domoticz_key_idx[2] = DOMOTICZ_KEY_IDX3;
  sysCfg.domoticz_key_idx[3] = DOMOTICZ_KEY_IDX4;

  sysCfg.hlw_pcal = HLW_PREF_PULSE;
  sysCfg.hlw_ucal = HLW_UREF_PULSE;
  sysCfg.hlw_ical = HLW_IREF_PULSE;
  sysCfg.hlw_kWhtoday = 0;
  sysCfg.hlw_kWhyesterday = 0;
  sysCfg.hlw_kWhdoy = 0;
  sysCfg.hlw_pmin = 0;
  sysCfg.hlw_pmax = 0;
  sysCfg.hlw_umin = 0;
  sysCfg.hlw_umax = 0;
  sysCfg.hlw_imin = 0;
  sysCfg.hlw_imax = 0;
  sysCfg.hlw_mpl = 0;                              // MaxPowerLimit
  sysCfg.hlw_mplh = MAX_POWER_HOLD;
  sysCfg.hlw_mplw = MAX_POWER_WINDOW;
  sysCfg.hlw_mspl = 0;                             // MaxSafePowerLimit
  sysCfg.hlw_msplh = SAFE_POWER_HOLD;
  sysCfg.hlw_msplw = SAFE_POWER_WINDOW;
  sysCfg.hlw_mkwh = 0;                             // MaxEnergy
  sysCfg.hlw_mkwhs = 0;                            // MaxEnergyStart
}

void CFG_Default()
{
  addLog_P(LOG_LEVEL_NONE, PSTR("Config: Use default configuration"));
  CFG_DefaultSet();
  CFG_Save();
}

void CFG_Migrate_Part2()
{
  addLog_P(LOG_LEVEL_NONE, PSTR("Config: Migrating configuration"));
  CFG_DefaultSet();

  sysCfg.seriallog_level = sysCfg2.seriallog_level;
  sysCfg.syslog_level = sysCfg2.syslog_level;
  strlcpy(sysCfg.syslog_host, sysCfg2.syslog_host, sizeof(sysCfg.syslog_host));
  strlcpy(sysCfg.sta_ssid[0], sysCfg2.sta_ssid1, sizeof(sysCfg.sta_ssid[0]));
  strlcpy(sysCfg.sta_pwd[0], sysCfg2.sta_pwd1, sizeof(sysCfg.sta_pwd[0]));
  strlcpy(sysCfg.otaUrl, sysCfg2.otaUrl, sizeof(sysCfg.otaUrl));
  strlcpy(sysCfg.mqtt_host, sysCfg2.mqtt_host, sizeof(sysCfg.mqtt_host));
  strlcpy(sysCfg.mqtt_grptopic, sysCfg2.mqtt_grptopic, sizeof(sysCfg.mqtt_grptopic));
  strlcpy(sysCfg.mqtt_topic, sysCfg2.mqtt_topic, sizeof(sysCfg.mqtt_topic));
  strlcpy(sysCfg.mqtt_topic2, sysCfg2.mqtt_topic2, sizeof(sysCfg.mqtt_topic2));
  strlcpy(sysCfg.mqtt_subtopic, sysCfg2.mqtt_subtopic, sizeof(sysCfg.mqtt_subtopic));
  sysCfg.timezone = sysCfg2.timezone;
  sysCfg.power = sysCfg2.power;
  if (sysCfg2.version >= 0x01000D00) {  // 1.0.13
    sysCfg.ledstate = sysCfg2.ledstate;
  }
  if (sysCfg2.version >= 0x01001600) {  // 1.0.22
    sysCfg.mqtt_port = sysCfg2.mqtt_port;
    strlcpy(sysCfg.mqtt_client, sysCfg2.mqtt_client, sizeof(sysCfg.mqtt_client));
    strlcpy(sysCfg.mqtt_user, sysCfg2.mqtt_user, sizeof(sysCfg.mqtt_user));
    strlcpy(sysCfg.mqtt_pwd, sysCfg2.mqtt_pwd, sizeof(sysCfg.mqtt_pwd));
    strlcpy(sysCfg.friendlyname, sysCfg2.mqtt_client, sizeof(sysCfg.friendlyname));
  }
  if (sysCfg2.version >= 0x01001700) {  // 1.0.23
    sysCfg.webserver = sysCfg2.webserver;
  }
  if (sysCfg2.version >= 0x01001A00) {  // 1.0.26
    sysCfg.bootcount = sysCfg2.bootcount;
    strlcpy(sysCfg.hostname, sysCfg2.hostname, sizeof(sysCfg.hostname));
    sysCfg.syslog_port = sysCfg2.syslog_port;
  }
  if (sysCfg2.version >= 0x01001B00) {  // 1.0.27
    sysCfg.weblog_level = sysCfg2.weblog_level;
  }
  if (sysCfg2.version >= 0x01001C00) {  // 1.0.28
    sysCfg.tele_period = sysCfg2.tele_period;
    if ((sysCfg.tele_period > 0) && (sysCfg.tele_period < 10)) sysCfg.tele_period = 10;   // Do not allow periods < 10 seconds
  }
  if (sysCfg2.version >= 0x01002000) {  // 1.0.32
    sysCfg.sta_config = sysCfg2.sta_config;
  }
  if (sysCfg2.version >= 0x01002300) {  // 1.0.35
    sysCfg.savedata = sysCfg2.savedata;
  }
  if (sysCfg2.version >= 0x02000000) {  // 2.0.0
    sysCfg.model = sysCfg2.model;
  }
  if (sysCfg2.version >= 0x02000300) {  // 2.0.3
    sysCfg.mqtt_button_retain = sysCfg2.mqtt_retain;
    sysCfg.savestate = sysCfg2.savestate;
  }
  if (sysCfg2.version >= 0x02000500) {  // 2.0.5
    sysCfg.hlw_pcal = sysCfg2.hlw_pcal;
    sysCfg.hlw_ucal = sysCfg2.hlw_ucal;
    sysCfg.hlw_ical = sysCfg2.hlw_ical;
    sysCfg.hlw_kWhyesterday = sysCfg2.hlw_kWhyesterday;
    sysCfg.mqtt_units = sysCfg2.mqtt_units;
  }
  if (sysCfg2.version >= 0x02000600) {  // 2.0.6
    sysCfg.hlw_pmin = sysCfg2.hlw_pmin;
    sysCfg.hlw_pmax = sysCfg2.hlw_pmax;
    sysCfg.hlw_umin = sysCfg2.hlw_umin;
    sysCfg.hlw_umax = sysCfg2.hlw_umax;
    sysCfg.hlw_imin = sysCfg2.hlw_imin;
    sysCfg.hlw_imax = sysCfg2.hlw_imax;
  }
  if (sysCfg2.version >= 0x02000700) {  // 2.0.7
    sysCfg.message_format = sysCfg2.message_format;
    strlcpy(sysCfg.domoticz_in_topic, sysCfg2.domoticz_in_topic, sizeof(sysCfg.domoticz_in_topic));
    strlcpy(sysCfg.domoticz_out_topic, sysCfg2.domoticz_out_topic, sizeof(sysCfg.domoticz_out_topic));
    sysCfg.domoticz_update_timer = sysCfg2.domoticz_update_timer;
    sysCfg.domoticz_relay_idx[0] = sysCfg2.domoticz_relay_idx[0];
    sysCfg.domoticz_relay_idx[1] = sysCfg2.domoticz_relay_idx[1];
    sysCfg.domoticz_relay_idx[2] = sysCfg2.domoticz_relay_idx[2];
    sysCfg.domoticz_relay_idx[3] = sysCfg2.domoticz_relay_idx[3];
    sysCfg.domoticz_key_idx[0] = sysCfg2.domoticz_key_idx[0];
    sysCfg.domoticz_key_idx[1] = sysCfg2.domoticz_key_idx[1];
    sysCfg.domoticz_key_idx[2] = sysCfg2.domoticz_key_idx[2];
    sysCfg.domoticz_key_idx[3] = sysCfg2.domoticz_key_idx[3];
    sysCfg.hlw_mpl = sysCfg2.hlw_mpl;              // MaxPowerLimit
    sysCfg.hlw_mplh = sysCfg2.hlw_mplh;
    sysCfg.hlw_mplw = sysCfg2.hlw_mplw;
    sysCfg.hlw_mspl = sysCfg2.hlw_mspl;            // MaxSafePowerLimit
    sysCfg.hlw_msplh = sysCfg2.hlw_msplh;
    sysCfg.hlw_msplw = sysCfg2.hlw_msplw;
    sysCfg.hlw_mkwh = sysCfg2.hlw_mkwh;            // MaxEnergy
    sysCfg.hlw_mkwhs = sysCfg2.hlw_mkwhs;          // MaxEnergyStart
  }
  if (sysCfg2.version >= 0x02001000) {  // 2.0.16
    sysCfg.hlw_kWhtoday = sysCfg2.hlw_kWhtoday;
    sysCfg.hlw_kWhdoy = sysCfg2.hlw_kWhdoy;
  }
  if (sysCfg2.version >= 0x02001200) {  // 2.0.18
    sysCfg.switchmode = sysCfg2.switchmode;
  }
  if (sysCfg2.version >= 0x02010000) {  // 2.1.0
    strlcpy(sysCfg.mqtt_fingerprint, sysCfg2.mqtt_fingerprint, sizeof(sysCfg.mqtt_fingerprint));
  }
  if (sysCfg2.version >= 0x02010200) {  // 2.1.2
    sysCfg.sta_active = sysCfg2.sta_active;
    strlcpy(sysCfg.sta_ssid[1], sysCfg2.sta_ssid2, sizeof(sysCfg.sta_ssid[1]));
    strlcpy(sysCfg.sta_pwd[1], sysCfg2.sta_pwd2, sizeof(sysCfg.sta_pwd[1]));
  }
  CFG_Save();
}

void CFG_Delta()
{
  if (sysCfg.version != VERSION) {      // Fix version dependent changes
    if (sysCfg.version < 0x03000600) {  // 3.0.6 - Add parameter
      sysCfg.pulsetime = APP_PULSETIME;
    }
    if (sysCfg.version < 0x03010100) {  // 3.1.1 - Add parameter
      sysCfg.poweronstate = APP_POWERON_STATE;
    }
    if (sysCfg.version < 0x03010200) {  // 3.1.2 - Add parameter
      if (sysCfg.poweronstate == 2) sysCfg.poweronstate = 3;
    }
    if (sysCfg.version < 0x03010600) {  // 3.1.6 - Add parameter
      sysCfg.blinktime = APP_BLINKTIME;
      sysCfg.blinkcount = APP_BLINKCOUNT;
    }
    if (sysCfg.version < 0x03011000) {  // 3.1.16 - Add parameter
      getClient(sysCfg.friendlyname, sysCfg.mqtt_client, sizeof(sysCfg.friendlyname));
    }

    sysCfg.version = VERSION;
  }
}

/********************************************************************************************/

void getClient(char* output, const char* input, byte size)
{
  char *token;
  uint8_t digits = 0;

  if (strstr(input, "%")) {
    strlcpy(output, input, size);
    token = strtok(output, "%");
    if (strstr(input, "%") == input) {
      output[0] = '\0';
    } else {
      token = strtok(NULL, "");
    }
    if (token != NULL) {
      digits = atoi(token);
      if (digits) {
        snprintf_P(output, size, PSTR("%s%c0%dX"), output, '%', digits);
        snprintf_P(output, size, output, ESP.getChipId());
      }
    }
  }
  if (!digits) strlcpy(output, input, size);
}

void setRelay(uint8_t power)
{
  if ((sysCfg.model >= SONOFF_DUAL) && (sysCfg.model <= CHANNEL_4)) {
    Serial.write(0xA0);
    Serial.write(0x04);
    Serial.write(power);
    Serial.write(0xA1);
    Serial.write('\n');
    Serial.flush();
  } else {
    digitalWrite(REL_PIN, power & 0x1);
#ifdef REL2_PIN
    if (Maxdevice > 1) digitalWrite(REL2_PIN, (power & 0x2));
#endif
#ifdef REL3_PIN
    if (Maxdevice > 2) digitalWrite(REL3_PIN, (power & 0x4));
#endif
#ifdef REL4_PIN
    if (Maxdevice > 3) digitalWrite(REL4_PIN, (power & 0x8));
#endif
  }
#ifdef USE_POWERMONITOR
  power_steady_cntr = 2;
#endif
}

void json2legacy(char* stopic, char* svalue)
{
  char *p, *token;
  uint16_t i, j;

  if (!strstr(svalue, "{\"")) return;  // No JSON

// stopic = stat/sonoff/RESULT
// svalue = {"POWER2":"ON"}
// --> stopic = "stat/sonoff/POWER2", svalue = "ON"
// svalue = {"Upgrade":{"Version":"2.1.2", "OtaUrl":"%s"}}
// --> stopic = "stat/sonoff/UPGRADE", svalue = "2.1.2"
// svalue = {"SerialLog":2}
// --> stopic = "stat/sonoff/SERIALLOG", svalue = "2"
// svalue = {"POWER":""}
// --> stopic = "stat/sonoff/POWER", svalue = ""

  token = strtok(svalue, "{\"");      // Topic
  p = strrchr(stopic, '/') +1;
  i = p - stopic;
  for (j = 0; j < strlen(token)+1; j++) stopic[i+j] = toupper(token[j]);
  token = strtok(NULL, "\"");         // : or :3} or :3, or :{
  if (strstr(token, ":{")) {
    token = strtok(NULL, "\"");       // Subtopic
    token = strtok(NULL, "\"");       // : or :3} or :3,
  }
  if (strlen(token) > 1) {
    token++;
    p = strchr(token, ',');
    if (!p) p = strchr(token, '}');
    i = p - token;
    token[i] = '\0';                  // Value
  } else {
    token = strtok(NULL, "\"");       // Value or , or }
    if ((token[0] == ',') || (token[0] == '}')) {  // Empty parameter
      token = NULL;
    }
  }
  if (token == NULL) {
    svalue[0] = '\0';
  } else {
    memcpy(svalue, token, strlen(token)+1);
  }
}

#ifdef USE_MQTT
#ifdef USE_DOMOTICZ
unsigned long getKeyIntValue(const char *json, const char *key)
{
  char *p, *b;
  int i;

  // search key
  p = strstr(json, key);
  if (!p) return 0;
  // search following separator :
  b = strchr(p + strlen(key), ':');
  if (!b) return 0;
  // Only the following chars are allowed between key and separator :
  for(i = b - json + strlen(key); i < p-json; i++) {
    switch (json[i]) {
    case ' ':
    case '\n':
    case '\t':
    case '\r':
      continue;
    default:
      return 0;
    }
  }
  // Convert to integer
  return atoi(b +1);
}
#endif  // USE_DOMOTICZ
#endif  // USE_MQTT

/********************************************************************************************/

void mqtt_publish_sec(const char* topic, const char* data, boolean retained)
{
  char log[TOPSZ+MESSZ];

#ifdef USE_MQTT
  if (mqttClient.publish(topic, data, retained)) {
    snprintf_P(log, sizeof(log), PSTR("MQTT: %s = %s%s"), topic, data, (retained) ? " (retained)" : "");
//    mqttClient.loop();  // Do not use here! Will block previous publishes
  } else  {
    snprintf_P(log, sizeof(log), PSTR("RSLT: %s = %s"), topic, data);
  }
#else
  snprintf_P(log, sizeof(log), PSTR("RSLT: %s = %s"), strrchr(topic,'/')+1, data);
#endif  // USE_MQTT

  addLog(LOG_LEVEL_INFO, log);
  if (sysCfg.ledstate &0x04) blinks++;
}

void mqtt_publish(const char* topic, const char* data, boolean retained)
{
  char *me;

  if (!strcmp(SUB_PREFIX,PUB_PREFIX)) {
    me = strstr(topic,SUB_PREFIX);
    if (me == topic) mqtt_cmnd_publish += 8;
  }
  mqtt_publish_sec(topic, data, retained);
}

void mqtt_publish(const char* topic, const char* data)
{
  mqtt_publish(topic, data, false);
}

void mqtt_publishPowerState(byte device)
{
  char stopic[TOPSZ], svalue[MESSZ], sdevice[10];

  if ((device < 1) || (device > Maxdevice)) device = 1;
  snprintf_P(sdevice, sizeof(sdevice), PSTR("%d"), device);
  snprintf_P(stopic, sizeof(stopic), PSTR("%s/%s/RESULT"), PUB_PREFIX, sysCfg.mqtt_topic);
  snprintf_P(svalue, sizeof(svalue), PSTR("{\"%s%s\":\"%s\"}"),
    sysCfg.mqtt_subtopic, (Maxdevice > 1) ? sdevice : "", (power & (0x01 << (device -1))) ? MQTT_STATUS_ON : MQTT_STATUS_OFF);
  if (sysCfg.message_format == JSON) mqtt_publish(stopic, svalue);
  json2legacy(stopic, svalue);
  mqtt_publish(stopic, svalue, sysCfg.mqtt_power_retain);
}

void mqtt_publishPowerBlinkState(byte device)
{
  char stopic[TOPSZ], svalue[MESSZ], sdevice[10];

  if ((device < 1) || (device > Maxdevice)) device = 1;
  snprintf_P(sdevice, sizeof(sdevice), PSTR("%d"), device);
  snprintf_P(stopic, sizeof(stopic), PSTR("%s/%s/RESULT"), PUB_PREFIX, sysCfg.mqtt_topic);
  snprintf_P(svalue, sizeof(svalue), PSTR("{\"%s%s\":\"BLINK %s\"}"),
    sysCfg.mqtt_subtopic, (Maxdevice > 1) ? sdevice : "", (blink_mask & (0x01 << (device -1))) ? MQTT_STATUS_ON : MQTT_STATUS_OFF);
  if (sysCfg.message_format != JSON) json2legacy(stopic, svalue);
  mqtt_publish(stopic, svalue);
}

#ifdef USE_MQTT
#ifdef USE_DOMOTICZ
void mqtt_publishDomoticzPowerState(byte device)
{
  char svalue[MESSZ];

  if (sysCfg.domoticz_relay_idx[device -1] && (strlen(sysCfg.domoticz_in_topic) != 0)) {
    if ((device < 1) || (device > Maxdevice)) device = 1;
    snprintf_P(svalue, sizeof(svalue), PSTR("{\"idx\":%d, \"nvalue\":%d, \"svalue\":\"\"}"),
      sysCfg.domoticz_relay_idx[device -1], (power & (0x01 << (device -1))) ? 1 : 0);
    mqtt_publish(sysCfg.domoticz_in_topic, svalue);
  }
}
#endif  // USE_DOMOTICZ
#endif  // USE_MQTT

void mqtt_connected()
{
  char stopic[TOPSZ], svalue[MESSZ];

#ifdef USE_MQTT
  snprintf_P(stopic, sizeof(stopic), PSTR("%s/%s/#"), SUB_PREFIX, sysCfg.mqtt_topic);
  mqttClient.subscribe(stopic);
  mqttClient.loop();  // Solve LmacRxBlk:1 messages
  snprintf_P(stopic, sizeof(stopic), PSTR("%s/%s/#"), SUB_PREFIX, sysCfg.mqtt_grptopic);
  mqttClient.subscribe(stopic);
  mqttClient.loop();  // Solve LmacRxBlk:1 messages
  snprintf_P(stopic, sizeof(stopic), PSTR("%s/%s/#"), SUB_PREFIX, MQTTClient); // Fall back topic
  mqttClient.subscribe(stopic);
  mqttClient.loop();  // Solve LmacRxBlk:1 messages
#ifdef USE_DOMOTICZ
  if (sysCfg.domoticz_relay_idx[0] && (strlen(sysCfg.domoticz_out_topic) != 0)) {
    snprintf_P(stopic, sizeof(stopic), PSTR("%s/#"), sysCfg.domoticz_out_topic); // domoticz topic
    mqttClient.subscribe(stopic);
    mqttClient.loop();  // Solve LmacRxBlk:1 messages
  }
#endif  // USE_DOMOTICZ
#endif  // USE_MQTT

  if (mqttflag) {
    if (sysCfg.message_format == JSON) {
      snprintf_P(stopic, sizeof(stopic), PSTR("%s/%s/RESULT"), PUB_PREFIX2, sysCfg.mqtt_topic);
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"Info1\":{\"AppName\":\"" APP_NAME "\", \"Version\":\"%s\", \"FallbackTopic\":\"%s\", \"GroupTopic\":\"%s\"}}"),
        Version, MQTTClient, sysCfg.mqtt_grptopic);
    } else {
      snprintf_P(stopic, sizeof(stopic), PSTR("%s/%s/INFO"), PUB_PREFIX2, sysCfg.mqtt_topic);
      snprintf_P(svalue, sizeof(svalue), PSTR(APP_NAME " version %s, FallbackTopic %s, GroupTopic %s"), Version, MQTTClient, sysCfg.mqtt_grptopic);
    }
    mqtt_publish(stopic, svalue);
#ifdef USE_WEBSERVER
    if (sysCfg.webserver) {
      if (sysCfg.message_format == JSON) {
        snprintf_P(svalue, sizeof(svalue), PSTR("{\"Info2\":{\"WebserverMode\":\"%s\", \"Hostname\":\"%s\", \"IPaddress\":\"%s\"}}"),
          (sysCfg.webserver == 2) ? "Admin" : "User", Hostname, WiFi.localIP().toString().c_str());
      } else {
        snprintf_P(svalue, sizeof(svalue), PSTR("Webserver active for %s on %s with IP address %s"),
          (sysCfg.webserver == 2) ? "Admin" : "User", Hostname, WiFi.localIP().toString().c_str());
      }
      mqtt_publish(stopic, svalue);
    }
#endif  // USE_WEBSERVER
#ifdef USE_MQTT
    if (MQTT_MAX_PACKET_SIZE < (TOPSZ+MESSZ)) {
      if (sysCfg.message_format == JSON) {
        snprintf_P(svalue, sizeof(svalue), PSTR("{\"Warning1\":\"Change MQTT_MAX_PACKET_SIZE in libraries/PubSubClient.h to at least %d\"}"), TOPSZ+MESSZ);
      } else {
        snprintf_P(svalue, sizeof(svalue), PSTR("WARNING: Change MQTT_MAX_PACKET_SIZE in libraries/PubSubClient.h to at least %d"), TOPSZ+MESSZ);
      }
      mqtt_publish(stopic, svalue);
    }
#endif  // USE_MQTT
    if (!spiffsPresent()) {
      if (sysCfg.message_format == JSON) {
        snprintf_P(svalue, sizeof(svalue), PSTR("{\"Warning2\":\"No persistent config. Please reflash with at least 16K SPIFFS\"}"));
      } else {
        snprintf_P(svalue, sizeof(svalue), PSTR("WARNING: No persistent config. Please reflash with at least 16K SPIFFS"));
      }
      mqtt_publish(stopic, svalue);
    }
    if (sysCfg.tele_period) tele_period = sysCfg.tele_period -9;
    status_update_timer = 2;
#ifdef USE_MQTT
#ifdef USE_DOMOTICZ
    domoticz_update_timer = 2;
#endif  // USE_DOMOTICZ
#endif  // USE_MQTT
  }
  mqttflag = 0;
}

void mqtt_reconnect()
{
  char stopic[TOPSZ], svalue[TOPSZ], log[LOGSZ];

  mqttcounter = MQTT_RETRY_SECS;

#ifdef USE_MQTT
  if (udpConnected) WiFiUDP::stopAll();
  if (mqttflag > 1) {
#ifdef USE_MQTT_TLS
    addLog_P(LOG_LEVEL_INFO, PSTR("MQTT: Verify TLS fingerprint..."));
    if (!espClient.connect(sysCfg.mqtt_host, sysCfg.mqtt_port)) {
      snprintf_P(log, sizeof(log), PSTR("MQTT: TLS CONNECT FAILED USING WRONG MQTTHost (%s) or MQTTPort (%d). Retry in %d seconds"),
        sysCfg.mqtt_host, sysCfg.mqtt_port, mqttcounter);
      addLog(LOG_LEVEL_DEBUG, log);
      return;
    }
    if (espClient.verify(sysCfg.mqtt_fingerprint, sysCfg.mqtt_host)) {
      addLog_P(LOG_LEVEL_INFO, PSTR("MQTT: Verified"));
    } else {
      addLog_P(LOG_LEVEL_DEBUG, PSTR("MQTT: WARNING - Insecure connection due to invalid Fingerprint"));
    }
#endif  // USE_MQTT_TLS
    mqttClient.setCallback(mqttDataCb);
    mqttflag = 1;
    mqttcounter = 1;
    return;
  }

  addLog_P(LOG_LEVEL_INFO, PSTR("MQTT: Attempting connection..."));
#ifndef USE_MQTT_TLS
#ifdef USE_DISCOVERY
#ifdef MQTT_HOST_DISCOVERY
  mdns_discoverMQTTServer();
#endif  // MQTT_HOST_DISCOVERY
#endif  // USE_DISCOVERY
#endif  // USE_MQTT_TLS
  mqttClient.setServer(sysCfg.mqtt_host, sysCfg.mqtt_port);
  snprintf_P(stopic, sizeof(stopic), PSTR("%s/%s"), LWT_PREFIX, sysCfg.mqtt_topic);
  snprintf_P(svalue, sizeof(svalue), PSTR("0"));
  if (mqttClient.connect(MQTTClient, sysCfg.mqtt_user, sysCfg.mqtt_pwd, stopic, 1, true, svalue)) {
    addLog_P(LOG_LEVEL_INFO, PSTR("MQTT: Connected"));
    mqttcounter = 0;
    snprintf_P(svalue, sizeof(svalue), PSTR("1"));
    mqtt_publish(stopic, svalue, true);
    udpConnected = false;
    mqtt_connected();
  } else {
    snprintf_P(log, sizeof(log), PSTR("MQTT: CONNECT FAILED, rc %d. Retry in %d seconds"), mqttClient.state(), mqttcounter);
    addLog(LOG_LEVEL_DEBUG, log);
  }
#else
  mqtt_connected();
#endif  // USE_MQTT
}

void mqttDataCb(char* topic, byte* data, unsigned int data_len)
{
  char *str;
  char svalue[MESSZ];

  if (!strcmp(SUB_PREFIX,PUB_PREFIX)) {
    str = strstr(topic,SUB_PREFIX);
    if ((str == topic) && mqtt_cmnd_publish) {
      if (mqtt_cmnd_publish > 8) mqtt_cmnd_publish -= 8; else mqtt_cmnd_publish = 0;
      return;
    }
  }

  uint16_t i = 0, grpflg = 0, index;
  char topicBuf[TOPSZ], dataBuf[data_len+1], dataBufUc[MESSZ];
  char *p, *mtopic = NULL, *type = NULL, *devc = NULL;
  char stopic[TOPSZ], stemp1[TOPSZ], stemp2[10];

  strncpy(topicBuf, topic, sizeof(topicBuf));
  memcpy(dataBuf, data, sizeof(dataBuf));
  dataBuf[sizeof(dataBuf)-1] = 0;

  snprintf_P(svalue, sizeof(svalue), PSTR("RSLT: Receive topic %s, data size %d, data %s"), topicBuf, data_len, dataBuf);
  addLog(LOG_LEVEL_DEBUG_MORE, svalue);

#ifdef USE_MQTT
#ifdef USE_DOMOTICZ
  domoticz_update_flag = 1;
  if (!strncmp(topicBuf, sysCfg.domoticz_out_topic, strlen(sysCfg.domoticz_out_topic)) != 0) {
    unsigned long idx = 0;
    int16_t nvalue;

    if (data_len < 20) return;
    idx = getKeyIntValue(dataBuf,"\"idx\"");
    nvalue = getKeyIntValue(dataBuf,"\"nvalue\"");

    snprintf_P(svalue, sizeof(svalue), PSTR("DMTZ: idx %d, nvalue %d"), idx, nvalue);
    addLog(LOG_LEVEL_DEBUG_MORE, svalue);

    data_len = 0;
    if (nvalue == 0 || nvalue == 1) {
      for (i = 0; i < Maxdevice; i++) {
        if ((idx > 0) && (idx == sysCfg.domoticz_relay_idx[i])) {
          snprintf_P(dataBuf, sizeof(dataBuf), PSTR("%d"), nvalue);
          data_len = strlen(dataBuf);
          break;
        }
      }
    }
    if (!data_len) return;
    if (((power >> i) &1) == nvalue) return;
    snprintf_P(stemp1, sizeof(stemp1), PSTR("%d"), i +1);
    snprintf_P(topicBuf, sizeof(topicBuf), PSTR("%s/%s/%s%s"),
      SUB_PREFIX, sysCfg.mqtt_topic, sysCfg.mqtt_subtopic, (Maxdevice > 1) ? stemp1 : "");

    snprintf_P(svalue, sizeof(svalue), PSTR("DMTZ: Receive topic %s, data size %d, data %s"), topicBuf, data_len, dataBuf);
    addLog(LOG_LEVEL_DEBUG_MORE, svalue);

    domoticz_update_flag = 0;
  }
#endif  // USE_DOMOTICZ
#endif  // USE_MQTT

  memmove(topicBuf, topicBuf+sizeof(SUB_PREFIX), sizeof(topicBuf)-sizeof(SUB_PREFIX));  // Remove SUB_PREFIX
/*
  // Use following code after release 3.0.0
  i = 0;
  for (str = strtok_r(topicBuf, "/", &p); str && i < 2; str = strtok_r(NULL, "/", &p)) {
    switch (i++) {
    case 0:  // Topic / GroupTopic / DVES_123456
      mtopic = str;
      break;
    case 1:  // TopicIndex / Text
      type = str;
    }
  }
  if (!strcmp(mtopic, sysCfg.mqtt_grptopic)) grpflg = 1;
  index = 1;
*/
  i = 0;
  for (str = strtok_r(topicBuf, "/", &p); str && i < 3; str = strtok_r(NULL, "/", &p)) {
    switch (i++) {
    case 0:  // Topic / GroupTopic / DVES_123456
      mtopic = str;
      break;
    case 1:  // TopicIndex / Text
      type = str;
      break;
    case 2:  // Text
      devc = str;
    }
  }
  if (!strcmp(mtopic, sysCfg.mqtt_grptopic)) grpflg = 1;

  index = 1;
  if (devc != NULL) {
    index = atoi(type);
    if ((index < 1) || (index > Maxdevice)) index = 0;
    type = devc;
  }
  if (!index) type = NULL;

  if (type != NULL) {
    for(i = 0; i < strlen(type); i++) {
      type[i] = toupper(type[i]);
      if (isdigit(type[i])) {
        index = atoi(type +i);
        break;
      }
    }
    type[i] = '\0';
  }

  for(i = 0; i <= sizeof(dataBufUc); i++) dataBufUc[i] = toupper(dataBuf[i]);

  snprintf_P(svalue, sizeof(svalue), PSTR("RSLT: DataCb Topic %s, Group %d, Index %d, Type %s, Data %s (%s)"),
    mtopic, grpflg, index, type, dataBuf, dataBufUc);
  addLog(LOG_LEVEL_DEBUG, svalue);

  snprintf_P(stopic, sizeof(stopic), PSTR("%s/%s/RESULT"), PUB_PREFIX, sysCfg.mqtt_topic);
  if (type != NULL) {
    snprintf_P(svalue, sizeof(svalue), PSTR("{\"Command\":\"Error\"}"));
    if (sysCfg.ledstate &0x02) blinks++;

    if (!strcmp(dataBufUc,"?")) data_len = 0;
    int16_t payload = atoi(dataBuf);     // -32766 - 32767
    uint16_t payload16 = atoi(dataBuf);  // 0 - 65535
    if (!strcmp(dataBufUc,"OFF") || !strcmp(dataBufUc,"STOP")) payload = 0;
    if (!strcmp(dataBufUc,"ON") || !strcmp(dataBufUc,"START") || !strcmp(dataBufUc,"USER")) payload = 1;
    if (!strcmp(dataBufUc,"TOGGLE") || !strcmp(dataBufUc,"ADMIN")) payload = 2;
    if (!strcmp(dataBufUc,"BLINK")) payload = 3;
    if (!strcmp(dataBufUc,"BLINKOFF")) payload = 4;

    if ((!strcmp(type,"POWER") || !strcmp(type,"LIGHT")) && (index > 0) && (index <= Maxdevice)) {
      snprintf_P(sysCfg.mqtt_subtopic, sizeof(sysCfg.mqtt_subtopic), PSTR("%s"), type);
      if ((data_len == 0) || (payload > 4)) payload = 9;
      do_cmnd_power(index, payload);
      return;
    }
    else if (!strcmp(type,"STATUS")) {
      if ((data_len == 0) || (payload < 0) || (payload > MAX_STATUS)) payload = 99;
      publish_status(payload);
      return;
    }
#if (MODULE != MOTOR_CAC)
    else if (!strcmp(type,"POWERONSTATE")) {
      if ((data_len > 0) && (payload >= 0) && (payload <= 3)) {
        sysCfg.poweronstate = payload;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"PowerOnState\":%d}"), sysCfg.poweronstate);
    }
#endif
    else if (!strcmp(type,"PULSETIME")) {
      if (data_len > 0) {
        sysCfg.pulsetime = payload16;  // 0 - 65535
        pulse_timer = 0;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"PulseTime\":%d}"), sysCfg.pulsetime);
    }
    else if (!strcmp(type,"BLINKTIME")) {
      if ((data_len > 0) && (payload > 2) && (payload <= 3600)) {
        sysCfg.blinktime = payload;
        if (blink_timer) blink_timer = sysCfg.blinktime;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"BlinkTime\":%d}"), sysCfg.blinktime);
    }
    else if (!strcmp(type,"BLINKCOUNT")) {
      if (data_len > 0) {
        sysCfg.blinkcount = payload16;  // 0 - 65535
        if (blink_counter) blink_counter = sysCfg.blinkcount *2;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"BlinkCount\":%d}"), sysCfg.blinkcount);
    }
    else if (!strcmp(type,"SAVEDATA")) {
      if ((data_len > 0) && (payload >= 0) && (payload <= 3600)) {
        sysCfg.savedata = payload;
        savedatacounter = sysCfg.savedata;
      }
      if (sysCfg.savestate) sysCfg.power = power;
      CFG_Save();
      if (sysCfg.savedata > 1) snprintf_P(stemp1, sizeof(stemp1), PSTR("Every %d seconds"), sysCfg.savedata);
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"SaveData\":\"%s\"}"), (sysCfg.savedata) ? (sysCfg.savedata > 1) ? stemp1 : MQTT_STATUS_ON : MQTT_STATUS_OFF);
    }
    else if (!strcmp(type,"SAVESTATE")) {
      if ((data_len > 0) && (payload >= 0) && (payload <= 1)) {
        sysCfg.savestate = payload;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"SaveState\":\"%s\"}"), (sysCfg.savestate) ? MQTT_STATUS_ON : MQTT_STATUS_OFF);
    }
    else if (!strcmp(type,"MESSAGEFORMAT")) {
      if ((data_len > 0) && (payload >= 0) && (payload < MAX_FORMAT)) {
        sysCfg.message_format = payload;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"MessageFormat\":\"%s\"}"), (sysCfg.message_format) ? "JSON" : "Legacy");
    }
    else if (!strcmp(type,"MODEL")) {
      if ((data_len > 0) && (payload >= 0) && (payload <= SONOFF_4CH)) {
        sysCfg.model = payload;
        restartflag = 2;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"Model\":%d}"), sysCfg.model);
    }
    else if (!strcmp(type,"UPGRADE") || !strcmp(type,"UPLOAD")) {
      if ((data_len > 0) && (payload == 1)) {
        otaflag = 3;
        snprintf_P(svalue, sizeof(svalue), PSTR("{\"Upgrade\":\"Version %s from %s\"}"), Version, sysCfg.otaUrl);
      } else {
        snprintf_P(svalue, sizeof(svalue), PSTR("{\"Upgrade\":\"Option 1 to upgrade\"}"));
      }
    }
    else if (!strcmp(type,"OTAURL")) {
      if ((data_len > 0) && (data_len < sizeof(sysCfg.otaUrl)))
        strlcpy(sysCfg.otaUrl, (payload == 1) ? OTA_URL : dataBuf, sizeof(sysCfg.otaUrl));
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"OtaUrl\":\"%s\"}"), sysCfg.otaUrl);
    }
    else if (!strcmp(type,"SERIALLOG")) {
      if ((data_len > 0) && (payload >= LOG_LEVEL_NONE) && (payload <= LOG_LEVEL_ALL)) {
        sysCfg.seriallog_level = payload;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"SerialLog\":%d}"), sysCfg.seriallog_level);
    }
    else if (!strcmp(type,"SYSLOG")) {
      if ((data_len > 0) && (payload >= LOG_LEVEL_NONE) && (payload <= LOG_LEVEL_ALL)) {
        sysCfg.syslog_level = payload;
        syslog_level = payload;
        syslog_timer = 0;
        snprintf_P(svalue, sizeof(svalue), PSTR("{\"SysLog\":%d}"), sysCfg.syslog_level);
      } else {
       snprintf_P(svalue, sizeof(svalue), PSTR("{\"SysLog\":\"%d (Setting %d)\"}"), syslog_level, sysCfg.syslog_level);
      }
    }
    else if (!strcmp(type,"LOGHOST")) {
      if ((data_len > 0) && (data_len < sizeof(sysCfg.syslog_host))) {
        strlcpy(sysCfg.syslog_host, (payload == 1) ? SYS_LOG_HOST : dataBuf, sizeof(sysCfg.syslog_host));
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"LogHost\":\"%s\"}"), sysCfg.syslog_host);
    }
    else if (!strcmp(type,"LOGPORT")) {
      if ((data_len > 0) && (payload > 0) && (payload < 32766)) {
        sysCfg.syslog_port = (payload == 1) ? SYS_LOG_PORT : payload;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"LogPort\":%d}"), sysCfg.syslog_port);
    }
    else if (!strcmp(type,"AP")) {
      if ((data_len > 0) && (payload >= 0) && (payload <= 2)) {
        switch (payload) {
        case 0:  // Toggle
          sysCfg.sta_active ^= 1;
          break;
        case 1:  // AP1
        case 2:  // AP2
          sysCfg.sta_active = payload -1;
        }
        restartflag = 2;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"Ap\":\"%d (%s)\"}"), sysCfg.sta_active +1, sysCfg.sta_ssid[sysCfg.sta_active]);
    }
    else if (!strcmp(type,"SSID") && (index > 0) && (index <= 2)) {
      if ((data_len > 0) && (data_len < sizeof(sysCfg.sta_ssid[0]))) {
        strlcpy(sysCfg.sta_ssid[index -1], (payload == 1) ? (index == 1) ? STA_SSID1 : STA_SSID2 : dataBuf, sizeof(sysCfg.sta_ssid[0]));
        sysCfg.sta_active = 0;
        restartflag = 2;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"SSid%d\":\"%s\"}"), index, sysCfg.sta_ssid[index -1]);
    }
    else if (!strcmp(type,"PASSWORD") && (index > 0) && (index <= 2)) {
      if ((data_len > 0) && (data_len < sizeof(sysCfg.sta_pwd[0]))) {
        strlcpy(sysCfg.sta_pwd[index -1], (payload == 1) ? (index == 1) ? STA_PASS1 : STA_PASS2 : dataBuf, sizeof(sysCfg.sta_pwd[0]));
        sysCfg.sta_active = 0;
        restartflag = 2;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"Password%d\":\"%s\"}"), index, sysCfg.sta_pwd[index -1]);
    }
    else if (!grpflg && !strcmp(type,"HOSTNAME")) {
      if ((data_len > 0) && (data_len < sizeof(sysCfg.hostname))) {
        strlcpy(sysCfg.hostname, (payload == 1) ? WIFI_HOSTNAME : dataBuf, sizeof(sysCfg.hostname));
        if (strstr(sysCfg.hostname,"%"))
          strlcpy(sysCfg.hostname, DEF_WIFI_HOSTNAME, sizeof(sysCfg.hostname));
        restartflag = 2;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"Hostname\":\"%s\"}"), sysCfg.hostname);
    }
    else if (!strcmp(type,"WIFICONFIG") || !strcmp(type,"SMARTCONFIG")) {
      if ((data_len > 0) && (payload >= WIFI_RESTART) && (payload < MAX_WIFI_OPTION)) {
        sysCfg.sta_config = payload;
        wificheckflag = sysCfg.sta_config;
        snprintf_P(stemp1, sizeof(stemp1), wificfg[sysCfg.sta_config]);
        snprintf_P(svalue, sizeof(svalue), PSTR("{\"WifiConfig\":\"%s selected\"}"), stemp1);
        if (WIFI_State() != WIFI_RESTART) {
//          snprintf_P(svalue, sizeof(svalue), PSTR("%s after restart"), svalue);
          restartflag = 2;
        }
      } else {
        snprintf_P(stemp1, sizeof(stemp1), wificfg[sysCfg.sta_config]);
        snprintf_P(svalue, sizeof(svalue), PSTR("{\"WifiConfig\":\"%d (%s)\"}"), sysCfg.sta_config, stemp1);
      }
    }
    else if (!strcmp(type,"FRIENDLYNAME")) {
      if ((data_len > 0) && (data_len < sizeof(sysCfg.friendlyname))) {
        strlcpy(sysCfg.friendlyname, (payload == 1) ? FRIENDLY_NAME : dataBuf, sizeof(sysCfg.friendlyname));
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"FriendlyName\":\"%s\"}"), sysCfg.friendlyname);
    }
#ifdef USE_WALL_SWITCH
    else if (!strcmp(type,"SWITCHMODE")) {
      if ((data_len > 0) && (payload >= 0) && (payload < MAX_SWITCH_OPTION)) {
        sysCfg.switchmode = payload;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"SwitchMode\":%d}"), sysCfg.switchmode);
    }
#endif  // USE_WALL_SWITCH
#ifdef USE_WEBSERVER
    else if (!strcmp(type,"WEBSERVER")) {
      if ((data_len > 0) && (payload >= 0) && (payload <= 2)) {
        sysCfg.webserver = payload;
      }
      if (sysCfg.webserver) {
        snprintf_P(svalue, sizeof(svalue), PSTR("{\"Webserver\":\"Active for %s on %s with IP address %s\"}"),
          (sysCfg.webserver == 2) ? "ADMIN" : "USER", Hostname, WiFi.localIP().toString().c_str());
      } else {
        snprintf_P(svalue, sizeof(svalue), PSTR("{\"Webserver\":\"%s\"}"), MQTT_STATUS_OFF);
      }
    }
    else if (!strcmp(type,"WEBLOG")) {
      if ((data_len > 0) && (payload >= LOG_LEVEL_NONE) && (payload <= LOG_LEVEL_ALL)) {
        sysCfg.weblog_level = payload;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"WebLog\":%d}"), sysCfg.weblog_level);
    }
#endif  // USE_WEBSERVER
    else if (!strcmp(type,"MQTTUNITS")) {
      if ((data_len > 0) && (payload >= 0) && (payload <= 1)) {
        sysCfg.mqtt_units = payload;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"MqttUnits\":\"%s\"}"), (sysCfg.mqtt_units) ? MQTT_STATUS_ON : MQTT_STATUS_OFF);
    }
#ifdef USE_MQTT
    else if (!strcmp(type,"MQTTHOST")) {
      if ((data_len > 0) && (data_len < sizeof(sysCfg.mqtt_host))) {
        strlcpy(sysCfg.mqtt_host, (payload == 1) ? MQTT_HOST : dataBuf, sizeof(sysCfg.mqtt_host));
        restartflag = 2;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"MqttHost\",\"%s\"}"), sysCfg.mqtt_host);
    }
    else if (!strcmp(type,"MQTTPORT")) {
      if ((data_len > 0) && (payload > 0) && (payload < 32766)) {
        sysCfg.mqtt_port = (payload == 1) ? MQTT_PORT : payload;
        restartflag = 2;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"MqttPort\":%d}"), sysCfg.mqtt_port);
    }
#ifdef USE_MQTT_TLS
    else if (!strcmp(type,"MQTTFINGERPRINT")) {
      if ((data_len > 0) && (data_len < sizeof(sysCfg.mqtt_fingerprint))) {
        strlcpy(sysCfg.mqtt_fingerprint, (payload == 1) ? MQTT_FINGERPRINT : dataBuf, sizeof(sysCfg.mqtt_fingerprint));
        restartflag = 2;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"MqttFingerprint\":\"%s\"}"), sysCfg.mqtt_fingerprint);
    }
#endif
    else if (!grpflg && !strcmp(type,"MQTTCLIENT")) {
      if ((data_len > 0) && (data_len < sizeof(sysCfg.mqtt_client))) {
        strlcpy(sysCfg.mqtt_client, (payload == 1) ? MQTT_CLIENT_ID : dataBuf, sizeof(sysCfg.mqtt_client));
        restartflag = 2;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"MqttClient\":\"%s\"}"), sysCfg.mqtt_client);
    }
    else if (!strcmp(type,"MQTTUSER")) {
      if ((data_len > 0) && (data_len < sizeof(sysCfg.mqtt_user))) {
        strlcpy(sysCfg.mqtt_user, (payload == 1) ? MQTT_USER : dataBuf, sizeof(sysCfg.mqtt_user));
        restartflag = 2;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("[\"MqttUser\":\"%s\"}"), sysCfg.mqtt_user);
    }
    else if (!strcmp(type,"MQTTPASSWORD")) {
      if ((data_len > 0) && (data_len < sizeof(sysCfg.mqtt_pwd))) {
        strlcpy(sysCfg.mqtt_pwd, (payload == 1) ? MQTT_PASS : dataBuf, sizeof(sysCfg.mqtt_pwd));
        restartflag = 2;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"MqttPassword\":\"%s\"}"), sysCfg.mqtt_pwd);
    }
#ifdef USE_DOMOTICZ
    else if (!strcmp(type,"DOMOTICZINTOPIC")) {
      if ((data_len > 0) && (data_len < sizeof(sysCfg.domoticz_in_topic))) {
        strlcpy(sysCfg.domoticz_in_topic, (payload == 1) ? DOMOTICZ_IN_TOPIC : dataBuf, sizeof(sysCfg.domoticz_in_topic));
        restartflag = 2;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"DomoticzInTopic\":\"%s\"}"), sysCfg.domoticz_in_topic);
    }
    else if (!strcmp(type,"DOMOTICZOUTTOPIC")) {
      if ((data_len > 0) && (data_len < sizeof(sysCfg.domoticz_out_topic))) {
        strlcpy(sysCfg.domoticz_out_topic, (payload == 1) ? DOMOTICZ_OUT_TOPIC : dataBuf, sizeof(sysCfg.domoticz_out_topic));
        restartflag = 2;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"DomoticzOutTopic\":\"%s\"}"), sysCfg.domoticz_out_topic);
    }
    else if (!strcmp(type,"DOMOTICZIDX") && (index > 0) && (index <= Maxdevice)) {
      if ((data_len > 0) && (payload >= 0)) {
        sysCfg.domoticz_relay_idx[index -1] = payload;
        restartflag = 2;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"DomoticzIdx%d\":%d}"), index, sysCfg.domoticz_relay_idx[index -1]);
    }
    else if (!strcmp(type,"DOMOTICZKEYIDX") && (index > 0) && (index <= Maxdevice)) {
      if ((data_len > 0) && (payload >= 0)) {
        sysCfg.domoticz_key_idx[index -1] = payload;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"DomoticzKeyIdx%d\":%d}"), index, sysCfg.domoticz_key_idx[index -1]);
    }
    else if (!strcmp(type,"DOMOTICZUPDATETIMER")) {
      if ((data_len > 0) && (payload >= 0) && (payload < 3601)) {
        sysCfg.domoticz_update_timer = payload;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"DomoticzUpdateTimer\":%d}"), sysCfg.domoticz_update_timer);
    }
#endif  // USE_DOMOTICZ
#endif  // USE_MQTT
    else if (!strcmp(type,"TELEPERIOD")) {
      if ((data_len > 0) && (payload >= 0) && (payload < 3601)) {
        sysCfg.tele_period = (payload == 1) ? TELE_PERIOD : payload;
        if ((sysCfg.tele_period > 0) && (sysCfg.tele_period < 10)) sysCfg.tele_period = 10;   // Do not allow periods < 10 seconds
        tele_period = sysCfg.tele_period;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"TelePeriod\":\"%d%s\"}"), sysCfg.tele_period, (sysCfg.mqtt_units) ? " Sec" : "");
    }
#ifdef USE_MQTT
    else if (!strcmp(type,"GROUPTOPIC")) {
      if ((data_len > 0) && (data_len < sizeof(sysCfg.mqtt_grptopic))) {
        for(i = 0; i <= data_len; i++)
          if ((dataBuf[i] == '/') || (dataBuf[i] == '+') || (dataBuf[i] == '#')) dataBuf[i] = '_';
        if (!strcmp(dataBuf, MQTTClient)) payload = 1;
        strlcpy(sysCfg.mqtt_grptopic, (payload == 1) ? MQTT_GRPTOPIC : dataBuf, sizeof(sysCfg.mqtt_grptopic));
        restartflag = 2;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"GroupTopic\":\"%s\"}"), sysCfg.mqtt_grptopic);
    }
    else if (!grpflg && !strcmp(type,"TOPIC")) {
      if ((data_len > 0) && (data_len < sizeof(sysCfg.mqtt_topic))) {
        for(i = 0; i <= data_len; i++)
          if ((dataBuf[i] == '/') || (dataBuf[i] == '+') || (dataBuf[i] == '#')) dataBuf[i] = '_';
        if (!strcmp(dataBuf, MQTTClient)) payload = 1;
        strlcpy(sysCfg.mqtt_topic, (payload == 1) ? MQTT_TOPIC : dataBuf, sizeof(sysCfg.mqtt_topic));
        restartflag = 2;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"Topic\":\"%s\"}"), sysCfg.mqtt_topic);
    }
    else if (!grpflg && !strcmp(type,"BUTTONTOPIC")) {
      if ((data_len > 0) && (data_len < sizeof(sysCfg.mqtt_topic2))) {
        for(i = 0; i <= data_len; i++)
          if ((dataBuf[i] == '/') || (dataBuf[i] == '+') || (dataBuf[i] == '#')) dataBuf[i] = '_';
        if (!strcmp(dataBuf, MQTTClient)) payload = 1;
        strlcpy(sysCfg.mqtt_topic2, (payload == 1) ? sysCfg.mqtt_topic : dataBuf, sizeof(sysCfg.mqtt_topic2));
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"ButtonTopic\":\"%s\"}"), sysCfg.mqtt_topic2);
    }
    else if (!strcmp(type,"BUTTONRETAIN")) {
      if ((data_len > 0) && (payload >= 0) && (payload <= 1)) {
        strlcpy(sysCfg.mqtt_topic2, sysCfg.mqtt_topic, sizeof(sysCfg.mqtt_topic2));
        if (!payload) {
          for(i = 1; i <= Maxdevice; i++) {
            send_button_power(i, 3);  // Clear MQTT retain in broker
          }
        }
        sysCfg.mqtt_button_retain = payload;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"ButtonRetain\":\"%s\"}"), (sysCfg.mqtt_button_retain) ? MQTT_STATUS_ON : MQTT_STATUS_OFF);
    }
    else if (!strcmp(type,"POWERRETAIN") || !strcmp(type,"LIGHTRETAIN")) {
      if ((data_len > 0) && (payload >= 0) && (payload <= 1)) {
        if (!payload) {
          for(i = 1; i <= Maxdevice; i++) {  // Clear MQTT retain in broker
            snprintf_P(stemp2, sizeof(stemp2), PSTR("%d"), i);
            snprintf_P(stemp1, sizeof(stemp1), PSTR("%s/%s/POWER%s"), PUB_PREFIX, sysCfg.mqtt_topic, (Maxdevice > 1) ? stemp2 : "");
            mqtt_publish(stemp1, "", sysCfg.mqtt_power_retain);
            snprintf_P(stemp1, sizeof(stemp1), PSTR("%s/%s/LIGHT%s"), PUB_PREFIX, sysCfg.mqtt_topic, (Maxdevice > 1) ? stemp2 : "");
            mqtt_publish(stemp1, "", sysCfg.mqtt_power_retain);
          }
        }
        sysCfg.mqtt_power_retain = payload;
      }
      snprintf_P(stemp1, sizeof(stemp1), PSTR("%s"), (!strcmp(sysCfg.mqtt_subtopic,"POWER")) ? "Power" : "Light");
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"%sRetain\":\"%s\"}"), stemp1, (sysCfg.mqtt_power_retain) ? MQTT_STATUS_ON : MQTT_STATUS_OFF);
    }
#endif  // USE_MQTT
    else if (!strcmp(type,"RESTART")) {
      switch (payload) {
      case 1:
        restartflag = 2;
        snprintf_P(svalue, sizeof(svalue), PSTR("{\"Restart\":\"Restarting\"}"));
        break;
      case 99:
        addLog_P(LOG_LEVEL_INFO, PSTR("APP: Restarting"));
        ESP.restart();
        break;
      default:
        snprintf_P(svalue, sizeof(svalue), PSTR("{\"Restart\":\"1 to restart\"}"));
      }
    }
    else if (!strcmp(type,"RESET")) {
      switch (payload) {
      case 1:
        restartflag = 211;
        snprintf_P(svalue, sizeof(svalue), PSTR("{\"Reset\":\"Reset and Restarting\"}"));
        break;
      case 2:
        restartflag = 212;
        snprintf_P(svalue, sizeof(svalue), PSTR("{\"Reset\":\"Erase, Reset and Restarting\"}"));
        break;
      default:
        snprintf_P(svalue, sizeof(svalue), PSTR("{\"Reset\":\"1 to reset\"}"));
      }
    }
    else if (!strcmp(type,"TIMEZONE")) {
      if ((data_len > 0) && (((payload >= -12) && (payload <= 12)) || (payload == 99))) {
        sysCfg.timezone = payload;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"Timezone\":\"%d%s\"}"), sysCfg.timezone, (sysCfg.mqtt_units) ? " Hr" : "");
    }
    else if (!strcmp(type,"LEDSTATE")) {
      if ((data_len > 0) && (payload >= 0) && (payload < MAX_LED_OPTION)) {
        sysCfg.ledstate = payload;
        if (!sysCfg.ledstate) digitalWrite(LED_PIN, LED_INVERTED);
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"LedState\":%d}"), sysCfg.ledstate);
    }
    else if (!strcmp(type,"CFGDUMP")) {
      CFG_Dump();
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"CfgDump\":\"Done\"}"));
    }
#ifdef SEND_TELEMETRY_I2C
    else if (!strcmp(type,"I2CSCAN")) {
      i2c_scan(svalue, sizeof(svalue));
    }
#endif //SEND_TELEMETRY_I2C
#ifdef USE_POWERMONITOR
    else if (!strcmp(type,"POWERLOW")) {
      if ((data_len > 0) && (payload >= 0) && (payload < 3601)) {
        sysCfg.hlw_pmin = payload;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"PowerLow\":\"%d%s\"}"), sysCfg.hlw_pmin, (sysCfg.mqtt_units) ? " W" : "");
    }
    else if (!strcmp(type,"POWERHIGH")) {
      if ((data_len > 0) && (payload >= 0) && (payload < 3601)) {
        sysCfg.hlw_pmax = payload;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"PowerHigh\":\"%d%s\"}"), sysCfg.hlw_pmax, (sysCfg.mqtt_units) ? " W" : "");
    }
    else if (!strcmp(type,"VOLTAGELOW")) {
      if ((data_len > 0) && (payload >= 0) && (payload < 501)) {
        sysCfg.hlw_umin = payload;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"VoltageLow\":\"%d%s\"}"), sysCfg.hlw_umin, (sysCfg.mqtt_units) ? " V" : "");
    }
    else if (!strcmp(type,"VOLTAGEHIGH")) {
      if ((data_len > 0) && (payload >= 0) && (payload < 501)) {
        sysCfg.hlw_umax = payload;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("[\"VoltageHigh\":\"%d%s\"}"), sysCfg.hlw_umax, (sysCfg.mqtt_units) ? " V" : "");
    }
    else if (!strcmp(type,"CURRENTLOW")) {
      if ((data_len > 0) && (payload >= 0) && (payload < 16001)) {
        sysCfg.hlw_imin = payload;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"CurrentLow\":\"%d%s\"}"), sysCfg.hlw_imin, (sysCfg.mqtt_units) ? " mA" : "");
    }
    else if (!strcmp(type,"CURRENTHIGH")) {
      if ((data_len > 0) && (payload >= 0) && (payload < 16001)) {
        sysCfg.hlw_imax = payload;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"CurrentHigh\":\"%d%s\"}"), sysCfg.hlw_imax, (sysCfg.mqtt_units) ? " mA" : "");
    }
#ifdef USE_POWERCALIBRATION
    else if (!strcmp(type,"HLWPCAL")) {
      if ((data_len > 0) && (payload > 0) && (payload < 32001)) {
        sysCfg.hlw_pcal = (payload == 1) ? HLW_PREF_PULSE : payload;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("(\"HlwPcal\":\"%d%s\"}"), sysCfg.hlw_pcal, (sysCfg.mqtt_units) ? " uS" : "");
    }
    else if (!strcmp(type,"HLWUCAL")) {
      if ((data_len > 0) && (payload > 0) && (payload < 32001)) {
        sysCfg.hlw_ucal = (payload == 1) ? HLW_UREF_PULSE : payload;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"HlwUcal\":\"%d%s\"}"), sysCfg.hlw_ucal, (sysCfg.mqtt_units) ? " uS" : "");
    }
    else if (!strcmp(type,"HLWICAL")) {
      if ((data_len > 0) && (payload > 0) && (payload < 32001)) {
        sysCfg.hlw_ical = (payload == 1) ? HLW_IREF_PULSE : payload;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"HlwIcal\":\"%d%s\"}"), sysCfg.hlw_ical, (sysCfg.mqtt_units) ? " uS" : "");
    }
#endif  // USE_POWERCALIBRATION
#ifdef FEATURE_POWER_LIMIT
    else if (!strcmp(type,"MAXPOWER")) {
      if ((data_len > 0) && (payload >= 0) && (payload < 3601)) {
        sysCfg.hlw_mpl = payload;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"MaxPower\":\"%d%s\"}"), sysCfg.hlw_mpl, (sysCfg.mqtt_units) ? " W" : "");
    }
    else if (!strcmp(type,"MAXPOWERHOLD")) {
      if ((data_len > 0) && (payload >= 0) && (payload < 3601)) {
        sysCfg.hlw_mplh = (payload == 1) ? MAX_POWER_HOLD : payload;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"MaxPowerHold\":\"%d%s\"}"), sysCfg.hlw_mplh, (sysCfg.mqtt_units) ? " Sec" : "");
    }
    else if (!strcmp(type,"MAXPOWERWINDOW")) {
      if ((data_len > 0) && (payload >= 0) && (payload < 3601)) {
        sysCfg.hlw_mplw = (payload == 1) ? MAX_POWER_WINDOW : payload;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"MaxPowerWindow\":\"%d%s\"}"), sysCfg.hlw_mplw, (sysCfg.mqtt_units) ? " Sec" : "");
    }
    else if (!strcmp(type,"SAFEPOWER")) {
      if ((data_len > 0) && (payload >= 0) && (payload < 3601)) {
        sysCfg.hlw_mspl = payload;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"SafePower\":\"%d%s\"}"), sysCfg.hlw_mspl, (sysCfg.mqtt_units) ? " W" : "");
    }
    else if (!strcmp(type,"SAFEPOWERHOLD")) {
      if ((data_len > 0) && (payload >= 0) && (payload < 3601)) {
        sysCfg.hlw_msplh = (payload == 1) ? SAFE_POWER_HOLD : payload;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"SafePowerHold\":\"%d%s\"}"), sysCfg.hlw_msplh, (sysCfg.mqtt_units) ? " Sec" : "");
    }
    else if (!strcmp(type,"SAFEPOWERWINDOW")) {
      if ((data_len > 0) && (payload >= 0) && (payload < 1440)) {
        sysCfg.hlw_msplw = (payload == 1) ? SAFE_POWER_WINDOW : payload;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"SafePowerWindow\":\"%d%s\"}"), sysCfg.hlw_msplw, (sysCfg.mqtt_units) ? " Min" : "");
    }
    else if (!strcmp(type,"MAXENERGY")) {
      if ((data_len > 0) && (payload >= 0) && (payload < 3601)) {
        sysCfg.hlw_mkwh = payload;
        hlw_mkwh_state = 3;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"MaxEnergy\":\"%d%s\"}"), sysCfg.hlw_mkwh, (sysCfg.mqtt_units) ? " Wh" : "");
    }
    else if (!strcmp(type,"MAXENERGYSTART")) {
      if ((data_len > 0) && (payload >= 0) && (payload < 24)) {
        sysCfg.hlw_mkwhs = payload;
      }
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"MaxEnergyStart\":\"%d%s\"}"), sysCfg.hlw_mkwhs, (sysCfg.mqtt_units) ? " Hr" : "");
    }
#endif  // FEATURE_POWER_LIMIT
#endif  // USE_POWERMONITOR
    else {
      type = NULL;
    }
  }
  if (type == NULL) {
    blinks = 201;
    snprintf_P(svalue, sizeof(svalue), PSTR("{\"Commands1\":\"Status, SaveData, SaveSate, Upgrade, Otaurl, Restart, Reset, WifiConfig, Seriallog, Syslog, LogHost, LogPort, SSId1, SSId2, Password1, Password2, AP%s\"}"), (!grpflg) ? ", Hostname" : "");
    if (sysCfg.message_format != JSON) json2legacy(stopic, svalue);
    mqtt_publish(stopic, svalue);

#ifdef USE_MQTT
    snprintf_P(svalue, sizeof(svalue), PSTR("{\"Commands2\":\"MqttHost, MqttPort, MqttUser, MqttPassword%s, MqttUnits, MessageFormat, GroupTopic, Timezone, LedState, TelePeriod\"}"), (!grpflg) ? ", MqttClient, Topic, ButtonTopic, ButtonRetain, PowerRetain" : "");
#else
    snprintf_P(svalue, sizeof(svalue), PSTR("{\"Commands2\":\"MqttUnits, MessageFormat, Timezone, LedState, TelePeriod\"}"), (!grpflg) ? ", MqttClient" : "");
#endif  // USE_MQTT
    if (sysCfg.message_format != JSON) json2legacy(stopic, svalue);
    mqtt_publish(stopic, svalue);

    snprintf_P(svalue, sizeof(svalue), PSTR("{\"Commands3\":\"%s%s, PulseTime, BlinkTime, BlinkCount"), (sysCfg.model == SONOFF) ? "Power, Light" : "Power1, Power2, Light1 Light2", (MODULE != MOTOR_CAC) ? ", PowerOnState" : "");
#ifdef USE_WEBSERVER
    snprintf_P(svalue, sizeof(svalue), PSTR("%s, Weblog, Webserver"), svalue);
#endif
#ifdef USE_MQTT
#ifdef USE_DOMOTICZ
    snprintf_P(svalue, sizeof(svalue), PSTR("%s, DomoticzInTopic, DomoticzOutTopic, DomoticzIdx, DomoticzKeyIdx, DomoticzUpdateTimer"), svalue);
#endif  // USE_DOMOTICZ
#endif  // USE_MQTT
#ifdef USE_WALL_SWITCH
    snprintf_P(svalue, sizeof(svalue), PSTR("%s, SwitchMode"), svalue);
#endif
#ifdef SEND_TELEMETRY_I2C
    snprintf_P(svalue, sizeof(svalue), PSTR("%s, I2CScan"), svalue);
#endif
    snprintf_P(svalue, sizeof(svalue), PSTR("%s\"}"), svalue);

#ifdef USE_POWERMONITOR
    if (sysCfg.message_format != JSON) json2legacy(stopic, svalue);
    mqtt_publish(stopic, svalue);
    snprintf_P(svalue, sizeof(svalue), PSTR("{\"Commands4\":\"PowerLow, PowerHigh, VoltageLow, VoltageHigh, CurrentLow, CurrentHigh"));
#ifdef FEATURE_POWER_LIMIT
    snprintf_P(svalue, sizeof(svalue), PSTR("%s, SafePower, SafePowerHold, SafePowerWindow, MaxPower, MaxPowerHold, MaxPowerWindow, MaxEnergy, MaxEnergyStart"), svalue);
#endif  // FEATURE_POWER_LIMIT
    snprintf_P(svalue, sizeof(svalue), PSTR("%s\"}"), svalue);
#endif  // USE_POWERMONITOR
  }
  if (sysCfg.message_format != JSON) json2legacy(stopic, svalue);
  mqtt_publish(stopic, svalue);
}

/********************************************************************************************/

#ifdef USE_MQTT
void send_button_power(byte device, byte state)
{
  char stopic[TOPSZ], svalue[TOPSZ], stemp1[10];

  if (device > Maxdevice) device = 1;
  snprintf_P(stemp1, sizeof(stemp1), PSTR("%d"), device);
  snprintf_P(stopic, sizeof(stopic), PSTR("%s/%s/%s%s"),
    SUB_PREFIX, sysCfg.mqtt_topic2, sysCfg.mqtt_subtopic, (Maxdevice > 1) ? stemp1 : "");
  
  if (state == 3) {
    svalue[0] = '\0';
  } else {
    if (!strcmp(sysCfg.mqtt_topic,sysCfg.mqtt_topic2) && (state == 2)) {
      state = ~(power >> (device -1)) & 0x01;
    }
    snprintf_P(svalue, sizeof(svalue), PSTR("%s"), (state) ? (state == 2) ? MQTT_CMND_TOGGLE : MQTT_STATUS_ON : MQTT_STATUS_OFF);
  }
#ifdef USE_DOMOTICZ
  if (sysCfg.domoticz_key_idx[device -1] && strlen(svalue)) {
    strlcpy(stopic, sysCfg.domoticz_in_topic, sizeof(stopic));
    snprintf_P(svalue, sizeof(svalue), PSTR("{\"command\":\"switchlight\", \"idx\":%d, \"switchcmd\":\"%s\"}"),
      sysCfg.domoticz_key_idx[device -1], (state) ? (state == 2) ? "Toggle" : "On" : "Off");
    mqtt_publish(stopic, svalue);
  } else {
    mqtt_publish_sec(stopic, svalue, sysCfg.mqtt_button_retain);
  }
#else
  mqtt_publish_sec(stopic, svalue, sysCfg.mqtt_button_retain);
#endif  // USE_DOMOTICZ
}
#endif  // USE_MQTT

void do_cmnd_power(byte device, byte state)
{
// device  = Relay number 1 and up
// state 0 = Relay Off
// state 1 = Relay on (turn off after sysCfg.pulsetime * 100 mSec if enabled)
// state 2 = Toggle relay
// state 3 = Blink relay
// state 4 = Stop blinking relay
// state 9 = Show power state

  if ((device < 1) || (device > Maxdevice)) device = 1;
  byte mask = 0x01 << (device -1);
  pulse_timer = 0;
  if (state <= 2) {
    if ((blink_mask & mask)) {
      blink_mask &= (0xFF ^ mask);  // Clear device mask
      mqtt_publishPowerBlinkState(device);
    }
    switch (state) {
    case 0: { // Off
      power &= (0xFF ^ mask);
      break; }
    case 1: // On
      power |= mask;
      break;
    case 2: // Toggle
      power ^= mask;
    }
    setRelay(power);
#ifdef USE_MQTT
#ifdef USE_DOMOTICZ
    if (domoticz_update_flag) mqtt_publishDomoticzPowerState(device);
    domoticz_update_flag = 1;
#endif  // USE_DOMOTICZ
#endif  // USE_MQTT
    if (device == 1) pulse_timer = (power & mask) ? sysCfg.pulsetime : 0;
  }
  else if (state == 3) { // Blink
    if (!(blink_mask & mask)) {
      blink_powersave = (blink_powersave & (0xFF ^ mask)) | (power & mask);  // Save state
      blink_power = (power >> (device -1))&1;  // Prep to Toggle
    }
    blink_timer = 1;
    blink_counter = ((!sysCfg.blinkcount) ? 64000 : (sysCfg.blinkcount *2)) +1;
    blink_mask |= mask;  // Set device mask
    mqtt_publishPowerBlinkState(device);
    return;
  }
  else if (state == 4) { // No Blink
    byte flag = (blink_mask & mask);
    blink_mask &= (0xFF ^ mask);  // Clear device mask
    mqtt_publishPowerBlinkState(device);
    if (flag) do_cmnd_power(device, (blink_powersave >> (device -1))&1);  // Restore state
    return;
  }
  mqtt_publishPowerState(device);
}

void stop_all_power_blink()
{
  byte i, mask;

  for (i = 1; i <= Maxdevice; i++) {
    mask = 0x01 << (i -1);
    if (blink_mask & mask) {
      blink_mask &= (0xFF ^ mask);  // Clear device mask
      mqtt_publishPowerBlinkState(i);
      do_cmnd_power(i, (blink_powersave >> (i -1))&1);  // Restore state
    }
  }
}

void do_cmnd(char *cmnd)
{
  char stopic[TOPSZ], svalue[128];
  char *start;
  char *token;

  token = strtok(cmnd, " ");
  if (token != NULL) {
    start = strrchr(token, '/');   // Skip possible cmnd/sonoff/ preamble
    if (start) token = start;
  }
  snprintf_P(stopic, sizeof(stopic), PSTR("%s/%s/%s"), SUB_PREFIX, sysCfg.mqtt_topic, token);
  token = strtok(NULL, "");
  snprintf_P(svalue, sizeof(svalue), PSTR("%s"), (token == NULL) ? "" : token);
  mqttDataCb(stopic, (byte*)svalue, strlen(svalue));
}

void publish_status(uint8_t payload)
{
  char stopic[TOPSZ], svalue[MESSZ], stemp1[TOPSZ], stemp2[10], stemp3[10];
  float ped, pi, pc;
  uint16_t pe, pw, pu;

  // Workaround MQTT - TCP/IP stack queueing when SUB_PREFIX = PUB_PREFIX
  snprintf_P(stopic, sizeof(stopic), PSTR("%s/%s/RESULT"),
    (!strcmp(SUB_PREFIX,PUB_PREFIX) && (!payload))?PUB_PREFIX2:PUB_PREFIX, sysCfg.mqtt_topic);

#ifndef USE_MQTT
  if (payload == 6) payload = 99;
#endif  // USE_MQTT

  if ((payload == 0) || (payload == 99)) {
    if (sysCfg.message_format == JSON) {
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"Status\":{\"Model\":%d, \"FriendlyName\":\"%s\", \"Topic\":\"%s\", \"ButtonTopic\":\"%s\", \"Subtopic\":\"%s\", \"Power\":%d, \"PowerOnState\":%d, \"LedState\":%d, \"SaveData\":%d, \"SaveState\":%d, \"ButtonRetain\":%d, \"PowerRetain\":%d}}"),
        sysCfg.model, sysCfg.friendlyname, sysCfg.mqtt_topic, sysCfg.mqtt_topic2, sysCfg.mqtt_subtopic, power, sysCfg.poweronstate, sysCfg.ledstate, sysCfg.savedata, sysCfg.savestate, sysCfg.mqtt_button_retain, sysCfg.mqtt_power_retain);
    } else {
      snprintf_P(svalue, sizeof(svalue), PSTR("%s, %d, %s, %s, %s, %s, %d, %d, %d, %d, %d, %d, %d"),
        Version, sysCfg.model, sysCfg.friendlyname, sysCfg.mqtt_topic, sysCfg.mqtt_topic2, sysCfg.mqtt_subtopic, power, sysCfg.poweronstate, sysCfg.ledstate, sysCfg.savedata, sysCfg.savestate, sysCfg.mqtt_button_retain, sysCfg.mqtt_power_retain);
    }
    if (payload == 0) mqtt_publish(stopic, svalue);
  }

#ifdef USE_POWERMONITOR
  if ((payload == 0) || (payload == 8)) {
    hlw_readEnergy(0, ped, pe, pw, pu, pi, pc);
    dtostrf(pi, 1, 3, stemp1);
    dtostrf(ped, 1, 3, stemp2);
    dtostrf(pc, 1, 2, stemp3);
    if (sysCfg.message_format == JSON) {
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"StatusPWR\":{\"Voltage\":%d, \"Current\":\"%s\", \"Power\":%d, \"Today\":\"%s\", \"Factor\":\"%s\"}}"),
        pu, stemp1, pw, stemp2, stemp3);
    } else {
      snprintf_P(svalue, sizeof(svalue), PSTR("PWR: Voltage %d V, Current %s A, Power %d W, Today %s kWh, Factor %s"),
        pu, stemp1, pw, stemp2, stemp3);
    }
    if (payload == 0) mqtt_publish(stopic, svalue);
  }

  if ((payload == 0) || (payload == 9)) {
    if (sysCfg.message_format == JSON) {
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"StatusPWRThreshold\":{\"PowerLow\":%d, \"PowerHigh\":%d, \"VoltageLow\":%d, \"VoltageHigh\":%d, \"CurrentLow\":%d, \"CurrentHigh\":%d}}"),
        sysCfg.hlw_pmin, sysCfg.hlw_pmax, sysCfg.hlw_umin, sysCfg.hlw_umax, sysCfg.hlw_imin, sysCfg.hlw_imax);
    } else {
      snprintf_P(svalue, sizeof(svalue), PSTR("PWR Threshold: PowerLow %d W, PowerHigh %d W, VoltageLow %d V, VoltageHigh %d V, CurrentLow %d mA, CurrentHigh %d mA"),
        sysCfg.hlw_pmin, sysCfg.hlw_pmax, sysCfg.hlw_umin, sysCfg.hlw_umax, sysCfg.hlw_imin, sysCfg.hlw_imax);
    }
    if (payload == 0) mqtt_publish(stopic, svalue);
  }
#endif  // USE_POWERMONITOR

  if ((payload == 0) || (payload == 1)) {
    if (sysCfg.message_format == JSON) {
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"StatusPRM\":{\"Baudrate\":%d, \"GroupTopic\":\"%s\", \"OtaUrl\":\"%s\", \"Uptime\":%d, \"BootCount\":%d, \"SaveCount\":%d}}"),
        Baudrate, sysCfg.mqtt_grptopic, sysCfg.otaUrl, uptime, sysCfg.bootcount, sysCfg.saveFlag);
    } else {
      snprintf_P(svalue, sizeof(svalue), PSTR("PRM: Baudrate %d, GroupTopic %s, OtaUrl %s, Uptime %d Hr, BootCount %d, SaveCount %d"),
        Baudrate, sysCfg.mqtt_grptopic, sysCfg.otaUrl, uptime, sysCfg.bootcount, sysCfg.saveFlag);
    }
    if (payload == 0) mqtt_publish(stopic, svalue);
  }

  if ((payload == 0) || (payload == 2)) {
    if (sysCfg.message_format == JSON) {
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"StatusFWR\":{\"Program\":\"%s\", \"Boot\":%d, \"SDK\":\"%s\"}}"),
        Version, ESP.getBootVersion(), ESP.getSdkVersion());
    } else {
      snprintf_P(svalue, sizeof(svalue), PSTR("FWR: Program %s, Boot %d, SDK %s"),
        Version, ESP.getBootVersion(), ESP.getSdkVersion());
    }
    if (payload == 0) mqtt_publish(stopic, svalue);
  }

  if ((payload == 0) || (payload == 3)) {
    if (sysCfg.message_format == JSON) {
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"StatusLOG\":{\"Seriallog\":%d, \"Weblog\":%d, \"Syslog\":%d, \"LogHost\":\"%s\", \"SSId1\":\"%s\", \"SSId2\":\"%s\", \"TelePeriod\":%d}}"),
        sysCfg.seriallog_level, sysCfg.weblog_level, sysCfg.syslog_level, sysCfg.syslog_host, sysCfg.sta_ssid[0], sysCfg.sta_ssid[1], sysCfg.tele_period);
    } else {
      snprintf_P(svalue, sizeof(svalue), PSTR("LOG: Seriallog %d, Weblog %d, Syslog %d, LogHost %s, SSId1 %s, SSId2 %s, TelePeriod %d"),
        sysCfg.seriallog_level, sysCfg.weblog_level, sysCfg.syslog_level, sysCfg.syslog_host, sysCfg.sta_ssid[0], sysCfg.sta_ssid[1], sysCfg.tele_period);
    }
    if (payload == 0) mqtt_publish(stopic, svalue);
  }

  if ((payload == 0) || (payload == 4)) {
    if (sysCfg.message_format == JSON) {
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"StatusMEM\":{\"ProgramSize\":%d, \"Free\":%d, \"Heap\":%d, \"SpiffsStart\":%d, \"SpiffsSize\":%d, \"FlashSize\":%d, \"ProgramFlashSize\":%d}}"),
        ESP.getSketchSize()/1024, ESP.getFreeSketchSpace()/1024, ESP.getFreeHeap()/1024, ((uint32_t)&_SPIFFS_start - 0x40200000)/1024,
        (((uint32_t)&_SPIFFS_end - 0x40200000) - ((uint32_t)&_SPIFFS_start - 0x40200000))/1024, ESP.getFlashChipRealSize()/1024, ESP.getFlashChipSize()/1024);
    } else {
      snprintf_P(svalue, sizeof(svalue), PSTR("MEM: ProgramSize %dkB, Free %dkB (Heap %dkB), SpiffsStart %dkB (%dkB), FlashSize %dkB (%dkB)"),
        ESP.getSketchSize()/1024, ESP.getFreeSketchSpace()/1024, ESP.getFreeHeap()/1024, ((uint32_t)&_SPIFFS_start - 0x40200000)/1024,
        (((uint32_t)&_SPIFFS_end - 0x40200000) - ((uint32_t)&_SPIFFS_start - 0x40200000))/1024, ESP.getFlashChipRealSize()/1024, ESP.getFlashChipSize()/1024);
    }
    if (payload == 0) mqtt_publish(stopic, svalue);
  }

  if ((payload == 0) || (payload == 5)) {
    if (sysCfg.message_format == JSON) {
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"StatusNET\":{\"Host\":\"%s\", \"IP\":\"%s\", \"Gateway\":\"%s\", \"Subnetmask\":\"%s\", \"Mac\":\"%s\", \"Webserver\":%d, \"WifiConfig\":%d}}"),
        Hostname, WiFi.localIP().toString().c_str(), WiFi.gatewayIP().toString().c_str(), WiFi.subnetMask().toString().c_str(),
        WiFi.macAddress().c_str(), sysCfg.webserver, sysCfg.sta_config);
    } else {
      snprintf_P(svalue, sizeof(svalue), PSTR("NET: Host %s, IP %s, Gateway %s, Subnetmask %s, Mac %s, Webserver %d, WifiConfig %d"),
        Hostname, WiFi.localIP().toString().c_str(), WiFi.gatewayIP().toString().c_str(), WiFi.subnetMask().toString().c_str(),
        WiFi.macAddress().c_str(), sysCfg.webserver, sysCfg.sta_config);
    }
    if (payload == 0) mqtt_publish(stopic, svalue);
  }

#ifdef USE_MQTT
  if ((payload == 0) || (payload == 6)) {
    if (sysCfg.message_format == JSON) {
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"StatusMQT\":{\"Host\":\"%s\", \"Port\":%d, \"ClientMask\":\"%s\", \"Client\":\"%s\", \"User\":\"%s\", \"MAX_PACKET_SIZE\":%d, \"KEEPALIVE\":%d}}"),
        sysCfg.mqtt_host, sysCfg.mqtt_port, sysCfg.mqtt_client, MQTTClient, sysCfg.mqtt_user, MQTT_MAX_PACKET_SIZE, MQTT_KEEPALIVE);
    } else {
      snprintf_P(svalue, sizeof(svalue), PSTR("MQT: Host %s, Port %d, Client %s (%s), User %s, MAX_PACKET_SIZE %d, KEEPALIVE %d"),
        sysCfg.mqtt_host, sysCfg.mqtt_port, sysCfg.mqtt_client, MQTTClient, sysCfg.mqtt_user, MQTT_MAX_PACKET_SIZE, MQTT_KEEPALIVE);
    }
    if (payload == 0) mqtt_publish(stopic, svalue);
  }
#endif  // USE_MQTT

  if ((payload == 0) || (payload == 7)) {
    if (sysCfg.message_format == JSON) {
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"StatusTIM\":{\"UTC\":\"%s\", \"Local\":\"%s\", \"StartDST\":\"%s\", \"EndDST\":\"%s\", \"Timezone\":%d}}"),
        rtc_time(0).c_str(), rtc_time(1).c_str(), rtc_time(2).c_str(), rtc_time(3).c_str(), sysCfg.timezone);
    } else {
      snprintf_P(svalue, sizeof(svalue), PSTR("TIM: UTC %s, Local %s, StartDST %s, EndDST %s, Timezone %d"),
        rtc_time(0).c_str(), rtc_time(1).c_str(), rtc_time(2).c_str(), rtc_time(3).c_str(), sysCfg.timezone);
    }
  }
  mqtt_publish(stopic, svalue);
}

/********************************************************************************************/

void every_second_cb()
{
  // 1 second rtc interrupt routine
  // Keep this code small (every_second is to large - it'll trip exception)

#ifdef FEATURE_POWER_LIMIT
  if (rtcTime.Valid) {
    if (rtc_loctime() == rtc_midnight()) {
      hlw_mkwh_state = 3;
    }
    if ((rtcTime.Hour == sysCfg.hlw_mkwhs) && (hlw_mkwh_state == 3)) {
      hlw_mkwh_state = 0;
    }
  }
#endif  // FEATURE_POWER_LIMIT
}

void every_second()
{
  char log[LOGSZ], stopic[TOPSZ], svalue[MESSZ], stime[21];

  snprintf_P(stime, sizeof(stime), PSTR("%04d-%02d-%02dT%02d:%02d:%02d"),
    rtcTime.Year, rtcTime.Month, rtcTime.Day, rtcTime.Hour, rtcTime.Minute, rtcTime.Second);

  if (pulse_timer > 111) pulse_timer--;

  if (syslog_timer) {  // Restore syslog level
    syslog_timer--;
    if (!syslog_timer) {
      syslog_level = sysCfg.syslog_level;
      if (sysCfg.syslog_level) {
        addLog_P(LOG_LEVEL_INFO, PSTR("SYSL: Syslog logging re-enabled"));  // Might trigger disable again (on purpose)
      }
    }
  }

#ifdef USE_MQTT
#ifdef USE_DOMOTICZ
  if ((sysCfg.domoticz_update_timer || domoticz_update_timer) && sysCfg.domoticz_relay_idx[0]) {
    domoticz_update_timer--;
    if (domoticz_update_timer <= 0) {
      domoticz_update_timer = sysCfg.domoticz_update_timer;
      for (byte i = 1; i <= Maxdevice; i++) mqtt_publishDomoticzPowerState(i);
    }
  }
#endif  // USE_DOMOTICZ
#endif  // USE_MQTT

  if (status_update_timer) {
    status_update_timer--;
    if (!status_update_timer) {
      for (byte i = 1; i <= Maxdevice; i++) mqtt_publishPowerState(i);
    }
  }

  if (sysCfg.tele_period) {
    tele_period++;
    if (tele_period == sysCfg.tele_period -1) {

#ifdef SEND_TELEMETRY_DS18B20
      dsb_readTempPrep();
#endif  // SEND_TELEMETRY_DS18B20

#ifdef SEND_TELEMETRY_DS18x20
      ds18x20_search();      // Check for changes in sensors number
      ds18x20_convert();     // Start Conversion, takes up to one second
#endif  // SEND_TELEMETRY_DS18x20

#ifdef SEND_TELEMETRY_DHT
      dht_readPrep();
#endif  // SEND_TELEMETRY_DHT

#ifdef SEND_TELEMETRY_I2C
      htu_detect();
      bmp_detect();
      bh1750_detect();
#endif // SEND_TELEMETRY_I2C

    }
    if (tele_period >= sysCfg.tele_period) {
      tele_period = 0;

      if (sysCfg.message_format == JSON) {
        snprintf_P(stopic, sizeof(stopic), PSTR("%s/%s/TELEMETRY"), PUB_PREFIX2, sysCfg.mqtt_topic);
        snprintf_P(svalue, sizeof(svalue), PSTR("{\"Time\":\"%s\", \"Uptime\":%d"), stime, uptime);
        for (byte i = 0; i < Maxdevice; i++) {
          if (Maxdevice == 1) {  // Legacy
            snprintf_P(svalue, sizeof(svalue), PSTR("%s, \"%s\":"), svalue, sysCfg.mqtt_subtopic);
          } else {
            snprintf_P(svalue, sizeof(svalue), PSTR("%s, \"%s%d\":"), svalue, sysCfg.mqtt_subtopic, i +1);
          }
          snprintf_P(svalue, sizeof(svalue), PSTR("%s\"%s\""), svalue, (power & (0x01 << i)) ? MQTT_STATUS_ON : MQTT_STATUS_OFF);
        }
        snprintf_P(svalue, sizeof(svalue), PSTR("%s, \"Wifi\":{\"AP\":%d, \"SSID\":\"%s\", \"RSSI\":%d}}"),
          svalue, sysCfg.sta_active +1, sysCfg.sta_ssid[sysCfg.sta_active], WIFI_getRSSIasQuality(WiFi.RSSI()));
        mqtt_publish(stopic, svalue);
      } else {
#ifdef SEND_TELEMETRY_POWER
        for (byte i = 0; i < Maxdevice; i++) {
          if (Maxdevice == 1) {  // Legacy
            snprintf_P(stopic, sizeof(stopic), PSTR("%s/%s/%s"), PUB_PREFIX2, sysCfg.mqtt_topic, sysCfg.mqtt_subtopic);
          } else {
            snprintf_P(stopic, sizeof(stopic), PSTR("%s/%s/%s%d"), PUB_PREFIX2, sysCfg.mqtt_topic, sysCfg.mqtt_subtopic, i +1);
          }
          strlcpy(svalue, (power & (0x01 << i)) ? MQTT_STATUS_ON : MQTT_STATUS_OFF, sizeof(svalue));
          mqtt_publish(stopic, svalue);
        }
#endif  // SEND_TELEMETRY_POWER
#ifdef SEND_TELEMETRY_UPTIME
        snprintf_P(stopic, sizeof(stopic), PSTR("%s/%s/UPTIME"), PUB_PREFIX2, sysCfg.mqtt_topic);
        snprintf_P(svalue, sizeof(svalue), PSTR("%d%s"), uptime, (sysCfg.mqtt_units) ? " Hr" : "");
        mqtt_publish(stopic, svalue);
#endif  // SEND_TELEMETRY_UPTIME
#ifdef SEND_TELEMETRY_WIFI
        snprintf_P(stopic, sizeof(stopic), PSTR("%s/%s/AP"), PUB_PREFIX2, sysCfg.mqtt_topic);
        snprintf_P(svalue, sizeof(svalue), PSTR("%d"), sysCfg.sta_active +1);
        mqtt_publish(stopic, svalue);
        snprintf_P(stopic, sizeof(stopic), PSTR("%s/%s/SSID"), PUB_PREFIX2, sysCfg.mqtt_topic);
        snprintf_P(svalue, sizeof(svalue), PSTR("%s"), sysCfg.sta_ssid[sysCfg.sta_active]);
        mqtt_publish(stopic, svalue);
        snprintf_P(stopic, sizeof(stopic), PSTR("%s/%s/RSSI"), PUB_PREFIX2, sysCfg.mqtt_topic);
        snprintf_P(svalue, sizeof(svalue), PSTR("%d%s"), WIFI_getRSSIasQuality(WiFi.RSSI()), (sysCfg.mqtt_units) ? " %" : "");
        mqtt_publish(stopic, svalue);
#endif  // SEND_TELEMETRY_WIFI
      }

      uint8_t djson = 0;
      if (sysCfg.message_format == JSON) {
        snprintf_P(svalue, sizeof(svalue), PSTR("{\"Time\":\"%s\""), stime);
      }
#ifdef SEND_TELEMETRY_DS18B20
      dsb_mqttPresent(stopic, sizeof(stopic), svalue, sizeof(svalue), &djson);
#endif  // SEND_TELEMETRY_DS18B20
#ifdef SEND_TELEMETRY_DS18x20
      ds18x20_mqttPresent(stopic, sizeof(stopic), svalue, sizeof(svalue), &djson);
#endif  // SEND_TELEMETRY_DS18x20
#if defined(SEND_TELEMETRY_DHT) || defined(SEND_TELEMETRY_DHT2)
      dht_mqttPresent(stopic, sizeof(stopic), svalue, sizeof(svalue), &djson);
#endif  // SEND_TELEMETRY_DHT/2
#ifdef SEND_TELEMETRY_I2C
      htu_mqttPresent(stopic, sizeof(stopic), svalue, sizeof(svalue), &djson);
      bmp_mqttPresent(stopic, sizeof(stopic), svalue, sizeof(svalue), &djson);
      bh1750_mqttPresent(stopic, sizeof(stopic), svalue, sizeof(svalue), &djson);
#endif // SEND_TELEMETRY_I2C
      if (djson) {
        snprintf_P(svalue, sizeof(svalue), PSTR("%s}"), svalue);
        mqtt_publish(stopic, svalue);
      }

#ifdef USE_POWERMONITOR
#ifdef SEND_TELEMETRY_ENERGY
      hlw_mqttPresent(stopic, sizeof(stopic), svalue, sizeof(svalue), &djson);
#endif  // SEND_TELEMETRY_ENERGY
#endif  // USE_POWERMONITOR

      if (sysCfg.message_format != JSON) {
        snprintf_P(stopic, sizeof(stopic), PSTR("%s/%s/TIME"), PUB_PREFIX2, sysCfg.mqtt_topic);
        snprintf_P(svalue, sizeof(svalue), PSTR("%s"), stime);
        mqtt_publish(stopic, svalue);
      }
    }
  }

#ifdef USE_POWERMONITOR
  hlw_margin_chk();
#endif  // USE_POWERMONITOR

  if ((rtcTime.Minute == 2) && (rtcTime.Second == 30)) {
    uptime++;
    if (sysCfg.message_format == JSON) {
      snprintf_P(stopic, sizeof(stopic), PSTR("%s/%s/TELEMETRY"), PUB_PREFIX2, sysCfg.mqtt_topic);
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"Time\":\"%s\", \"Uptime\":%d}"), stime, uptime);
    } else {
      snprintf_P(stopic, sizeof(stopic), PSTR("%s/%s/UPTIME"), PUB_PREFIX2, sysCfg.mqtt_topic);
      snprintf_P(svalue, sizeof(svalue), PSTR("%d%s"), uptime, (sysCfg.mqtt_units) ? " Hr" : "");
    }
    mqtt_publish(stopic, svalue);
  }
}

void stateloop()
{
  uint8_t button, flag, switchflag, power_now;
  char scmnd[20], log[LOGSZ], stopic[TOPSZ], svalue[MESSZ];

  timerxs = millis() + (1000 / STATES);
  state++;
  if (state == STATES) {             // Every second
    state = 0;
    every_second();
  }

  if (mqtt_cmnd_publish) mqtt_cmnd_publish--;  // Clean up

  if ((pulse_timer > 0) && (pulse_timer < 112)) {
    pulse_timer--;
    if (!pulse_timer) do_cmnd_power(1, 0);
  }

  if (blink_mask) {
    blink_timer--;
    if (!blink_timer) {
      blink_timer = sysCfg.blinktime;
      blink_counter--;
      if (!blink_counter) {
        stop_all_power_blink();
      } else {
        blink_power ^= 1;
        power_now = (power & (0xFF ^ blink_mask)) | ((blink_power) ? blink_mask : 0);
        setRelay(power_now);
      }
    }
  }

  if ((sysCfg.model >= SONOFF_DUAL) && (sysCfg.model <= CHANNEL_4)) {
    if (ButtonCode) {
      snprintf_P(log, sizeof(log), PSTR("APP: Button code %04X"), ButtonCode);
      addLog(LOG_LEVEL_DEBUG, log);
      button = PRESSED;
      if (ButtonCode == 0xF500) holdcount = (STATES *4) -1;
      ButtonCode = 0;
    } else {
      button = NOT_PRESSED;
    }
  } else {
    button = digitalRead(KEY_PIN);
  }
  if ((button == PRESSED) && (lastbutton == NOT_PRESSED)) {
    multipress = (multiwindow) ? multipress +1 : 1;
    snprintf_P(log, sizeof(log), PSTR("APP: Multipress %d"), multipress);
    addLog(LOG_LEVEL_DEBUG, log);
    blinks = 201;
    multiwindow = STATES /2;         // 1/2 second multi press window
  }
  lastbutton = button;
  if (button == NOT_PRESSED) {
    holdcount = 0;
  } else {
    holdcount++;
    if (holdcount == (STATES *4)) {  // 4 seconds button hold
      snprintf_P(scmnd, sizeof(scmnd), PSTR("reset 1"));
      multipress = 0;
      do_cmnd(scmnd);
    }
  }
  if (multiwindow) {
    multiwindow--;
  } else {
    if ((!restartflag) && (!holdcount) && (multipress > 0) && (multipress < MAX_BUTTON_COMMANDS +3)) {
      if ((sysCfg.model >= SONOFF_DUAL) && (sysCfg.model <= CHANNEL_4)) {
        flag = ((multipress == 1) || (multipress == 2));
      } else  {
        flag = (multipress == 1);
      }
#ifdef USE_MQTT
      if (flag && mqttClient.connected() && strcmp(sysCfg.mqtt_topic2, "0")) {
        send_button_power(multipress, 2);  // Execute command via MQTT using ButtonTopic to sync external clients
      } else
#endif  // USE_MQTT
      {
        if ((multipress == 1) || (multipress == 2)) {
          if (WIFI_State()) {  // WPSconfig, Smartconfig or Wifimanager active
            restartflag = 1;
          } else {
            do_cmnd_power(multipress, 2);    // Execute command internally
          }
        } else {
          snprintf_P(scmnd, sizeof(scmnd), commands[multipress -3]);
          do_cmnd(scmnd);
        }
      }
      multipress = 0;
    }
  }

#ifdef KEY2_PIN
  if (Maxdevice > 1) {
    button = digitalRead(KEY2_PIN);
    if ((button == PRESSED) && (lastbutton2 == NOT_PRESSED)) {
      if (mqttClient.connected() && strcmp(sysCfg.mqtt_topic2, "0")) {
        send_button_power(2, 2);   // Execute commend via MQTT
      } else {
        do_cmnd_power(2, 2);       // Execute command internally
      }
    }
    lastbutton2 = button;
  }
#endif

#ifdef KEY3_PIN
  if (Maxdevice > 2) {
    button = digitalRead(KEY3_PIN);
     if ((button == PRESSED) && (lastbutton3 == NOT_PRESSED)) {
      if (mqttClient.connected() && strcmp(sysCfg.mqtt_topic2, "0")) {
        send_button_power(3, 2);   // Execute commend via MQTT
      } else {
        do_cmnd_power(3, 2);       // Execute command internally
      }
    }
    lastbutton3 = button;
  }
#endif

#ifdef KEY4_PIN
  if (Maxdevice > 3) {
    button = digitalRead(KEY4_PIN);
    if ((button == PRESSED) && (lastbutton4 == NOT_PRESSED)) {
      if (mqttClient.connected() && strcmp(sysCfg.mqtt_topic2, "0")) {
        send_button_power(4, 2);   // Execute commend via MQTT
      } else {
        do_cmnd_power(4, 2);       // Execute command internally
      }
    }
    lastbutton4 = button;
  }
#endif

#ifdef USE_WALL_SWITCH
  button = digitalRead(SWITCH_PIN);
  if (button != lastwallswitch) {
    switchflag = 3;
    switch (sysCfg.switchmode) {
    case TOGGLE:
      switchflag = 2;                // Toggle
      break;
    case FOLLOW:
      switchflag = button & 0x01;    // Follow wall switch state
      break;
    case FOLLOW_INV:
      switchflag = ~button & 0x01;   // Follow inverted wall switch state
      break;
    case PUSHBUTTON:
      if ((button == PRESSED) && (lastwallswitch == NOT_PRESSED)) switchflag = 2;  // Toggle with pushbutton to Gnd
      break;
    case PUSHBUTTON_INV:
      if ((button == NOT_PRESSED) && (lastwallswitch == PRESSED)) switchflag = 2;  // Toggle with releasing pushbutton from Gnd
    }
    if (switchflag < 3) {
      if (mqttClient.connected() && strcmp(sysCfg.mqtt_topic2,"0")) {
        send_button_power(1, switchflag);   // Execute commend via MQTT
      } else {
        do_cmnd_power(1, switchflag);       // Execute command internally
      }
    }
    lastwallswitch = button;
  }
#endif // USE_WALL_SWITCH

  if (!(state % ((STATES/10)*2))) {
    if (blinks || restartflag || otaflag) {
      if (restartflag || otaflag) {
        blinkstate = 1;   // Stay lit
      } else {
        blinkstate ^= 1;  // Blink
      }
      if ((sysCfg.ledstate &0x06) || (blinks > 200) || (blinkstate)) {
        digitalWrite(LED_PIN, (LED_INVERTED) ? !blinkstate : blinkstate);
      }
      if (!blinkstate) {
        blinks--;
        if (blinks == 200) blinks = 0;
      }
    } else {
      if (sysCfg.ledstate &0x01) {
        digitalWrite(LED_PIN, (LED_INVERTED) ? !power : power);
      }
    }
  }

  switch (state) {
  case (STATES/10)*2:
    if (otaflag) {
      otaflag--;
      if (otaflag <= 0) {
        otaflag = 12;
        ESPhttpUpdate.rebootOnUpdate(false);
        // Try multiple times to get the update, in case we have a transient issue.
        // e.g. Someone issued "cmnd/sonoffs/update 1" and all the devices
        //      are hammering the OTAURL.
        for (byte i = 0; i < OTA_ATTEMPTS && !otaok; i++) {
          // Delay an increasing pseudo-random time for each interation.
          // Starting at 0 (no delay) up to a maximum of OTA_ATTEMPTS-1 seconds.
          delay((ESP.getChipId() % 1000) * i);
          otaok = (ESPhttpUpdate.update(sysCfg.otaUrl) == HTTP_UPDATE_OK);
        }
      }
      if (otaflag == 10) {  // Allow MQTT to reconnect
        otaflag = 0;
        snprintf_P(stopic, sizeof(stopic), PSTR("%s/%s/UPGRADE"), PUB_PREFIX, sysCfg.mqtt_topic);
        if (otaok) {
          snprintf_P(svalue, sizeof(svalue), PSTR("Successful. Restarting"));
          restartflag = 2;
        } else {
          snprintf_P(svalue, sizeof(svalue), PSTR("Failed %s"), ESPhttpUpdate.getLastErrorString().c_str());
        }
        mqtt_publish(stopic, svalue);
      }
    }
    break;
  case (STATES/10)*4:
    if (savedatacounter) {
      savedatacounter--;
      if (savedatacounter <= 0) {
        if (sysCfg.savestate) {
          if (!((sysCfg.pulsetime > 0) && (sysCfg.pulsetime < 30) && ((sysCfg.power &0xFE) == (power &0xFE)))) sysCfg.power = power;
        }
        CFG_Save();
        savedatacounter = sysCfg.savedata;
      }
    }
    if (restartflag) {
      if (restartflag == 211) {
        CFG_Default();
        restartflag = 2;
      }
      if (restartflag == 212) {
        CFG_Erase();
        CFG_Default();
        restartflag = 2;
      }
      if (sysCfg.savestate) sysCfg.power = power;

#ifdef USE_POWERMONITOR
      hlw_savestate();
#endif  // USE_POWERMONITOR

      CFG_Save();
      restartflag--;
      if (restartflag <= 0) {
        addLog_P(LOG_LEVEL_INFO, PSTR("APP: Restarting"));
        ESP.restart();
      }
    }
    break;
  case (STATES/10)*6:
    WIFI_Check(wificheckflag);
    wificheckflag = WIFI_RESTART;
    break;
  case (STATES/10)*8:
#ifdef USE_MQTT
    if ((WiFi.status() == WL_CONNECTED) && (!mqttClient.connected())) {
      if (!mqttcounter) {
        mqtt_reconnect();
      } else {
        mqttcounter--;
      }
    }
#else
    if (WiFi.status() == WL_CONNECTED) {
      if (!mqttcounter) {
        mqtt_reconnect();
      }
    }
#endif  // USE_MQTT
    break;
  }
}

void serial()
{
  char log[LOGSZ];

  while (Serial.available()) {
    yield();
    SerialInByte = Serial.read();

    // Sonoff dual 19200 baud serial interface
    if (Hexcode) {
      Hexcode--;
      if (Hexcode) {
        ButtonCode = (ButtonCode << 8) | SerialInByte;
        SerialInByte = 0;
      } else {
        if (SerialInByte != 0xA1) ButtonCode = 0;  // 0xA1 - End of Sonoff dual button code
      }
    }
    if (SerialInByte == 0xA0) {                    // 0xA0 - Start of Sonoff dual button code
      SerialInByte = 0;
      ButtonCode = 0;
      Hexcode = 3;
    }

    if (SerialInByte > 127) { // binary data...
      SerialInByteCounter = 0;
      Serial.flush();
      return;
    }
    if (isprint(SerialInByte)) {
      if (SerialInByteCounter < INPUT_BUFFER_SIZE) {  // add char to string if it still fits
        serialInBuf[SerialInByteCounter++] = SerialInByte;
      } else {
        SerialInByteCounter = 0;
      }
    }
    if (SerialInByte == '\n') {
      serialInBuf[SerialInByteCounter] = 0;  // serial data completed
      if (sysCfg.seriallog_level < LOG_LEVEL_INFO) sysCfg.seriallog_level = LOG_LEVEL_INFO;
      snprintf_P(log, sizeof(log), PSTR("CMND: %s"), serialInBuf);
      addLog(LOG_LEVEL_INFO, log);
      do_cmnd(serialInBuf);
      SerialInByteCounter = 0;
      Serial.flush();
      return;
    }
  }
}

/********************************************************************************************/

void setup()
{
  char log[LOGSZ];
  byte idx;

  Serial.begin(Baudrate);
  delay(10);
  Serial.println();
  sysCfg.seriallog_level = LOG_LEVEL_INFO;  // Allow specific serial messages until config loaded

  snprintf_P(Version, sizeof(Version), PSTR("%d.%d.%d"), VERSION >> 24 & 0xff, VERSION >> 16 & 0xff, VERSION >> 8 & 0xff);
  if (VERSION & 0x1f) {
    idx = strlen(Version);
    Version[idx] = 96 + (VERSION & 0x1f);
    Version[idx +1] = 0;
  }
  if (!spiffsPresent())
    addLog_P(LOG_LEVEL_ERROR, PSTR("SPIFFS: ERROR - No spiffs present. Please reflash with at least 16K SPIFFS"));
#ifdef USE_SPIFFS
  initSpiffs();
#endif
  CFG_Load();
  CFG_Delta();

  if (!sysCfg.model) {
    sysCfg.model = SONOFF;
#if (MODULE == SONOFF)
    pinMode(REL_PIN, INPUT_PULLUP);
    if (digitalRead(REL_PIN)) sysCfg.model = SONOFF_DUAL;
#endif
#if (MODULE == SONOFF_2)
#ifdef REL3_PIN
    pinMode(REL3_PIN, INPUT_PULLUP);
    if (!digitalRead(REL3_PIN)) sysCfg.model = SONOFF_4CH;
#endif
#endif
  }
  Maxdevice = sysCfg.model;  // SONOFF(_2)/MOTOR_CAC (1), SONOFF_DUAL (2), CHANNEL_4 (3) or SONOFF_4CH (4)
  if ((sysCfg.model >= SONOFF_DUAL) && (sysCfg.model <= CHANNEL_4)) {
    Baudrate = 19200;
    Maxdevice = 2;
    if (sysCfg.model == CHANNEL_4) Maxdevice = 4;
  }
  if (MODULE == ELECTRO_DRAGON) Maxdevice = 2;

  if (Serial.baudRate() != Baudrate) {
    snprintf_P(log, sizeof(log), PSTR("APP: Need to change baudrate to %d"), Baudrate);
    addLog(LOG_LEVEL_INFO, log);
    delay(100);
    Serial.flush();
    Serial.begin(Baudrate);
    delay(10);
    Serial.println();
  }

  sysCfg.bootcount++;
  snprintf_P(log, sizeof(log), PSTR("APP: Bootcount %d"), sysCfg.bootcount);
  addLog(LOG_LEVEL_DEBUG, log);
  savedatacounter = sysCfg.savedata;
  syslog_level = sysCfg.syslog_level;

  if (strstr(sysCfg.hostname, "%")) strlcpy(sysCfg.hostname, DEF_WIFI_HOSTNAME, sizeof(sysCfg.hostname));
  if (!strcmp(sysCfg.hostname, DEF_WIFI_HOSTNAME)) {
    snprintf_P(Hostname, sizeof(Hostname)-1, sysCfg.hostname, sysCfg.mqtt_topic, ESP.getChipId() & 0x1FFF);
  } else {
    snprintf_P(Hostname, sizeof(Hostname)-1, sysCfg.hostname);
  }
  WIFI_Connect(Hostname);

  getClient(MQTTClient, sysCfg.mqtt_client, sizeof(MQTTClient));

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, (LED_INVERTED) ? !blinkstate : blinkstate);

  if ((sysCfg.model < SONOFF_DUAL) || (sysCfg.model > CHANNEL_4)) {
    pinMode(KEY_PIN, INPUT_PULLUP);
    pinMode(REL_PIN, OUTPUT);
#ifdef KEY2_PIN
    if (Maxdevice > 1) pinMode(KEY2_PIN, INPUT_PULLUP);
#endif
#ifdef REL2_PIN
    if (Maxdevice > 1) pinMode(REL2_PIN, OUTPUT);
#endif
#ifdef KEY3_PIN
    if (Maxdevice > 2) pinMode(KEY3_PIN, INPUT_PULLUP);
#endif
#ifdef REL3_PIN
    if (Maxdevice > 2) pinMode(REL3_PIN, OUTPUT);
#endif
#ifdef KEY4_PIN
    if (Maxdevice > 3) pinMode(KEY4_PIN, INPUT_PULLUP);
#endif
#ifdef REL4_PIN
    if (Maxdevice > 3) pinMode(REL4_PIN, OUTPUT);
#endif
  }

  if (MODULE == MOTOR_CAC) sysCfg.poweronstate = 1;  // Needs always on else in limbo!
  if (sysCfg.poweronstate == 0) {       // All off
    power = 0;
    setRelay(power);
  }
  else if (sysCfg.poweronstate == 1) {  // All on
    power = ((0x00FF << Maxdevice) >> 8);
    setRelay(power);
  }
  else if (sysCfg.poweronstate == 2) {  // All saved state toggle
    power = (sysCfg.power & ((0x00FF << Maxdevice) >> 8)) ^ 0xFF;
    if (sysCfg.savestate) setRelay(power);
  }
  else {                                // All saved state
    power = sysCfg.power & ((0x00FF << Maxdevice) >> 8);
    if (sysCfg.savestate) setRelay(power);
  }
  blink_powersave = power;

#ifdef USE_WALL_SWITCH
  pinMode(SWITCH_PIN, INPUT_PULLUP);            // set pin to input, fitted with external pull up on Sonoff TH10/16 board
  lastwallswitch = digitalRead(SWITCH_PIN);     // set global now so doesn't change the saved power state on first switch check
#endif  // USE_WALL_SWITCH

#if defined(SEND_TELEMETRY_DHT) || defined(SEND_TELEMETRY_DHT2)
  dht_init();
#endif

#ifdef SEND_TELEMETRY_I2C
  Wire.begin(I2C_SDA_PIN,I2C_SCL_PIN);
#endif // SEND_TELEMETRY_I2C

#ifdef USE_POWERMONITOR
  hlw_init();
#endif  // USE_POWERMONITOR

  rtc_init(every_second_cb);

  snprintf_P(log, sizeof(log), PSTR("APP: Project %s %s (Topic %s, Fallback %s, GroupTopic %s) Version %s"),
    PROJECT, sysCfg.friendlyname, sysCfg.mqtt_topic, MQTTClient, sysCfg.mqtt_grptopic, Version);
  addLog(LOG_LEVEL_INFO, log);
}

void loop()
{
#ifdef USE_WEBSERVER
  pollDnsWeb();
#endif  // USE_WEBSERVER
#ifdef USE_WEMO_EMULATION
  pollUDP();
#endif  // USE_WEMO_EMULATION

  if (millis() >= timerxs) stateloop();

#ifdef USE_MQTT
  mqttClient.loop();
#endif  // USE_MQTT

  if (Serial.available()) serial();

  yield();
}
