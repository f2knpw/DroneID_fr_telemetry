/*
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

  article original
    https://www.helicomicro.com/2020/05/26/une-solution-open-source-pour-le-signalement-electronique-a-distance-a-moins-de-40-e/

  legal text :
    https://www.legifrance.gouv.fr/codes/article_lc/LEGIARTI000039307454
  spec DroneID_fr
    https://www.legifrance.gouv.fr/eli/arrete/2019/12/27/ECOI1934044A/jo/texte

  DroneID library : https://github.com/khancyr/droneID_FR
  DroneID on a TTgo ESP32 board (onboard beacon) : https://github.com/khancyr/TTGO_T_BEAM


*/
#include <Arduino.h>
#include "board_def.h"

#include "droneID_FR.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
const char* Wssid = "my SSID";
const char* Wpassword = "my password";

//Json
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

//OTA
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
int modeOTA = 0;
uint8_t program = 0; //(0 : beacon, 1: OTA update, 2: OTA updating)

extern "C" {
#include "esp_wifi.h"
  esp_err_t esp_wifi_80211_tx(wifi_interface_t ifx, const void *buffer, int len, bool en_sys_seq);
}

//#define DEBUG
#define OTA_DEBUG
#define sDEBUG


UBLOX_GPS_OBJECT();

droneIDFR drone_idfr;

/********************************************************************************************************************
   MODIFIEZ LES VALEURS ICI
 ********************************************************************************************************************/
// Set these to your desired credentials.
const char ssid[] = "JPG01_droneID"; //Le nom du point d'acces wifi CHANGEZ LE par ce que vous voulez !!!
const char *password = "my password";     //Mot de passe du wifi
//IMPORTANT --> CHANGEZ l'ID du drone par celui que Alphatango vous a fourni (Trigramme + Modèle + numéro série) !
const char drone_id[] = "000TRI000000000000000000055555";
float lat, lon, GSpd, Hdg, GAlt;
long RTC;


/********************************************************************************************************************/
// NE PAS TOUCHEZ A PARTIR D'ICI !!!
// Le wifi est sur le channel 6 conformement à la spécification
static constexpr uint8_t wifi_channel = 6;
// Ensure the drone_id is max 30 letters
static_assert((sizeof(ssid) / sizeof(*ssid)) <= 32, "AP SSID should be less than 32 letters");
// Ensure the drone_id is max 30 letters
static_assert((sizeof(drone_id) / sizeof(*drone_id)) <= 31, "Drone ID should be less that 30 letters !"); // 30 lettres + null termination
// beacon frame definition
static constexpr uint16_t MAX_BEACON_SIZE = 40 + 32 + droneIDFR::FRAME_PAYLOAD_LEN_MAX;  // default beaconPacket size + max ssid size + max drone id frame size
uint8_t beaconPacket[MAX_BEACON_SIZE] = {
  0x80, 0x00,							            // 0-1: Frame Control
  0x00, 0x00,							            // 2-3: Duration
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff,				// 4-9: Destination address (broadcast)
  0x24, 0x62, 0xab, 0xdd, 0xb0, 0xbd,				// 10-15: Source address FAKE  // TODO should bet set manually
  0x24, 0x62, 0xab, 0xdd, 0xb0, 0xbd,				// 16-21: Source address FAKE
  0x00, 0x00,							            // 22-23: Sequence / fragment number (done by the SDK)
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,	// 24-31: Timestamp (GETS OVERWRITTEN TO 0 BY HARDWARE)
  0xB8, 0x0B,							            // 32-33: Beacon interval: set to 3s == 3000TU== BB8, bytes in reverse order  // TODO: manually set it
  0x21, 0x04,							            // 34-35: Capability info
  0x03, 0x01, 0x06,						        // 36-38: DS Parameter set, current channel 6 (= 0x06), // TODO: manually set it
  0x00, 0x20,                     				// 39-40: SSID parameter set, 0x20:maxlength:content
  // 41-XX: SSID (max 32)
};

bool has_set_home = false;
bool hasGotNewVal = false;
double home_alt = 0.0;



uint64_t dispMap = 0;
String dispInfo;
char buff[5][256];

uint64_t gpsSec = 0;

/**
   Phase de configuration.
*/
void setup()
{
  Serial.begin(115200);
  delay(1000);
  pinMode(22, OUTPUT);
  digitalWrite(22, LOW);

#ifdef ENABLE_SERIAL
  Serial2.begin(GPS_BAUD_RATE, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
#endif

  /********************************************************************************************************************
     ICI ON INITIALISE LE WIFI
  */
  /**
     Pour mon exemple, je crée un point d'accés. Il fait rien par defaut.
  */
  Serial.println("Starting AP");
  WiFi.softAP(ssid, nullptr, wifi_channel);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  Serial.print("AP mac address: ");
  Serial.println(WiFi.macAddress());
  wifi_config_t conf_current;
  esp_wifi_get_config(WIFI_IF_AP, &conf_current);
  // Change WIFI AP default beacon interval sending to 1s.
  conf_current.ap.beacon_interval = 1000;
  drone_idfr.set_drone_id(drone_id);
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &conf_current));

  //OTA
  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  ArduinoOTA.setHostname("TX16s");
  Serial.println("hostname OTA : TX16s");
  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

//  ArduinoOTA
//  .onStart([]() {
//    String type;
//    if (ArduinoOTA.getCommand() == U_FLASH)
//      type = "sketch";
//    else // U_SPIFFS
//      type = "filesystem";
//
//    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
//    Serial.println("Start updating " + type);
//  })
//  .onEnd([]() {
//    Serial.println("\nEnd");
//  })
//  .onProgress([](unsigned int progress, unsigned int total) {
//    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
//  })
//  .onError([](ota_error_t error) {
//    Serial.printf("Error[%u]: ", error);
//    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
//    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
//    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
//    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
//    else if (error == OTA_END_ERROR) Serial.println("End Failed");
//  });

}

/**
   Début du code principal. C'est une boucle infinie.
*/
void loop()
{
  switch (program)
  {
    case 0:
      {
        // Ici on lit les données qui arrivent du GPS via la telemetry de la TX16s et le port AUX1 et on les traite
        String rxValue = "";
        //        char c;
        if (Serial2.available() > 0)
        {
          rxValue = Serial2.readStringUntil('\n');
        }

        String test = "";
        if (rxValue.length() > 0)
        {
#ifdef DEBUG
          Serial.print("Received : ");
#endif
          for (int i = 0; i < rxValue.length(); i++)
          {
#ifdef DEBUG
            Serial.print(rxValue[i]);
#endif
            test = test + rxValue[i];
          }
#ifdef DEBUG
          Serial.println();
#endif

          int i;
          StaticJsonDocument<200> doc;
          DeserializationError error = deserializeJson(doc, test);
          // Test if parsing succeeds.
          if (error) {
#ifdef DEBUG
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.c_str());
            Serial.println("deserializeJson() failed");
#endif
          }
          else
          {
            //{"RTC":1693769182, "lat":43.5000000, "lon":1.4900000, "GSpd":0, "Hdg":193.33999633789, "GAlt":183.60000610352}

            // Fetch values
            modeOTA = doc["OTA"];
            lat =  doc["lat"]  ;
            //lat = lat / 10000000.; //no longer needed
            lon =  doc["lon"]  ;
            //lon = lon / 10000000.; //no longer needed
            GSpd = doc["GSpd"]  ;
            Hdg = doc["Hdg"]  ;
            GAlt = doc["GAlt"];
#ifdef DEBUG
            Serial.print("mode OTA ");
            Serial.print(modeOTA);
            Serial.print(", lat ");
            Serial.print(lat);
            Serial.print(", lon ");
            Serial.print(lon);
            Serial.print(", GSpd ");
            Serial.print(GSpd);
            Serial.print(", Hdg ");
            Serial.print(Hdg);
            Serial.print(", GAlt ");
            Serial.println(GAlt);
#endif
            hasGotNewVal = true;
          }

          rxValue = "";
        }

        // On traite le case si la position GPS est valide.
        if (lat != 0 && lon != 0)
        {
          // On renseigne le point de démarrage quand la précision est satisfaisante

          if (!has_set_home && lat != 0  && lon != 0)
          {
            Serial.println("Setting Home Position");
            drone_idfr.set_home_lat_lon(lat, lon);
            has_set_home = true;
            home_alt = GAlt;
          }
          // On envoie les données a la librairie d'identification drone pour le formattage.
          drone_idfr.set_lat_lon(lat, lon);
          drone_idfr.set_altitude(GAlt);
          drone_idfr.set_heading(Hdg);
          drone_idfr.set_ground_speed(GSpd);
          drone_idfr.set_heigth(GAlt - home_alt);

        }

        /**
           On regarde s'il temps d'envoyer la trame d'identification drone: soit toutes les 3s soit si le drone s'est déplacé de 30m en moins de 3s.
        */
        if (drone_idfr.time_to_send())
        {
#ifdef sDEBUG
          Serial.print("Send beacon : ");
#endif
          /**
             On commence par renseigner le ssid du wifi dans la trame
          */
          // write new SSID into beacon frame
          const size_t ssid_size = (sizeof(ssid) / sizeof(*ssid)) - 1; // remove trailling null termination
          beaconPacket[40] = ssid_size;  // set size
          memcpy(&beaconPacket[41], ssid, ssid_size); // set ssid
          const uint8_t header_size = 41 + ssid_size;  //TODO: remove 41 for a marker
          /**
             On génère la trame wifi avec l'identfication
          */
          const uint8_t to_send = drone_idfr.generate_beacon_frame(beaconPacket, header_size);  // override the null termination
          // Décommenter ce block pour voir la trame entière sur le port usb
          //        Serial.println("beaconPacket : ");
          //        for (auto i = 0; i < sizeof(beaconPacket); i++) {
          //          Serial.print(beaconPacket[i], HEX);
          //          Serial.print(" ");
          //        }
          //        Serial.println(" ");
          if (has_set_home)
          {
            ESP_ERROR_CHECK(esp_wifi_80211_tx(WIFI_IF_AP, beaconPacket, to_send, true));  //On envoie la trame
#ifdef sDEBUG
            Serial.print(" new val = ");
            Serial.println(hasGotNewVal);
#endif
          }
          else
          {
            Serial.print("home point not set...");
            Serial.print(" new val = ");
            Serial.println(hasGotNewVal);
          }
          hasGotNewVal = false;
          drone_idfr.set_last_send(); //On reset la condition d'envoi
        }
//
//        // Ici on lit les données qui arrivent du GPS via la telemetry de la TX16s et le port AUX1 et on les traite
//        String rxValue2 = "";
//
//        if (Serial.available() > 0)
//        {
//          rxValue2 = Serial.readStringUntil('\n');
//        }
//
//        String test2 = "";
//        if (rxValue2.length() > 0)
//        {
//#ifdef OTA_DEBUG
//          Serial.print("Received : ");
//#endif
//          for (int i = 0; i < rxValue2.length(); i++)
//          {
//#ifdef OTA_DEBUG
//            Serial.print(rxValue2[i]);
//#endif
//            test2 = test2 + rxValue2[i];
//          }
//#ifdef OTA_DEBUG
//          Serial.println();
//#endif
          if (modeOTA==1) program++; // goto OTA update
//        }
      }
      break;

    case 1:
      {
        Serial.println("OTA update");
        // Set WiFi to station mode and disconnect from an AP if it was previously connected
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        delay(100);
        //wifi connection
        Serial.print("Attempting to connect to SSID: ");
        Serial.println(Wssid);
        WiFi.begin(Wssid, Wpassword);

        // attempt to connect to Wifi network:
        while (WiFi.status() != WL_CONNECTED) {
          Serial.print(".");
          // wait 1 second for re-trying
          delay(1000);
        }

        Serial.print("Connected to ");
        Serial.println(Wssid);
        ArduinoOTA.begin();
        program ++; // goto OTA updating
      }
      break;

    case 2:
      {
        //Serial.println("OTA updating");
        //OTA
        ArduinoOTA.handle();
      }
      break;
  }
}
