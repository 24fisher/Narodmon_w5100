
#include <Adafruit_BMP085.h>
#include <SPI.h>
#include <Ethernet.h>
#include <DHT.h>
#include <Wire.h> // Comes with Arduino IDE
#include <LiquidCrystal_I2C.h>


#define DHT22PIN 2 
#define DHT11PIN 3

#define DHTTYPE DHT22   // DHT 22  (AM2302)                         // Set maximum number of devices in order to dimension
  
#define RELAYPIN_LAMP1 7 //Основная лампа
#define TEMPERATURE_LOW_LIMIT 15
#define TEMPERATURE_HIGH_LIMIT 15

//Adafruit_BMP085 dps = Adafruit_BMP085();  
 
bool Debug = true; //режим отладки
 
//****************************************************************************************
byte mac[] = {  0xAB, 0xC1, 0x23, 0x45, 0x67, 0x89 }; //MAC-address
char mac_post[]="ABC123456789";

const unsigned long postingInterval = 600000;  // интервал между отправками данных в миллисекундах (10 минут)
//****************************************************************************************
 
//IPAddress server(94,19,113,221); // IP сервера
IPAddress server(94,142,140,101); // IP сервера


EthernetClient client;
 
unsigned long lastConnectionTime = 0;           // время последней передачи данных
boolean lastConnected = false;                  // состояние подключения
 
int HighByte, LowByte, TReading, SignBit, Tc_100, Whole, Fract;
 
char replyBuffer[160];

DHT dht(DHT22PIN, DHTTYPE);

DHT dht11(DHT11PIN, DHT11);

//LiquidCrystal_I2C lcd(0x3F, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE); // Set the LCD I2C address
LiquidCrystal_I2C lcd(0x3F, 1, 3); //


int _cycle_counter = 0;
bool resetFlag = false;

void LCDPrint(int rowNum, String str, bool bClear=false)
{
  if(bClear)
  {
    lcd.clear();
  }
  lcd.setCursor(0,rowNum);
  lcd.print(str); 
  Serial.println(str);
  delay(1000);
}

 
void setup() {

   Serial.begin(9600); // Used to type in characters
   //lcd.begin(16,2); // initialize the lcd for 16 chars 2 lines, turn on backlight

   lcd.begin();
   // ------- Quick 3 blinks of backlight -------------
   for(int i = 0; i< 3; i++)
   {
     lcd.backlight();
     delay(250);
     lcd.noBacklight();
     delay(250);
   }
   lcd.backlight(); // finish with backlight on

   delay(1000);  
  
   LCDPrint(0,"Power on!",1); 
   LCDPrint(1,"narodmon_w5100"); 
   LCDPrint(2,"v2.0"); 
   LCDPrint(3,"Starting arduino...");  
   LCDPrint(0,"Connecting toLAN...",1); 
   delay(1000);  


   
    // Ethernet connection:
    if (Ethernet.begin(mac) == 0) 
    {

      LCDPrint(0,"Ethernet error!",1); 
      LCDPrint(1,"Rebooting soon"); 
      delay(5000);
      //for(;;);
      //Ждем минуту
      //delay(60000);
      //String strMessage = "";
      char strMessage[4];
      //String counter_string = "";
      char counter_string[4];
      LCDPrint(0,"Rebooting in",1); 
      for (int i=60;i>0;i--)
      {
        
        dtostrf(i,2,0,counter_string);
        //counter_string = "";
        
        LCDPrint(0,"Rebooting in",0); 
        lcd.setCursor(0,1); //Start at character 0 on line 0
        lcd.printstr(counter_string); 
        
      }
      //Ребутимся  и пробуем еще раз
      resetFlag = true; //вызываем reset
      
    }
    else
    {
      LCDPrint(1,"Connected!"); 
    }
    
    delay(1000);  
    // секунда для инициализации Ethernet
    delay(1000);

     _cycle_counter = 0;
    lastConnectionTime = millis()-postingInterval+15000; //первое соединение через 15 секунд после запуска
   dht11.begin();
  // dps.begin(); 

}


void(* resetFunc) (void) = 0; // объявляем функцию reset


void loop()
{
  if ((_cycle_counter > 72)||(resetFlag))//перезагрузка 2 раза в сутки либо при отсутствии соединения ~через пол часа
  {
    delay(1000); // ждем одну секунду

    LCDPrint(0,"Rebooting...",1); 
    
    resetFunc(); //вызываем reset
    
    delay(300);
    
    LCDPrint(0,"Rebooting ",1); 
    LCDPrint(1,"failed! "); 
  }
  //Если вдруг нам случайно приходят откуда-то какие-то данные,
  //то просто читаем их и игнорируем, чтобы очистить буфер
  if (client.available()) 
  {
    client.read();
  }
 
  if (!client.connected() && lastConnected) 
  {
     if (Debug)
    {
      Serial.println("disconnecting.");
    }
      client.stop();
  }
  
  

  if (!client.connected() && (millis() - lastConnectionTime < postingInterval)) 
  {
    
    char time[10];
    char postingTime[10];
    dtostrf(((millis() - lastConnectionTime)/1000),3,0,time);
    dtostrf((postingInterval/1000),3,0,postingTime);
    
    lcd.setCursor(0,3);
    lcd.print("Time: ");
    lcd.printstr(time);
    lcd.print("/");
    lcd.printstr(postingTime);
    delay(995);
    //LCDPrint(3,time); 
  }
  
    //если не подключены и прошло определённое время, то делаем замер,
  //переподключаемся и отправляем данные
  if (!client.connected() && (millis() - lastConnectionTime > postingInterval)) 
  {

    if (Debug)

    {

      char time[10];
      dtostrf(postingInterval,4,1,time);
      Serial.println(time);
    dtostrf(millis() - lastConnectionTime,4,1,time);
      Serial.println(time);

      dtostrf(lastConnectionTime,4,1,time);
      Serial.println(time);


    }
    
    //формирование HTTP-запроса
    memset(replyBuffer, 0, sizeof(replyBuffer));
 
    
    delay(5000);
    char t_string[10];
    char h_string[10];
    char t_11_string[10];
    char cycle_counter_string[5];
    String s_cycle_counter_string;
    char s_pressure_string[3];
    
    
    //Теперь опросим датчики

    LCDPrint(0,"Collecting sensors",1); 
    LCDPrint(1,"Readings..."); 
    
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    float t11 = dht11.readTemperature();
    int Pressure = 0;
   // Pressure = dps.readPressure(); 
    
    dtostrf(_cycle_counter,5,0,cycle_counter_string);
    dtostrf(t,4,1,t_string);
    dtostrf(t11,4,1,t_11_string);
    s_cycle_counter_string = cycle_counter_string;
    s_cycle_counter_string.trim();
    dtostrf(Pressure,4,1,s_pressure_string);
    

    strcat(replyBuffer,"ID=");
    strcat(replyBuffer,mac_post);
    strcat(replyBuffer,"&");
    strcat(replyBuffer,mac_post);
    strcat(replyBuffer,"01");
    strcat(replyBuffer,"=");
    strcat(replyBuffer,t_string);
    
    strcat(replyBuffer,"&");
    strcat(replyBuffer,mac_post);
    strcat(replyBuffer,"02");
    strcat(replyBuffer,"=");
    strcat(replyBuffer,t_11_string);

    strcat(replyBuffer,"&");
    strcat(replyBuffer,mac_post);
    strcat(replyBuffer,"03");
    strcat(replyBuffer,"=");
    strcat(replyBuffer,s_cycle_counter_string.c_str());

    //strcat(replyBuffer,"&");
    //strcat(replyBuffer,mac_post);
    //strcat(replyBuffer,"04");
    //strcat(replyBuffer,"=");
    //strcat(replyBuffer,s_pressure_string);

    LCDPrint(2,"Readings collected!"); 
    delay(2000);
    //lcd printing
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Fl2_t:");
    lcd.setCursor(7,0);
    lcd.printstr(t_string);
    lcd.print("/");
    lcd.print("cycle");
    lcd.setCursor(0,1);
    lcd.print("Fl1_t:");
    lcd.setCursor(7,1);
    lcd.printstr(t_11_string);
    lcd.print ("/");
    lcd.printstr(s_cycle_counter_string.c_str());
    strcat(replyBuffer,'\0');
 
    if (Debug)
    {
      Serial.println(replyBuffer);
      Serial.print("Content-Length: ");
      Serial.println(len(replyBuffer));

      //Serial.println(s_pressure_string);
    }
    delay(5000);
    //отправляем запрос
    httpRequest();

    //repeating lcd printing
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Fl2_t:");
    lcd.setCursor(7,0);
    lcd.printstr(t_string);
    lcd.print("/");
    lcd.print("cycle");
    lcd.setCursor(0,1);
    lcd.print("Fl1_t:");
    lcd.setCursor(7,1);
    lcd.printstr(t_11_string);
    lcd.print ("/");
    lcd.printstr(s_cycle_counter_string.c_str());
    strcat(replyBuffer,'\0');
    
    _cycle_counter++;
 
  }
  //храним последнее состояние подключения
  lastConnected = client.connected();
}


void httpRequest() {

  LCDPrint(0,"Connecting to ",1); 
  LCDPrint(1,"narodmon host.."); 
  if (client.connect(server, 80))
  {
    
    // send the HTTP POST request:
    client.println("POST http://narodmon.ru/post.php HTTP/1.0");
    client.println("Host: narodmon.ru");
    //client.println("User-Agent: arduino-ethernet");
    //client.println("Connection: close");
    client.println("Content-Type: application/x-www-form-urlencoded");
    client.print("Content-Length: ");
    client.println(len(replyBuffer));
    client.println();
    client.println(replyBuffer);
    client.println();
 
    lastConnectionTime = millis();
     if (Debug)
      {
        
      }
     LCDPrint(0,"Data sent!",1); 
     delay(5000);
      
  } 
  else
  {
    if (Debug)
    {
      
    }
      LCDPrint(0,"Connection",1); 
      LCDPrint(1,"to host error!"); 
      LCDPrint(2,"Retrying soon.."); 
      delay(5000);
      
      
    client.stop();
  }
}
 
 
int len(char *buf)
{
  int i=0; 
  do
  {
    i++;
  } while (buf[i]!='\0');
  return i;
}
 
void itos(int n, char bufp[3]) //int to string
{
  char buf[3]={'0','0','\0'}; 
  int i = 1;
 
  while (n > 0) {
    buf[i] = (n % 10)+48;
    i--;
    n /= 10;
  }
 
  for (i=0; i<3; i++)
    bufp[i]=buf[i];
}

