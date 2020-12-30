#if defined(ARDUINO_spresense_ast)

#include <SDHCI.h>
#include <Audio.h>
SDClass ThisSD;
AudioClass *ThisAudio;
File SoundFile;

void setup() {
  // Using Serial2 to receive data - same baud rate as ESP8266
  Serial2.begin(115200);
  // Initializing audio
  ThisAudio = AudioClass::getInstance();
  ThisAudio->begin();
  ThisAudio->setRenderingClockMode(AS_CLKMODE_NORMAL);
  ThisAudio->setPlayerMode(AS_SETPLAYER_OUTPUTDEVICE_SPHP, AS_SP_DRV_MODE_LINEOUT);
  ThisAudio->setVolume(0);
  ThisAudio->initPlayer(AudioClass::Player0, AS_CODECTYPE_MP3, "/mnt/sd0/BIN", AS_SAMPLINGRATE_AUTO, AS_CHANNEL_STEREO);
  // Sound check
  for (int j = 0; j < 2; j++) for (int i = 1000; i <= 3000; i += 500) {
      ThisAudio->setBeep(1, -20, i);
      delay(50);
      ThisAudio->setBeep(0, 0, 0);
      delay(25);
    }
}

void loop() {
  // Wait for text
  if (Serial2.available()) {
    // Store the whole string
    String UARTText = Serial2.readString();
    while (UARTText.length() > 0) {
      // Open file from first piece of the string
      SoundFile = ThisSD.open(UARTText.substring(0, UARTText.indexOf("|")) + ".mp3");
      // If good file name
      if (SoundFile == 1) PlaySound();
      // Default file name for carrier type
      else if (isAlpha(UARTText.charAt(0))) {
        SoundFile = ThisSD.open("Load.mp3");
        PlaySound();
      }
      // Close file
      SoundFile.close();
      // Split the string and take the first piece out
      if (UARTText.indexOf("|") > 0) UARTText = UARTText.substring(UARTText.indexOf("|") + 1, UARTText.length());
      // No more pieces - do this to exit while loop
      else UARTText = "";
    }
  }
}

void PlaySound() {
  // Prime sound player
  ThisAudio->writeFrames(AudioClass::Player0, SoundFile);
  // Start sound player
  ThisAudio->startPlayer(AudioClass::Player0);
  // Getting the next chunk(s) of sound until the end
  while (!ThisAudio->writeFrames(AudioClass::Player0, SoundFile));
  // Stop audio player
  ThisAudio->stopPlayer(AudioClass::Player0);
}

#elif defined(ARDUINO_ESP8266_WEMOS_D1MINIPRO)

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

StaticJsonDocument<2000> JDoc;
unsigned long TimeStamp = 0;
WiFiClient client;
HTTPClient http;

#define Time2Seconds(S) 3600 * S.substring(11, 13).toInt() + 60 * S.substring(14, 16).toInt() + S.substring(17, 19).toInt();

void setup() {
  // Same baud rate as Spresense
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  // SSID and password
  WiFi.begin("SSID", "password");
  // Getting data LED
  pinMode(LED_BUILTIN, OUTPUT);
  // Wait here until connected to internet
  while (WiFi.status() != WL_CONNECTED) yield();
}

void loop() {
  // Clock is checked twice a minute
  if (TimeStamp + 30000 <  millis() || TimeStamp == 0) {
    TimeStamp = millis();
    // Toggle LED
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    // Base URL
    String URL = "http://spacelandclocks.herokuapp.com/socket.io/?EIO=3&transport=polling&sid=";
    // GET SID
    if (http.begin(client, URL) && http.GET() == 200) {
      if (!deserializeJson(JDoc, http.getString().substring(http.getString().indexOf("{")))) {
        // Add SID to base URL
        URL += String((const char*) JDoc["sid"]);
        // Some kind of subscription
        if (http.begin(client, URL) && http.POST("26:42[\"join\",\"announcements\"]") == 200) {
          // GET the real data
          if (http.begin(client, URL) && http.GET() == 200) {
            if (!deserializeJson(JDoc, http.getString().substring(http.getString().indexOf("{"), http.getString().lastIndexOf("}")))) {
              // Text to send to Spresense
              String UARTText = "";
              // Get DZ data
              for (JsonPair DZ : JDoc.as<JsonObject>()) {
                //Serial.println(DZ.key().c_str()); // Will be used to filter Dropzones
                // Only need loads data
                for (JsonVariant Load : DZ.value()["loads"].as<JsonArray>()) {
                  // To add to text to send to Spresense
                  String LoadText;
                  // To calculate time remaining
                  int S1 = 0, S2 = 0, M = 0;
                  LoadText = "|" + Load["loadNumber"].as<String>() + "|";
                  String Str = Load["plane"].as<String>();
                  int Ind = (Str.indexOf("-") > -1 ? Str.indexOf("-") : Str.indexOf(" "));
                  LoadText = Str.substring(0, Ind) + LoadText;
                  S2 = Time2Seconds(Load["departureTime"].as<String>());
                  S1 = Time2Seconds(Load["jumpRunDbTime"].as<String>());
                  // Minutes only
                  M = ((S2 - S1) / 60) % 60;
                  // Every five minutes if load is not on hold
                  if (S2 >= S1 && M % 5 == 0) UARTText += LoadText + String(M) + "m|";
                }
              }
              // Send to Spresense
              Serial.println(UARTText);
            }
            http.end();
          }
          http.end();
        }
      }
      http.end();
    }
  }
}

#else
#error Unsupported board selection.
#endif
