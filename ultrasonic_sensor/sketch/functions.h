// This-->tab == "functions.h"

// Expose Espressif SDK functionality
extern "C" {
//#include "user_interface.h"
  typedef void (*freedom_outside_cb_t)(uint8 status);
  int  wifi_register_send_pkt_freedom_cb(freedom_outside_cb_t cb);
  void wifi_unregister_send_pkt_freedom_cb(void);
  int  wifi_send_pkt_freedom(uint8 *buf, int len, bool sys_seq);
}

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266HTTPClient.h>

#include "./structures.h"

#define MAX_APS_TRACKED 10
#define MAX_CLIENTS_TRACKED 300
#define disable 0
#define enable  1

beaconinfo aps_known[MAX_APS_TRACKED];                    // Array to save MACs of known APs
int aps_known_count = 0;                                  // Number of known APs
int nothing_new = 0;
clientinfo clients_known[MAX_CLIENTS_TRACKED];            // Array to save MACs of known CLIENTs
int clients_known_count = 0;                              // Number of known CLIENTs

clientinfo clients_connected_to_pg[MAX_CLIENTS_TRACKED];  // Track only the clients connected to the PG Wifis
int connected_to_pg_count = 0;                            // Number of known CLIENTs connected to pg

char logUrl[] = "http://skmn3za6iccq1vpo.myfritz.net/ultrasonic_counter/submit_devices";
const char* ssid       = "PG-Coworking";
const char* password   = "PionierGarageHatEndlichInternet";
unsigned int channel = 1;


int register_beacon(beaconinfo beacon)
{
  if (memcmp("PG-Coworking", beacon.ssid, sizeof("PG-Coworking")) != 0 &&
      memcmp("PG-Office", beacon.ssid, sizeof("PG-Office")) != 0 &&
      memcmp("eduroam", beacon.ssid, sizeof("eduroam")) != 0 &&
      memcmp("KIT", beacon.ssid, sizeof("KIT")) != 0) {
        return 1;
      }

  int known = 0;   // Clear known flag
  
  for (int u = 0; u < aps_known_count; u++)
  {
    if (! memcmp(aps_known[u].bssid, beacon.bssid, ETH_MAC_LEN)) {
      known = 1;
      break;
    }   // AP known => Set known flag
  }
  if (! known)  // AP is NEW, copy MAC to array and return it
  {
    memcpy(&aps_known[aps_known_count], &beacon, sizeof(beacon));
    aps_known_count++;

    if ((unsigned int) aps_known_count >=
        sizeof (aps_known) / sizeof (aps_known[0]) ) {
      Serial.printf("exceeded max aps_known\n");
      aps_known_count = 0;
    }
  }
  return known;
}

int register_client(clientinfo ci)
{
  // Look if the accesspoints are PG-related
  int pg_known = 0;
  for (int u = 0; u < connected_to_pg_count; u++)
  {
    if (! memcmp(clients_connected_to_pg[u].station, ci.station, ETH_MAC_LEN)) {
      pg_known = 1;
      break;
    }
  }
  if (! pg_known) {
    for (int u = 0; u < aps_known_count; u++)
    {
      if (! memcmp(aps_known[u].bssid, ci.bssid, ETH_MAC_LEN)) {  
        memcpy(&clients_connected_to_pg[connected_to_pg_count], &ci, sizeof(ci));
        connected_to_pg_count++;

        if ((unsigned int) connected_to_pg_count >= sizeof (clients_connected_to_pg) / sizeof (clients_connected_to_pg[0]) ) {
          Serial.printf("exceeded max clients_known\n");
          connected_to_pg_count = 0;
        }
      }
    }
  }

  /*
  int known = 0;   // Clear known flag
  for (int u = 0; u < clients_known_count; u++)
  {
    if (! memcmp(clients_known[u].station, ci.station, ETH_MAC_LEN)) {
      known = 1;
      break;
    }
  }
  if (! known)
  {
    memcpy(&clients_known[clients_known_count], &ci, sizeof(ci));
    clients_known_count++;

    if ((unsigned int) clients_known_count >=
        sizeof (clients_known) / sizeof (clients_known[0]) ) {
      Serial.printf("exceeded max clients_known\n");
      clients_known_count = 0;
    }

  }
  */
  return pg_known;
}

void print_beacon(beaconinfo beacon)
{
  if (beacon.err != 0) {
    //Serial.printf("BEACON ERR: (%d)  ", beacon.err);
  } else {
    Serial.printf("BEACON: <==================== [%32s]  ", beacon.ssid);
    for (int i = 0; i < 6; i++) {
      if (i != 5)
        Serial.printf("%02X:", beacon.bssid[i]);
      else
        Serial.printf("%02X", beacon.bssid[i]);
    }
    Serial.printf("   %2d", beacon.channel);
    Serial.printf("   %4d\r\n", beacon.rssi);
  }
}

void print_client(clientinfo ci)
{
  int u = 0;
  int known = 0;   // Clear known flag
  if (ci.err != 0) {
    // nothing
  } else {
    Serial.printf("DEVICE: ");
    for (int i = 0; i < 6; i++) {
    if (i != 5)
      Serial.printf("%02X:", ci.station[i]);
    else
      Serial.printf("%02X", ci.station[i]);
    }
    //for (int i = 0; i < 6; i++) Serial.printf("%02x", ci.station[i]);
    Serial.printf(" ==> ");

    for (u = 0; u < aps_known_count; u++)
    {
      if (! memcmp(aps_known[u].bssid, ci.bssid, ETH_MAC_LEN)) {
        Serial.printf("[%32s]", aps_known[u].ssid);
        known = 1;     // AP known => Set known flag
        break;
      }
    }

    if (! known)  {
      Serial.printf("   Unknown/Malformed packet \r\n");
    } else {
      Serial.printf("%2s", " ");
      for (int i = 0; i < 6; i++) {
        if (i != 5)
          Serial.printf("%02X:", ci.ap[i]);
        else
          Serial.printf("%02X", ci.ap[i]);
      }
      //for (int i = 0; i < 6; i++) Serial.printf("%02x", ci.ap[i]);
      Serial.printf("  %3d", aps_known[u].channel);
      Serial.printf("   %4d\r\n", ci.rssi);
    }
  }
}

void promisc_cb(uint8_t *buf, uint16_t len)
{
  int i = 0;
  uint16_t seq_n_new = 0;
  if (len == 12) {
    struct RxControl *sniffer = (struct RxControl*) buf;
  } else if (len == 128) {
    struct sniffer_buf2 *sniffer = (struct sniffer_buf2*) buf;
    struct beaconinfo beacon = parse_beacon(sniffer->buf, 112, sniffer->rx_ctrl.rssi);
    if (register_beacon(beacon) == 0) {
      print_beacon(beacon);
      nothing_new = 0;
    }
  } else {
    struct sniffer_buf *sniffer = (struct sniffer_buf*) buf;
    //Is data or QOS?
    if ((sniffer->buf[0] == 0x08) || (sniffer->buf[0] == 0x88)) {
      struct clientinfo ci = parse_data(sniffer->buf, 36, sniffer->rx_ctrl.rssi, sniffer->rx_ctrl.channel);
      if (memcmp(ci.bssid, ci.station, ETH_MAC_LEN)) {
        if (register_client(ci) == 0) {
          print_client(ci);
          nothing_new = 0;
        }
      }
    }
  }
}

void sendData() {
  int bufferSize = sizeof("data={ \"devices\": [ ") +
    connected_to_pg_count * (
      sizeof("{\"bssid\": \"") +
      12 +
      66 +
      sizeof("\"}, ")
      ) +
    sizeof(" ] }");
  
  char *data = (char *)malloc(bufferSize);

  memset(data, '\0', bufferSize);
  
  char *target = data;

  target += sprintf(target, "%s", "data={ \"devices\": [ ");
  
  for (int i = 0; i < connected_to_pg_count; i++) {
    target += sprintf(target, "%s", "{\"bssid\": \"");
    for (int j = 0; j < ETH_MAC_LEN; j++) {
      target += sprintf(target, "%02X", clients_connected_to_pg[i].station[j]);
    }
    
    target += sprintf(target, "%s", "\", \"ssid\": \"");
    
    for (int u = 0; u < aps_known_count; u++)
    {
      if (! memcmp(aps_known[u].bssid, clients_connected_to_pg[i].bssid, ETH_MAC_LEN)) {
        target += sprintf(target, "%s", aps_known[u].ssid);
        break;
      }
    }
    target += sprintf(target, "%s", "\"}, ");
  }

  target += sprintf(target, "%s", " ] }");
  
  Serial.println(data);

  wifi_promiscuous_enable(disable);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while ( WiFi.status() != WL_CONNECTED ) {
    delay ( 500 );
    Serial.print ( "." );
  }
  Serial.println("Connected.");
  
  Serial.println("sending...");
  HTTPClient http;
  http.begin(logUrl);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded"); 
  int httpCode = http.POST(data);
  free(data);
  Serial.printf("Response Code: %d\n", httpCode);
  Serial.println(http.getString()); //Print request response payload 
  http.end();

  clients_known_count = 0;
  connected_to_pg_count = 0;
  
  wifi_set_opmode(STATION_MODE);            // Promiscuous works only with station mode
  wifi_set_channel(channel);
  wifi_promiscuous_enable(disable);
  wifi_promiscuous_enable(enable);
}
