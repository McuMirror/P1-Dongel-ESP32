/*
***************************************************************************  
**  Program  : P1-Dongel-ESP32
**  Copyright (c) 2025 Smartstuff / based on DSMR Api Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      

BACKLOG
- verbruik - teruglevering lijn door maandgrafiek (Erik)
- auto switch 3 - 1 fase max fuse
- temperatuur ook opnemen in grafieken (A van Dijken)
- SSE of websockets voor de communicatie tussen client / dongle ( P. van Bennekom )
- 90 dagen opslaan van uur gegevens ( R de Grijs )
- Interface HomeKit ivm triggeren op basis van energieverbruik/teruglevering (Thijs v Z)
- grafische weergave als standaardoptie weergave en cijferlijsten als tweede keuze. Nu is het andersom. 
- Consistentie tijd-assen, links oud, rechts nieuw
  - in Actueel staat de laatste meting rechts en de oudste meting links
  - in de uurstaat loopt de tijd van rechts (oudst) naar links (laatste uurmeting)
- Harold B: Dark-mode frontend
- Harold B: dynamische tarieven dus de onderverdeling naar Tarief 1 en 2 is niet relevant. (Overigens de P1-meter levert wel twee standen aan). Persoonlijk vind ik de grafieken onleesbaar worden (ik lever ook terug) vier verschillende kleurtjes groen en vier kleurtjes rood. Dus het heeft mijn voorkeur om dit onderscheid in de grafieken achterwege te laten. Dus als dat aan te sturen zou zijn via de instellingen, heel graag!
- Idee voor een toekomstige release: hergebruik de Prijsplafond grafieken voor een vergelijk tussen Afname en Levering gedurende het jaar. Ik zit steeds uit te rekenen of ik overschot aan kWh heb of inmiddels een tekort. De grafieken maken dat wel helder. ( Leo B )
- front-end: issue Stroom ( terug + afname bij 3 fase wordt opgeteled ipv - I voor teruglevering )
- Rob v D: 'Actueel' --> 'Grafisch' staat gasverbruik (blauw) vermeld, terwijl ik geen gas heb (verbruik is dan ook nul). Waterverbruik zie ik daar niet. In de uur/dag/maand overzichten zie ik wel water en geen gas.
- RNGhours files vergroten (nu 48h -> 336h) (Broes)
- teruglevering dashboard verkeerde verhoudingen ( Pieter ) 
- RNGDays 31 days
- eigen NTP kunnen opgeven of juist niet (stopt pollen)
- HA auto update ala : https://www.zigbee2mqtt.io/guide/usage/ota_updates.html#automatic-checking-for-available-updates
- Daily Insights: Inzichten vanaf opstarten dongle / 00:00 reset
    - loadbalancing over de fases heen
    - detail P per fase afgelopen uur (sample eens per 10s)
- kwartierpiek historie opnemen (wanneer nieuwe piek ontstaat)
- dynamische prijzen inlezen

V6
- mqtt 1 data and 1 vital json (with uptime)
- HA mqtt autodiscovery for the new format
- async webserver 
- RNGDays 31 days
- autodetect v2 meters
- connect via proxy to dongle

Default checks
- wifi
- port 82
- MQTT
- webserver
- EID
- dev_pairing
- ethernet
- 4h test

next
- improvement: modbus in own process = non-blocking 
- check and repair rng files on startup
- hostname aanpassen met laatste 3 segmenten mac-adres

- winter -> zomertijd issue. 2e uur mist en data van 2 dagen geleden staat er dan.
--> oplossing : 

bool isDSTTransition(int lastHour, int currentHour) {
    return (lastHour == 1 && currentHour == 3); // Detecteer de sprong
}
- inlezen van solar config in frontend

************************************************************************************
Arduino-IDE settings for P1 Dongle hardware ESP32:
  - Board: "ESP32C3 Dev Module"
  - Flash mode: "QIO"
  - Flash size: "4MB (32Mb)"
  - CorenDebug Level: "None"
  - Flash Frequency: "80MHz"
  - CPU Frequency: "160MHz"
  - Upload Speed: "961600"                                                                                
  - Port: <select correct port>
*/
/******************** compiler options  ********************************************/

// #define DEBUG
// #define INSIGHTS
// #define XTRA_LOG

//PROFILES -> NO PROFILE = WiFi P1 Dongle Pro
// #define ULTRA         //ultra (mini) dongle
// #define ETHERNET      //ethernet dongle
// #define ETH_P1EP          //ethernet pro+ dongle
// #define NRG_DONGLE 
// #define DEVTYPE_H2OV2 // P1 Dongle Pro with h2o and p1 out

//SPECIAL
// #define __Az__

//FEATURES
#define DEV_PAIRING
#define MBUS
//#define MQTT_DISABLE
//#define NO_STORAGE
//#define VOLTAGE_MON
// #define NO_HA_AUTODISCOVERY
//#define POST_TELEGRAM
// #define MQTTKB
// #define MB_RTU

#include "DSMRloggerAPI.h"
#include <esp_task_wdt.h>

void setup() 
{
  // make_version();
  uint16_t Freq = getCpuFrequencyMhz();
  setCpuFrequencyMhz(80); //lower power mode
  DebugBegin(115200);
  USBPrintf( "\n\n------> BOOTING %s [%s] <------\n\n", _DEFAULT_HOSTNAME, Firmware.Version ); 
  Debugf("Original cpu speed: %d\n",Freq);
  PushButton.begin(IO_BUTTON);

  P1StatusBegin(); //leest laatste opgeslagen status & rebootcounter + 1
  SetConfig();
  lastReset = getResetReason();
  DebugT(F("Last reset reason: ")); Debugln(lastReset);
  DebugFlush();
//================ File System =====================================
  if ( LittleFS.begin(true) ) { DebugTln(F("FS Mount OK\r")); FSmounted = true;  } 
  else DebugTln(F("!!!! FS Mount ERROR\r"));   // Serious problem with File System 
//================ Status update ===================================
  actT = epoch(actTimestamp, strlen(actTimestamp), true); // set the time to actTimestamp!
  P1StatusWrite();
  LogFile("",false); // write reboot status to file
  if (!LittleFS.exists(SETTINGS_FILE)) writeSettings(); //otherwise the dongle crashes some times on the first boot
  else readSettings(true);

//=============scan, repair and convert RNG files ==================
  // CheckRingFile(RINGDAYS);
  // loadRingfile(RINGDAYS);
  // printRecordArray(RNGDayRec, RingFiles[RINGDAYS].slots, "RINGDAYS");
  // saveRingfile(RINGDAYS);

  // patchJsonFile_Add7thValue(RINGMONTHS);
  // patchJsonFile_Add7thValue(RINGHOURS);
  // convertRingfileWithSlotExpansion(RINGDAYS,32);
//=============start Networkstuff ==================================
  
  startNetwork();
  PostMacIP(); //post mac en ip
  delay(100);
#ifdef INSIGHTS  
  if ( Insights.begin(INSIGHTS_KEY) ) Debugf("ESP Insights enabled Node ID %s\n", Insights.nodeID());
#endif  
  startTelnet();
  startMDNS(settingHostname);
  startNTP();

//================ Check necessary files ============================
  if (!DSMRfileExist(settingIndexPage, false) ) {
    DebugTln(F("Oeps! Index file not pressent, try to download it!\r"));
    GetFile(settingIndexPage, PATH_DATA_FILES); //download file from cdn
    if (!DSMRfileExist(settingIndexPage, false) ) {
      DebugTln(F("Oeps! Index file not pressent, try to download it!\r"));
      GetFile(settingIndexPage, URL_INDEX_FALLBACK);
    }
    if (!DSMRfileExist(settingIndexPage, false) ) { //check again
      DebugTln(F("Index file still not pressent!\r"));
      FSNotPopulated = true;
      }
  }
 
  if (!DSMRfileExist("/Frontend.json", false) ) {
    DebugTln(F("Frontend.json not pressent, try to download it!"));
    GetFile("/Frontend.json", PATH_DATA_FILES);
  }
  
  setupFSexplorer();
 
  esp_register_shutdown_handler(ShutDownHandler);

  setupWater();

  if (EnableHistory) CheckRingExists();
  SetupNetSwitch();

//================ Start Slimme Meter ===============================

#ifdef MBUS
  mbusSetup();
  SetupMB_RTU();
#endif  
  ReadSolarConfigs();
  ReadAccuConfig();
  delay(500);
  setCpuFrequencyMhz(Freq); //restore original clockspeed

  //create a task that will be executed in the fP1Reader() function, with priority 1
  //p1 task runs always on core 0. On the dual core models Arduino runs on core 1. It isn't possible that the process runs on both cores.
  if( xTaskCreatePinnedToCore( fP1Reader, "p1-reader", 1024*8, NULL, 2, &tP1Reader, /*core*/ 0 ) == pdPASS ) DebugTln(F("Task tP1Reader succesfully created"));
  if( xTaskCreatePinnedToCore( fMqtt    , "mqtt"     , 1024*6, NULL, 1, NULL      , /*core*/ 0 ) == pdPASS ) DebugTln(F("Task MQTT succesfully created"));
  DebugTf("Startup complete! actTimestamp[%s]\r\n", actTimestamp);  

} // setup()

//P1 reader task
void fP1Reader( void * pvParameters ){
  DebugTln(F("Enable slimme meter..."));
  esp_task_wdt_add(nullptr);
  SetupP1In();
  SetupP1Out();
  slimmeMeter.enable(false);
#ifdef ULTRA
  digitalWrite(DTR_IO, LOW); //default on
#endif   
  while(true) {
    PrintHWMark(0);
    handleSlimmemeter();
    P1OutBridge();
    esp_task_wdt_reset();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
  LogFile("P1 reader: unexpected task exit", true);
  vTaskDelete(NULL);
}

void fMqtt( void * pvParameters ){
#ifndef MQTT_DISABLE    
  DebugTln(F("Start MQTT Thread"));
  MQTTSetBaseInfo();
  MQTTsetServer();
  esp_task_wdt_add(nullptr);
  while(true) {
    PrintHWMark(1);
    handleMQTT();
    esp_task_wdt_reset();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
#endif
  LogFile("Mqtt: unexpected task exit", true);
  vTaskDelete(NULL);
}

void loop () { 
  httpServer.handleClient();      
  if ( DUE(StatusTimer) && (telegramCount > 2) ) { 
    P1StatusWrite();
    MQTTSentStaticInfo();
    CHANGE_INTERVAL_MIN(StatusTimer, 30);
  }
  // WifiWatchDog();
  handleKeyInput();
  handleRemoteUpdate();
  PushButton.handler();
  handleWater();
  handleEnergyID();  
  PostTelegram();
  GetSolarDataN();
  GetAccuDataN();
  handleVirtualP1();
  PrintHWMark(2);

} // loop()


/***************************************************************************
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to permit
* persons to whom the Software is furnished to do so, subject to the
* following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
* THE USE OR OTHER DEALINGS IN THE SOFTWARE.
* 
***************************************************************************/
