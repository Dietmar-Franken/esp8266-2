/*
 * Copyright (c) 2015. Mario Mikočević <mozgy>
 *
 * MIT Licence
 *
 */

// Serial printing ON/OFF
#include "Arduino.h"
#define DEBUG true
// #define Serial if(DEBUG)Serial
// #define DEBUG_OUTPUT Serial

#include "ESP8266WiFi.h"
#include "FS.h"

ADC_MODE(ADC_VCC);

#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson

#include <Ticker.h>
Ticker tickerWiFiScan;
#define WAITTIME 600
boolean tickerFired;

char tmpstr[40];

File fh_netdata;
String line;

void flagWiFiScan( void ) {
  tickerFired = true;
}

String bssidToString( uint8_t *bssid ) {

  char mac[18] = {0};

  sprintf( mac,"%02X:%02X:%02X:%02X:%02X:%02X", bssid[0],  bssid[1],  bssid[2], bssid[3], bssid[4], bssid[5] );
  return String( mac );

}

bool update_netdata( int netNum ) {

  int netId;
  int netFound = 0;

  DynamicJsonBuffer jsonBuffer;

//    fh_netdata.println( "{\"count\":0,\"max\":0}" );
//    fh_netdata.println( "{\"count\":0,\"max\":0,\"networks\":[{\"ssid\":\"ssid\",\"bssid\":\"bssid\",\"rssi\":0,\"ch\":1,\"enc\":\"*\"}]}" );

// create new data from network list
  JsonObject& WiFiData = jsonBuffer.createObject();
  WiFiData["count"] = netNum;
  WiFiData["max"] = netNum;

  JsonArray& WiFiDataArray  = WiFiData.createNestedArray("networks");

  fh_netdata = SPIFFS.open( "/netdata.txt", "r" );

  if ( !fh_netdata ) {

// no last data
    Serial.println( "Data file doesn't exist yet." );

    fh_netdata = SPIFFS.open( "/netdata.txt", "w" );
    if ( !fh_netdata ) {
      Serial.println( "Data file creation failed" );
      return false;
    }
    for ( int i = 0; i < netNum; ++i ) {

      JsonObject& tmpObj = jsonBuffer.createObject();

      tmpObj["id"] = i;
      tmpObj["ssid"] = WiFi.SSID(i);
      tmpObj["bssid"] = bssidToString( WiFi.BSSID(i) );
      tmpObj["rssi"] = WiFi.RSSI(i);
      tmpObj["ch"] = WiFi.channel(i);
      tmpObj["enc"] = ((WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*");

      Serial.print("Add - ");tmpObj.printTo( Serial );Serial.println();
      WiFiDataArray.add( tmpObj );

    }

// WiFiData is wifi scan snapshot
//    Serial.println("Scanned wifi data ->");
//    WiFiData.printTo( Serial );
//    WiFiData.prettyPrintTo( Serial );
//    Serial.println("");

  } else {

// read last WiFi data from file
//    Serial.println( "Reading saved wifi data .." );
    line = fh_netdata.readStringUntil('\n');
//    Serial.print( "Line (read) " );Serial.println( line );

    JsonObject& WiFiDataFile = jsonBuffer.parseObject( line );
    if ( !WiFiDataFile.success() ) {
      Serial.println( "parsing failed" );
      // parsing failed, removing old data
      SPIFFS.remove( "/netdata.txt" );
      return false;
    }

    int netNumFile = WiFiDataFile["count"];
    int netMaxFile = WiFiDataFile["max"];
    netId = netMaxFile;

//    WiFiDataFile.prettyPrintTo( Serial );
//    Serial.println("");

    JsonArray& tmpArray = WiFiDataFile["networks"];
    for ( JsonArray::iterator it = tmpArray.begin(); it != tmpArray.end(); ++it ) {
      JsonObject& tmpObj = *it;
      WiFiDataArray.add( tmpObj );
      Serial.print( "Copy - " );tmpObj.printTo( Serial );Serial.println();
    }

    for ( int i = 0; i < netNum; i++ ) {
      bool wifiNetFound = false;
      for ( int j = 0; j < netNumFile; j++ ) {
        String ssid1 = WiFi.SSID(i);
        String ssid2 = WiFiDataArray[j]["ssid"];
        if ( ssid1 == ssid2 ) {
          String bssid1 = bssidToString( WiFi.BSSID(i) );
          String bssid2 = WiFiDataArray[j]["bssid"];
          if ( bssid1 == bssid2 ) {
            wifiNetFound = true;
            Serial.print( "Station - " );Serial.print(ssid1);
            Serial.print( ", scanned RSSI - " );Serial.print(WiFi.RSSI(i));
            Serial.print( ", saved RSSI - " );
            String rssi2 = WiFiDataArray[j]["rssi"];
            Serial.println(rssi2);
          }
        }
      }
      if ( !wifiNetFound ) {

        JsonObject& tmpObj = jsonBuffer.createObject();

        tmpObj["id"] = netId;
        tmpObj["ssid"] = WiFi.SSID(i);
        tmpObj["bssid"] = bssidToString( WiFi.BSSID(i) );
        tmpObj["rssi"] = WiFi.RSSI(i);
        tmpObj["ch"] = WiFi.channel(i);
        tmpObj["enc"] = ((WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*");

        WiFiDataArray.add( tmpObj );
        Serial.print( "Found new - " );tmpObj.printTo( Serial );Serial.println();

        netFound++;
        netId++;
      }
    }

    WiFiData["count"] = netNumFile + netFound;
    WiFiData["max"] = netId;

//    Serial.println("Computed wifi data ->");
//    WiFiData.prettyPrintTo( Serial );

    fh_netdata.close();
    // SPIFFS.remove( "/netdata.txt" );

    fh_netdata = SPIFFS.open( "/netdata.txt", "w" );
    if ( !fh_netdata ) {
      Serial.println( "Data file creation failed" );
      return false;
    }

  }
  WiFiData.printTo( fh_netdata );
  fh_netdata.println( "\n" );
  fh_netdata.close();

  return true;
}

void parse_networks( int netNum ) {

/*
  for (int i = 0; i < net_num; ++i)
  {
    // Print SSID and RSSI for each network found
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.print(WiFi.SSID(i));
    Serial.print(" (");
    Serial.print(WiFi.RSSI(i));
    Serial.print(")");
    Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*");
    delay(10);
  }
 */

  if ( !update_netdata( netNum ) ) {
    Serial.println( "Something went WRONG!" );
  }

}

void do_wifiscan( void ) {
  int netCount;

  Serial.println( "scan start" );

  // WiFi.scanNetworks will return the number of networks found
  netCount = WiFi.scanNetworks();
//  Serial.println( "scan done" ); // no need if Serial.setDebugOutput(true)
  if ( netCount == 0 ) {
    Serial.println( "no network found" );
  } else {
    Serial.print(netCount);
    Serial.println( " network(s) found" );
    parse_networks( netCount );
  }
  Serial.println();

}

void ElapsedStr( char *str ) {

  unsigned long sec, minute, hour;

  sec = millis() / 1000;
  minute = ( sec % 3600 ) / 60;
  hour = sec / 3600;
  sprintf( str, "Elapsed " );
  if ( hour == 0 ) {
    sprintf( str, "%s   ", str );
  } else {
    sprintf( str, "%s%2d:", str, hour );
  }
  if ( minute >= 10 ) {
    sprintf( str, "%s%2d:", str, minute );
  } else {
    if ( hour != 0 ) {
      sprintf( str, "%s0%1d:", str, minute );
    } else {
      sprintf( str, "%s ", str );
      if ( minute == 0 ) {
        sprintf( str, "%s  ", str );
      } else {
        sprintf( str, "%s%1d:", str, minute );
      }
    }
  }
  if ( ( sec % 60 ) < 10 ) {
    sprintf( str, "%s0%1d", str, ( sec % 60 ) );
  } else {
    sprintf( str, "%s%2d", str, ( sec % 60 ) );
  }

}

void setup() {

  Serial.begin(115200);
  delay(10);
  Serial.setDebugOutput(true);

  Serial.println();
  Serial.printf( "Sketch size: %u\n", ESP.getSketchSize() );
  Serial.printf( "Free size: %u\n", ESP.getFreeSketchSpace() );
  Serial.printf( "Heap: %u\n", ESP.getFreeHeap() );
  Serial.printf( "Boot Vers: %u\n", ESP.getBootVersion() );
  Serial.printf( "CPU: %uMHz\n", ESP.getCpuFreqMHz() );
  Serial.printf( "SDK: %s\n", ESP.getSdkVersion() );
  Serial.printf( "Chip ID: %u\n", ESP.getChipId() );
  Serial.printf( "Flash ID: %u\n", ESP.getFlashChipId() );
  Serial.printf( "Flash Size: %u\n", ESP.getFlashChipRealSize() );
  Serial.printf( "Vcc: %u\n", ESP.getVcc() );
  Serial.println();

  bool result = SPIFFS.begin();
  if( !result ) {
    Serial.println( "SPIFFS open failed!" );
  }

/*
  // comment format section after DEBUGING done
  result = SPIFFS.format();
  if( !result ) {
    Serial.println("SPIFFS format failed!");
  }
 */
//  SPIFFS.remove( "/netdata.txt" );

  // Set WiFi to station mode and disconnect from an AP if it was previously connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  tickerWiFiScan.attach( WAITTIME, flagWiFiScan );
  tickerFired = true;

  Serial.println( "Setup done" );
}

void loop() {

  if( tickerFired ) {
    tickerFired = false;
    do_wifiscan();
    ElapsedStr( tmpstr );
    Serial.println( tmpstr );
    Serial.print( "Heap: " ); Serial.println( ESP.getFreeHeap() );
  }

}
