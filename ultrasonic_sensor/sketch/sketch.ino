// by Ray Burnette 20161013 compiled on Linux 16.3 using Arduino 1.6.12
#include <NTPClient.h>
#include "./functions.h"

// uint8_t channel = 1;

const long utcOffsetInSeconds = 3600;
//char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
long timerOffset = 0;

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);


void setup() {
  Serial.begin(57600);

  WiFi.begin(ssid, password);

  while ( WiFi.status() != WL_CONNECTED ) {
    delay ( 500 );
    Serial.print ( "." );
  }
  Serial.println("Connected.");
  timeClient.begin();
  timerOffset = timeClient.getEpochTime();
  
  Serial.printf("\n\nSDK version:%s\n\r", system_get_sdk_version());
  Serial.println(F("Type:   /-------MAC------/-----WiFi Access Point SSID-----/  /----MAC---/  Chnl  RSSI"));

  wifi_set_opmode(STATION_MODE);            // Promiscuous works only with station mode
  wifi_set_channel(channel);
  wifi_promiscuous_enable(disable);
  wifi_set_promiscuous_rx_cb(promisc_cb);   // Set up promiscuous callback
  wifi_promiscuous_enable(enable);
}

void loop() {
  channel = 1;
  wifi_set_channel(channel);
  while (true) {
    nothing_new++;                          // Array is not finite, check bounds and adjust if required
    if (nothing_new > MAX_CLIENTS_TRACKED) {
      nothing_new = 0;
      channel++;
      if (channel == 15) break;             // Only scan channels 1 to 14
      wifi_set_channel(channel);
    }

    if (timeClient.getEpochTime() % 600 == (timerOffset - 1) % 600) {
      sendData();
      delay(10000);
    }
    
    delay(1);  // critical processing timeslice for NONOS SDK! No delay(0) yield()
    // Press keyboard ENTER in console with NL active to repaint the screen
    /*
    if ((Serial.available() > 0) && (Serial.read() == '\n')) {
      Serial.println("\n-------------------------------------------------------------------------------------\n");
      //for (int u = 0; u < clients_known_count; u++) print_client(clients_known[u]);
      for (int u = 0; u < aps_known_count; u++) print_beacon(aps_known[u]);
      for (int u = 0; u < connected_to_pg_count; u++) print_client(clients_connected_to_pg[u]);
      Serial.println("\n-------------------------------------------------------------------------------------\n");
    }
    */
  }
}
