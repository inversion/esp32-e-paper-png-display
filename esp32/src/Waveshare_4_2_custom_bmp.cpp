/* Adapted from https://github.com/G6EJD/ESP32-e-Paper-Weather-Display
 * License:
 * This software, the ideas and concepts is Copyright (c) David Bird 2014 and beyond.

All rights to this software are reserved.

It is prohibited to redistribute or reproduce of any part or all of the software contents in any form other than the following:

 1. You may print or download to a local hard disk extracts for your personal and non-commercial use only.

 2. You may copy the content to individual third parties for their personal use, but only if you acknowledge the author David Bird as the source of the material.

 3. You may not, except with my express written permission, distribute or commercially exploit the content.

 4. You may not transmit it or store it in any other website or other form of electronic retrieval system for commercial purposes.

 5. You MUST include all of this copyright and permission notice ('as annotated') and this shall be included in all copies or substantial portions of the software and where the software use is visible to an end-user.

THE SOFTWARE IS PROVIDED "AS IS" FOR PRIVATE USE ONLY, IT IS NOT FOR COMMERCIAL USE IN WHOLE OR PART OR CONCEPT.

FOR PERSONAL USE IT IS SUPPLIED WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.

IN NO EVENT SHALL THE AUTHOR OR COPYRIGHT HOLDER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
*/
#include <Arduino.h>

#include "credentials.h"
#include <WiFi.h> // Built-in
#include "time.h" // Built-in
#include <SPI.h>  // Built-in
#define ENABLE_GxEPD2_display 0
#include <GxEPD2_BW.h>
#include <pngle.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#define SCREEN_WIDTH 400.0 // Set for landscape mode, don't remove the decimal place!
#define SCREEN_HEIGHT 300.0
// Connections for e.g. TT-Display TTGO
// Ref https://github.com/Xinyuan-LilyGO/TTGO-T-Display/issues/14#issuecomment-593701428
// Ref https://github.com/Xinyuan-LilyGO/TTGO-T-Display/issues/32
// Ref https://github.com/Xinyuan-LilyGO/TTGO-T-Display/issues/32#issuecomment-1032578050
static const uint8_t EPD_BUSY = 17; // to EPD BUSY
static const uint8_t EPD_CS = 2;    // to EPD CS
static const uint8_t EPD_RST = 12;  // to EPD RST
static const uint8_t EPD_DC = 15;   // to EPD DC
static const uint8_t EPD_SCK = 25;  // to EPD CLK
static const uint8_t EPD_MISO = 19; // Master-In Slave-Out not used, as no data from display
static const uint8_t EPD_MOSI = 26; // to EPD DIN

GxEPD2_BW<GxEPD2_420, GxEPD2_420::HEIGHT> display(GxEPD2_420(/*CS=D8*/ EPD_CS, /*DC=D3*/ EPD_DC, /*RST=D4*/ EPD_RST, /*BUSY=D2*/ EPD_BUSY));

String time_str, date_str; // strings to hold time and received weather data
int wifi_signal, CurrentHour = 0, CurrentMin = 0, CurrentSec = 0;
long StartTime = 0;

uint8_t *bitmap_data;
int scanned_to_line = 0;

long SleepDuration = 30; // Sleep time in minutes, aligned to the nearest minute boundary, so if 30 will always update at 00 or 30 past the hour

void BeginSleep()
{
  display.powerOff();
  long SleepTimer = SleepDuration * 60;                         // theoretical sleep duration
  long offset = (CurrentMin % SleepDuration) * 60 + CurrentSec; // number of seconds elapsed after last theoretical wake-up time point
  if (offset > SleepDuration / 2 * 60)
  {                               // waking up too early will cause <offset> too large
    offset -= SleepDuration * 60; // then we should make it negative, so as to extend this coming sleep duration
  }
  esp_sleep_enable_timer_wakeup((SleepTimer - offset) * 1000000LL); // do compensation to cover ESP32 RTC timer source inaccuracies
#ifdef BUILTIN_LED
  pinMode(BUILTIN_LED, INPUT); // If it's On, turn it off and some boards use GPIO-5 for SPI-SS, which remains low after screen use
  digitalWrite(BUILTIN_LED, HIGH);
#endif
  Serial.println("Entering " + String(SleepTimer) + "-secs of sleep time");
  Serial.println("Awake for : " + String((millis() - StartTime) / 1000.0, 3) + "-secs");
  Serial.println("Starting deep-sleep period...");
  esp_deep_sleep_start(); // Sleep for e.g. 30 minutes
}

uint8_t StartWiFi()
{
  Serial.print("\r\nConnecting to: ");
  Serial.println(String(ssid));
  IPAddress dns(8, 8, 8, 8); // Google DNS
  WiFi.disconnect();
  WiFi.mode(WIFI_STA); // switch off AP
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
  unsigned long start = millis();
  uint8_t connectionStatus;
  bool AttemptConnection = true;
  while (AttemptConnection)
  {
    connectionStatus = WiFi.status();
    if (millis() > start + 15000)
    { // Wait 15-secs maximum
      AttemptConnection = false;
    }
    if (connectionStatus == WL_CONNECTED || connectionStatus == WL_CONNECT_FAILED)
    {
      AttemptConnection = false;
    }
    delay(50);
  }
  if (connectionStatus == WL_CONNECTED)
  {
    wifi_signal = WiFi.RSSI(); // Get Wifi Signal strength now, because the WiFi will be turned off to save power!
    Serial.println("WiFi connected at: " + WiFi.localIP().toString());
    Serial.printf("WiFi RSSI: %d\n", wifi_signal);
  }
  else
  {
    Serial.println("WiFi connection *** FAILED ***");
  }
  return connectionStatus;
}

void StopWiFi()
{
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
}

boolean UpdateLocalTime()
{
  struct tm timeinfo;
  char time_output[30], day_output[30], update_time[30];
  while (!getLocalTime(&timeinfo, 10000))
  { // Wait for 5-sec for time to synchronise
    Serial.println("Failed to obtain time");
    return false;
  }
  CurrentHour = timeinfo.tm_hour;
  CurrentMin = timeinfo.tm_min;
  CurrentSec = timeinfo.tm_sec;
  return true;
}

boolean SetupTime()
{
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer, "time.nist.gov"); //(gmtOffset_sec, daylightOffset_sec, ntpServer)
  setenv("TZ", Timezone, 1);                                                 // setenv()adds the "TZ" variable to the environment with a value TimeZone, only used if set to 1, 0 means no change
  tzset();                                                                   // Set the TZ environment variable
  delay(100);
  bool TimeStatus = UpdateLocalTime();
  return TimeStatus;
}

void InitialiseDisplay()
{
  display.init(115200, true, 2, false);
  // display.init(); for older Waveshare HAT's
  SPI.end();
  SPI.begin(EPD_SCK, EPD_MISO, EPD_MOSI, EPD_CS);
  display.fillScreen(GxEPD_WHITE);
  display.setFullWindow();
}

void clear_bitmap_data()
{
  for (int i = 0; i < (int)SCREEN_WIDTH * (int)SCREEN_HEIGHT / 8; i++)
  {
    bitmap_data[i] = 0x00;
  }
  scanned_to_line = 0;
}

void on_draw(pngle_t *pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t rgba[4])
{

  uint8_t r = rgba[0]; // 0 - 255
  uint8_t g = rgba[1]; // 0 - 255
  uint8_t b = rgba[2]; // 0 - 255
  uint8_t a = rgba[3]; // 0: fully transparent, 255: fully opaque

  // Convert to grayscale using NTSC formula
  uint8_t gray = 0.299 * r + 0.587 * g + 0.114 * b;

  int color = a > 0 ? (gray > 254 ? 0 : 1) : 0;

  int bitmap_i = (y * (int)SCREEN_WIDTH + x) / 8;
  int bitmap_shift = 7 - (y * (int)SCREEN_WIDTH + x) % 8;
  bitmap_data[bitmap_i] |= color << bitmap_shift;

  if (y > scanned_to_line)
  {
    scanned_to_line = y;
    Serial.printf("Scanned to line %d of %dx%d\n", scanned_to_line, w, h);
  }
}

boolean GetImage()
{
  boolean success = false;
  boolean had_error = false;
  WiFiClientSecure *client = new WiFiClientSecure;

  Serial.print("[HTTP] begin...\n");

  if (!client)
  {
    Serial.println("Unable to create client");
    return success;
  }
  client->setInsecure();
  // TODO: Lazy
  // client->setCACert(rootCACertificate);

  int pct_done = 0;

  // Ensure https is destroyed by the scoped block
  {
    HTTPClient https;
    https.begin(*client, HTTPS_SCREENSHOT_URL);

    Serial.print("[HTTP] GET...\n");
    // start connection and send HTTP header
    int httpCode = https.GET();
    if (httpCode > 0)
    {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTP] GET... code: %d\n", httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK)
      {
        pngle_t *pngle = pngle_new();

        pngle_set_draw_callback(pngle, on_draw);

        // get length of document (is -1 when Server sends no Content-Length header)
        int total = https.getSize();
        int orig_total = total;
        int remain = 0;

        Serial.printf("Content-Length: %d\n", total);
        // TODO: Assert it's actually a png
        String contentType = https.header("Content-Type");
        Serial.printf("Content-Type: %s\n", contentType.c_str());

        // create buffer for read
        uint8_t buff[2048] = {0};

        // get tcp stream
        WiFiClient *stream = https.getStreamPtr();

        // read all data from server
        while (https.connected() && (total > 0 || remain > 0))
        {
          // get available data size
          size_t size = stream->available();
          if (size > sizeof(buff) - remain)
          {
            size = sizeof(buff) - remain;
          }

          int len = stream->readBytes(buff + remain, size);
          if (len > 0)
          {
            int fed = pngle_feed(pngle, buff, remain + len);

            if (fed < 0)
            {
              Serial.printf("pngle_error at %d: %s\n", len, pngle_error(pngle));
              had_error = true;
              break;
            }

            total -= len;
            remain = remain + len - fed;
            if (remain > 0) memmove(buff, buff + fed, remain);

            int pct_done_new = 100 - (total * 100 / orig_total);
            if (pct_done_new - pct_done >= 10)
            {
              pct_done = pct_done_new;
              Serial.printf("Downloaded %d%% (%d / %d)\n", pct_done, total, orig_total);
            }
          }

          delay(1);
        }

        Serial.println();
        Serial.print("[HTTP] connection closed or file end.\n");
        Serial.printf("Downloaded %d%% (%d / %d)\n", 100 - (total * 100 / orig_total), total, orig_total);
        Serial.printf("Scanned to line %d\n", scanned_to_line);
        pngle_destroy(pngle);

        if (!had_error)
        {
          success = true;
        }
      }
    }
    else
    {
      Serial.printf("[HTTP] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
    }

    https.end();
  }

  delete client;

  return success;
}

void updateDisplay()
{
  display.drawImage(bitmap_data, 0, 0, (int)SCREEN_WIDTH, (int)SCREEN_HEIGHT, true, false, false);
  delay(50000);
  clear_bitmap_data();
}

void loop()
{ // this will never run!
}

void setup()
{
  StartTime = millis();
  Serial.begin(115200);

  Serial.println("Setup called");

  // 1-bit per pixel
  bitmap_data = (uint8_t *)malloc((int)SCREEN_WIDTH * (int)SCREEN_HEIGHT * sizeof(uint8_t) / 8);

  if (bitmap_data == NULL)
  {
    // Handle the error, e.g., by printing an error message and stopping the program
    Serial.println("Failed to allocate memory for bitmap_data");
    return;
  }

  Serial.println("Allocated bitmap_data array");

  clear_bitmap_data();

  InitialiseDisplay();

  while (StartWiFi() != WL_CONNECTED)
  {
    delay(500);
  }
  if (SetupTime() == true)
  {
    byte Attempts = 1;
    bool RxPng = false;
    WiFiClient client; // wifi client object
    while ((RxPng == false) && Attempts <= 3)
    {
      if (GetImage())
      {
        RxPng = true;
      }
      else
      {
        Serial.println("Failed to get image, attempt " + String(Attempts) + " of 3");
      }
      Attempts++;
    }
    if (RxPng)
    {
      StopWiFi(); // Reduces power consumption
      updateDisplay();
    }
  }
  BeginSleep();
}
