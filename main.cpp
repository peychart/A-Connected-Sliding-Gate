//References: https://github.com/espressif/arduino-esp32 & https://www.arduino.cc/en/Reference/HomePage
//See: https://github.com/espressif/arduino-esp32/tree/master/libraries
//and: https://github.com/me-no-dev/ESPAsyncWebServer#using-platformio
//peychart@netcourrier.com 20171021
// Licence: GNU v3

#define DEBUGG
#define WITHWIFI
#define OTAPASSWD "30499"
#define WEBUI

#include <FS.h>
#include <SPIFFS.h>
#include <esp_sleep.h>
#ifdef WITHWIFI
  #include <WiFi.h>
  #ifdef WEBUI
    #include <AsyncTCP.h>
    #include <ESPAsyncWebServer.h>
    //OTA:
  #endif
  #ifdef OTAPASSWD
    //See: https://github.com/me-no-dev/ESPAsyncWebServer#setting-up-the-server
    #include <ESPmDNS.h>
    #include <ArduinoOTA.h>
    #include <Hash.h>
  #endif
#endif

//Ajust the following:
uint8_t ResetConfig   = 1;      //Just change this value to reset current outputs config on the next boot...
#define HOSTNAME        "Sliding-Gate" //Can be change by interface
#define DEFAULTWIFIPASS "defaultPassword"
#define MAXWIFIERRORS   10      //Attempts before host mode...
#define SSIDMax()       3
enum    pins            {  MOTORR,      MOTORF,       LIGHT,   OUTPUTSCOUNT, OPENTORQUE,  CLOSETORQUE, /* AUTOCLOSE,     PLACE,*/     HIGHPINS,    COMMAND,     DETECT,     PHOTOBEAM,  OPENINGLIMIT, CLOSINGLIMIT, PINSCOUNT};
gpio_num_t gpio[]     = {GPIO_NUM_13, GPIO_NUM_12, GPIO_NUM_14, GPIO_NUM_0,  GPIO_NUM_35, GPIO_NUM_34, /*GPIO_NUM_22, GPIO_NUM_21,*/ GPIO_NUM_0, GPIO_NUM_32, GPIO_NUM_33, GPIO_NUM_26,  GPIO_NUM_18,  GPIO_NUM_19           };

//Avoid to change the following:
#define LOOPDELAY                 250
#define OPEN                      true
#define CLOSE                     false
#define DEEPSLEEPDELAY            120000
#define DEFAULTAUTOCLOSEDELAY     30000
#define MAXAUTOCLOSEDELAY         120000
#define EMULATEBLINKING           1500
#define MAXIMUMCLOSINGTIME        25000
#define MILLISEC()                (millis()+MAXIMUMCLOSINGTIME)
#define MAX(n,m)                  ((m>n) ?(m) :(n))
#define MIN(n,m)                  ((m<n) ?(m) :(n))
#define resetDeepSleepDelay()     deepSleepDelay=DEEPSLEEPDELAY/LOOPDELAY
#define enableDeepSleep()         disableDeepSleep=false
#define disableDeepSleep()        disableDeepSleep=true
#define deepSleepEnabled()        disableDeepSleep==false
#define setAutoClose(n)           autoclose=( (n<0) ?-1 :(((n<DEFAULTAUTOCLOSEDELAY) ?DEFAULTAUTOCLOSEDELAY :n)/LOOPDELAY) );
#define resetAutoClose()          autocloseDelay=autoclose
#define disableAutoClose()        setAutoClose(-1)
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
bool disabledDetection=true, enableDeepSleep(), disableMovingTimeControl=false, overTorqueCheck=false, closedLimit=true, openedLimit=false;
#ifdef WEBUI
//see: PNG to base64 converter...
String gateMovingPng= "image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAaoAAABkCAYAAADXGnSGAAAAAXNSR0IDN8dNUwAAAAlwSFlzAAALEgAACxIB0t1+/AAABHtJREFUeNrt3N9rW2UYB/AnS5p0drPrilXcxjrrpFD8wXSiSKVlXk0Rb/Rm4J3+I/4NIuitXujF6IW9tui9XhUGim4VJ46VUTthCUljy9qOtulZ2pw0ycnnA4WQNM95z/u+J9/znvzI1ddFRiy0+PyZAMeCY4FuU8jSzszOLR76ufPTUxGjJgSOBccC3eaYLgBAUAGAoAJAUAGAoAIAQQWAoAIgUX3zj0QFXQDQITldYEUFgKACgHZy6Y+WLegCY0pXmhFU8Min33wf+/2+cS63/4X4pYHn43z1jz337621llijF5yr/LrvY0l99PDxfOLje/pr6HxLbf3k6/mHvb7erHqL76PkN4cujVpb9dKq9Ti3CxOJY5f6uCXM8+rmth7Xpq12fXntPSsqSCssbhUubN8uF2t7Hi9V8pnooz+LF1Pfx921HtWptdjWCZO6wdhpk6CihzV68e100KQdelkO0Ub7mtV9y4pqqRy1en+8hAsqeiLs6I3+6uaTg6ydaOSiGEmXCrPEp/4AelCp2j/7akUFkMIK7ahXZ4U++kkLKyqAHjCUL++8neufS+VWVABdsipLWpn9VyvtuD0Wy7FSPCWoIA0PPpps6v9yc4sN779+ZTKujh58u2nWG/zuRmq1GrUr7Tr77ftBpNmmDWmNYZrz4bC1Gs2HTsyFNMa5F7j0B4CgAgBBBYCgAgBBBQCCCgBBBQCCCgBBBQCCCgAEFQCCCgAEFQCCCgAEFQAIKgAEFQAIKgAEFQAIKgAQVAAIKgAQVAAIKl0AgKACAEEFgKACAEEFAIIKAEEFAIIKAEEFAIIKAAQVAIIKAAQVAIIKAAQVAAgqAAQVAAgqAAQVAAgqABBUAAgqABBUAAgqABBUACCoABBUACCoAEBQASCoAEBQASCoAEBQAYCgAkBQAYCgAkBQAYCgAgBBBYCgAgBBBUA/KugC2m2hS2q0q95CRuuolY0xzIJcfV1mdmZu8dDPnZ+eiqujJsRhD6rZhL4vVfJN1SkXay09v5l6h62VZr2jqLNfXzZ7LLz702JH960d9drdtk70V9I4//DBVMxYUQHQTGh0ok6WCCq6WitnvP22T1nsKxBUCDlBIPQQVODFsX/6q5tPDsyv3uXj6QBYUZFtM+t/P14+G6uDw13Rno+/+iKWJ9+O4fq9WMmNbJ9N736T+lL13/hs9s2e3WaSjU/tteL6lUkTu4kxbuSoxjjJyQcrMS2oYKfpM8Nd05bXnhuPperf8fva2YiEj7zPXngqta8kdGKb7eSrGk2OcXl1fWBPduEYD2eq7136I3MmTo/F8tLtHfdtrWzO1Y/HxTt348PqrXhxsLe3Sefn1VZIGWMrKjiQz995Ne7lBuLnf/6Km3FmOzQ2znbvRCVmThyPN8ZH4+WxkZ7eJtmfVwgqMuz9Sy/FyI3f4pebS7FarsWx+7ko5fIx8cyz8fr4C/HW08V45fQTPb9Nsj+vyNhPKMFu396txf1KJQrH8vHkUDFOrL+GnFqLuDyQrW2S/XklqACgS/0PWnCl069GubEAAAAASUVORK5CYII=";
String gateOpenedPng= "image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAakAAABmCAYAAABx5W6OAAAAAXNSR0IDN8dNUwAAAAlwSFlzAAALEgAACxIB0t1+/AAABLVJREFUeNrt3dtrHGUYB+B3sjm09mQarGJbiK2VQPBAtaKUSEK9kCoigr0RvPQf8W8QQa+90IuSC3spBr3Xq0BAkRqxYmkpPQhmuwc3sYlJO9lkd2ezs7PPA5sMyeT9Zr+Zb37z7SGb1BsCBtj9WB0CtSg1vg7Va6uLEUlEeSiJytoaQ2tfkwfr///9v59vHkD1TeuMNG4l3QsdSYQUAHk1vL6w0GGhWX1JwRgTkKOQmptfbLvIlZnpiAmdSbEYE9B7Q7oAACEFAEIKACEFAEIKACEFADupx9Z3sHfZsB4HYNcSMykAEFIACCkAaIvnpBhICwVpA4pqVkgx6D7+8pto+iEAB091VP+tr5f6un9Oln/e9ndJ0vzZ8yRp/iElj/Z7LcNarf2+mVrjbtYzeKFAqZZdrZ1cGz7ddP/t5b5br3f/wTo7bdv69n3+4TtmUrA88uwOa1QHun9+Hz2z9n1l9NF+GCu390lZea1V5P3X79smpBhYaSe5jSvfpNK4ZBzu+fZ0ctJ1EtdXeVUZW4nqLseXkIK0hxxiNHZ6GIPsLgp6WSvPgVfU8GxlfHl1H6QYq+gDyMP4MpOC1IFR1wkUfjbbq1lZK+PLTArSJNXMTwgwqA6UVrYutzC+hBSkOFy503ENT7xT1NnYw7ed/F0d27LcyvjycB+kWL50PpL5xY7rXL4wFRcndjl526a9Vmqs27fNe7TaqZW2XVnVyUOtLPsqy/7Kulba/czj8WAmBUDfEFIACCkAEFIACCkAEFIACCkAEFIAIKQAEFIAIKQAEFIAIKQAQEgBIKQAQEgBIKQAQEgBgJACQEgBgJACQEgBgJACACEFgJACACEFgJACACEFAEIKACEFAEIKACEFAEIKACEFAEIKAIQUAEIKAIQUAEIKAIQUAAgpAIQUAAgpAIQUAAgpABBSAAgpABBSAAgpABBSACCkABBSACCkABBSACCkAEBIASCkAEBIASCkAEBIASCkAEBIAYCQAkBIAUBXDesCeNRCzmot5PC+LRS4v9Xq7TZtltQb1hbmF9sucmVmOi5OOLHRXyE01+SYHyuXYmW02tGYeP/bpZb+Jq291e1oV1b1ulknD7Wy7vu81kqrl8fjYdV3703HrJkUtDeAWqnRyQkF+uE4z7JOGiEFXVKUgOrm/ciytguCYhJSUNAwcNLWV0IKwKzJ7LCLvAQdADMpyJPZxu37cyfi7r4j267z9g+LHbVx+cLUrtb76IvP4ubUG3GkfituJ+MbV9APPxl9tnInPpl7PZP734s22Xtp+zlNnvbzoX9ux4yQgoiZ40e6Wn+3b8t45dRkLFf+jF9rJyKavLR67pknMnurRy/aZO+l7ueVu42deyjH+3nruPRwH/TY6aPH4ubytS0/W5/RnKzvjzPXb8QHld/i+X393Sb5OLbWA6pf9rOZFPTYp2++HLeSkfjxrz/iahzfCIzVK9zrUY7Zg/vjtcmJePHYeF+3yWAcW0IKCujdsy/E+NIv8dPV5bi7Uo2he0mMJaU4/dTT8erkc3H+ydF46ehjfd8mg3FsZWnj3yIBvffVjWrcK5djeKgUhw+MxsHGuePxWsS5kWK1yWAcW0IKgEL7F2J0wJ6hnb2LAAAAAElFTkSuQmCC";
String gateClosedPng= "image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAacAAABkCAYAAAAi5P82AAAAAXNSR0IDN8dNUwAAAAlwSFlzAAALEgAACxIB0t1+/AAABPRJREFUeNrt3d1vU2UcB/Df6co6ZAgDBCOSIMsIySIafInGYCB6YTAxxMRb/xYS/wbjhX+BXBhu8M5kiXde6BXJgoZMFIyEiQoktqw9tmytvHTdS0/Xc7rPJ9m6tNvznD7nec53z3NO2yRtijXMRX/OBDC0sdVYHuJpEo/dliKxA8it8np+6eylK5uu4PLp2Yj9GhqMLVi/kiYAoJAzJ6C40idW75L2Qr5VPYQTMCzVRi1ap5ZL6fJ5prFS83uSRCNdvm19gXACtn721PxqJCZLCCcgJx7Ojtoh1ZoxhZPNCCcgJwH15AwqSbULwgkYWjClD9OosTJfqrfOPKVJlCzzIZyAYRlPKl0SS7uQb5aeASjezGkug0rmtDMMRHtsle9FLE3+//OhHREzFe3DiIbTxMX5lXWB/ir5oF3OBlXH60/dV6mN2WuPOFL7adXH1nr9SpL0bsu13nZxHW/LuKrWSfl0wEtLN8vTQ2ijxrrLu/Hbg4jj0309x15vf5SenzVAGN2ZU3nhalT7HEB5MKpB9+v4TObPb9Taqt1G+Rt8Vx2BYLPhxGAP3KM+OzT77W6pUjX4QDjRT5gIuewl/a6Vw4hztR4MQWVJG4CZE+RsJlaONOQTmDlBbuwaq0YkdQ0BZk6QnxnY/XolDsZi/Kn5IB/h9PV7J+LcBj5WOunyGo6NlpF1WckqryvZTFkTq7z+K6vt8vzyv11JHx/TDqPMsh4AwgkAhBMAwgkAhBMAwgkAhBMAwgkAhBMACCcAhBMACCcAhBMACCcAhBMACCcAhBMACCcAEE4ACCcAEE4ACCcAEE4ACCcAEE4AIJwAEE4AIJwAEE4AIJwAEE4AIJwAEE4AIJwAQDgBIJwAQDgBIJwAQDgBIJwAQDgBgHACQDgBgHACQDgBgHACQDgBgHACQDgBgHACAOEEgHACAOEEgHACAOEEgHACAOEEAMIJAOEEAMIJAOEEAMIJAOEEAMIJgO2rvNUVzuWkjO1Sluc3GtsF202SNq324MTF+SgvXI37x6c3XcHl07Px8bfzm/rb6nj9qfsqtbGhlpXlNuVxu7qVo90HU04WY+vD766s+nh6ftYRDjOnQejn4ANZBfOgyilrcije+GgN8CzDadSDbpDPzz8JgHB65ICYdUDl8UAtNAEKFE5bOasi/2Fif8P2kutLydsHpFYYtdfxszovAEB+9bxa75vF5dteVwStpXVF0UZ9+uUXsXji3diT3ol/HxzozJSeDKZTS//EZ2ff7rsRHq3v72SqE4yDqo9i69ZfullPf+l3bH1/41pMNtI4UC7HQqPe+Xl6am+cPrzHzqKwei7rndufTSUbLef1Y0fj+tLvca3xYue+avpX8/vux2ZVZ196LpNtfKy+HpdSZ1Ufxda1v1TvNjvJ7i3vLxdOHrNDGEm5XNab3ncwFq/fXB7z7cG/MvCPpDtj5tbt+GTpl3h5Ivv6OmG4Uu8g6qPYuvWXQfZPMHPKic/ffy3uJDvihz9uxEIc7oRF67/RW1GLM5M7462j++OVg1OFrI9i019gm4ZTy0enTsbU/M/x48L1uFutR+leEpVkLKaffyHePHo83jk0Hq/ue6aw9VFs+gsMVs8LIjq/dKm/k7b9rLt/dbse92q1KJfG4tld4zHZHO97GxFv7BhMg2x1fRRbv/1lmGMLCh9OALCVfGQGALnzH2AeyoCCyG3NAAAAAElFTkSuQmCC";
String stopGatePng="image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAYAAABzenr0AAAAAXNSR0IDN8dNUwAAAAlwSFlzAAALEgAACxIB0t1+/AAAACxJREFUWMPtzkEBADAIAKHT/p23GH4gAVO9Dm3HBAQEBAQEBAQEBAQEBAQEPiESAT9AXSQQAAAAAElFTkSuQmCC";
String openGatePng= "image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAYAAABzenr0AAAAAXNSR0IDN8dNUwAAAAlwSFlzAAALEgAACxIB0t1+/AAAAWlJREFUWMPF18srRVEUx/FzL4q8ijCXhCElJiZGkgGZGohSMjFlwgQDxpIUZWAuj5GhR8o/oKRQHEoKV97fXXtJ0o3TOWud+kxvv3PX3ue3dxAEwQbOMIeGwOC5wgeecYhhVGoGCH0AkcE2OlFiEUBcYgXNyLUIIE4wiVqrAM4bDjCEMosA4gHr6EahRQBxjXk0xbE+ogRw3nGKCdRYBBAv2EM/KiwCiEdsogMFFgHEBRbQiByLAOIY46i2CiD9sosBlCKtHUDcYw3tyLcIIG78Qv1apGnF1n315447/2lX/QdcoY2h/reXTjKA++0lX+luS6a0FmHGl1YXijW3oZvtPkZQrv0ldMU0FaWYwhjmvIgW5GmWkSufHfSgSLOO3X4+wmBcx/fwn3OejvsC85cAt1hGa9Q5Rw3whC30JnlJCbPMeRRV2jcjVxgzqNMqq/Nv+3kVbUnMOdsz6y+jff7UkuST+nEgyf8EvirPglqaXooAAAAASUVORK5CYII=";
String closeGatePng=
"image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAYAAABzenr0AAAAAXNSR0IDN8dNUwAAAAlwSFlzAAALEgAACxIB0t1+/AAAAcRJREFUWMO9100rRFEcx/ExFxnynAkZTZKFJeUNWMnKSxClvAUbXoC1LO3sLESSZzUTysKCokTIwwxRZoQZfE/9r24TY+517jn1Sdn8fl3/+z9XIGD+dGISF1gwGRzGKPbwhk/cmgiuQj+W8CLBtjs/g4vRgxnc5AT7XqADEzj9Jdi3AnUYwQ6yf4RrLVCBAcwjVUCwtgLq79yNKSRcBGsp0I5xnOHDQ7jnAg0YRBzvHoM9FQihD4tI/zPYVQELXZjGlabgggu0YQwnmoPzFgiiGkOIOfa2kQJl6MUsnn0M/rFASAYsaSD4u0DQUUCtzie5pzOm7mgrp8AcNuX2UgNY63N+2sr5hXos9zJ8MSnYIrveSAFnEfXOr+MAlWiV3W+kgH3UK3iMZVyjHs0oMlXAPupTahcbsobV06gxWcA+D9iS+VAn8s/5cF0gIFfvpczHkXztNqLESwFdn9vD2Jf94WoRWRoKpCR8W9Z3xMV8pC2Nr1RC5iMur2tU7hZjBZzzsYZD+ackkmd/aC9gn1cpsCI3XlgETRVwzodzf0TlqRgrYK/1pFxyqkwpmmR/PFoyKBnHB0nWpyJqPs6xKj/L1ax8AWUFSZYuNgtmAAAAAElFTkSuQmCC";
String disabledOpenGatePng= "image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAYAAABzenr0AAAAAXNSR0IDN8dNUwAAAAlwSFlzAAALEgAACxIB0t1+/AAAABR6VFh0YXV0aG9yAAB42ivIyMwBAAQ6Aa41gl/YAAABb0lEQVRYw8WXuy8FQRSH514EDaGUIAoJCkFDvAr+LqW/QyEoxaOh0lEQnSBCxCverrd4XN8k58QqXLv3zsz+ki+7m2x2vuyZ2T2TMcaMm59MwZYJmDIYjVx3QhscwlMaAjY10AfVcA5voQU0jTAIjyLyFVpAY0syBKdwnYaATRa6oBUO4CW0gKYW+uV4BO+hBTQNUpYcXJYyP4oV0LTLRD2GmzQE9Bnd0Awn8BxaQFMv349KEfkILaBpgmG4gwvIhxbQdECvvI3bNARsKqAHWmBflm0+pICmDgbk/Cw6P7ImbEZgTH50qQhoPvWkPOCgtr+YhZ3olzOUwBxsyMC/JqFvgTVYgYe/bvAlYGf6TJz+wYfABOzFvdmlwILUOVGP4ELADrosvWPilCJgW7N5+eEUnWIFnG1gkgoswrqLXjCpwKbU+d71kvlP4AqmZWPiJYUEJmHX146okMASrLqsc1yBbVlWOY/jZaRpfZXrqm9nQlDVc3XtUAAAAABJRU5ErkJggg==";
String disabledCloseGatePng= "image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAYAAABzenr0AAAAAXNSR0IDN8dNUwAAAAlwSFlzAAALEgAACxIB0t1+/AAAABR6VFh0YXV0aG9yAAB42ivIyMwBAAQ6Aa41gl/YAAABi0lEQVRYw8WX2ytEURSHnTlGJEZESdLIJbfEK6LEkxfFg/BEefA3+VPc5cGDUqK8uZZcnkjk9u1au3bHaOaMfZZVX82ZOZfvzPrtffYJSvSrG5ag0WwEihdugBkYdr/UEKiGEZjP9WOSAqUwCKuQ/m2npAQ65I7b8+3oW6AWZmG00AN8CVTCpFw8VgUe+twPy5Ap5gR/EWiDBel30VWMQD1MwYSP3sURqIAxWPSZ2kIEQunzClT5Hq/5BFphDnqSmq1yCaTkTs28PZ70PB0VKIcheVqpVBAJ2Zr2sznlfP74h7XBjxYYoU6Yhl4NgTCy/QUPsA8XMvzSmgKuyDVswhP0aQvYeoMz2IIaaNYWsPUCB3AEWZFRFbD1CDtwKSEt0xYw9QlXko9XWWarCrj5OIVd+Sey2gK2nuFQZFri5iP0GOg7yYdpz0Ch5w49jyqbjw1pUZe2gC0TzhPYgzpo0hZw82Hmj2MJaUZbwE7r97ANt/K6FrgCZhHy7ixIknosm3ycw7pc17y23XwDDIY65dqdmrcAAAAASUVORK5CYII=";
#endif
String hostname= HOSTNAME;
String ssid[SSIDMax()];     //Identifiants WiFi /Wifi idents
String password[SSIDMax()]; //Mots de passe WiFi /Wifi passwords

portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
volatile bool value[PINSCOUNT], done[PINSCOUNT];
#define setIntr(i)    {portENTER_CRITICAL_ISR(&mux);value[i]=true; portEXIT_CRITICAL_ISR(&mux);}
#define unsetIntr()   {portENTER_CRITICAL_ISR(&mux);for(uint8_t i=OUTPUTSCOUNT+1; i<PINSCOUNT; i++)if(done[i])value[i]=done[i]=false;portEXIT_CRITICAL_ISR(&mux);}
#ifdef DEBUGG
  #define DEBUG_println(n) Serial.println(n)
  #define DEBUG_print(n)   Serial.print(n)
#else
  #define DEBUG_println(n)
  #define DEBUG_print(n)
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
    DEBUG_println("(STOPPED)");
} }

void startOpening(){
  stopMotors();
  direction=OPEN;
  closedLimit=false;
  if(isOpened()){
    direction=!direction;
    DEBUG_println("OPENINGLIMIT up...");
    return;
  } DEBUG_println("(OPENING)");
  digitalWrite(gpio[LIGHT], value[LIGHT]=HIGH);
  initMovingTimeMeasure(direction);
  digitalWrite(gpio[!place ?MOTORR : MOTORF], value[!place ?MOTORR : MOTORF]=HIGH);
}

void startClosing(){
  stopMotors();
  direction=CLOSE;
  if(getPin(PHOTOBEAM)){
    DEBUG_println("PHOTOBEAM up...");
    return;
  }openedLimit=false;
  if(isClosed()){
    direction=!direction;
    DEBUG_println("CLOSINGLIMIT up...");
    return;
  } DEBUG_println("(CLOSING)");
  digitalWrite(gpio[LIGHT], value[LIGHT]=HIGH);
  initMovingTimeMeasure(direction);
  digitalWrite(gpio[!place ?MOTORF : MOTORR], value[!place ?MOTORF : MOTORR]=HIGH);
}

void securityCheck(){
  if(isMoving()){

    //PhotoBeam check:
    if(isClosing() && getPin(PHOTOBEAM)){
      stopMotors();
      DEBUG_println("PHOTOBEAM secure detect...");
    }

    //Limits check:
    if(isOpening() && getPin(OPENINGLIMIT)){
      stopMotors();
      openedLimit=true;
      direction=CLOSE;
      DEBUG_println("OPENINGLIMIT secure detect...");
    }if(isClosing() && getPin(CLOSINGLIMIT)){
      stopMotors();
      closedLimit=true;
      direction=OPEN;
      DEBUG_println("CLOSINGLIMIT secure detect...");
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
      }DEBUG_println("DEFAULTLIMITS secure detect...");
    }

    //Torques check:
    if(overTorqueCheck && (getPin(OPENTORQUE) || getPin(CLOSETORQUE))){
      stopMotors();
      direction=!direction;
      disableAutoClose();
      DEBUG_println("OVERTORQUE secure detect...");
    }

    //setPin motors error check:
    if(getPin(MOTORF) && getPin(MOTORR)){
      stopMotors();
      DEBUG_println("DEFAULTMOTOR secure detect...");
    }
} }

void IRAM_ATTR photoBeam_intr()   {setIntr(PHOTOBEAM);}
void IRAM_ATTR limitOpen_intr()   {setIntr(OPENINGLIMIT);}
void IRAM_ATTR limitClose_intr()  {setIntr(CLOSINGLIMIT);}
void IRAM_ATTR openTorque_intr()  {setIntr(OPENTORQUE);}
void IRAM_ATTR closeTorque_intr() {setIntr(CLOSETORQUE);}
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
  String page="<!doctype html><html lang='us-US'><head><meta charset='utf-8'>\n";
//page += "<head><meta content='text/html; charset=utf-8' http-equiv='content-type'/>\n";
  page += "<title>" + hostname + "</title>\n";
  page += "<meta content='phil-20180412' name='author'>\n";
  page += "<meta content='WEB interface for connected sliding gate' name='description'>\n";
  page += "<meta content='sliding gate' name='keywords'>\n";
  page += "<style>body {background-color:papayawhip; font-family:Arial,Helvetica,Sans-Serif; Color:#000088;}\n";
  page += "ul {list-style-type:square;} li{padding-left:5px; margin-bottom:10px;}\n";
  page += "td {text-align:left; min-width:100px; vertical-align:middle;}\n";
  page += ".modal {display:none; position:fixed; z-index:1; left:0%; top:0%; height:100%; width:100%; overflow:scroll; background-color:#000000;}\n";
  page += ".modal-content {background-color:#fff7e6; margin:5% auto; padding:15px; border:2px solid #888; height:90%; width:90%; min-height:755px;}\n";
  page += ".closeHelp{color:#aaa; float:right; font-size:30px; font-weight:bold;}\n";
  page += ".duration {width:50px; width: 40px; height:10px;}\n";
  page += "div#bandeau {width:600px; height:40px; background-color: firebrick;}\n";
  page += "div#mainframe {width:600px; height:auto;}\n";
  page += ".ligneOpenCloseStop {width:100%; height:100px;}";
  page += "div#blocClose {float:left; width:80px; height:100%; display: flex;}\n";
  page += ".blockStatus-opened {float:left; width:440px; height:100%; background-color: chartreuse; background-image:url('data:" + gateOpenedPng + "'); background-repeat: no-repeat;background-position: center center;}\n";
  page += ".blockStatus-closed {float:left; width:440px; height:100%; background-color: red       ; background-image:url('data:" + gateClosedPng + "'); background-repeat: no-repeat;background-position: center center;}\n";
  page += ".blockStatus-moving {float:left; width:440px; height:100%; background-color: chartreuse; background-image:url('data:" + gateMovingPng + "'); background-repeat: no-repeat;background-position: center center;}\n";
  page += "div#blocOpen {float:right; width:80px; height:100%; display: flex;}\n";
  page += "div.ligne {width:100%; height:30px;}\n";
  page += "div.libelle {float: left; width:250px; height:100%; display:flex; align-items:center;}\n";
  page += "div.value {float: left; width: auto; height:100%; display:flex; align-items:center;}\n";
  page += "div#piedpage {width:600px; height:20px; background-color: firebrick; clear:both;}\n";
  page += ".open    {height:100%; width:100%; background-image:url('data:" + openGatePng  + "'); background-repeat: no-repeat;background-position: center center;}\n";
  page += ".close   {height:100%; width:100%; background-image:url('data:" + closeGatePng + "'); background-repeat: no-repeat;background-position: center center;}\n";
  page += ".disOpen {height:100%; width:100%; background-image:url('data:" + disabledOpenGatePng  + "'); background-repeat: no-repeat;background-position: center center;}\n";
  page += ".disClose{height:100%; width:100%; background-image:url('data:" + disabledCloseGatePng + "'); background-repeat: no-repeat;background-position: center center;}\n";
  page += "button#stop{height:100%; width:100%; background-image:url('data:" + stopGatePng  + "'); background-repeat: no-repeat;background-position: center center;}\n";
  page += "</style>\n";
  page += "</head>\n";
  page += "<body onload='init();'>\n";
  page += "<script>\nthis.timer=0; this.checkDelaySubmit=0;\n";
  page += "function init(){var e;\n";
  page += "e=document.getElementById('example1');\ne.innerHTML=document.URL+'close'; e.href=e.innerHTML;\n";
  page += "e=document.getElementById('example2');\ne.innerHTML=document.URL+'status'; e.href=e.innerHTML;\n";
  page += "refresh();\n";
  page += "}\n";
  page += "function refresh(v=" + ultos(autocloseDelay*LOOPDELAY/1000) + "){clearTimeout(this.timer);\n";
  page += "document.getElementById('about').style.display='none';\n";
  page += "this.timer=setTimeout(function(){/*location.reload(true)*/;\n";
  page += "getStatus(); refresh(v);\n";
  page += "},v*1000);}\n";
  page += "function getStatus(){setTimeout(function(){\n";
  page += "var ret, req=new XMLHttpRequest(); req.open('GET', document.URL+'status', false); req.send(null);\n";
  page += "ret=req.responseText; ret=parseInt(ret.substr(1,ret.length-2));;\n";
  page += "if(ret<0){\n";
  page += "  document.getElementById('blockStatus').className='blockStatus-closed';\n";
  page += "  document.getElementById('close').className='disClose'; document.getElementById('open').className='open';\n";
  page += "  document.getElementById('close').disabled=true;        document.getElementById('open').disabled=false;\n";
  page += "}else if(ret){\n";
  page += "  document.getElementById('blockStatus').className='blockStatus-opened';\n";
  page += "  document.getElementById('close').className='close'; document.getElementById('open').className='disOpen';\n";
  page += "  document.getElementById('close').disabled=false;    document.getElementById('open').disabled=true;\n";
  page += "}else{\n";
  page += "  document.getElementById('blockStatus').className='blockStatus-moving';\n";
  page += "  document.getElementById('close').className='close'; document.getElementById('open').className='open';\n";
  page += "  document.getElementById('close').disabled=true;     document.getElementById('open').disabled=true;\n";
  page += "}}, 1500);}\n";
  page += "function showHelp(){refresh(120); document.getElementById('about').style.display='block';}\n";
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
  page += "var previousDelay, checkDelaySubmit=0;\n";
  page += "function checkDelay(e,min){\n";
  page += "previousDelay=e.value;if( e.value>-1 && e.value<min){if(previousDelay-e.step==-1)e.value=min;else e.value=-1;}\n";
  page += "clearTimeout(this.checkDelaySubmit); this.checkDelaySubmit=setTimeout(function(){this.checkDelaySubmit=0; document.getElementById('mainform').submit();}, 2000);\n";
  page += "}\n";
  page += "</script><div id='about' class='modal'><div class='modal-content'>";
  page += "<span class='closeHelp' onClick='refresh();'>&times;</span>";
  page += "<h1>About</h1>";
  page += "This device is a connected device that allows you to control the status of the controled sliding gate from a home automation application like Domotics or Jeedom.<br><br>";
  page += "In addition, it also has its own WEB interface which can be used to configure and control it from a web browser (the firmware can also be upgraded from this page). ";
  page += "Otherwise, its URL is used by the home automation application to control it, simply by forwarding the desired state (Json format), like this:";
  page += "<a id='example1' style='padding:0 0 0 5px;'></a><br><br>";
  page += "The status of the sliding gate can also be requested from the following URL: ";
  page += "<a id='example2' style='padding:0 0 0 5px;'></a><br><br>";
  page += "The following allows you to configure some networks parameters (until a private SSID is set, the socket works as an access point (AP) with its own SSID and default password: \"" + hostname + "/" + DEFAULTWIFIPASS + "\"), while the main page allows you to configure the operating parameters of the sliding gate itself.<br><br>";
  page += "By connecting to this default SSID and going to its website (port 80), the connection to private SSID (up to 3) can be initialized. If later, all of the defined networks become unavailable, the default SSID (AP) becomes periodically active again (also attempting to reconnect the private defines), thereby allowing to update new accessible private WIFI networks.<br><br>";
  page += "<h2><form method='POST'>";
  page += "Network name: <input type='text' name='hostname' value='" + hostname + "' style='width:110;'>";
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
  page += "<h6><a href='update' onclick=\"javascript:event.target.port=30499\">Firmware update</a>";
  page += " - <a href='https://github.com/peychart/A-Connected-Sliding-Gate'>Website here</a></h6>";
  page += "</div></div>\n";

  page += "<div id='bandeau'>\n";
  page += "<h1 style='color:black;'>" + hostname + " ["+ getMyMacAddress() + "]<span class='closeHelp' onclick='showHelp();'>?</span></h1>\n";
  page += "</div>\n";

  page += "<div id='mainframe'><form id='mainform' method='POST'>\n";
  page += "<div class='ligneOpenCloseStop'>\n";
  page += "<div id='blocClose'><button id='close' name='close' title='Close gate' onclick='getStatus();' " + (String)(isClosed() ?"class='disClose' disabled" :"class='close'") + "></button></div>\n";
  page += "<div id='blocOpen'><button id='open' name='open' title='Open gate' onclick='getStatus();' " + (String)(isOpened() ?"class='disOpen' disabled" :"class='open'") + "></button></div>\n";
  page += "<div id='blockStatus' class='blockStatus-" + (String)(isClosed() ?"closed" :(isOpened() ?"opened" :"moving")) + "'></div>\n";
  page += "</div>\n";
  page += "<div class='ligneOpenCloseStop'> <button id='stop' name='stop' title='Stop gate' onclick='getStatus();'></button></div>\n";
  page += "<div class='ligne'></div>\n";
  page += "<div class='ligne'>\n";
  page += "<div class='libelle'> Autoclosing </div>\n";
  page += "<div class='value'>:&nbsp; <input name='autoclose' value='" + (String)(isAutocloseEnabled() ?ultos(autoclose*LOOPDELAY/1000) :"-1") + "' data-unit='1' min='-1'\n";
  page += "max='" + ultos(MAXAUTOCLOSEDELAY/1000) + "' class='duration' step='1' onchange='checkDelay(this, " + ultos(DEFAULTAUTOCLOSEDELAY/1000) + ");'\n";
  page += "type='number'>&nbsp;secondes</div>\n";
  page += "</div>\n";
  page += "<div class='ligne'>\n";
  page += "<div class='libelle'> Operating time control </div>\n";
  page += "<div class='value'>:&nbsp; <input " + (String)(!disableMovingTimeControl ?"checked" :"") + " name='movingtime' type='checkbox' onchange='submit()';>\n";
  page += "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<button id='resetOperatingTime' name='stop' title='Remeasure the operating time' style='height:25px;'>Reset measures</button></div>\n";
  page += "</div>\n";
  page += "<div class='ligne'>\n";
  page += "<div class='libelle'> Presence detector function </div>\n";
  page += "<div class='value'>:&nbsp; <input name='detection' type='checkbox' " + (String)(!disabledDetection ?"checked" :"") + " onchange='submit();'></div>\n";
  page += "</div>\n";
  page += "<div class='ligne'>\n";
  page += "<div class='libelle'> Allows deep sleep </div>\n";
  page += "<div class='value'>:&nbsp; <input name='deepsleep' " + (String)(deepSleepEnabled() ?"checked" :"") + " type='checkbox' onchange='submit();'></div>";
  page += "</div>\n";
  page += "<div class='ligne'>\n";
  page += "<div class='libelle'> Allows control of the motor torque </div>\n";
  page += "<div class='value'>:&nbsp; <input name='torque' type='checkbox' " + (String)(overTorqueCheck ?"checked" :"") + " onchange='submit();'></div>\n";
  page += "</form></div>\n";
  page += "</div>\n";

  page += "<div id='piedpage'><h6><a href='https://github.com/peychart/A-Connected-Sliding-Gate'>Website here</a></h6></div>\n";
//  page += "<form id='switchs' method='POST'>";
  page += "</body></html>";
  return page;
}

bool WiFiHost(){
  DEBUG_println();
  DEBUG_println("No custom SSID defined: setting soft-AP configuration ... ");
  WiFi.mode(WIFI_AP);
  WiFiAP=WiFi.softAP(hostname.c_str(), password[0].c_str());
  DEBUG_println(String("Connecting [") + WiFi.softAPIP().toString() + "] from: " + hostname + "/" + password[0]);
  nbWifiAttempts=(nbWifiAttempts==-1 ?1 :nbWifiAttempts); WifiAPDelay=60;
  return WiFiAP;
}

bool WiFiConnect(){
  if(WiFi.status()==WL_CONNECTED){
    ESP.restart(); //ArduinoOTA.end();
    return true;
  }delay(10);

  if(!ssid[0][0] || !nbWifiAttempts--)
    return WiFiHost();

  for(uint8_t i=0; i<SSIDMax(); i++) if(ssid[i].length()){
    DEBUG_println(); DEBUG_println();

    //Connection au reseau Wifi /Connect to WiFi network
    WiFi.mode(WIFI_STA);
    DEBUG_println("Connecting [" + getMyMacAddress() + "] to: " + ssid[i]);
    WiFi.begin(ssid[i].c_str(), password[i].c_str());

    //Attendre la connexion /Wait for connection
    for(uint8_t j=0; j<16 && WiFi.status()!=WL_CONNECTED; j++){
      delay(500);
      DEBUG_print(".");
    } DEBUG_println();

    if(WiFi.status()==WL_CONNECTED){
      //Affichage de l'adresse IP /print IP address
      DEBUG_println("WiFi connected");
      DEBUG_println("IP address: " + WiFi.localIP().toString());
      nbWifiAttempts=MAXWIFIERRORS;
      return true;
    }else DEBUG_println("WiFi Timeout.");
  }
  return false;
}
#endif

bool readConfig(bool);
void writeConfig(){        //Save current config:
  if(!readConfig(false))
    return;
  File f=SPIFFS.open("/config.txt", "w+");
  if(f){ uint8_t i;
    f.println(ResetConfig);
    f.println(hostname);                //Save hostname
    for(uint8_t j=i=0; j<SSIDMax(); j++){   //Save SSIDs
      if(!password[j].length()) ssid[j]="";
      if(ssid[j].length()){
        f.println(ssid[j]);
        f.println(password[j]);
        i++;
    } }while(i++<SSIDMax()){f.println(); f.println();}
    f.println(disabledDetection);
    f.println(autoclose); setAutoClose(autoclose);
    f.println(place);
    f.println(movingTime[OPEN]);
    f.println(movingTime[CLOSE]);
    f.println(disableMovingTimeControl);
    f.println(overTorqueCheck);
    f.close();
  }
  #ifdef WITHWIFI
    if(WiFiAP && ssid[0].length()) WiFiConnect();
  #endif
}

String readString(File f){ String ret=f.readStringUntil('\n'); ret.remove(ret.indexOf('\r')); return ret; }
inline bool getConfig(String        &v, File f, bool w){String          r=readString(f);               if(r==v) return false; if(w)v=r; return true;}
inline bool getConfig(int           &v, File f, bool w){int             r=atoi(readString(f).c_str()); if(r==v) return false; if(w)v=r; return true;}
inline bool getConfig(long          &v, File f, bool w){long            r=atol(readString(f).c_str()); if(r==v) return false; if(w)v=r; return true;}
inline bool getConfig(unsigned long &v, File f, bool w){unsigned long   r=atol(readString(f).c_str()); if(r==v) return false; if(w)v=r; return true;}
inline bool getConfig(bool          &v, File f, bool w){bool            r=atol(readString(f).c_str()); if(r==v) return false; if(w)v=r; return true;}
bool readConfig(bool w=true){      //Get config (return false if config is not modified):
  bool ret=false;
  File f=SPIFFS.open("/config.txt", "r");
  if(f && ResetConfig!=atoi(readString(f).c_str())) f.close();
  if(!f){    //Write default config:
    if(w){
      ssid[0]=""; password[0]=DEFAULTWIFIPASS;
      hostname=HOSTNAME;
      SPIFFS.format(); writeConfig();
    }return true;
  }
  ret|=getConfig(hostname, f, w);
  for(uint8_t i=0; i<SSIDMax(); i++){
    ret|=getConfig(ssid[i], f, w);
    ret|=getConfig(password[i], f, w);
  }if(!ssid[0].length()) password[0]=DEFAULTWIFIPASS; //Default values
  ret|=getConfig(disabledDetection, f, w); if(deepSleepEnabled() && (COMMAND>HIGHPINS || DETECT>HIGHPINS)) disabledDetection=true;
  ret|=getConfig(autoclose, f, w);
  ret|=getConfig(place, f, w);
  ret|=getConfig(movingTime[OPEN],  f, w); if(shouldMeasureTime(OPEN))  unsetMeasureTime(OPEN);
  ret|=getConfig(movingTime[CLOSE], f, w); if(shouldMeasureTime(CLOSE)) unsetMeasureTime(CLOSE);
  ret|=getConfig(disableMovingTimeControl, f, w);
  ret|=getConfig(overTorqueCheck, f, w);
  f.close(); return ret;
}

#ifdef WEBUI
void handleSubmitSSIDConf(AsyncWebServerRequest *request){     //Setting:
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

void  handleMainForm(AsyncWebServerRequest *request){
  if(request->hasArg("open")){
    direction=OPEN; setIntr(COMMAND);
    return;
  }else if(request->hasArg("close")){
    direction=CLOSE; setIntr(COMMAND);
    return;
  }else if(request->hasArg("stop")){
    stopMotors();
    return;
  }
  setAutoClose(atoi(request->arg("autoclose").c_str())*1000);
  disableMovingTimeControl=true; if(request->hasArg("movingtime")) disableMovingTimeControl=false;
  if(request->hasArg("resetOperatingTime")) {unsetMeasureTime(OPEN); unsetMeasureTime(CLOSE);}
  { disabledDetection=true; if(request->hasArg("detection")) disabledDetection=false;
    disabledDetection&=((COMMAND>HIGHPINS || DETECT>HIGHPINS) ?true :false);
  }
  disableDeepSleep(); if(request->hasArg("deepsleep")) enableDeepSleep();
  overTorqueCheck=false; if(request->hasArg("torque")) overTorqueCheck=true;
  writeConfig();
}

void  handleRoot(AsyncWebServerRequest *request){
  if(request->hasArg("hostname")){
    hostname=request->arg("hostname");
    writeConfig();
  }else if(request->hasArg("password")){
    handleSubmitSSIDConf(request);
  }else if(request->hasArg("autoclose")){
    handleMainForm(request);
  }request->send(200, "text/html", getPage());
}
#endif

void handleInterrupt(){     // Inputs treatment:
  for(uint8_t i=OUTPUTSCOUNT+1; i<PINSCOUNT; i++){
    if(!value[i] || done[i]) continue;
    done[i]=true;
    switch(i){
      case OPENINGLIMIT:
        if(isOpening()){
          stopMotors();
          openedLimit=true;
          direction=CLOSE;
        } DEBUG_println("OPENINGLIMIT detected...");
        break;
      case CLOSINGLIMIT:
        if(isClosing()){
          stopMotors();
          closedLimit=true;
          direction=OPEN;
        } DEBUG_println("CLOSINGLIMIT detected...");
        break;
      case PHOTOBEAM:
        if(isClosing()) stopMotors();
        DEBUG_println("PHOTOBEAM detected...");
        break;
      case OPENTORQUE:
      case CLOSETORQUE:
        if(!overTorqueCheck)
          break;
        stopMotors();
        direction=!direction;
        disableAutoClose();
        DEBUG_println("OVERTORQUE detected...");
        break;
      case DETECT:
        if(!disabledDetection && !isMoving())
          startOpening();
        DEBUG_println("DETECTION detected...");
        break;
      case COMMAND:
        if(activatedCommand)
          break;
        activatedCommand=true;
        if(isMoving()) {
          stopMotors(); direction=!direction;
          DEBUG_println("COMMAND stop motor enabled...");
        }else{
          if(getPin(PHOTOBEAM)) direction=OPEN;
          if(direction==OPEN)
               startOpening();
          else startClosing();
          DEBUG_println("COMMAND start motor enabled...");
        }break;
/*      case AUTOCLOSE:
        DEBUG_println("AUTOCLODE DETECTED...");
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
#ifdef OTAPASSWD
  SPIFFS.end();
//Allow OnTheAir updates:
  //See: http://docs.platformio.org/en/latest/platforms/espressif32.html#authentication-and-upload-options
  //     http://esp8266.github.io/Arduino/versions/2.1.0/doc/ota_updates/ota_updates.html
  //MDNS.begin(hostname.c_str());
  ArduinoOTA.setHostname(hostname.c_str());

  // No authentication by default
  // ArduinoOTA.setPassword("admin");
  // Password can be set with it's md5 value as well
  //ArduinoOTA.setPasswordHash("$1$vBpbvvV6$UOWSqzPn4kFizSWcg48Vy.");
  ArduinoOTA.setPassword(OTAPASSWD);

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
  });disableDeepSleep();
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
  }for(i++; i<PINSCOUNT; i++){               //Entrées/inputs:
    //pinMode(gpio[i], (i<HIGHPINS ?INPUT :INPUT_PULLUP));
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
      case OPENTORQUE:  attachInterrupt(digitalPinToInterrupt(gpio[i]), openTorque_intr,  (i<HIGHPINS ?RISING :FALLING)); break;
      case CLOSETORQUE: attachInterrupt(digitalPinToInterrupt(gpio[i]), closeTorque_intr, (i<HIGHPINS ?RISING :FALLING)); break;
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
  server.on("/",                           HTTP_GET,  [](AsyncWebServerRequest *request){handleRoot(request);});
  server.on("/",                           HTTP_POST, [](AsyncWebServerRequest *request){handleRoot(request);});
  server.on("/status",                     HTTP_GET,  [](AsyncWebServerRequest *request){request->send(200, "text/plain", (isClosed() ?"[-1]" :(isOpened() ?"[1]" : "[0]")));});
  server.on("/open",                       HTTP_GET,  [](AsyncWebServerRequest *request){startOpening();  handleRoot(request);});
  server.on("/close",                      HTTP_GET,  [](AsyncWebServerRequest *request){startClosing();  handleRoot(request);});
  server.on("/stop",                       HTTP_GET,  [](AsyncWebServerRequest *request){stopMotors();    handleRoot(request);});
  server.on("/allowAutoClose",             HTTP_GET,  [](AsyncWebServerRequest *request){setAutoClose(DEFAULTAUTOCLOSEDELAY); writeConfig(); handleRoot(request);});
  server.on("/disableAutoClose",           HTTP_GET,  [](AsyncWebServerRequest *request){disableAutoClose();                  writeConfig(); handleRoot(request);});
  server.on("/allowDetection",             HTTP_GET,  [](AsyncWebServerRequest *request){disabledDetection=((deepSleepEnabled() && (COMMAND>HIGHPINS || DETECT>HIGHPINS)) ?true :false); writeConfig(); handleRoot(request);});
  server.on("/disableDetection",           HTTP_GET,  [](AsyncWebServerRequest *request){disabledDetection=true;              writeConfig(); handleRoot(request);});
  server.on("/enableDeepSleep",            HTTP_GET,  [](AsyncWebServerRequest *request){enableDeepSleep();                   writeConfig(); handleRoot(request);});
  server.on("/disableDeepSleep",           HTTP_GET,  [](AsyncWebServerRequest *request){disableDeepSleep();                  writeConfig(); handleRoot(request);});
//server.on("/forward",                    HTTP_GET,  [](AsyncWebServerRequest *request){place=false;                         writeConfig(); handleRoot(request);});
//server.on("/reverse",                    HTTP_GET,  [](AsyncWebServerRequest *request){place=true;                          writeConfig(); handleRoot(request);});
  server.on("/enableTorqueeCheck",          HTTP_GET,  [](AsyncWebServerRequest *request){overTorqueCheck=true;                writeConfig(); handleRoot(request);});
  server.on("/disableoverTorqueCheck",     HTTP_GET,  [](AsyncWebServerRequest *request){overTorqueCheck=false;               writeConfig(); handleRoot(request);});
  server.on("/enableOperationTimeControl", HTTP_GET,  [](AsyncWebServerRequest *request){disableMovingTimeControl=false;      writeConfig(); handleRoot(request);});
  server.on("/disableOperationTimeControl",HTTP_GET,  [](AsyncWebServerRequest *request){disableMovingTimeControl=true;       writeConfig(); handleRoot(request);});
  server.on("/resetOperationTimeControl",  HTTP_GET,  [](AsyncWebServerRequest *request){disableMovingTimeControl=true;       unsetMeasureTime(OPEN); unsetMeasureTime(CLOSE); writeConfig(); handleRoot(request);});
  server.on("/about",                      HTTP_GET,  [](AsyncWebServerRequest *request){request->send(200, "text/plain", "Hello world!..."); });
  #endif

  DEBUG_println("Hello World!");
}

////////////////////////////////////// LOOP ////////////////////////////////////////////////
void loop(){
  if(WiFi.status()==WL_CONNECTED)
    ArduinoOTA.handle();

  if(shouldReboot){
    DEBUG_println("Rebooting...");
    delay(500);
    ESP.restart();
  }if(isClosed()){
    if(deepSleepEnabled() && !--deepSleepDelay){ //deepSleep management:
      DEBUG_println("DEEPSLEEP!...");
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
            #ifdef OTAPASSWD
              OTASetup();
            #endif
            #ifdef WEBUI
              server.begin();
              DEBUG_println("WEBServer started on port 80.");
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
