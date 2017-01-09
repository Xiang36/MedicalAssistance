/*
 Setup your scale and start the sketch WITHOUT a weight on the scale
 Once readings are displayed place the weight on the scale
 Press +/- or a/z to adjust the calibration_factor until the output readings match the known weight
 Arduino pin 6 -> HX711 CLK
 Arduino pin 5 -> HX711 DOUT
 Arduino pin 5V -> HX711 VCC
 Arduino pin GND -> HX711 GND 
*/
#include <SoftwareSerial.h>
#include "HX711.h"
#define DEBUG true //顯示輸出值

HX711 scale(3, 2);

float weightVal = 0;
float heartVal = 0;
float calibration_factor = 2230; // this calibration factor is adjusted according to my load cell
float units;
float ounces;
const int ledPin = 13;
const int sensorPin = A0;
const double alpha = 0.75;              // smoothing參數 可自行調整0~1之間的值
const double beta = 0.5;                // find peak參數 可自行調整0~1之間的值
const int period = 20;                  // sample脈搏的delay period

// WiFi 連線參數
String SID = "iPhone"; // WiFi 名稱
String PWD = "Xiang123Wifi";               // WiFi 密碼
String IP = "140.120.54.104";          // Server IP
String file ="DrPulse/DrPulse/api/InjectionData/AddData";   // GET 的檔案位址
boolean upload = false;
SoftwareSerial esp8266(8,9); // RX：接收資料, TX：傳送資料

void setup() {
  Serial.begin(9600);
  pinMode(ledPin, OUTPUT);
  Serial.println("HX711 calibration sketch");
  Serial.println("Remove all weight from scale");
  Serial.println("After readings begin, place known weight on scale");
  Serial.println("Press + or a to increase calibration factor");
  Serial.println("Press - or z to decrease calibration factor");
  esp8266.begin(115200);
  scale.set_scale();
  scale.tare();  //Reset the scale to 0

  long zero_factor = scale.read_average(); //Get a baseline reading

  init_wifi(); // 將 WiFi 模組（ESP8266）進行初始化
}

void loop() {
  senseHeartRate();
}
void calculateWeight(){
  scale.set_scale(calibration_factor); //Adjust to this calibration factor
//  Serial.print("Weight value = ");
  units = scale.get_units(), 10;
  if (units < 5)
  {
    units = 0.00;
  }
  ounces = units * 0.035274;
//  Serial.println(units);
  weightVal = units;

  if(Serial.available())
  {
    char temp = Serial.read();
    if(temp == '+' || temp == 'a')
      calibration_factor += 1;
    else if(temp == '-' || temp == 'z')
      calibration_factor -= 1;
  }
}
void senseHeartRate()
{
    int count = 0;                              // 記錄心跳次數
    double oldValue = 0;                        // 記錄上一次sense到的值
    double oldChange = 0;                       // 記錄上一次值的改變
       
    unsigned long startTime = millis();         // 記錄開始測量時間
   
    while(millis() - startTime < 10000) {       // sense 10 seconds
        int rawValue = analogRead(sensorPin);   // 讀取心跳sensor的值
        double value = alpha*oldValue + (1-alpha)*rawValue;     //smoothing value
        calculateWeight();
        heartVal = value;
        uploadData();
//        Serial.println(value);
        delay(1000);
        //find peak
        double change = value-oldValue;         // 計算跟上一次值的改變量
        if (change>beta && oldChange<-beta) {   // heart beat
            count = count + 1;
        }
         
        oldValue = value;
        oldChange = change;
        delay(period);
    }
    Serial.print(count);
    Serial.print(",");
    //BTSerial.println(count*6);          //use bluetooth to send result to android
}
// 初始化 WiFi 模組（ESP8266）
void init_wifi(){
  Serial.println("|---Reset---|");
  sendCommand("AT+RST\r\n",5000,DEBUG); // 將模組初始化，也就是重新設定
  sendCommand("AT+CWMODE=1\r\n",2000,DEBUG); // 更改模式
  sendCommand("AT+CWJAP=\""+SID+"\",\""+PWD+"\"\r\n",5000,DEBUG); // 設定 WiFi 連線參數（名稱及密碼）
  sendCommand("AT+CIPMUX=0\r\n",2000,DEBUG); // 設定連線一個 Server
  Serial.println("|---Reset Finish---|");
}

// 傳送指令
String sendCommand(String command, const int timeout, boolean debug)
{
    String response = "";
    
    esp8266.print(command); // 傳送指令給 ESP8266
    
    long int time = millis();
    
    while( (time + timeout) > millis())
    {
      // 如果 ESP8266 有收到資料
      while(esp8266.available())
      {    
        // 以字元儲存 ESP8266 的 OUTPUT
        char c = esp8266.read(); 
        response+=c;
      }  
    }
    
    if(debug)
    {
      Serial.print(response);
    }
    
    return response;
}
// 上傳資料
void uploadData()
{

  // 將指令轉成字串
  // TCP 連線
  String cmd = "AT+CIPSTART=\"TCP\",\""; // 與 Server 進行連線
  cmd += IP; //host
  cmd += "\",80"; // 設定 Server 的 port
  esp8266.println(cmd);
   
  if(esp8266.find("Error")){
    Serial.println("AT+CIPSTART error"); // 與 Server 連線錯誤
    return;
  }
  
  // 設定 GET 的指令字串
  // 利用 HTTP 中的 GET 方法將光敏電阻的值送到 Server 當中
  Serial.println(heartVal);
  Serial.println(weightVal);
  String getStr = "GET /" + file + "/" + String(heartVal) + "/" + String(weightVal) + "/1";
//  String getStr = "GET /" + file + "/80.2/200/1";

  getStr +=" HTTP/1.1\r\nHost:"+IP;
  getStr += "\r\n\r\n";

  // 送出資料的長度
  cmd = "AT+CIPSEND=";
  cmd += String(getStr.length());
  esp8266.println(cmd);
  
  if(esp8266.find(">")){
    esp8266.print(getStr);
  }
  else{
    // 關閉 WiFi 連線
    esp8266.println("AT+CIPCLOSE");
    // alert user
    Serial.println("AT+CIPCLOSE");
  }
  delay(1000);  
}
