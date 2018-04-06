//References: https://github.com/espressif/arduino-esp32 & https://www.arduino.cc/en/Reference/HomePage
//See: https://github.com/espressif/arduino-esp32/tree/master/libraries
//and: https://github.com/me-no-dev/ESPAsyncWebServer#using-platformio
//peychart@netcourrier.com 20171021
// Licence: GNU v3

#define WITHWIFI
#define DEBUGG
#include <FS.h>
#include <SPIFFS.h>
#include <esp_sleep.h>
#ifdef WITHWIFI
  #define OTA       //See: https://github.com/me-no-dev/ESPAsyncWebServer#setting-up-the-server
  #define WEBUI
  #include <WiFi.h>
  #ifdef WEBUI
    #include <AsyncTCP.h>
    #include <ESPAsyncWebServer.h>
    //OTA:
  #endif
  #ifdef OTA
    #include <ESPmDNS.h>
    #include <WiFiUdp.h>
    #include <ArduinoOTA.h>
    #include <Hash.h>
  #endif
#endif

//Ajust the following:
uint8_t ResetConfig   = 2;      //Just change this value to reset current outputs config on the next boot...
#define HOSTNAME        "slidingGate" //Can be change by interface
#define DEFAULTWIFIPASS "defaultPassword"
#define MAXWIFIERRORS   10      //Attempts before host mode...
#define SSIDMax()       3
enum    pins            {  MOTORR,      MOTORF,       LIGHT,   OUTPUTSCOUNT, OPENCOUPLE,  CLOSECOUPLE, /* AUTOCLOSE,     PLACE,*/     HIGHPINS,    COMMAND,     DETECT,     PHOTOBEAM,  OPENINGLIMIT, CLOSINGLIMIT, PINSCOUNT};
gpio_num_t gpio[]     = {GPIO_NUM_13, GPIO_NUM_12, GPIO_NUM_14, GPIO_NUM_0,  GPIO_NUM_35, GPIO_NUM_34, /*GPIO_NUM_22, GPIO_NUM_21,*/ GPIO_NUM_0, GPIO_NUM_32, GPIO_NUM_33, GPIO_NUM_26,  GPIO_NUM_18,  GPIO_NUM_19           };

//Avoid to change the following:
#define LOOPDELAY                 250
#define OPEN                      true
#define CLOSE                     false
#define DEEPSLEEPDELAY            120000
#define DEFAULTAUTOCLOSEDELAY     30000
#define EMULATEBLINKING           1500
#define MAXIMUMCLOSINGTIME        25000
#define MILLISEC()                (millis()+MAXIMUMCLOSINGTIME)
#define MAX(n,m)                  ((m>n) ?(m) :(n))
#define MIN(n,m)                  ((m<n) ?(m) :(n))
#define resetDeepSleepDelay()     deepSleepDelay=DEEPSLEEPDELAY/LOOPDELAY
#define enableDeepSleep()         disableDeepSleep=false
#define disableDeepSleep()        disableDeepSleep=true
#define deepSleepEnabled()        disableDeepSleep==false
#define setAutoClose(n)           autoclose=((n<DEFAULTAUTOCLOSEDELAY) ?DEFAULTAUTOCLOSEDELAY :n)/LOOPDELAY;
#define resetAutoClose()          autocloseDelay=autoclose
#define disableAutoClose()        autoclose*=((autoclose>0) ?-1 :1)
#define isAutocloseEnabled()      autoclose>=0
#define shouldAutoclosing()       !autocloseDelay
#define shouldMeasureTime(d)      ((movingTime[d]<(0.8*MAXIMUMCLOSINGTIME)) || (movingTime[d]>MAXIMUMCLOSINGTIME))
#define unsetMeasureTime(d)       movingTime[d] =(unsigned long)(-1L)
#define isValidMovingTime(d)      movingTime[d]!=(unsigned long)(-1L)
#define initMovingTimeMeasure(d)  {startMovingTime[d]=MILLISEC(); if(shouldMeasureTime(d)){unsetMeasureTime(d); if(isClosed() || isOpened()) movingTime[d]=startMovingTime[d];}}
#define stopMovingTimeMeasure(d)  if(shouldMeasureTime(d)){if(isValidMovingTime(d) && getPin(LIGHT) && (isClosed() || isOpened())) movingTime[d]=MILLISEC()-startMovingTime[d]; else unsetMeasureTime(d);}
unsigned long    startMovingTime[]={0L,0L}, movingTime[]={(unsigned long)(-1L),(unsigned long)(-1L)};
unsigned short   WiFiCount=0, resetDeepSleepDelay();
int              setAutoClose(DEFAULTAUTOCLOSEDELAY);
unsigned int     blink=0, resetAutoClose();
bool place=true, direction=OPEN, activatedCommand=false, shouldReboot=false;
bool disableDetection=true, enableDeepSleep(), disableMovingTimeControl=false, overCoupleCheck=false, closedLimit=true, openedLimit=false;
String hostName= HOSTNAME;
String ssid[SSIDMax()];     //Identifiants WiFi /Wifi idents
String password[SSIDMax()]; //Mots de passe WiFi /Wifi passwords

portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
volatile bool value[PINSCOUNT], done[PINSCOUNT];
#define setIntr(i)    {portENTER_CRITICAL_ISR(&mux);value[i]=true; portEXIT_CRITICAL_ISR(&mux);}
#define unsetIntr()   {portENTER_CRITICAL_ISR(&mux);for(uint8_t i=OUTPUTSCOUNT+1; i<PINSCOUNT; i++)if(done[i])value[i]=done[i]=false;portEXIT_CRITICAL_ISR(&mux);}
#ifdef DEBUGG
  #define DEBUGG_println(n) Serial.println(n)
  #define DEBUGG_print(n)   Serial.print(n)
#else
  #define DEBUGG_println(n)
  #define DEBUGG_print(n)
#endif

#ifdef WITHWIFI
  bool WiFiAP=false;
  unsigned short nbWifiAttempts=MAXWIFIERRORS, WifiAPDelay;
  #ifdef WEBUI
    AsyncWebServer server(80);  //Instanciation du serveur port 80
//  AsyncWebSocket ws("/ws");
//  AsyncEventSource events("/events");
  #endif
#endif

inline bool getPin(uint8_t i) {return( (i<OUTPUTSCOUNT) ?(value[i]==HIGH) :((i<HIGHPINS) ?(digitalRead(gpio[i])==HIGH) :(digitalRead(gpio[i])==LOW)) );}

inline bool isOpening(){return  getPin(!place ?MOTORR :MOTORF);}
inline bool isOpened() {return (getPin(OPENINGLIMIT) || openedLimit);}
inline bool isClosing(){return  getPin(!place ?MOTORF :MOTORR);}
inline bool isClosed() {return (getPin(CLOSINGLIMIT) || closedLimit);}
inline bool isMoving() {return  getPin(LIGHT);}

void stopMotors(){
  digitalWrite(gpio[MOTORR], LOW); digitalWrite(gpio[MOTORF], LOW); digitalWrite(gpio[LIGHT], LOW);
  stopMovingTimeMeasure(direction);
  if(value[LIGHT] || value[MOTORF] || value[MOTORR]){
    value[MOTORR]=value[MOTORF]=value[LIGHT]=LOW;
    resetAutoClose(); resetDeepSleepDelay();
    delay(500);
    DEBUGG_println("(STOPPED)");
} }

void startOpening(){
  stopMotors();
  direction=OPEN;
  closedLimit=false;
  if(isOpened()){
    direction=!direction;
    DEBUGG_println("OPENINGLIMIT up...");
    return;
  } DEBUGG_println("(OPENING)");
  digitalWrite(gpio[LIGHT], value[LIGHT]=HIGH);
  initMovingTimeMeasure(direction);
  digitalWrite(gpio[!place ?MOTORR : MOTORF], value[!place ?MOTORR : MOTORF]=HIGH);
}

void startClosing(){
  stopMotors();
  direction=CLOSE;
  if(getPin(PHOTOBEAM)){
    DEBUGG_println("PHOTOBEAM up...");
    return;
  }openedLimit=false;
  if(isClosed()){
    direction=!direction;
    DEBUGG_println("CLOSINGLIMIT up...");
    return;
  } DEBUGG_println("(CLOSING)");
  digitalWrite(gpio[LIGHT], value[LIGHT]=HIGH);
  initMovingTimeMeasure(direction);
  digitalWrite(gpio[!place ?MOTORF : MOTORR], value[!place ?MOTORF : MOTORR]=HIGH);
}

void securityCheck(){
  if(isMoving()){

    //PhotoBeam check:
    if(isClosing() && getPin(PHOTOBEAM)){
      stopMotors();
      DEBUGG_println("PHOTOBEAM secure detect...");
    }

    //Limits check:
    if(isOpening() && getPin(OPENINGLIMIT)){
      stopMotors();
      openedLimit=true;
      direction=CLOSE;
      DEBUGG_println("OPENINGLIMIT secure detect...");
    }if(isClosing() && getPin(CLOSINGLIMIT)){
      stopMotors();
      closedLimit=true;
      direction=OPEN;
      DEBUGG_println("CLOSINGLIMIT secure detect...");
    }

    //maximumMovingTime check:
    if(!disableMovingTimeControl && (MILLISEC()-startMovingTime[direction])>=((shouldMeasureTime(direction)) ?MAXIMUMCLOSINGTIME :movingTime[direction])){
      stopMotors();
      if(direction==CLOSE){
        direction=OPEN;
        closedLimit=true;
      }else{
        direction=CLOSE;
        openedLimit=true;
      }DEBUGG_println("DEFAULTLIMITS secure detect...");
    }

    //Couples check:
    if(overCoupleCheck && (getPin(OPENCOUPLE) || getPin(CLOSECOUPLE))){
      stopMotors();
      direction=!direction;
      disableAutoClose();
      DEBUGG_println("OVERCOUPLE secure detect...");
    }

    //setPin motors error check:
    if(getPin(MOTORF) && getPin(MOTORR)){
      stopMotors();
      DEBUGG_println("DEFAULTMOTOR secure detect...");
    }
} }

void IRAM_ATTR photoBeam_intr()   {setIntr(PHOTOBEAM);}
void IRAM_ATTR limitOpen_intr()   {setIntr(OPENINGLIMIT);}
void IRAM_ATTR limitClose_intr()  {setIntr(CLOSINGLIMIT);}
void IRAM_ATTR openCouple_intr()  {setIntr(OPENCOUPLE);}
void IRAM_ATTR closeCouple_intr() {setIntr(CLOSECOUPLE);}
void IRAM_ATTR detect_intr()      {setIntr(DETECT);}
void IRAM_ATTR command_intr()     {setIntr(COMMAND);}
//void IRAM_ATTR place_intr()       {setIntr(AUTOCLOSE);}
//void IRAM_ATTR place_intr()       {setIntr(PLACE);}

#ifdef WITHWIFI
String getMyMacAddress(){
  char ret[18];
  uint8_t mac[6]; WiFi.macAddress(mac);
  sprintf(ret, "%02x:%02x:%02x:%02x:%02x:%02x",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  return ret;
}

String ultos(unsigned long v){ char ret[11]; sprintf(ret, "%ld", v); return ret; }
String  getPage(){
  String page="<html lang='us-US'><head><meta charset='utf-8'/>";
  page += "<title>" + hostName + "</title>\n";
  page += "<style>body{background-color:#fff7e6; font-family:Arial,Helvetica,Sans-Serif; Color:#000088;}\n";
  page += " ul{list-style-type:square;} li{padding-left:5px; margin-bottom:10px;}\n";
  page += " td{text-align:left; min-width:100px; vertical-align:middle;}\n";
  page += " .modal{display:none; position:fixed; z-index:1; left:0%; top:0%; height:100%; width:100%; overflow:scroll; background-color:#000000;}\n";
  page += " .modal-content{background-color:#fff7e6; margin:5% auto; padding:15px; border:2px solid #888; height:90%; width:90%; min-height:755px;}\n";
  page += " .close{color:#aaa; float:right; font-size:30px; font-weight:bold;}\n";
  page += " .duration{width:50px;}\n";
  page += "</style></head>\n<body onload='init();'>\n";
  page += "<script>\nthis.timer=0;\n";
  page += "function init(){var e;\n";
  page += "e=document.getElementById('example1');\ne.innerHTML=document.URL+'open'; e.href=e.innerHTML;\n";
  page += "e=document.getElementById('example2');\ne.innerHTML=document.URL+'JsonData'; e.href=e.innerHTML;\n";
  page += "refresh();}\n";
  page += "function refresh(v=60){this.timer=setTimeout(function(){location.reload(true);},v*1000);}\n";
  page += "function showHelp(){clearTimeout(this.timer); refresh(600); document.getElementById('about').style.display='block';}\n";
  page += "function saveSSID(f){\n";
  page += "if((f=f.parentNode)){var s, p=false;\n";
  page += "for(var i=0; i<f.children.length; i++){\n";
  page += "if(f.children[i].type=='password'){\n";
  page += "if (!p) p=f.children[i];\n";
  page += "else if(p.value!=f.children[i].value) p.value='';\n";
  page += "}else if(f.children[i].type=='text') s=f.children[i];\n";
  page += "}if(s.value==''){\n";
  page += "alert('Empty SSID...'); f.reset(); s.focus();\n";
  page += "}else if(p.value==''){\n";
  page += "var ssid=s.value; f.reset(); s.value=ssid;\n";
  page += "alert('Incorrect password...'); p.focus();\n";
  page += "}else f.submit();\n";
  page += "}}\n";
  page += "function deleteSSID(f){\n";
  page += "if((f=f.parentNode))\n";
  page += "for(var i=0; i<f.children.length; i++)\n";
  page += "if(f.children[i].type=='text')\n";
  page += "if(f.children[i].value!=''){\n";
  page += "if(confirm('Are you sure to remove this SSID?')){\n";
  page += "for(var i=0; i<f.children.length; i++)\n";
  page += "if(f.children[i].type=='password') f.children[i].value='';\n";
  page += "f.submit();\n";
  page += "}}else alert('Empty SSID...');\n";
  page += "}\n";
  page += "function submitSwitchs(){\n";
  page += "var e=document.getElementById('switchs'), l=e.getElementsByTagName('INPUT')\n";
  page += "for(var i=0; i<l.length; i++)\n";
  page += " if(l[i].type=='number')\n";
  page += "  l[i].value *= l[i].getAttribute('data-unit');\n";
  page += "e.submit();\n";
  page += "}\n";
  page += "</script><div id='about' class='modal'><div class='modal-content'>";
  page += "<span class='close' onClick='location.reload(true);'>&times;</span>";
  page += "<h1>About</h1>";
  page += "This device is a connected device that allows you to control the status of the controled sliding gate from a home automation application like Domotics or Jeedom.<br><br>";
  page += "In addition, it also has its own WEB interface which can be used to configure and control it from a web browser (the firmware can also be upgraded from this page). ";
  page += "Otherwise, its URL is used by the home automation application to control it, simply by forwarding the desired state (Json format), like this:";
  page += "<a id='example1' style='padding:0 0 0 5px;'></a> (-1 -> unchanged)<br><br>";
  page += "The state of the sliding gate can also be requested from the following URL: ";
  page += "<a id='example2' style='padding:0 0 0 5px;'></a><br><br>";
  page += "The following allows you to configure some networks parameters (until a private SSID is set, the socket works as an access point (AP) with its own SSID and default password: \"" + hostName + "/" + DEFAULTWIFIPASS + "\"), while the main page allows you to configure the operating parameters of the sliding gate itself.<br><br>";
  page += "By connecting to this default SSID and going to its website (port 80), the connection to private SSID (up to 3) can be initialized. If later, all of the defined networks become unavailable, the default SSID (AP) becomes periodically active again (also attempting to reconnect the private defines), thereby allowing to update new accessible private WIFI networks.<br><br>";
  page += "<h2><form method='POST'>";
  page += "Network name: <input type='text' name='hostname' value='" + hostName + "' style='width:110;'>";
  page += " <input type='button' value='Submit' onclick='submit();'>";
  page += "</form></h2>";
  page += "<h2>Network connection:</h2>";
  page += "<table style='width:100%'><tr>";
  for(int i=0; i<SSIDMax(); i++){
   page += "<td><div><form method='POST'>";
   page += "SSID " + String(i+1) + ":<br><input type='text' name='SSID' value='" + ssid[i] + (ssid[i].length() ?"' readonly": "'") + "><br>";
   page += "Password:<br><input type='password' name='password' value='" + String(ssid[i][0] ?password[i] :"") + "'><br>";
   page += "Confirm password:<br><input type='password' name='confirm' value='" + String(ssid[i][0] ?password[i] :"") + "'><br><br>";
   page += "<input type='button' value='Submit' onclick='saveSSID(this);'>";
   page += "<input type='button' value='Remove' onclick='deleteSSID(this);'>";
   page += "</form></div></td>";
  }page += "</tr></table>";
  page += "<h6><a href='update' onclick=\"javascript:event.target.port=8081\">Firmware update</a>";
  page += " - <a href='https://github.com/peychart/wifiPowerStrip'>Website here</a></h6>";
  page += "</div></div>";
  page += "<table style='border:0; width:100%;'><tbody><tr>";
  page += "<td><h1>" + hostName + " - " + (WiFiAP ?WiFi.softAPIP().toString() :WiFi.localIP().toString()) + " [" + getMyMacAddress() + "]" + " :</h1></td>";
  page += "<td style='text-align:right; vertical-align:top;'><p><span class='close' onclick='showHelp();'>?</span></p></td>";
  page += "<tr></tbody></table>";
  page += "<h3>Status :</h3>";
  page += "<form id='switchs' method='POST'><ul>";
  for (uint8_t i=0; i<OUTPUTSCOUNT; i++){
   page += "<li><table><tbody><tr><td>";
   page += (getPin(i) ?"1" :"0");
   page += "</td><td style='width:220px;'>";
   page += "<INPUT type='radio' name=";
   page += (getPin(i) ?"1" :"0");
   page += " value='1' onchange='submitSwitchs();'";
   page += (getPin(i) ?"checked" :"");
   page += ">ON </INPUT>";
   page += "(during <INPUT type='number'  name='noName-max-duration' value='-1' data-unit=1 min='-1' max='61' class='duration';/>-)</td>";
   page += "<td><INPUT type='radio' name=noName value='0' onchange='submitSwitchs();'";
   page += (getPin(i) ?"checked" :"");
   page += ">OFF</INPUT>";
   page += "</td></tr></tbody></table></li>";
  }page += "</ul></body></html>";
  return page;
}

bool WiFiHost(){
  DEBUGG_println();
  DEBUGG_println("No custom SSID defined: setting soft-AP configuration ... ");
  WiFi.mode(WIFI_AP);
  WiFiAP=WiFi.softAP(hostName.c_str(), password[0].c_str());
  DEBUGG_println(String("Connecting [") + WiFi.softAPIP().toString() + "] from: " + hostName + "/" + password[0]);
  nbWifiAttempts=(nbWifiAttempts==-1 ?1 :nbWifiAttempts); WifiAPDelay=60;
  return WiFiAP;
}

bool WiFiConnect(){
  if(WiFi.status()==WL_CONNECTED){
    ESP.restart();
//  ArduinoOTA.end();
    return true;
  }delay(10);

  if(!ssid[0][0] || !nbWifiAttempts--)
    return WiFiHost();

  for(uint8_t i=0; i<SSIDMax(); i++) if(ssid[i].length()){
    DEBUGG_println(); DEBUGG_println();

    //Connection au reseau Wifi /Connect to WiFi network
    WiFi.mode(WIFI_STA);
    DEBUGG_println("Connecting [" + getMyMacAddress() + "] to: " + ssid[i]);
    WiFi.begin(ssid[i].c_str(), password[i].c_str());

    //Attendre la connexion /Wait for connection
    for(uint8_t j=0; j<16 && WiFi.status()!=WL_CONNECTED; j++){
      delay(500);
      DEBUGG_print(".");
    } DEBUGG_println();

    if(WiFi.status()==WL_CONNECTED){
      //Affichage de l'adresse IP /print IP address//flag to use from web update to reboot the ESP
      DEBUGG_println("WiFi connected");
      DEBUGG_println("IP address: " + WiFi.localIP().toString());
      DEBUGG_println();
      nbWifiAttempts=MAXWIFIERRORS;
      return true;
    }else DEBUGG_println("WiFi Timeout.");
  }
  return false;
}
#endif

void writeConfig(){        //Save current config:
  uint8_t i;
  File f=SPIFFS.open("/config.txt", "w+");
  if(f){
    f.println(ResetConfig);
    f.println(hostName);                //Save hostname
    for(uint8_t j=i=0; j<SSIDMax(); j++){   //Save SSIDs
      if(!password[j].length()) ssid[j]="";
      if(ssid[j].length()){
        f.println(ssid[j]);
        f.println(password[j]);
        i++;
    } }while(i++<SSIDMax()){f.println(); f.println();}
    f.println(disableDetection);
    f.println(autoclose);
    f.println(place);
    f.println(movingTime[OPEN]);
    f.println(movingTime[CLOSE]);
    f.close();
  }
  #ifdef WITHWIFI
    if(WiFiAP && ssid[0].length()) WiFiConnect();
  #endif
}

String readString(File f){ String ret=f.readStringUntil('\n'); ret.remove(ret.indexOf('\r')); return ret; }
void readConfig(){      //Get config:
  File f=SPIFFS.open("/config.txt", "r");
  if(f && ResetConfig!=atoi(readString(f).c_str())) f.close();
  if(!f){                         //Write default config:
    ssid[0]=""; password[0]=DEFAULTWIFIPASS;
    hostName=HOSTNAME;
    SPIFFS.format(); writeConfig();
  }else{                          //Get config:
    hostName=readString(f);                //Get hostname
    for(uint8_t i=0; i<SSIDMax(); i++){        //Get SSIDs
      ssid[i]=readString(f);
      password[i]=readString(f);
    }
    disableDetection=atoi(readString(f).c_str());
    if(deepSleepEnabled() && (COMMAND>HIGHPINS || DETECT>HIGHPINS))  disableDetection=true;
    autoclose=atoi(readString(f).c_str());
    place=atoi(readString(f).c_str());
    movingTime[OPEN] =atol(readString(f).c_str()); if(shouldMeasureTime(OPEN))  unsetMeasureTime(OPEN);
    movingTime[CLOSE]=atol(readString(f).c_str()); if(shouldMeasureTime(CLOSE)) unsetMeasureTime(CLOSE);
    f.close();

    if(!ssid[0].length())        //Default values
      password[0]=DEFAULTWIFIPASS;
} }

#ifdef WEBUI
void handleSubmitSSIDConf(AsyncWebServerRequest *request){           //Setting:
  int count=0;
  for(uint8_t i=0; i<SSIDMax(); i++) if(ssid[i].length()) count++;
  for(uint8_t i=0; i<count; i++)
    if(ssid[i]==request->arg("SSID")){
      password[i]=request->arg("password");
      if(!password[i].length())    //Modify password
        ssid[i]=="";               //Delete ssid
      writeConfig();
      if(!ssid[i].length()) readConfig();
      return;
  }if(count<SSIDMax()){            //Add ssid:
    ssid[count]=request->arg("SSID");
    password[count]=request->arg("password");
  }writeConfig();
}

void  handleRoot(AsyncWebServerRequest *request){
  if(request->hasArg("hostname")){
    hostName=request->hasArg("hostname");              //Set host name:
    writeConfig();
  }else if(request->hasArg("password"))                //Set WiFi connections:
    handleSubmitSSIDConf(request);
  request->send(200, "text/html", getPage());
}
#endif

void handleInterrupt(){
// Inputs treatment:
  for(uint8_t i=OUTPUTSCOUNT+1; i<PINSCOUNT; i++){
    if(!value[i] || done[i]) continue;
    done[i]=true;
    switch(i){
      case OPENINGLIMIT:
        if(isOpening()){
          stopMotors();
          openedLimit=true;
          direction=CLOSE;
        } DEBUGG_println("OPENINGLIMIT detected...");
        break;
      case CLOSINGLIMIT:
        if(isClosing()){
          stopMotors();
          closedLimit=true;
          direction=OPEN;
        } DEBUGG_println("CLOSINGLIMIT detected...");
        break;
      case PHOTOBEAM:
        if(isClosing()) stopMotors();
        DEBUGG_println("PHOTOBEAM detected...");
        break;
      case OPENCOUPLE:
      case CLOSECOUPLE:
        if(!overCoupleCheck)
          break;
        stopMotors();
        direction=!direction;
        disableAutoClose();
        DEBUGG_println("OVERCOUPLE detected...");
        break;
      case DETECT:
        if(!disableDetection && !isMoving())
          startOpening();
        DEBUGG_println("DETECTION detected...");
        break;
      case COMMAND:
        if(activatedCommand)
          break;
        activatedCommand=true;
        if(isMoving()) {
          stopMotors(); direction=!direction;
          DEBUGG_println("COMMAND stop motor enabled...");
        }else{
          if(getPin(PHOTOBEAM)) direction=OPEN;
          if(direction==OPEN)
               startOpening();
          else startClosing();
          DEBUGG_println("COMMAND start motor enabled...");
        }break;
/*      case AUTOCLOSE:
        DEBUGG_println("AUTOCLODE DETECTED...");
        if(!disableAutoclose && !isMoving())
          startClosing();
        break;
      case PLACE:
        delay(LOOPDELAY); place=getPin(PLACE);
*/
  } }
  securityCheck();
}

void OTASetup(){;
  #ifdef OTA
  SPIFFS.end();
//Allow OnTheAir updates:
  //See: http://docs.platformio.org/en/latest/platforms/espressif32.html#authentication-and-upload-options
  //MDNS.begin(hostName.c_str());
  ArduinoOTA.setHostname(hostName.c_str());

  // No authentication by default
  // ArduinoOTA.setPassword("admin");
  // Password can be set with it's md5 value as well
  //ArduinoOTA.setPasswordHash("$1$vBpbvvV6$UOWSqzPn4kFizSWcg48Vy.");
  ArduinoOTA.setPassword("XXXXXX");

  // Port defaults to 3232
  ArduinoOTA.setPort(30499);

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
  else // U_SPIFFS
      type = "filesystem";
      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

  SPIFFS.begin();
  #endif
}

void setup(){
  Serial.begin(115200);

  //Open config:
  SPIFFS.begin();
  readConfig();

  //initialisation des broches /pins init
  uint8_t i=0; for(; i<OUTPUTSCOUNT; i++){   //Sorties/ouputs:
    pinMode(gpio[i], OUTPUT);
    digitalWrite(gpio[i], LOW);
  }for(i++; i<PINSCOUNT; i++){                //Entrées/inputs:
//  pinMode(gpio[i], (i<HIGHPINS ?INPUT :INPUT_PULLUP));
    pinMode(gpio[i], INPUT);
    //See: https://www.arduino.cc/en/Reference/attachInterrupt
    // or: https://www.arduino.cc/reference/en/language/functions/external-interrupts/attachinterrupt/
    //attachInterrupt(gpio[i], switching, FALLING);

    //with ESP32, see: https://techtutorialsx.com/2017/09/30/esp32-arduino-external-interrupts/
    //                 http://www.iotsharing.com/2017/06/how-to-use-interrupt-in-arduino-esp32.html
    //                 http://www.iotsharing.com/2017/06/how-to-use-interrupt-in-arduino-esp32.html
    switch(i){
      case PHOTOBEAM:   attachInterrupt(digitalPinToInterrupt(gpio[i]), photoBeam_intr,   (i<HIGHPINS ?RISING :FALLING)); break;
      case OPENINGLIMIT:attachInterrupt(digitalPinToInterrupt(gpio[i]), limitOpen_intr,   (i<HIGHPINS ?RISING :FALLING)); break;
      case CLOSINGLIMIT:attachInterrupt(digitalPinToInterrupt(gpio[i]), limitClose_intr,  (i<HIGHPINS ?RISING :FALLING)); break;
      case OPENCOUPLE:  attachInterrupt(digitalPinToInterrupt(gpio[i]), openCouple_intr,  (i<HIGHPINS ?RISING :FALLING)); break;
      case CLOSECOUPLE: attachInterrupt(digitalPinToInterrupt(gpio[i]), closeCouple_intr, (i<HIGHPINS ?RISING :FALLING)); break;
      case DETECT:      attachInterrupt(digitalPinToInterrupt(gpio[i]), detect_intr,      (i<HIGHPINS ?RISING :FALLING)); break;
      case COMMAND:     attachInterrupt(digitalPinToInterrupt(gpio[i]), command_intr,     (i<HIGHPINS ?RISING :FALLING)); break;
//    case AUTOCLOSE:   attachInterrupt(digitalPinToInterrupt(gpio[i]), autoclose_intr,   (i<HIGHPINS ?RISING :FALLING)); break;
//    case PLACE:       attachInterrupt(digitalPinToInterrupt(gpio[i]), place_intr,       (i<HIGHPINS ?RISING :FALLING)); break;
  } }
  //place=getPin(PLACE);
  for(uint8_t i=0; i<PINSCOUNT; i++) value[i]=done[i]=false;
  openedLimit|=getPin(OPENINGLIMIT); closedLimit&=!openedLimit;

  //Awake from deepSleep with AWAKE pin;
  if(digitalRead(gpio[COMMAND])==LOW){   //It's a wake up...
    setIntr(COMMAND); direction=OPEN; closedLimit=true;
  }

  //Pin(s) wake up config:
  if(COMMAND>HIGHPINS || DETECT>HIGHPINS)
        esp_sleep_enable_ext0_wakeup(gpio[COMMAND], LOW);
  else  esp_sleep_enable_ext1_wakeup(BIT(gpio[COMMAND]) & BIT(gpio[DETECT]), ESP_EXT1_WAKEUP_ANY_HIGH);

  #ifdef WEBUI
  //Definition des URL d'entree /Input URL definition
  server.on("/",                          HTTP_GET, [](AsyncWebServerRequest *request){handleRoot(request);});
  server.on("/open",                      HTTP_GET, [](AsyncWebServerRequest *request){startOpening();  handleRoot(request);});
  server.on("/close",                     HTTP_GET, [](AsyncWebServerRequest *request){startClosing();  handleRoot(request);});
  server.on("/stop",                      HTTP_GET, [](AsyncWebServerRequest *request){stopMotors();    handleRoot(request);});
  server.on("/allowAutoClose",            HTTP_GET, [](AsyncWebServerRequest *request){setAutoClose(DEFAULTAUTOCLOSEDELAY); writeConfig(); handleRoot(request);});
  server.on("/disableAutoClose",          HTTP_GET, [](AsyncWebServerRequest *request){disableAutoClose();                  writeConfig(); handleRoot(request);});
  server.on("/allowDetection",            HTTP_GET, [](AsyncWebServerRequest *request){disableDetection=((deepSleepEnabled() && (COMMAND>HIGHPINS || DETECT>HIGHPINS)) ?true :false); writeConfig(); handleRoot(request);});
  server.on("/disableDetection",          HTTP_GET, [](AsyncWebServerRequest *request){disableDetection=true;               writeConfig(); handleRoot(request);});
  server.on("/enableDeepSleep",           HTTP_GET, [](AsyncWebServerRequest *request){enableDeepSleep();                   writeConfig(); handleRoot(request);});
  server.on("/disableDeepSleep",          HTTP_GET, [](AsyncWebServerRequest *request){disableDeepSleep();                  writeConfig(); handleRoot(request);});
  server.on("/forward",                   HTTP_GET, [](AsyncWebServerRequest *request){place=false;                         writeConfig(); handleRoot(request);});
  server.on("/reverse",                   HTTP_GET, [](AsyncWebServerRequest *request){place=true;                          writeConfig(); handleRoot(request);});
  server.on("/enableCloupeCheck",         HTTP_GET, [](AsyncWebServerRequest *request){overCoupleCheck=true;                writeConfig(); handleRoot(request);});
  server.on("/disableOverCoupleCheck",    HTTP_GET, [](AsyncWebServerRequest *request){overCoupleCheck=false;               writeConfig(); handleRoot(request);});
  server.on("/enableMovingTimeControl",   HTTP_GET, [](AsyncWebServerRequest *request){disableMovingTimeControl=false;      writeConfig(); handleRoot(request);});
  server.on("/disableMovingTimeControl",  HTTP_GET, [](AsyncWebServerRequest *request){disableMovingTimeControl=true;       writeConfig(); handleRoot(request);});
  server.on("/about",                     HTTP_GET, [](AsyncWebServerRequest *request){request->send(200, "text/plain", "Hello world!..."); });
  #endif

  DEBUGG_println("Hello World!");
}

////////////////////////////////////// LOOP ////////////////////////////////////////////////
void loop(){
  if(WiFi.status()==WL_CONNECTED)
    ArduinoOTA.handle();

  if(shouldReboot){
    DEBUGG_println("Rebooting...");
    delay(500);
    ESP.restart();
  }if(isClosed()){
    if(deepSleepEnabled() && !--deepSleepDelay){ //deepSleep management:
      DEBUGG_println("DEEPSLEEP!...");
      writeConfig();
      esp_deep_sleep_start();
  } }
  else if(isAutocloseEnabled() && !isMoving()){
    if(shouldAutoclosing())
      startClosing();
    else autocloseDelay--;
  }if(activatedCommand && !getPin(COMMAND) && !value[COMMAND])
    activatedCommand=false;

  //commands manaement:
  handleInterrupt();
  delay(LOOPDELAY/2);
  handleInterrupt();
  delay(LOOPDELAY/2);
  unsetIntr();

  //WiFi management:
  #ifdef WITHWIFI
    if(!WiFiCount){
      if(!isMoving()){ WiFiCount=60000/LOOPDELAY;            //Test connexion/Check WiFi every mn
        if(WiFi.status()!=WL_CONNECTED && (!WiFiAP || !WifiAPDelay--)){
          if(WiFiConnect()){;
            #ifdef OTA
              OTASetup();
            #endif
            #ifdef WEBUI
              server.begin();
              DEBUGG_println("WEBServer started on port 80.");
            #endif
    } } } }
    else WiFiCount--;
  #endif

  //Light blinking emulation:
  if(EMULATEBLINKING && getPin(LIGHT) && ++blink>MAX(EMULATEBLINKING,1000)/LOOPDELAY){
    if(digitalRead(gpio[LIGHT])==HIGH){
      digitalWrite(gpio[LIGHT], LOW);
      blink=MIN(EMULATEBLINKING-1000,1000);
    }else{
      digitalWrite(gpio[LIGHT], HIGH);
      blink=0;
  } }
}
