#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include "DHT.h"
#include <Servo.h>
#include <Adafruit_NeoPixel.h>


// LCD screen pins : A4 -> SDA
//                   A5 -> SCL

#define BTNRIGHT 0
#define BTNUP 1
#define BTNDOWN 2
#define BTNLEFT 3
#define BTNNONE 4
#define debounceBTN 150


#define RELAY_PIN1 13
#define RELAY_PIN2 3
#define RELAY_PIN3 4
#define RELAY_PIN4 5

#define PIXEL_PIN 6 
#define DHT_PIN 7
#define SERVO_PIN 9 
#define ENABLE_SERVO_PIN 10
#define ENABLE_FAN1_PIN 11
#define ENABLE_FAN2_PIN 12
#define DOOR_SWITCH_PIN 2
#define BUTTON_PIN A7

#define DHT_TYPE DHT22
#define REFRESH_DISP 200
#define REFRESH_LIGHTS 500
#define REFRESH_AIRFLOW 1000
#define REFRESH_TEMP 5000
#define TIME_TO_IDLE 600000
#define SERVO_ACTIVE_TIME 2000
#define AUTO_AIRFLOW_REFRESH 30000
#define ANIMATION_DELAY 2500

#define closedState 60
#define openedState 250



Adafruit_NeoPixel strip = Adafruit_NeoPixel(60, PIXEL_PIN, NEO_GRB + NEO_KHZ800);
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE); // Set the LCD I2C address
DHT dht(DHT_PIN, DHT_TYPE);
Servo myservo;




unsigned long lastExecDHT = 0;
unsigned long lastExecLIGHT = 0;
unsigned long lastExecAirFlow = 0;
unsigned long lastExecDISP = 0;
unsigned long activatedServo = 0;
unsigned long lastAction = 0;
unsigned long lastAutoAirFlow = 0;

float humidity = 0;
float temperature = 0;
byte lightMode = 0; // 0:DEFAULT | 1:ALWAYS ON | 2:ALWAYS OFF | 3:ANIMATION 
byte lightState = 0;
byte servoState = openedState; // SERVO VALUE SEE closedState and openedState
bool servoEnState = 0; // enable or disable servo
bool fan1State = 1; 
bool fan2State = 1;
byte coolingMode = 0; // 0:COOLING | 1:AUTO | 2:HEAT | 3:NONE
byte coolingState = 0;
byte menu_state = 0; // 0:MAIN | 1:VIEWPARAMS | 2:CHANGEPARAMS
byte menu2_state = 0; // 0:MAIN | 1:COOLING | 2:LIGHT | 3:3DPrinterState

bool printerState = 1;

bool backLightState = 1;

float controlledTemp = 40;

// =======================================================SETUP===============================================================
void setup()
{
  //Serial.begin(115200);
  setupfcn();
  startAnimation();
}




// =======================================================LOOP===============================================================
void loop()
{
   unsigned long rightNow = millis();

   // get new temp/hum readings
   updateDHT(rightNow);
   
   updateENABLESERVO(rightNow);
   // Set lights    
   manageLight(rightNow);

   // Set airflow control
   manageAirFlow(rightNow);
   
   //check buttons and update display
   updateDISPLAY(rightNow);

   //check to make screen go dark
   checkForIdle(rightNow);
   //Serial.println(millis());
}



void manageAirFlow(unsigned long currentTime){ 
  if (currentTime > lastExecAirFlow+REFRESH_AIRFLOW)
  {

    switch(coolingMode){
      case 0: // COOLING
        if (coolingState != 0){
          controlServo(openedState, currentTime);
          digitalWrite(ENABLE_FAN1_PIN, HIGH);
          fan1State = 1;
          digitalWrite(ENABLE_FAN2_PIN, HIGH);
          fan2State = 1;          
          coolingState = 0;
        }
        break;
        
      case 1: // AUTO
        if (coolingState != 1){
          coolingState = 1;
        }
        break;
        
      case 2: // HEAT
        if (coolingState != 2){
          controlServo(closedState, currentTime);
          digitalWrite(ENABLE_FAN1_PIN, HIGH);
          fan1State = 1;
          digitalWrite(ENABLE_FAN2_PIN, LOW);
          fan2State = 0;          
          coolingState = 2;
        }
        break;

      case 3: // OFF
        if (coolingState != 3){
          controlServo(openedState, currentTime);
          digitalWrite(ENABLE_FAN1_PIN, LOW);
          fan1State = 0;
          digitalWrite(ENABLE_FAN2_PIN, LOW);
          fan2State = 0;          
          coolingState = 3;
        }
        break;
    }
    lastExecAirFlow = millis(); 
  }




  // Auto control section
  if (coolingState == 1){
    if (currentTime > lastAutoAirFlow+AUTO_AIRFLOW_REFRESH){
      float tempDifference = temperature-controlledTemp;

      if (tempDifference < -15){ //room too cold, but printer off
        if (fan1State != 0){
          digitalWrite(ENABLE_FAN1_PIN, LOW);
          fan1State = 0;
        }
        if (fan2State != 0){
          digitalWrite(ENABLE_FAN2_PIN, LOW);
          fan2State = 0;
        }
        if (servoState != openedState){
          controlServo(openedState, currentTime);
        }  
      }

      else if (tempDifference < -5){ //room too cold, but printer on
        if (fan1State != 1){
          digitalWrite(ENABLE_FAN1_PIN, HIGH);
          fan1State = 1;
        }
        if (fan2State != 0){
          digitalWrite(ENABLE_FAN2_PIN, LOW);
          fan2State = 0;
        }
        if (servoState != closedState){
          controlServo(closedState, currentTime);
        }  
      }

      else if (abs(tempDifference) < 5){ //room about right
        if (fan1State != 1){
          digitalWrite(ENABLE_FAN1_PIN, HIGH);
          fan1State = 1;
        }
        if (fan2State != 0){
          digitalWrite(ENABLE_FAN2_PIN, LOW);
          fan2State = 0;
        }
        if (servoState != openedState){
          controlServo(openedState, currentTime);
        }  
      }
      else if (tempDifference > 5){ //room too hot
        if (fan1State != 1){
          digitalWrite(ENABLE_FAN1_PIN, HIGH);
          fan1State = 1;
        }
        if (fan2State != 1){
          digitalWrite(ENABLE_FAN2_PIN, HIGH);
          fan2State = 1;
        }
        if (servoState != openedState){
          controlServo(openedState, currentTime);
        }  
      }

      else { //room too hot
        // do nothing
      }
      lastAutoAirFlow = millis();
    }
  }
}



void controlServo(byte servoCommand, unsigned long currentTime){
  digitalWrite(ENABLE_SERVO_PIN, HIGH);
  servoEnState = 1;
  myservo.write(servoCommand);
  servoState = servoCommand;
  activatedServo = currentTime;
}

void updateENABLESERVO(unsigned long currentTime){
     if ((currentTime > activatedServo+SERVO_ACTIVE_TIME)&&(servoEnState)){
      digitalWrite(ENABLE_SERVO_PIN, LOW);
      servoEnState = 0;
   }
}



//byte lightMode = 0; // 0:DEFAULT | 1:ALWAYS ON | 2:ALWAYS OFF | 3:ANIMATION 
void manageLight(unsigned long currentTime){ 
  if (currentTime > lastExecLIGHT+REFRESH_LIGHTS)
  {
    //Serial.print("LIGHTS");
    bool doorSwitch = digitalRead(DOOR_SWITCH_PIN);
    bool boolLightState = 0;
    if (lightState>0){boolLightState = 1;}
    switch(lightMode){
      case 0: // default
        if (doorSwitch != boolLightState){
          if (doorSwitch){
            turnLightsOn();
            lightState = 1;
          }
          else {
            turnLightsOff();
            lightState = 0;
          }
        }
        break;
  
      case 1: // always on
        if (lightState != 1){
          turnLightsOn();
          lightState = 1;
        }
        break;
  
      case 2: // always off
        if (lightState != 0){
          turnLightsOff();
          lightState = 0;
        }
        break;
  
      case 3: // ANIMATION
        if (lightState != 1){
          turnLightsOn();
          lightState = 1;
        }
        break;
  
    }  
    lastExecLIGHT = millis();  
  }
}

void turnLightsOn(){
  colorWipe(strip.Color(255, 255, 255), 5);
}

void turnLightsOff(){
  colorWipe(strip.Color(0, 0, 0), 5);
}




void checkForIdle(unsigned long currentTime){
  if (currentTime > lastAction+TIME_TO_IDLE)
   {
      lcd.noBacklight();
      backLightState = 0;
   }
}


void updateDHT(unsigned long currentTime){
  if (currentTime > lastExecDHT+REFRESH_TEMP)
   {
      //Serial.print("DHT");
      humidity = dht.readHumidity();
      temperature = dht.readTemperature();
      lastExecDHT = currentTime;
   }
}






//void updateLCD(unsigned long currentTime){
//  byte btn = read_LCD_buttons();
//   if (btn == BTNNONE){
//     if (currentTime > lastExecDISP+REFRESH_DISP)
//     {
//        simpleInfo(btn);
//     }
//   }
//   else {
//    if (currentTime > lastExecDISP+REFRESH_DISP)
//     {
//        interpretBTN(btn);
//        simpleInfo(btn);
//     }
//   }
//}
//
//void simpleInfo(byte btn){
//  lcd.home ();
//  lcd.print("MENU1: ");
//  lcd.print(String(menu_state));
//  lcd.print(" ");
//  lcd.print(String(btn));
//  lcd.print("        ");
//  lcd.setCursor ( 0, 1 ); 
//  lcd.print("MENU2: ");
//  lcd.print(String(menu2_state));
//  lcd.print(" ");
//  lcd.print(String(analogRead(BUTTON_PIN)));
//  lcd.print("       ");
//}

//byte menu_state = 0; // 0:MAIN | 1:VIEWPARAMS | 2:CHANGEPARAMS
//byte menu2_state = 0 // 0:COOLING | 1:TEMP | 2:LIGHT | 3:3DPrinterState
//byte coolingMode = 0; // 0:COOLING | 1:AUTO | 2:HEAT | 3:NONE
//byte lightMode = 0; // 0:DEFAULT | 1:ALWAYS ON | 2:ALWAYS OFF | 3:ANIMATION

void updateDISPLAY(unsigned long currentTime){
   byte btn = read_LCD_buttons();
   if (btn == BTNNONE){
     if (currentTime > lastExecDISP+REFRESH_DISP)
     {
        generateDisplay();
     }
   }
   else {
    if (currentTime > lastExecDISP+REFRESH_DISP)
     {
        interpretBTN(btn);
        generateDisplay();
     }
  }
}



void interpretBTN(byte btn){
  if (menu_state == 0){
    switch (btn){
      case BTNRIGHT :
        menu_state = 1;
        menu2_state = 0;
        break;
        
      case BTNDOWN :
        menu_state = 1;
        menu2_state = 0;
        break;

      case BTNUP :
        menu_state = 1;
        menu2_state = 3;
        break;
    }
  }

  else if (menu_state == 1){
    switch (btn){
      case BTNRIGHT :
        menu_state = 2;
        break;

      case BTNLEFT :
        menu_state = 0;
        break;
        
      case BTNDOWN :
        menu2_state = menu2_state+1;
        if (menu2_state>3){
          menu2_state = 0;
        }
        break;

      case BTNUP :
        if (menu2_state==0){
          menu2_state = 3;
        }
        else{
          menu2_state = menu2_state-1;
        }
        break;
     }
  }

  else if (menu_state == 2){
    switch (btn){
      case BTNRIGHT :
        menu_state = 0;
        break;

      case BTNLEFT :
        menu_state = 1;
        break;
        
      case BTNDOWN :
        switch(menu2_state){
          case 0: 
            coolingMode = coolingMode + 1;
            if (coolingMode>3){coolingMode = 0;}
            break;
            
          case 1:
            controlledTemp = controlledTemp - 2;
            break;

          case 2:
            lightMode = lightMode + 1;
            if (lightMode>3){lightMode = 0;}
            break;

          case 3:
            printerState = not printerState;
            break;
        }
        break;

      case BTNUP :
        switch(menu2_state){
          case 0: 
            if (coolingMode==0){
              coolingMode = 3;
            }
            else{
              coolingMode = coolingMode-1;
            }
            break;
            
          case 1:
            controlledTemp = controlledTemp + 2;
            break;

          case 2:
            if (lightMode==0){
              lightMode = 3;
            }
            else{
              lightMode = lightMode-1;
            }
            break;

          case 3:
            printerState = not printerState;
            break;
        }
        break;
     
     }
    
  }
  
}






void generateDisplay(){
  if (menu_state == 0){
    lcd.home ();
    lcd.print(String(temperature));
    lcd.print((char)223);
    lcd.print("C   ");
    lcd.print(String(humidity));
    lcd.print("%");
    lcd.setCursor ( 0, 1 );        // go to the next line
    lcd.print("MODE: ");
    switch (coolingMode){
      case 0: 
        lcd.print("COOLING   ");
        break;
      case 1: 
        lcd.print("AUTO ");
        lcd.print(String(temperature-controlledTemp));
        break;
      case 2: 
        lcd.print("FILTERING ");
        break;
      case 3: 
        lcd.print("OFF       ");
        break;
    }
  }


  
  else {
    if (menu_state == 1){
    lcd.home ();
    lcd.print("PARAMETERS :    ");
    }
    if (menu_state == 2){
    lcd.home ();
    lcd.print("CHANGE PARAMETER");
    }
    lcd.setCursor ( 0, 1 );
    switch(menu2_state){
      case 0:
        lcd.print("MODE: ");
        switch (coolingMode){
          case 0: 
            lcd.print("COOLING   ");
            break;
          case 1: 
            lcd.print("AUTO      ");
            break;
          case 2: 
            lcd.print("FILTERING ");
            break;
          case 3: 
            lcd.print("OFF       ");
            break;
          }
          break;
          
            
       case 1:
          lcd.print("CTRL TEMP: ");
          lcd.print(String(controlledTemp));
          lcd.print("  ");
          break;

        

       case 2:
          lcd.print("LIGHT: ");
            switch (lightMode){
              case 0: 
                lcd.print("DEFAULT ");
                lcd.print(String(digitalRead(DOOR_SWITCH_PIN)));
                break;
              case 1: 
                lcd.print("ALWAYS ON");
                break;
              case 2: 
                lcd.print("ALW. OFF ");
                break;
              case 3: 
                lcd.print("ANIMATION");
                break;
            }
            break;

        case 3:
          lcd.print("3D PRINTER: ");
          switch(printerState){
            case 0 : 
              lcd.print("OFF ");
              digitalWrite(RELAY_PIN1, LOW);
              digitalWrite(RELAY_PIN2, LOW);
              digitalWrite(RELAY_PIN3, LOW);
              digitalWrite(RELAY_PIN4, LOW);
              
              break;
            case 1 : 
              lcd.print("ON  ");
              digitalWrite(RELAY_PIN1, HIGH);
              digitalWrite(RELAY_PIN2, HIGH);
              digitalWrite(RELAY_PIN3, HIGH);
              digitalWrite(RELAY_PIN4, HIGH);
              break;
          }  
      }
   } 
}




int read_LCD_buttons()
{
 int adc_key_in = analogRead(BUTTON_PIN);      // read the value from the sensor 
 // my buttons when read are centered at these valies: 0, 144, 329, 504, 741
 // we add approx 50 to those values and check to see if we are close
 if (adc_key_in > 900) return BTNNONE; // We make this the 1st option for speed reasons since it will be the most likely result
 // For V1.1 us this threshold

 if (backLightState == 0){
    lcd.backlight();
    backLightState = 1;
    startAnimation();
    lastAction = millis();
    return BTNNONE;
 }
 lastAction = millis();
 if (adc_key_in < 50){   
   delay(debounceBTN);
   if (analogRead(BUTTON_PIN) < 50){
    return BTNUP;}
   else{return BTNNONE;}}  
 if (adc_key_in < 250){   
   delay(debounceBTN);
   if (analogRead(BUTTON_PIN) < 250){
     return BTNLEFT;}
   else{return BTNNONE;}}
 if (adc_key_in < 475){
   delay(debounceBTN);
   if (analogRead(BUTTON_PIN) < 475){
     return BTNDOWN;}
   else{return BTNNONE;}} 
 if (adc_key_in < 675){  
   delay(debounceBTN);
   if (analogRead(BUTTON_PIN) < 675){
     return BTNRIGHT; }
   else{return BTNNONE;}}

 return BTNNONE;  // when all others fail, return this...
}







void setupfcn(){
  pinMode(RELAY_PIN1, OUTPUT);
  digitalWrite(RELAY_PIN1, HIGH);
  pinMode(RELAY_PIN2, OUTPUT);
  digitalWrite(RELAY_PIN2, HIGH);
  pinMode(RELAY_PIN3, OUTPUT);
  digitalWrite(RELAY_PIN3, HIGH);
  pinMode(RELAY_PIN4, OUTPUT);
  digitalWrite(RELAY_PIN4, HIGH);
  
  strip.begin();
  strip.show();
  
  dht.begin();

  myservo.attach(SERVO_PIN);
  
  pinMode(ENABLE_SERVO_PIN, OUTPUT);
  digitalWrite(ENABLE_SERVO_PIN, LOW); 
  pinMode(ENABLE_FAN1_PIN, OUTPUT);
  digitalWrite(ENABLE_FAN1_PIN, HIGH); 
  pinMode(ENABLE_FAN2_PIN, OUTPUT);
  digitalWrite(ENABLE_FAN2_PIN, HIGH); 

  pinMode(DOOR_SWITCH_PIN, INPUT_PULLUP);
  pinMode(BUTTON_PIN, INPUT);
  

  lcd.begin(16,2);               // initialize the lcd 
  lcd.home ();                   // go home

  controlServo(openedState, millis());
}


void startAnimation()
{
  lcd.setCursor ( 0, 0 );lcd.print(">              <");lcd.setCursor ( 0, 1 ); lcd.print(">              <");
  lcd.setCursor ( 0, 0 );lcd.print(">>            <<");lcd.setCursor ( 0, 1 ); lcd.print(">>            <<");
  lcd.setCursor ( 0, 0 );lcd.print(">>>          <<<");lcd.setCursor ( 0, 1 ); lcd.print(">>>          <<<");
  lcd.setCursor ( 0, 0 );lcd.print(">>>>        <<<<");lcd.setCursor ( 0, 1 ); lcd.print(">>>>        <<<<");
  lcd.setCursor ( 0, 0 );lcd.print(">>>>>      <<<<<");lcd.setCursor ( 0, 1 ); lcd.print(">>>>>      <<<<<");
  lcd.setCursor ( 0, 0 );lcd.print(">>>>>>    <<<<<<");lcd.setCursor ( 0, 1 ); lcd.print(">>>>>>    <<<<<<");
  lcd.setCursor ( 0, 0 );lcd.print(">>>>>>>  <<<<<<<");lcd.setCursor ( 0, 1 ); lcd.print(">>>>>>>  <<<<<<<");
  lcd.setCursor ( 0, 0 );lcd.print(">>>>>>>><<<<<<<<");lcd.setCursor ( 0, 1 ); lcd.print(">>>>>>>><<<<<<<<");
  lcd.setCursor ( 0, 0 );lcd.print(">>>>>>>><<<<<<<<");lcd.setCursor ( 0, 1 ); lcd.print(">>>>>>>><<<<<<<<");
  lcd.setCursor ( 0, 0 );lcd.print(">>>>>>>><<<<<<<<");lcd.setCursor ( 0, 1 ); lcd.print(">>>>>>>><<<<<<<<");
  lcd.setCursor ( 0, 0 );lcd.print(">>>>>>>><<<<<<<<");lcd.setCursor ( 0, 1 ); lcd.print(">>>>>>>><<<<<<<<");
  lcd.setCursor ( 0, 0 );lcd.print(" >>>>>>><<<<<<< ");lcd.setCursor ( 0, 1 ); lcd.print(" >>>>>>><<<<<<< ");
  lcd.setCursor ( 0, 0 );lcd.print("  >>>>>><<<<<<  ");lcd.setCursor ( 0, 1 ); lcd.print("  >>>>>><<<<<<  ");
  lcd.setCursor ( 0, 0 );lcd.print("   >>>>><<<<<   ");lcd.setCursor ( 0, 1 ); lcd.print("   >>>>><<<<<   ");
  lcd.setCursor ( 0, 0 );lcd.print("    >>>><<<<    ");lcd.setCursor ( 0, 1 ); lcd.print("    >>>><<<<    ");
  lcd.setCursor ( 0, 0 );lcd.print("     >>><<<     ");lcd.setCursor ( 0, 1 ); lcd.print("     >>><<<     ");
  lcd.setCursor ( 0, 0 );lcd.print("      >><<      ");lcd.setCursor ( 0, 1 ); lcd.print("      >><<      ");
  lcd.setCursor ( 0, 0 );lcd.print("       ><       ");lcd.setCursor ( 0, 1 ); lcd.print("       ><       ");
  lcd.setCursor ( 0, 0 );lcd.print("       <>       ");lcd.setCursor ( 0, 1 ); lcd.print("       <>       ");
  lcd.setCursor ( 0, 0 );lcd.print("      <<>>      ");lcd.setCursor ( 0, 1 ); lcd.print("      <<>>      ");
  lcd.setCursor ( 0, 0 );lcd.print("     <<<>>>     ");lcd.setCursor ( 0, 1 ); lcd.print("     <<<>>>     ");
  lcd.setCursor ( 0, 0 );lcd.print("    <<<<>>>>    ");lcd.setCursor ( 0, 1 ); lcd.print("    <<<<>>>>    ");
  lcd.setCursor ( 0, 0 );lcd.print("   <<<<<>>>>>   ");lcd.setCursor ( 0, 1 ); lcd.print("   <<<<<>>>>>   ");
  lcd.setCursor ( 0, 0 );lcd.print("  <<<<<<>>>>>>  ");lcd.setCursor ( 0, 1 ); lcd.print("  <<<<<<>>>>>>  ");
  lcd.setCursor ( 0, 0 );lcd.print(" <<<<<<<>>>>>>> ");lcd.setCursor ( 0, 1 ); lcd.print(" <<<<<<<>>>>>>> ");
  lcd.setCursor ( 0, 0 );lcd.print("<<<<<<<<>>>>>>>>");lcd.setCursor ( 0, 1 ); lcd.print("<<<<<<<<>>>>>>>>");
  lcd.setCursor ( 0, 0 );lcd.print("<<<<<<<  >>>>>>>");lcd.setCursor ( 0, 1 ); lcd.print("<<<<<<<  >>>>>>>");
  lcd.setCursor ( 0, 0 );lcd.print("<<<<<<    >>>>>>");lcd.setCursor ( 0, 1 ); lcd.print("<<<<<<    >>>>>>");
  lcd.setCursor ( 0, 0 );lcd.print("<<<<<      >>>>>");lcd.setCursor ( 0, 1 ); lcd.print("<<<<<      >>>>>");
  lcd.setCursor ( 0, 0 );lcd.print("<<<<        >>>>");lcd.setCursor ( 0, 1 ); lcd.print("<<<<        >>>>");
  lcd.setCursor ( 0, 0 );lcd.print("<<<          >>>");lcd.setCursor ( 0, 1 ); lcd.print("<<<          >>>");
  lcd.setCursor ( 0, 0 );lcd.print("<<            >>");lcd.setCursor ( 0, 1 ); lcd.print("<<            >>");
  lcd.setCursor ( 0, 0 );lcd.print("<              >");lcd.setCursor ( 0, 1 ); lcd.print("<              >");
  lcd.setCursor ( 0, 0 );lcd.print("   3D Printer   ");lcd.setCursor ( 0, 1 ); lcd.print(" Enclosure v1.0 ");
  delay(1000);

}


// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
    delay(wait);
  }
}

