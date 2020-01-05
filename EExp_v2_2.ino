#include <ESP8266WiFi.h>     //INTERNET stuff
#include <ESP8266WiFiMulti.h>
#include <DNSServer.h> 
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <Ticker.h>
#define FS_NO_GLOBALS // SPIFFS and SD card coexistion
#include <FS.h> //accessing SPIFFS files, add 'fs::' in front of the file declaration
#include <WebSocketsServer.h>
//-----SD Card------------------------------------
#include <SPI.h>
#include <SD.h>
//--------------------Temperature-----------------
#include <OneWire.h>
#include <DallasTemperature.h>
//--------------------Pressure and Temperature---
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085.h>
#include <Adafruit_BME280.h>

Adafruit_BMP085 bmp; // if it's new change to bme in handle func and setup!
Adafruit_BME280 bme;
//functions: readTemperature(), readPressure(), readAltitude()
//----------------------OLED Display-------Setup-------
#include "SSD1306Wire.h"     //OLED display stuff
SSD1306Wire display(0x3c, D2, D3); //Old was D1 and D2
//Possible commands are:
//setFont,setTextAlignment,drawString,drawStringMaxWidth,
//setPixel,drawRect,drawHorizontalLine,drawProgressBar,drawXbm
//------------------------------------------------
#define WEB_SERVER_PORT 80
#define WEB_SOCKET_PORT 81

ESP8266WiFiMulti wifiMulti;      // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'
ESP8266WebServer server(WEB_SERVER_PORT);     // create a web server on port 80
WebSocketsServer webSocket(WEB_SOCKET_PORT);  // create a websocket server on port 81

//------------------------------------------------
fs::File fsUploadFile;           // a File variable to store the received file Spiffs
File myFile;                     // SD card instance                  
//------------------------------------------------
#define ONE_WIRE_BUS D4       //  a oneWire instance to 
OneWire oneWire(ONE_WIRE_BUS); //communicate with any OneWire devices 
DallasTemperature sensors(&oneWire); // Pass our oneWire reference to Temperature.
//------------------------------------------------
const char *ssid = "ExpertNET";       // The name of the Wi-Fi network that will be created //////////////////////OVO PROMJENITIIIIIII
String ssidd     = "ExpertNET";       // This one tooo
const char *password = "12345678";   // The password required to connect to it, leave blank for an open network
const char *OTAName = "ESP8266";     // A name and a password for the OTA service
const char *OTAPassword = "esp8266";
const int chipSelect = D4;   //SD CARD  OLD Version = D8    

int brojStranice = 3;
Ticker timer;

#define LED 2  //On board LED
#define LED_RED     15            // specify the pins with an RGB LED connected
#define LED_GREEN   12            // ignore this
#define LED_BLUE    13

struct WebPage;
struct WebPage
  {
    String link;
    int type;
    void (* http_function)();
    void (* function)();
    void (* webSocketEventFunction)(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
  };

const int webPagesSize = 10;
WebPage webPages [webPagesSize];

void initPages()
{
  webPages[0].link                    = "/edit.html";
  webPages[0].type                    = HTTP_POST;
  webPages[0].http_function           = []() {server.send(200, "text/plain", ""); };
  webPages[0].function                = handleFileUpload;
  webPages[0].webSocketEventFunction  = NULL;
  
  webPages[1].link                    = "/index";
  webPages[1].type                    = HTTP_POST;
  webPages[1].http_function           = []() {server.send(200, "text/plain", ""); };
  webPages[1].function                = NULL;
  webPages[1].webSocketEventFunction  = NULL;
  
  webPages[2].link                    = "/NEWWIFI";
  webPages[2].type                    = HTTP_POST;
  webPages[2].http_function           = handleLogin;
  webPages[2].function                = NULL;
  webPages[2].webSocketEventFunction  = NULL;
  
  webPages[3].link                    = "/exp1.html";
  webPages[3].type                    = HTTP_POST;
  webPages[3].http_function           = []() {server.send(200, "text/plain", "");};
  webPages[3].function                = NULL;
  webPages[3].webSocketEventFunction  = webSocketEvent3;
  
  webPages[4].link                    = "/activatedexp";  
  webPages[4].type                    = -1; //
  webPages[4].http_function           = NULL;
  webPages[4].function                = activatedexp_function;
  webPages[4].webSocketEventFunction  = NULL;
  
  webPages[5].link                    = "/readADC"; //empty 
  webPages[5].type                    = -1;
  webPages[5].http_function           = NULL;
  webPages[5].function                = handleADC;
  webPages[5].webSocketEventFunction  = NULL;
  
  webPages[6].link                    = "/exp2.html";
  webPages[6].type                    = HTTP_POST;
  webPages[6].http_function           = []() {server.send(200, "text/plain", "");};
  webPages[6].function                = NULL;
  webPages[6].webSocketEventFunction  = NULL;// webSocketEvent6;
  
  webPages[7].link                    = "/exp2"; //soon obsolete
  webPages[7].type                    = -1;
  webPages[7].http_function           = NULL;
  webPages[7].function                = exp2_function;
  webPages[7].webSocketEventFunction  = NULL;
  
  webPages[8].link                    = "/readBAR"; //soon obsolete
  webPages[8].type                    = -1;
  webPages[8].http_function           = NULL;
  webPages[8].function                = handleBAR;
  webPages[8].webSocketEventFunction  = NULL;
  
  webPages[9].link                    = "/wsfile.html";
  webPages[9].type                    = HTTP_POST;
  webPages[9].http_function           = []() {server.send(200, "text/plain", "");};
  webPages[9].function                = NULL;
  webPages[9].webSocketEventFunction  = webSocketEvent9;
}

////////////////////////////////////////////////////////
//                       Page 0 --- handle upload file// DONE
////////////////////////////////////////////////////////
void handleFileUpload() { // upload a new file to the SPIFFS
  HTTPUpload& upload = server.upload();
  String path;
  if (upload.status == UPLOAD_FILE_START) {
    path = upload.filename;
    if (!path.startsWith("/")) path = "/" + path;
    if (!path.endsWith(".gz")) {                         // The file server always prefers a compressed version of a file
      String pathWithGz = path + ".gz";                  // So if an uploaded file is not compressed, the existing compressed
      if (SPIFFS.exists(pathWithGz))                     // version of that file must be deleted (if it exists)
        SPIFFS.remove(pathWithGz);
    }
    Serial.print("handleFileUpload Name: "); Serial.println(path);
    fsUploadFile = SPIFFS.open(path, "w");            // Open the file for writing in SPIFFS (create if it doesn't exist)
    path = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile) {                                   // If the file was successfully created
      fsUploadFile.close();                               // Close the file again
      Serial.print("handleFileUpload Size: "); Serial.println(upload.totalSize);
      server.sendHeader("Location", "/success.html");     // Redirect the client to the success page
      server.send(303);
    } else {
      server.send(500, "text/plain", "500: couldn't create file");
    }
  }
}

bool handleFileRead(String path) { // send the right file to the client (if it exists)
  Serial.println("handleFileRead: " + path);
  if (path.endsWith("/")) path += "index.html";          // If a folder is requested, send the index file
  //__________ First check if it's in SPIFFS______________
  String contentType = getContentType(path);             // Get the MIME type
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) { // If the file exists, either as a compressed archive, or normal
    if (SPIFFS.exists(pathWithGz))                         // If there's a compressed version available
      path += ".gz";                                         // Use the compressed verion
    fs::File file = SPIFFS.open(path, "r");                    // Open the file
    size_t sent = server.streamFile(file, contentType);    // Send it to the client
    file.close();                                          // Close the file again
    Serial.println(String("\tSent file: ") + path);
    return true;
  }
  //-------- Now check if it's in SD care. IMPORTANT... NAMING has to be like CHART.JS
  if (SD.exists(String(pathWithGz)) || SD.exists(String(path))) { // If the file exists, either as a compressed archive, or normal
    Serial.println("Found on SD CARD");
    if (SD.exists(pathWithGz)) {                        // If there's a compressed version available
      path += ".gz";  }                                       // Use the compressed verion
    File file = SD.open(String(path));                    // Open the file
    size_t sent = server.streamFile(file, contentType);    // Send it to the client
    file.close();                                          // Close the file again
    Serial.println(String("\tSent SD file: ") + path);
    return true;
  }
  Serial.println(String("\tFile Not Found: ") + path);   // If the file doesn't exist, return false
  return false;
}


////////////////////////////////////////////////////////
//                       Page 2 --- WiFi connections  //
////////////////////////////////////////////////////////
void handleLogin() {                         // If a POST request is made to URI /login
  if( ! server.hasArg("NAME") || ! server.hasArg("PASSWORD") 
      || server.arg("NAME") == NULL || server.arg("PASSWORD") == NULL) { // If the POST request doesn't have username and password data
    server.send(400, "text/plain", "400: Invalid Request");         // The request is invalid, so send HTTP status 400
    return;
  }
  String NAME = server.arg("NAME");
  String PSWD = server.arg("PASSWORD");
  server.sendHeader("Location","/");
  server.send(303);
  Serial.println(NAME);
  Serial.println(PSWD);
  if (SPIFFS.exists("LOGIN.txt")) { // LOOKING for login data from spiffs and SD card
    fs::File myFile = SPIFFS.open("LOGIN.txt","r");
  } else {
  myFile = SD.open("LOGIN.txt", FILE_WRITE);
  }  
  String finalString="";
  if (myFile) {
    Serial.println("LOGIN FOUND");
    myFile.println(NAME + "," + PSWD);
    myFile.close();
  }
}
///////////////////////////////////////////////////////
//                       Page 3 ---- Analog reading
///////////////////////////////////////////////////////
void webSocketEvent3(uint8_t num, WStype_t type, uint8_t * payload, size_t length){
  if(type == WStype_TEXT){
    display.clear();
    display.drawString(0, 0,"Web Sockets Started A0" );
    display.drawString(0, 13,"with data rate" );

    float dataRate = (float) atof((const char *) &payload[0]);
    timer.detach();
    if (dataRate!=0) {timer.attach(dataRate, getData3);}
    
    display.drawString(0, 26,String(dataRate));
    display.display();
    }
}

void getData3() {
  String json = "{\"value\":";
  json += analogRead(A0);
  json += "}";
  Serial.println(json);
  webSocket.broadcastTXT(json.c_str(), json.length());
}

///////////////////////////////////////////////////////
//                       Page 4 ---- activatedexp_function
///////////////////////////////////////////////////////
void activatedexp_function(){
  String NAME = server.arg(0);
  Serial.println(NAME);
    display.clear();
    display.drawString(0, 0,"Experiment "+NAME+" started");
    display.drawString(0, 14,"Have Fun!");
    display.display();
    Serial.println("Experiment "+NAME+" started");
    brojStranice = NAME.toInt(); 
}

///////////////////////////////////////////////////////
//                       Page 5 
///////////////////////////////////////////////////////
void handleADC() 
{
 
}

///////////////////////////////////////////////////////
//                       Page 7 
///////////////////////////////////////////////////////
void exp2_function(){
    display.clear();
    display.drawString(0, 0,"Experiment 2 Started" );
    display.display();
    delay(50);
    //bmp.begin(0x77);
    bme.begin(0x76);  
}
///////////////////////////////////////////////////////
//                       Page 8 -- Barometric sensor
///////////////////////////////////////////////////////
void handleBAR() { 
 //float tempB = bmp.readTemperature();
 //float presB = bmp.readPressure();
 /// or
 float tempB = bme.readTemperature();
 float presB = bme.readPressure();
 
 String Value = String(tempB)+","+String(presB);
 server.send(200, "text/plain", Value); //Send value only to client ajax request
 //Serial.println("Temp and Pres: " + Value);
}

///////////////////////////////////////////////////////
//                       Page 9 
///////////////////////////////////////////////////////
void webSocketEvent9(uint8_t num, WStype_t type, uint8_t * payload, size_t length){
  if(type == WStype_TEXT){
    display.clear();
    display.drawString(0, 0,"Web Sockets Started" );
    display.drawString(0, 13,"with data rate" );
    bme.begin(0x76);
    
    float dataRate = (float) atof((const char *) &payload[0]);
    timer.detach();
    if (dataRate!=0) {timer.attach(dataRate, getData9);}

    display.drawString(0, 26,String(dataRate));
    display.display();
    }
}

void getData9() {
//  Serial.println(bmp.readTemperature());
  String json = "{\"value\":";
  json += bme.readTemperature();
  json += "}";
  Serial.println(json);
  webSocket.broadcastTXT(json.c_str(), json.length());
}

///////////////////////////////////////////////////////

void handleNotFound() { // if the requested file or page doesn't exist, return a 404 not found error
  if (!handleFileRead(server.uri())) {// check if the file exists in the flash memory (SPIFFS), if so, send it
    server.send(404, "text/plain", "404: File Not Found");
  }
}


///////////////////////////////////////////////////////


void turnOnWebPage(int i)
{
  if(webPages[i].type != -1 && webPages[i].function != NULL )
    server.on(webPages[i].link,  (HTTPMethod)webPages[i].type, webPages[i].http_function, webPages[i].function);
  else if(webPages[i].type != -1 && webPages[i].function == NULL )
    server.on(webPages[i].link,  (HTTPMethod)webPages[i].type, webPages[i].http_function);
  else if(webPages[i].type == -1 && webPages[i].function != NULL )
    server.on(webPages[i].link,  webPages[i].function); 
}

void init_server()              // Starts the initial servers
{ 
  startWiFi();                 // Start a Wi-Fi access point, and try to connect to some given access points. Then wait for either an AP or STA connection
  //startOTA();                  // Start the Over The Air (OTA) service
  startSPIFFS();               // Start the SPIFFS and list all contents
  startWebSocket();            // Start a WebSocket server

  initPages();

  for(int i = 0; i < webPagesSize; i ++)
    turnOnWebPage(i);
  server.onNotFound(handleNotFound);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length)
{
  webPages[brojStranice].webSocketEventFunction(num, type, payload, length);
}


void init_display()
{
  display.init();              // DISPLAY stuff  
  display.clear();
  display.flipScreenVertically();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
}

void init_SDCard()
{
    while (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    display.drawString(0, 0,"SD CARD ERROR!" );
    display.display();
    delay(1000);//
    }
    Serial.println("card initialized.");
}

void init_OneWire()
{  sensors.begin();             // Start up the OneWire Library!
  Serial.print("Locating devices...\n Found: ");
  delay(40);
  int deviceCount = sensors.getDeviceCount();
  Serial.print(deviceCount, DEC);
  Serial.println(" OneWire devices.\n\n---------");
}

bool write_to_SD(String location, String text)
{
  bool was_open = false;
  File dataFile = SD.open(location, FILE_WRITE);
  if (dataFile)
  {
    was_open = true;
    dataFile.println(text);
    delay(500);
    dataFile.close();
    Serial.println("Saved to SD");
    SD.end();
  }
  else
    was_open = false;

   return was_open;
}

//##########################################################################################################################
/*__________________________________________________________SETUP__________________________________________________________*/
//##########################################################################################################################

void setup() {
  Serial.begin(115200);
  init_display();
  init_SDCard();
  init_server(); //also starts exp pages
  init_OneWire();
  
  server.begin(); 
  bme.begin();

  //------------ All initialized----------\/ 
  display.drawString(0, 0,"All Ready :)" ); // x,y coordinate
  display.drawString(0, 13,"Go to: 192.168.4.1" );
  display.drawString(0, 26, ssidd);
  display.drawString(0, 40, password);
  display.display();
  delay(1000);
  
}

/*__________________________________________________________LOOP__________________________________________________________*/


void loop() {
  webSocket.loop();                           // constantly check for websocket events
  server.handleClient();                      // run the server
  //ArduinoOTA.handle();                        // listen for OTA events
      
}


/*__________________________________________________________THE REST_________________________________________________________________*/
/*__________________________________________________________SETUP_FUNCTIONS__________________________________________________________*/
//#####################################################################################################################################

void startWiFi() { // Start a Wi-Fi access point, and try to connect to some given access points. Then wait for either an AP or STA connection
  WiFi.softAP(ssid, password);             // Start the access point
  Serial.print("Access Point \"");
  Serial.print(ssid);
  Serial.println("\" started\r\n");

  int numberOfNetworks = WiFi.scanNetworks();
  if (SPIFFS.exists("LOGIN.txt")) { // LOOKING for login data from spiffs and SD card
    fs::File myFile = SPIFFS.open("LOGIN.txt", "r");
  } else {
  myFile = SD.open("LOGIN.txt");
  }  
  String finalString="";
  if (myFile) {
    Serial.println("LOGIN.txt FOUND");
    while (myFile.available()) {//This will read lines of username/passwords
      finalString+=(char)myFile.read();
      if (finalString.endsWith("\n")){
         String usr = finalString.substring(0,finalString.indexOf(","));
         String psw = finalString.substring(finalString.indexOf(",")+1,finalString.indexOf("\n"));
         Serial.println("Found the following login data: ");
         Serial.println(usr);
         Serial.println(psw);
         finalString="";
         for(int i =0; i<numberOfNetworks; i++){
             if (usr == WiFi.SSID(i)){
                WiFi.begin(usr, password); // Connect to the network 
                delay(2000);               
                Serial.print("Connecting to ");
                Serial.print(usr); Serial.println(" ...");
                delay(6000);
                Serial.print("IP address:\t");
                Serial.println(WiFi.localIP()); 
                myFile.close();
                break;
         }}}}
    myFile.close();
  } else {
    Serial.println("error opening LOGIN.txt");
  }
}


void startOTA() { // Start the OTA service
  ArduinoOTA.setHostname(OTAName);
  ArduinoOTA.setPassword(OTAPassword);
  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\r\nEnd");
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
  Serial.println("OTA ready\r\n");
}

void startSPIFFS() { // Start the SPIFFS and list all contents
  SPIFFS.begin();                             // Start the SPI Flash File System (SPIFFS)
  Serial.println("SPIFFS started. Contents:");
  {
    fs::Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {                      // List the file system contents
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      Serial.printf("\tFS File: %s, size: %s\r\n", fileName.c_str(), formatBytes(fileSize).c_str());
    }
    Serial.printf("\n");
  }
}

void startWebSocket() { // Start a WebSocket server
  webSocket.begin();                          // start the websocket server
  webSocket.onEvent(webSocketEvent);          // if there's an incomming websocket message, go to function 'webSocketEvent'
  Serial.println("WebSocket server started.");
}

typedef std::function<void(void)> THandlerFunction;



/*__________________________________________________________HELPER_FUNCTIONS__________________________________________________________*/
String MakeMine(const char *Template)
{
  uint16_t uChipId = ESP.getChipId();
  String Result = String(Template) + String(uChipId, HEX);
  return Result;
}


String formatBytes(size_t bytes) { // convert sizes in bytes to KB and MB
  if (bytes < 1024) {
    return String(bytes) + "B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + "KB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  }
}

String getContentType(String filename) { // determine the filetype of a given filename, based on the extension
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}
