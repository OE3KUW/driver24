/******************************************************************

                        driver 24

                                                    қuran jan 2024
******************************************************************/

#include <Arduino.h>
#include <PS4Controller.h>

#define FASTLED_ALL_PINS_HARDWARE_SPI
#include <FastLED.h>

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "SPIFFS.h"

#include <Wire.h>


// -------  defines -----------------------------------------------

#define TRUE                            true
#define FALSE                           false
#define WAIT_ONE_SEC                    10000
#define WAIT_250_MSEC                   2500
#define WAIT_ONE_10MSEC                 100

#define H                               HIGH
#define L                               LOW

//  --- #define ON_BOARD_LED                    5

#define NUM_LEDS                        4
#define DATA_PIN                        23
#define CLOCK_PIN                       18

#define WHEEL_L                         2
#define WHEEL_R                         A4
#define WHEEL_L_DIRECTION               15 
#define WHEEL_R_DIRECTION               A5

#define BATTERY_LEVEL                   A3      // GPIO 39

#define REFV                            685.0     // factor

#define MFS                             0x1e




// -------  global Variables --------------------------------------

volatile int oneSecFlag;
volatile int qSecFlag;
volatile int tenMSecFlag;

hw_timer_t *timer = NULL;
void IRAM_ATTR myTimer(void);

volatile int vL, vR;

CRGB leds[NUM_LEDS];

volatile int pulse; 
volatile int connected;
volatile int LDir;
volatile int RDir;

volatile int pointer;
volatile float batteryLevel = 0.;
volatile int winkelL = 0; 
volatile int oldWinkelL = 0;
volatile int go;
volatile int startWiFi = 0;

volatile int i2cAnswer;
char text[80];



//const char *ssid = "Wolfgang Uriel Kurans Handy"; 
//const char *password = "x1234567"; 


//const char* ssid = "iot122023";                                                                                  //                     "A1-A82861";///********/;
//const char* password = "iot122023secret";                                                                        //                     "7PMGDV96J8";///********/;

const char* ssid = "A1-A82861";                                                                                                                                                                                          
const char* password = "7PMGDV96J8";


AsyncWebServer server(80);
AsyncWebSocket ws("/ws");


IPAddress lclIP(192,168,2,219);
IPAddress gateway(192,168,2,1);
IPAddress subnet(255,255,255,0);


const int led5 = 5;
bool ledState = 1;

// -------  functions ---------------------------------------------

void initSPIFFS()
{
    if (!SPIFFS.begin(true))
    {
        Serial.println("An error has occurred while mounting SPIFFS");
    }
    else
    {
        Serial.println("SPIFFS mounted successfully!");
    }
}

void initWiFi()
{
    //WiFi.softAPConfig(lclIP, gateway, subnet);
    WiFi.mode(WIFI_STA);

    WiFi.begin(ssid, password);

    Serial.println("Connection to WiFi . . .");
    while ((WiFi.status() != WL_CONNECTED) && (startWiFi < 21))
    {
        Serial.print(" .");
        delay(1000);
        startWiFi++;
    }

    Serial.println("IP:");
    Serial.println(WiFi.localIP());
}

String processor(const String& var)
{
    String battery;

    if (var == "STATE") 
    {
        if (digitalRead(led5))
        {
            ledState = 1; return "ON";
            Serial.println("on");
        }
        else
        {
            ledState = 0; return "OFF";
            Serial.println("off");
        }
    }

    if (var == "BATTERY")
    {
       battery = String(batteryLevel).c_str();
       return battery;
    }

    return String();
}

void notifyClients(String state)
{
    ws.textAll(state);
    //ws.text(1, state); // State
}


void handleWebSocketMessage(void *arg, uint8_t * data, size_t len)
{
    AwsFrameInfo * info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len && info->opcode == WS_TEXT)
    {
        data[len] = 0;
        if (strcmp((char*)data, "bON") == 0)
        {
            ledState = 1;
            notifyClients("ON");

            Serial.println("handleWebSocketMessage: on");

        }
        if (strcmp((char*)data, "bOFF") == 0)
        {
            ledState = 0;
            notifyClients("OFF");
            Serial.println("handleWebSocketMessage: off");
        }
    }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient * client, AwsEventType type, 
             void * arg, uint8_t * data, size_t len)
{
    switch(type)
    {
        case WS_EVT_CONNECT: 
             Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        break;

        case WS_EVT_DISCONNECT:
             Serial.printf("WebSocket client #%u disconnected\n", client->id());
        break;

        case WS_EVT_DATA:
             handleWebSocketMessage(arg, data, len);
        break;

        case WS_EVT_PONG:
        case WS_EVT_ERROR:
        break;
    }
}

void initWebSocket()
{
    ws.onEvent(onEvent);
    server.addHandler(&ws);
}


void setup() 
{
    Serial.begin(115200);
    Serial.println("start!");
    go = 1; 


    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &myTimer, true);
    timerAlarmWrite(timer, 100, true);  // 0.1 msec
    timerAlarmEnable(timer);
    
    oneSecFlag = FALSE; 
    qSecFlag = FALSE;
    tenMSecFlag = FALSE; 
    PS4.begin("10:20:30:40:50:62");


    Serial.println("ready!");

    // -- pinMode(ON_BOARD_LED, OUTPUT);
    pinMode(led5, OUTPUT);

    pinMode(WHEEL_L, OUTPUT);
    pinMode(WHEEL_R, OUTPUT);
    pinMode(WHEEL_L_DIRECTION, OUTPUT);
    pinMode(WHEEL_R_DIRECTION, OUTPUT);
    
    pinMode(BATTERY_LEVEL, INPUT);

    // -- digitalWrite(ON_BOARD_LED, L); // on ! ... blue 

    digitalWrite(led5, L);


    digitalWrite(WHEEL_L_DIRECTION, L);
    digitalWrite(WHEEL_R_DIRECTION, H);

    digitalWrite(WHEEL_L, L); // stop !
    digitalWrite(WHEEL_R, L); // stop !

    pulse = 0;
    connected = 0;
    LDir = 0;
    RDir = 1;
    pointer = 0;

    vL = vR = 0;

// Fast Leds:

    FastLED.addLeds<SK9822, DATA_PIN, CLOCK_PIN, RGB>(leds, NUM_LEDS);
    
    leds[0] = CRGB{255, 255, 255}; // R B G
    leds[1] = CRGB{255, 255, 255};
    leds[2] = CRGB{255, 255, 255};
    leds[3] = CRGB{255, 255, 255};

    FastLED.show();

    // Magnetfeldsensor:  preparation phase ...

    Wire.begin();
    Wire.beginTransmission(MFS);
    Wire.write(0x02);        // select mode register
    Wire.write(0x00);        // continuous measurement mode
    i2cAnswer = Wire.endTransmission();

    Serial.println("i2c Anser: ");
    Serial.println(i2cAnswer);
    Serial.println("--------------------------------");
    


    initSPIFFS();
    initWiFi();
    initWebSocket();


    server.on("/", HTTP_GET,
    [](AsyncWebServerRequest * request)
    {
        request->send(SPIFFS, "/index.html", "text/html", false, processor);
    });

    server.serveStatic("/", SPIFFS, "/");

    server.on("/logo", HTTP_GET, 
    [](AsyncWebServerRequest *request)
    {
        request->send(SPIFFS, "/logo.png", "image/png");
    }
             );

    // Start server:

    server.begin();
}

void loop() 
{
    uint8_t leftstickXDATA0 = 0;
    uint8_t leftstickXDATA1 = 0;
    static int t1 = 0;
    String battery;
    int akn;
    static int angleXY, angleYZ, angleZX;
    int u, v, w, x, y, z, n;
    long a, b;
    static int maxa = 0, mina = 0xffff, maxb, minb = 0xffff; 
    float r, m;
    String winkel;

    ws.cleanupClients();

    if (ledState == 1) {digitalWrite(led5, 0); go = 1;} else { digitalWrite(led5, 1); go = 0;}

    if (oneSecFlag == TRUE)  // all Seconds 
    {
            oneSecFlag = FALSE;

            batteryLevel = analogRead(BATTERY_LEVEL) / REFV;

            //batteryLevel += 3.75;  // for tests only
            
            //if (batteryLevel > 359) batteryLevel = 0;

            battery = String(batteryLevel).c_str();

            battery = "BAT" + battery;

            notifyClients(battery);

// Magnetfeldsensor:  not implemented jet! 
            Wire.beginTransmission(MFS);
            Wire.write(0x03);
            akn = Wire.endTransmission();

            n = Wire.requestFrom(MFS, 6);
            if (n >= 6)
            {
//                x =  Wire.read() << 8; x |= Wire.read();
//                z =  Wire.read() << 8; z |= Wire.read();
//                y =  Wire.read() << 8; y |= Wire.read();
                  u = Wire.read();
                  v = Wire.read();
                  w = Wire.read();
                  x = Wire.read();
                  y = Wire.read();
                  z = Wire.read();

            }

            //angleXY = atan2(-y,  x) / M_PI * 180;  if (angleXY < 0) angleXY += 360;
            //angleYZ = atan2(-z, -y) / M_PI * 180;  if (angleYZ < 0) angleYZ += 360;
            //angleZX = atan2( x, -z) / M_PI * 180;  if (angleZX < 0) angleZX += 360;

            //x = angleXY;
            //y = angleYZ;
            //z = angleZX;


            a = ((unsigned char)(u + 128) << 8 ) + v;
            b = ((unsigned char)(y + 128) << 8 ) + z;

            if (a > maxa) maxa = a; 
            if (b > maxb) maxb = b; 
            
            if (a < mina) mina = a;
            if (b < minb) minb = b; 

            r = (a - ((maxa + mina) / 2.)) / ((maxa - mina) / 2.); // Mittelwert / Amplitude
            m = (b - ((maxb + minb) / 2.)) / ((maxb - minb) / 2.);
           
            angleXY = (atan2(r, m) / M_PI)  * 180.;  
            if (angleXY < 0) angleXY += 360;
            
             
            sprintf(text,"r %.2f m %.2f | angle %03d | u %03d v %03d | w %03d x %03d | y %03d z %03d", 
            r, m, angleXY, u, v, w,  x, y, z);
            Serial.println(text);
    }



    if (go == 0)
    {
        vL = vR = 0;

        leds[0] = CRGB{0, 0, 0}; // R B G
        leds[1] = CRGB{0, 0, 0};
        leds[2] = CRGB{0, 0, 0};
        leds[3] = CRGB{0, 0, 0};

        FastLED.show();
    }
    else
    {

        if (qSecFlag)  // all 250 msec ... 
        {
            qSecFlag = FALSE;

            pulse = pulse ? 0 : 1;

            // digitalWrite(ON_BOARD_LED, pulse);
            // Serial.print(x); Serial.println(" V");

            if(PS4.isConnected())
            {
                connected = 1;

                // digitalWrite(ON_BOARD_LED, pulse);  // off

                leds[0] = CRGB{0, 0, 255}; // R B G
                leds[1] = CRGB{0, 255, 0};
                leds[2] = CRGB{255, 0, 255};  // Yellow: FF00FF
                leds[3] = CRGB{255, 0, 0};

                FastLED.show();

                if(PS4.Triangle()) Serial.println("Triangle!"); 
                if(PS4.Up()) Serial.println("Up!");

                // Serial.print(PS4.L2Value());
                // Serial.print(PS4.R2Value());
                // Serial.print(PS4.GyrX());
                // Serial.println(PS4.GyrY());
            
            }
            else
            {
                pointer++; if (pointer >= 4) pointer = 0;

                switch(pointer)
                {
                    case 0:
                        leds[0] = CRGB{255, 255, 255}; // R B G
                        leds[1] = CRGB{255, 255, 255};
                        leds[2] = CRGB{0, 0, 0};  // Yellow: FF00FF
                        leds[3] = CRGB{0, 0, 0};
                    break;

                    case 1:
                        leds[0] = CRGB{0, 0, 0}; // R B G
                        leds[1] = CRGB{255, 255, 255};
                        leds[2] = CRGB{255, 255, 255};  // Yellow: FF00FF
                        leds[3] = CRGB{0, 0, 0};
                    break;

                    case 2:
                        leds[0] = CRGB{0, 0, 0}; // R B G
                        leds[1] = CRGB{0, 0, 0};
                        leds[2] = CRGB{255, 255, 255};  // Yellow: FF00FF
                        leds[3] = CRGB{255, 255, 255};
                    break;

                    case 3:
                        leds[0] = CRGB{255, 255, 255}; // R B G
                        leds[1] = CRGB{0, 0, 0};
                        leds[2] = CRGB{0, 0, 0};  // Yellow: FF00FF
                        leds[3] = CRGB{255, 255, 255};
                    break;
                }

                FastLED.show();

            }

            if (LDir == H) winkelL = winkelL + vL/10.; else winkelL = winkelL - vL/10.;
            if (RDir == H) winkelL = winkelL + vR/10.; else winkelL = winkelL - vR/10.;

            winkelL = - angleXY; // driver 24 ! 
//            Serial.print("WinkelL: ");
//            Serial.println(winkelL);

            if (abs((winkelL - oldWinkelL)) > 0.1) 
            {
                winkel = String(winkelL).c_str();
                winkel = "WIN" + winkel;
                notifyClients(winkel);
                oldWinkelL = winkelL;
            }
        }    

        if (tenMSecFlag)
        {
            // "entprellt" 

            tenMSecFlag = FALSE;

            if (connected)
            {
                vL = PS4.L2Value();
                vR = PS4.R2Value();
            
                if(PS4.L1()) {LDir = LDir ? 0 : 1;  digitalWrite(WHEEL_L_DIRECTION, LDir);}
                if(PS4.R1()) {RDir = RDir ? 0 : 1;  digitalWrite(WHEEL_R_DIRECTION, RDir);}
            }
        }
    }

}    

//** Timer Interrupt:   ******************************************

void IRAM_ATTR myTimer(void)   // periodic timer interrupt, expires each 0.1 msec
{
    static int32_t otick  = 0;
    static int32_t qtick = 0;
    static int32_t mtick = 0;
    static unsigned char ramp = 0;
    
    otick++;
    qtick++;
    mtick++;
    ramp++;

    if (otick >= WAIT_ONE_SEC) 
    {
        oneSecFlag = TRUE;
        otick = 0; 
    }

    if (qtick >= WAIT_250_MSEC) 
    {
        qSecFlag = TRUE;
        qtick = 0; 
    }

    if (mtick >= WAIT_ONE_10MSEC) 
    {
        tenMSecFlag = TRUE;
        mtick = 0; 
    }

    // PWM:

    if (ramp > vL) digitalWrite(WHEEL_L, L);  else digitalWrite(WHEEL_L, H);
    if (ramp > vR) digitalWrite(WHEEL_R, L);  else digitalWrite(WHEEL_R, H);

}
