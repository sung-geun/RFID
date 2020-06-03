
#include <Arduino.h>
#include <ArduinoJson.h> //JSON 형식 데이터 사용
#include <WiFiManager.h>  // WIFI 사용을 위해
#include <WebSocketsClient.h> // 소켓 통신위해

#include <Z_UpdateChk.h>   //프로그램 업데이트 라이브러리

#include <Adafruit_GFX.h>    //LCD 라이브러리
#include <Adafruit_ILI9341.h> //LCD 라이브러리

#include <SPI.h>   //RFID 
#include <MFRC522.h>   //RFID


#pragma region 공통 선언부
#pragma region 1. 와이파이

/**************************************1. 와이파이 설정 변수*************************************************/
String devIP;               //IP담는 변수
WiFiManager wifiManager;  //testttㅅㅅㅅㅅㅅㅅ
 
#pragma endregion  
#pragma region 2. 펌웨어 자동 업데이트 설정 변수
/**************************************2. 펌웨어 자동 업데이트 설정 변수**************************************/
int FW_VERSION =1;  //업데이트 할 때마다 버전 업
const char* fwUrlBase = "http://210.105.97.17/arduino/IOT_RFID/";
String FW_fileName =  "IOT_RFID.ino.d1_mini";  //지금 파일명으로 만들어짐 RFID.ino.~ 로 만들어진다
Z_UpdateChk z_updateChk;
#pragma endregion  
#pragma region 3  모듈의 기준정보를 담기위한 변수들(jSON 사용) 
/********************************3 모듈의 기준정보를 담기위한 변수들(jSON 사용) ************************************************/

const char* Jsonfilename = "/config.json";  //모듈의 기준정보가 들어가 있는 파일명
DynamicJsonDocument configDoc(1024);
//모듈의 기준정보관련 변수 
JsonObject configParam ;

    // String FILENAME;
    // String ID;
    // String DEVICE;
    // String IP;
    // String COMPANY;
    // String PLANT;
    // String WORKCENTER;
    // String SENSOR;
    // String DATA;         	
 

#pragma endregion  
#pragma region 4. 모듈에 대한 기준정보 설정을 할수있는 서버페이지 관련 변수
/******************************4. 모듈에 대한 기준정보 설정을 할수있는 서버페이지 관련 변수*****************************/
WebSocketsClient configWsSocket;
#pragma endregion  
#pragma region 5. DB 및 환경설정을 위한 데이터 변수
/******************************5.DB 및 환경설정을 위한 데이터 변수 *****************************/
WebSocketsClient dataWsSocket;

#pragma endregion  
#pragma region 6. TFT LCD 설정
/********************* TFT LCD 설정**************************/
// For the Adafruit shield, these are the default.
#define TFT_CS D0  //for D1 mini or TFT I2C Connector Shield (V1.1.0 or later)    //핀 설정
#define TFT_DC D8 //for D1 mini or TFT I2C Connector Shield (V1.1.0 or later)     //핀 설정
//  #define TFT_CS 10  //for D1 mini or TFT I2C Connector Shield (V1.1.0 or later)
//  #define TFT_DC 9 //for D1 mini or TFT I2C Connector Shield (V1.1.0 or later)
// #define TFT_RST -1 //for D1 mini or TFT I2C Connector Shield (V1.1.0 or later)
// #define TS_CS D3  //for D1 mini or TFT I2C Connector Shield (V1.1.0 or later)

// Use hardware SPI (on Uno, #13, #12, #11) and the above for CS/DC
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
int lastPositionY=0;    //LCD 밑에 값을 지우기 위한 값?
#pragma endregion  
#pragma endregion   	 

/*******************************5. DHT22 변수**********************************************************/
// int pinDHT22 = D6;
// SimpleDHT22 dht22(pinDHT22);
/*******************************5. RFID변수 **********************************************************/
#define RST_PIN  5  // RST-PIN für RC522 - RFID - SPI - Modul GPIO5 
#define SS_PIN  4  // SDA-PIN für RC522 - RFID - SPI - Modul GPIO4 
//#define SS_PIN D8   //핀 설정
//#define RST_PIN D3  //핀 설정
 
MFRC522 rfid(SS_PIN, RST_PIN); // Instance of the class  //RFID 변수
MFRC522::MIFARE_Key key;   //RFID 변수

// Init array that will store new NUID 
byte nuidPICC[4];  //RFID 변수

/*******************************6. SOUND,LED 변수 **********************************************************/
int myLed = D3;
int mysound = D4;

/***************************************************************************/
void setup(){
    
    pinMode(myLed, OUTPUT);
    pinMode(mysound, OUTPUT);
    digitalWrite(myLed,LOW); 
    digitalWrite(mysound,LOW);     

    #pragma region setup 셋업   
    tft.begin();                    //LCD 시작
    tft.setRotation(1);
    tft.fillScreen(ILI9341_BLACK);  // 스크린을 검은색으로 가득 채운다
    tft.setCursor(0, 0);            //좌표 찍기
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2); //텍스트는 하얀색 사이즈는 2로 함
   
    Serial.begin(115200);
    for(uint8_t t = 4; t > 0; t--) {
		Serial.printf("[SETUP] BOOT WAIT %d...\n", t);
        
		Serial.flush();
		delay(500);
	}
    /******************1SPIFFS 연결// 설정파일 불러오기****************/    
    if(!SPIFFS.begin()){               
        Serial.println("SPIFFS Mount Fail...");        
        tft.println("SPIFFS Mount Fail...");        
        return;   //setup업이 완료되어야 다음단계 넘어가는데 못넘어가면 그냥 멈춘다 아두이노 직접 확인 필요!
    } 
    Serial.println("SPIFFS Mount Success");
    tft.println("SPIFFS Mount Success");

    /******************2.Wifi***********************************/
    devIP = IPSetup();// 와이파이 ip 설정
    
    /*************4. 모듈의 기준정보 데이터 Json으로 저장******/          
    loadJsonConfig();    
    tftText();  //json 파일 읽은 후 LCD에 텍스트값 뿌리기

    /*************3. 펌웨어 업데이트 정보 확인************************/   
    // z_updateChk.getUpdateChk(FW_fileName,FW_VERSION,fwUrlBase);
    z_updateChk.getUpdateChk(FW_fileName,FW_VERSION, ("http://210.105.97.17/arduino/"+configParam["SENSOR"].as<String>() + "/").c_str());


    /*************5. 데이터 및 환경설정 웹소켓********************/    
	dataWsSocket.begin("192.168.211.75", 15000, "/");// server address, port and URL	
	dataWsSocket.onEvent(dataWebSocketEvent);// event handler	
	dataWsSocket.setReconnectInterval(5000);    // try ever 5000 again if connection has failed

	configWsSocket.begin("210.105.97.17", 5678, "/");// server address, port and URL	
	configWsSocket.onEvent(configWebSocketEvent);// event handler	
	configWsSocket.setReconnectInterval(5000);    // try ever 5000 again if connection has failed

#pragma endregion   

    //RFID사용시 주석처리 해제해야함
    //  /*******************RFID***********************/
    SPI.begin(); // Init SPI bus
    rfid.PCD_Init(); // Init MFRC522 

    
}

//딜레이용으로 변수 사용
long interval = 100;
long previousMillis = 0;
String previousData="";


//루프는 순수 센서들만 돌도록 만듬
void loop(){

    
     dataWsSocket.loop();   //DATA 전송 루프(handle)
     configWsSocket.loop(); //환경 설정 루프(handle)
    
    
    unsigned long currentMillis = millis();    // delay 대신 1초를 지연 시키는 것
    if(currentMillis - previousMillis > interval) 
    {             
        previousMillis = currentMillis;    
        // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
        if ( ! rfid.PICC_IsNewCardPresent()){   
            //Serial.println(F("PICC_IsNewCardPresent "));
            delay(50); 
     
            return;
        }                
        // Verify if the NUID has been readed
        if ( ! rfid.PICC_ReadCardSerial()){
            Serial.println(F("PICC_ReadCardSerial: "));
            delay(50);

            return;
        }
        // Show some details of the PICC (that is: the tag/card)
        Serial.print(F("Card UID:"));
        String str = uid_string(rfid.uid.uidByte, rfid.uid.size);
        Serial.println(str);           
        
        tft.fillRect(0,lastPositionY,ILI9341_TFTWIDTH,ILI9341_TFTHEIGHT,ILI9341_BLACK);
        tft.setCursor(0,lastPositionY);
        tft.print(str);               
        
        
        
        
        digitalWrite(myLed,HIGH);
        digitalWrite(mysound,HIGH);
        delay(200);
        //configParam["DATA"] = (String)str;
        //String output="";    
        // serializeJson(configParam, output);  
        // Serial.println(output);
        // dataWsSocket.sendTXT(output); 
        DataJsonSend(str);
        digitalWrite(myLed,LOW); 
        digitalWrite(mysound,LOW);                  

    }
}
void DataJsonSend(String jsonString){
    StaticJsonDocument<512> doc;
    
    doc = configParam;    
    doc["DATA"].set(jsonString);
    String sendData="";
    serializeJson(doc, sendData);   
    Serial.println(sendData);
    dataWsSocket.sendTXT(sendData); 
    doc.clear();
    
    
}   


// UID string
String uid_string(byte *buffer, byte bufferSize) {
  String uid = "";
  
  for (byte i = 0; i < bufferSize; ++i) {
    uid += buffer[i];
  }

  return uid;
}     


void array_to_string(byte array[], unsigned int len, char buffer[])
{
       for (unsigned int i = 0; i < len; i++)
       {
           byte nib1 = (array[i] >> 4) & 0x0F;
           byte nib2 = (array[i] >> 0) & 0x0F;
           buffer[i*2+0] = nib1  < 0xA ? '0' + nib1  : 'A' + nib1  - 0xA;
           buffer[i*2+1] = nib2  < 0xA ? '0' + nib2  : 'A' + nib2  - 0xA;
       }
       buffer[len*2] = '\0';
} 

void tftText(){

    tft.fillScreen(ILI9341_BLACK);
    tft.setCursor(0, 0);
    tft.println();
    tft.setTextColor(ILI9341_WHITE);
    tft.print("Ver: ");
    tft.println(FW_VERSION);
    tft.print("DEVICE: ");
    tft.println(configParam["DEVICE"].as<String>());
    tft.print("IP: ");
    tft.println(configParam["IP"].as<String>());

    tft.print("COMPANY: ");
    tft.println(configParam["COMPANY"].as<String>());    
    tft.print("WC: "); 
    tft.println(configParam["WORKCENTER"].as<String>()); 
    tft.print("SENSOR: "); 
    tft.println(configParam["SENSOR"].as<String>()); 
    tft.println();
    tft.setTextColor(ILI9341_GREEN);
    tft.println("SCAN ");
    tft.println();
    tft.setTextColor(ILI9341_RED);
    lastPositionY = tft.getCursorY();

    //tft.println(data);     
}

//1.와이파이 설정 함수
String  IPSetup()
{
    tft.println("WIFI connecting..");
         
    //초기 IP연결 192.168.4.1 임 //폰이나 노트북으로 연결 후 WIFI잡아줘야함(웹접속)
    wifiManager.autoConnect("IOT_DEVICE");  
    Serial.println("WIFI success...");
    tft.println("WIFI success...");
    //String myIP ="";
    String myIP = wifiManager.getIP();    
    
    Serial.printf("IP ADDRESS : ");    
    tft.println("IP ADDRESS");
    Serial.println(myIP);
    tft.println(myIP);
   
    return myIP;
}

bool loadJsonConfig() 
{
  // Json 파일 읽기
  File configFile = SPIFFS.open(Jsonfilename,"r");   //"r" 은 읽으세요 "w" 쓰세요
  if(!configFile)
  {
      Serial.println("Failed to open Json config");
      configParam = configDoc.to<JsonObject>();
      configParam["ID"] = "11";
      configParam["DEVICE"] = "ARDUINO";
      configParam["IP"] = devIP;
      configParam["COMPANY"] = "000";
      configParam["PLANT"] = "000";
      configParam["WORKCENTER"] = "000";
      configParam["SENSOR"] = "000";    
      configParam["DATA"] = "000";
      return false;

  }
  Serial.println("open Json config");
  // Json문서의 크기가 작으면 stack을 사용해서 실행 크기를 줄이고 성능을 높일 수 있다고 함.(StaticJsonDocument 사용)
  // Json문서의 크기가 크면 heap을 쓰는게 타당하다고.(DynamicJsonDocument 사용)
  
  DeserializationError error = deserializeJson(configDoc, configFile);/// Deserialize the JSON document : Json문서 분석
   // Test if parsing succeeds. : 파싱 에러 발생 시 출력
  if (error) 
  {
    Serial.print("Json Load Error :Failed to read file, using default configuration ");
    Serial.println(error.c_str());     
  }
//json객체에 저장
  //json객체에 저장
  configParam = configDoc.as<JsonObject>();
  configParam["ID"] = "11";
  
}


// Json 파일로 쓰기
void saveJsonConfig(DynamicJsonDocument doc) {
 

  // 파일 쓰기 준비
    File file = SPIFFS.open(Jsonfilename, "w");
    if (!file) {
        Serial.println("Failed to open SPIFFS");
        
        return;
    }
       

    //파일 쓰기 
    if (serializeJson(doc, file) == 0) {
        Serial.println("Failed to Write Json file");

    }
    Serial.println("Json write Success");
    // 파일닫기
    file.close();
}


void dataWebSocketEvent(WStype_t type, uint8_t * payload, size_t length) {

	switch(type) {
		
		case WStype_TEXT:{
			//Serial.printf("[dataWebSocketEvent] get text: %s\n", payload);								 			 
			String command = String((char*)payload);
  			
		    //Serial.println(command);
            DynamicJsonDocument dataSocketJson(1024);
			DeserializationError error = deserializeJson(dataSocketJson, command);
            
			if (error) {
				Serial.print(F("dataWebSocketEvent-deserializeJson() failed: "));
				Serial.println(error.c_str());
				return;
			}
            JsonObject rcvParam = dataSocketJson.as<JsonObject>();
            //Serial.print(" rcvParam :");
            //Serial.println(rcvParam["ID"].as<String>());        
                        
            if( rcvParam["ID"].as<String>()=="81"){
                dataWsSocket.sendTXT("{'ID':'82'}");
            }
            
			
		}
		break;
		
    }

}

void configWebSocketEvent(WStype_t type, uint8_t * payload, size_t length) {

    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[configWebSocketEvent] Disconnected!\n");
            break;
        case WStype_CONNECTED: {
            String output;                    
                configParam["ID"] = "11";                 
                serializeJson(configDoc, output);   
                Serial.println(output);
                configWsSocket.sendTXT(output);    
                                
        }
            break;
        case WStype_TEXT:{
            Serial.printf("[configWebSocketEvent] get text: %s\n", payload);                                             
            String command = String((char*)payload);
            Serial.print(F("WStype_TEXT: "));
            Serial.println(command);
            DynamicJsonDocument WebdataDoc(1024);
            DeserializationError error = deserializeJson(WebdataDoc, command);
            if (error) 
            {
                Serial.print(F("configWebSocketEvent-deserializeJson() failed: "));
                Serial.println(error.c_str());
                return;
            }
             
            if(WebdataDoc["ID"].as<String>()=="23")
            {
                Serial.print("configWebSocketEvent command :");
                Serial.println(command);
               
                configParam = WebdataDoc.as<JsonObject>();
                                            
                String output="";                    
                configParam["ID"] = "14";                 
                serializeJson(configParam, output); 
                saveJsonConfig(WebdataDoc);
                tftText();
                configWsSocket.sendTXT(output);                                 

                Serial.print("configParam");
                Serial.println(output);
                output=""; 
                serializeJson(configDoc, output);   
                Serial.print("configDoc");
                Serial.println(output);

                output=""; 
                serializeJson(WebdataDoc, output);  
                Serial.print("WebdataDoc");
                Serial.println(output);
                
            }
            
            
        }
        break;
        
    }

}

