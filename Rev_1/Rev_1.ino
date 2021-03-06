

///// Rev_1 /////
///// Parts of the code are shared with my "i2c_Between_Arduinos" project /////
///// Added a Servo for balancing the platform using weights /////
///// Added necessary functions for using the Servo /////
///// Added a Range Mapping function that can process type 'double' /////
///// Range Mapping fucntion is used for converting Accel's data range into the Servo's angle range /////
///// Servo reacts properly without any jittering /////
///// OLED Display also shows a 5th page for the Self Balancing data /////
///// Self Balancing data: Accel's Y-Axis data and Servo's Angle /////

///// GitHub: @Hizbi-K /////



#include <Arduino.h>
#include <Wire.h>
#include <U8glib.h>
#include <Servo2.h>
#include <stdlib.h>


U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_DEV_0|U8G_I2C_OPT_NO_ACK|U8G_I2C_OPT_FAST);  // Fast I2C / TWI 


Servo balanceServo;


byte sendToSlave_1 = 0;
byte receiveFromSlave_1 = 0;

byte sendToSlave_2 = 0;
byte receiveFromSlave_2 = 0;


int BUT_1 = A0;
bool butState_1 = LOW;
bool keyPressed_1 = false;
bool stateLED_1 = LOW;

int BUT_2 = A1;
bool butState_2 = HIGH;
bool keyPressed_2 = false;
bool stateLED_2 = LOW;


int encoder_Clk = 2;
volatile bool current_Clk_State = HIGH;

int encoder_Data = 3;
volatile bool current_Data_State = HIGH;

#define INTERRUPT 0       // Defining Digital Pin 2 for Hardware Interrupt 

volatile bool encoderStatus = HIGH;
volatile bool encoderBusy = false;

int encoder_BUT = 4;
bool butState_3 = LOW;
bool keyPressed_3 = false;


volatile int upCounter = 0;
volatile int downCounter = 100;


struct rawAccelStruct
{
  int16_t x = 0;
  int16_t y = 0;
  int16_t z = 0;
} rawAccel;

struct cookedAccelStruct
{
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
} cookedAccel;


struct rawGyroStruct
{
  int16_t x = 0;
  int16_t y = 0;
  int16_t z = 0;
} rawGyro;

struct cookedGyroStruct
{
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
} cookedGyro;

struct offsetGyroStruct
{
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
} offsetGyro;


int servoPin = 5;                  // Servo digital PWM pin

double servoAngleDefault = 80.0;
double angleMax = 143.0;
double angleMin = 23.0;

double previousServoAngle = servoAngleDefault;
double servoAngle = servoAngleDefault;


int switchDisplay = 1;                // Display the first page at start


String displayLED_1 = "OFF";
String displayLED_2 = "OFF";


unsigned long currentMillis = 0;
unsigned long previousMillis_1 = 0;
unsigned long previousMillis_2 = 0;
unsigned long previousMillis_3 = 0;
unsigned long previousMillis_4 = 0;
unsigned long previousMillis_5 = 0;
unsigned long previousMillis_6 = 0;

unsigned long debounceInterval = 50;
unsigned long encoderDebounce = 50;
unsigned long timeOffMPU = 100;
unsigned long servoInterval = 100;


void pressBUT_1 ();
void pressBUT_2 ();
void pressEncoderBUT ();

void encoderISR ();

void init_Accel_Gyro ();
void readingAccel();
void readingGyro();

void leftLED_Comm ();
void rightLED_Comm ();

void drawPage_1 ();
void drawPage_2 ();
void drawPage_3 ();
void drawPage_4 ();
void drawPage_5 ();


void servoStop ();
void servoReactBalance ();


double rangeMapper (double , double , double , double , double );


/////////////////////////////////////////////////////////////////////////



void setup() 
{

    if ( u8g.getMode() == U8G_MODE_R3G3B2 )           // Assigning default color values.
    {
      u8g.setColorIndex(255);     // white
    }
    else if ( u8g.getMode() == U8G_MODE_GRAY2BIT ) 
    {
      u8g.setColorIndex(3);         // max intensity
    }
    else if ( u8g.getMode() == U8G_MODE_BW )          // I think we are using this, since our display
    {                                                 // is Black & Blue, so Index(1) means to turn on
      u8g.setColorIndex(1);         // pixel on       // the blue pixels (or white pixels in B&W diplays).
    }
    else if ( u8g.getMode() == U8G_MODE_HICOLOR ) 
    {
      u8g.setHiColorByRGB(255,255,255);
    }

  
    pinMode(BUT_1, INPUT_PULLUP);
    pinMode(BUT_2, INPUT_PULLUP);

    pinMode(encoder_Clk, INPUT_PULLUP);
    pinMode(encoder_Data, INPUT_PULLUP);
    pinMode(encoder_BUT, INPUT_PULLUP);

    pinMode(servoPin, OUTPUT);


    attachInterrupt(INTERRUPT, encoderISR, CHANGE);


    init_Accel_Gyro();


    balanceServo.attach(servoPin);
    balanceServo.write(servoAngleDefault);


    Wire.begin(); 
  
}



void loop() 
{

  currentMillis = millis();


  pressBUT_1();

  pressBUT_2();

  pressEncoderBUT();


  if (switchDisplay == 3)
  {
    if ((currentMillis - previousMillis_6) > timeOffMPU)
    {
      readingAccel();

      previousMillis_6 = currentMillis;
    }
    
  }


  if (switchDisplay == 4)
  {
    if ((currentMillis - previousMillis_6) > timeOffMPU)
    {
      readingGyro();

      previousMillis_6 = currentMillis;
    }
    
  }


  if (switchDisplay == 5)
  {
    if ((currentMillis - previousMillis_6) > timeOffMPU)
    {
      readingAccel();

      servoReactBalance();
     
      previousMillis_6 = currentMillis;
    }

  }
  

  if (switchDisplay == 2)                               // Only using the encoder if the display
  {                                                     // has been switched to the Counter page
    if (encoderBusy == true)
    {
      
      if ((encoderStatus == HIGH) && ((currentMillis - previousMillis_4) > encoderDebounce))          // Up counter for clockwise rotation
      {
        upCounter = upCounter + 1;
        rightLED_Comm();         // Flashing the appropriate LED to show the Encoder's direction

        if (displayLED_1 == "ON")
        {
          leftLED_Comm();        // Turning the other LED OFF
        }

        if (upCounter == 101)     // Resetting the counter
        {
          upCounter = 0;
        }

      }

      else if ((encoderStatus == LOW) && ((currentMillis - previousMillis_5) > encoderDebounce))      // Down counter for anit-clockwise rotation
      {
        downCounter = downCounter - 1;
        leftLED_Comm();          // Flashing the appropriate LED to show the Encoder's direction

        if (displayLED_2 == "ON")
        {
          rightLED_Comm();       // Turning the other LED OFF
        }


        if (downCounter < 0)      // Resetting the counter
        {
          downCounter = 100;
        }

      }

      encoderBusy = false;             // Allowing Arduino to read Encoder again

    }

  }



  switch(switchDisplay)       // Switching between the LED Status and Counter pages
  {

    case 1:
    {
      u8g.firstPage();  

      do 
      {  
        drawPage_1();         // LED Status page      
      } 
      while (u8g.nextPage());

      break;
    }

    case 2:
    {
      u8g.firstPage();  

      do 
      {  
        drawPage_2();         // Counter page      
      } 
      while (u8g.nextPage());

      break;   
    }

    case 3:
    {
      u8g.firstPage();  

      do 
      {  
        drawPage_3();         // 6050 MPU Accel page      
      } 
      while (u8g.nextPage());

      break;   
    }

    case 4:
    {
      u8g.firstPage();  

      do 
      {  
        drawPage_4();         // 6050 MPU Gyro page      
      } 
      while (u8g.nextPage());

      break;   
    }

    case 5:
    {
      u8g.firstPage();  

      do 
      {  
        drawPage_5();         // Accel's Y-Axis and Servo Angle page      
      } 
      while (u8g.nextPage());

      break;   
    }

  }


}



/////////////////////////////////////////////////////////////////////////



void pressBUT_1 ()      // Using a normal tactile push button
{
    
    
    if (butState_1 != digitalRead(BUT_1))
    {  
      
      if (butState_1 == HIGH)
      {
        butState_1 = digitalRead(BUT_1);
        keyPressed_1 = true;

        previousMillis_1 = currentMillis;
      }
      else if (butState_1 == LOW)
      {
        butState_1 = HIGH;
        keyPressed_1 = false;
      }

    }

  
    if ((butState_1 == LOW) && (keyPressed_1 == true) && ((currentMillis - previousMillis_1) > debounceInterval))
    {
      sendToSlave_1 = 0;
      
      Wire.beginTransmission(9); 
      Wire.write(sendToSlave_1);  
      Wire.endTransmission();

      Wire.requestFrom(9,1);
      receiveFromSlave_1 =  Wire.read();


      if (receiveFromSlave_1 == sendToSlave_1)
      {
        
        if (stateLED_1 == LOW)
        {
          displayLED_1 = "ON";
          stateLED_1 = HIGH;
        }
        else if (stateLED_1 == HIGH)
        {
          displayLED_1 = "OFF";
          stateLED_1 = LOW;
        }

      }
      else if (receiveFromSlave_1 != sendToSlave_1)
      {
        displayLED_1 = "Com. Error";
      }
      
      
      sendToSlave_1 = 0;
      receiveFromSlave_1 = 0;
      
     
      
      if (digitalRead(BUT_1) == LOW)      // No toggling until the finger has been lifted off
      {
        keyPressed_1 =  false;
        butState_1 = LOW;
      }
      else if (digitalRead(BUT_1) == HIGH)
      {
        keyPressed_1 =  true;
        butState_1 = HIGH;
      }
      

    } 
  

}



/////////////////////////////////////////////////////////////////////////



void pressBUT_2 ()        // Using a normal tactile push button
{
    
    
    if (butState_2 != digitalRead(BUT_2))
    {  
      
      if (butState_2 == HIGH)
      {
        butState_2 = digitalRead(BUT_2);
        keyPressed_2 = true;

        previousMillis_2 = currentMillis;
      }
      else if (butState_2 == LOW)
      {
        butState_2 = HIGH;
        keyPressed_2 = false;
      }

    }

  
    if ((butState_2 == LOW) && (keyPressed_2 == true) && ((currentMillis - previousMillis_2) > debounceInterval))
    {
      sendToSlave_2 = 1;
      
      Wire.beginTransmission(9); 
      Wire.write(sendToSlave_2);  
      Wire.endTransmission();
      

      Wire.requestFrom(9,1);
      receiveFromSlave_2 =  Wire.read();


      if (receiveFromSlave_2 == sendToSlave_2)
      {
        
        if (stateLED_2 == LOW)
        {
          displayLED_2 = "ON";
          stateLED_2 = HIGH;
        }
        else if (stateLED_2 == HIGH)
        {
          displayLED_2 = "OFF";
          stateLED_2 = LOW;
        }

      }
      else if (receiveFromSlave_2 != sendToSlave_2)
      {
        displayLED_2 = "Com. Error";
      }


      sendToSlave_2 = 0;
      receiveFromSlave_2 = 0;

      
      if (digitalRead(BUT_2) == LOW)      // No toggling until the finger has been lifted off
      {
        keyPressed_2 =  false;
        butState_2 = LOW;
      }
      else if (digitalRead(BUT_2) == HIGH)
      {
        keyPressed_2 =  true;
        butState_2 = HIGH;
      }
      

    } 
  

}



/////////////////////////////////////////////////////////////////////////



void pressEncoderBUT ()       // Using the button from Rotatory Encoder
{
    
    
    if (butState_3 != digitalRead(encoder_BUT))
    {  
      
      if (butState_3 == HIGH)
      {
        butState_3 = digitalRead(encoder_BUT);
        keyPressed_3 = true;

        previousMillis_3 = currentMillis;
      }
      else if (butState_3 == LOW)
      {
        butState_3 = HIGH;
        keyPressed_3 = false;
      }

    }

  
    if ((butState_3 == LOW) && (keyPressed_3 == true) && ((currentMillis - previousMillis_3) > debounceInterval))
    {
        
      if (switchDisplay == 1)
      {
        switchDisplay = 2;

        upCounter = 0;     
        downCounter = 100;
      }
      else if (switchDisplay == 2)
      {
        switchDisplay = 3;

        previousMillis_6 = currentMillis;
      }
      else if (switchDisplay == 3)
      {
        switchDisplay = 4;

        previousMillis_6 = currentMillis;
      }
      else if (switchDisplay == 4)
      {
        switchDisplay = 5;

        previousMillis_6 = currentMillis;

        previousServoAngle = servoAngleDefault;
      }
      else if (switchDisplay == 5)
      {
        switchDisplay = 1;
      }
            
      
      if (digitalRead(encoder_BUT) == LOW)      // No toggling until the finger has been lifted off
      {
        keyPressed_3 =  false;
        butState_3 = LOW;
      }
      else if (digitalRead(encoder_BUT) == HIGH)
      {
        keyPressed_3 =  true;
        butState_3 = HIGH;
      }
      

    } 
  

}



/////////////////////////////////////////////////////////////////////////



void encoderISR()     // ISR for taking in the Encoder readings quickly
{
  
  current_Clk_State = digitalRead(encoder_Clk);
  current_Data_State = digitalRead(encoder_Data);


  if (current_Clk_State == LOW)
  {
    
    encoderStatus = current_Data_State;
    previousMillis_4 = currentMillis;

  }

  else if (current_Clk_State == HIGH)
  {

    encoderStatus = !current_Data_State;
    previousMillis_5 = currentMillis;

  }

  encoderBusy = true;
  
}



/////////////////////////////////////////////////////////////////////////



void init_Accel_Gyro ()
{
  
  Wire.beginTransmission(0x68);     // 6050 MPU's i2c Address
  Wire.write(0x6B);                 // Accessing the MPU's power management
  Wire.write(0x0);                  // Activating the 6050 MPU from sleep 
  Wire.endTransmission();


  Wire.beginTransmission(0x68);     // 6050 MPU's i2c Address
  Wire.write(0x1C);                 // Accessing Accelerometer settings
  Wire.write(0x0);                  // Setting sensivity to +- 2 g
  Wire.endTransmission();


  Wire.beginTransmission(0x68);     // 6050 MPU's i2c Address
  Wire.write(0x1B);                 // Accessing Gyroscope settings
  Wire.write(0x0);                  // Setting sensivity to +- 250 degree/sec
  Wire.endTransmission();

}



/////////////////////////////////////////////////////////////////////////



void readingAccel ()
{
  
  Wire.beginTransmission(0x68);     // 6050 MPU's i2c Address
  Wire.write(0x3B);                 // Asking for Accelerometer readings
  Wire.endTransmission();

  Wire.requestFrom(0x68, 6);        // Actually receiving the Accelerometer readings here, total 6-Bytes

  while (Wire.available() < 6)      // Making sure the receiving buffer is filled before reading
  {
    // Do nothing, just make sure the buffer is filled with 6 bytes,
    // 2 for each axis.
  }


  rawAccel.x = Wire.read() << 8;               // Reading 1 byte and making some space
  rawAccel.x = rawAccel.x | Wire.read();       // Reading another byte and taking a union (ORing)

  rawAccel.y = Wire.read() << 8;               // Reading 1 byte and making some space
  rawAccel.y = rawAccel.y | Wire.read();       // Reading another byte and taking a union (ORing)

  rawAccel.z = Wire.read() << 8;               // Reading 1 byte and making some space
  rawAccel.z = rawAccel.z | Wire.read();       // Reading another byte and taking a union (ORing)


  // Note that the 6050 MPU provides a 2-Byte or 16-Bit data for each axis readings, but i2c only
  // sends 1-Byte buffer at a time. So we have to take those two bytes, and "shift-add" them together.

  // Best explained using this example:

  // First Byte that is received: 10101010
  // Shifting the First Byte to make space for the second one: 10101010 00000000

  // Second Byte is received: 01010101
  // ORing them: 10101010 00000000 + 01010101

  // This results in: 10101010 01010101
  // Which is actually the 16-Bit reading taken by the MPU, but due to i2c limitations, 
  // it is split up in 2-Bytes.


  // Converting to actual meaningfull values. Dividing by "16384.0" due 
  // to the "+- 2 g" sensitivity settings. Check DataSheet for other sensitivities.


  cookedAccel.x = rawAccel.x / 16384.0;
  cookedAccel.x = cookedAccel.x - 0.06;         // Offset calculated by averaging the readings manually

  cookedAccel.y = rawAccel.y / 16384.0;
  cookedAccel.y = cookedAccel.y - 0.04;         // Offset calculated by averaging the readings manually

  cookedAccel.z = rawAccel.z / 16384.0;         
  cookedAccel.z = cookedAccel.z - 0.09;         // Offset calculated by averaging the readings manually


  if ((cookedAccel.y > 0.02) && (cookedAccel.y <= 0.95))
  {
    rightLED_Comm();

    if (displayLED_1 == "ON")
    {
      leftLED_Comm();       // Turning the other LED OFF
    }
  }

  if ((cookedAccel.y > -1.00) && (cookedAccel.y <= -0.02))
  {
    leftLED_Comm();

    if (displayLED_2 == "ON")
    {
      rightLED_Comm();       // Turning the other LED OFF
    }
  }

  if ((cookedAccel.y > -0.02) && (cookedAccel.y <= 0.02))
  {
    
    if (displayLED_1 == "ON")
    {
      leftLED_Comm();       // Turning the left LED OFF
    }

    if (displayLED_2 == "ON")
    {
      rightLED_Comm();       // Turning the right LED OFF
    }

  }

}



/////////////////////////////////////////////////////////////////////////



void readingGyro ()
{
  
  Wire.beginTransmission(0x68);     // 6050 MPU's i2c Address
  Wire.write(0x43);                 // Asking for Gyroscope readings
  Wire.endTransmission();

  Wire.requestFrom(0x68, 6);        // Actually receiving the Gyroscope readings here, total 6-Bytes

  while (Wire.available() < 6)      // Making sure the receiving buffer is filled before reading
  {
    // Do nothing, just make sure the buffer is filled with 6 bytes,
    // 2 for each axis.
  }
                  

  rawGyro.x = Wire.read() << 8;                // Reading 1 byte and making some space
  rawGyro.x = rawGyro.x | Wire.read();         // Reading another byte and taking a union (ORing)
 

  rawGyro.y = Wire.read() << 8;                // Reading 1 byte and making some space
  rawGyro.y = rawGyro.y | Wire.read();         // Reading another byte and taking a union (ORing)

  rawGyro.z = Wire.read() << 8;                // Reading 1 byte and making some space
  rawGyro.z = rawGyro.z | Wire.read();         // Reading another byte and taking a union (ORing)


  // Note that the 6050 MPU provides a 2-Byte or 16-Bit data for each axis readings, but i2c only
  // sends 1-Byte buffer at a time. So we have to take those two bytes, and "shift-add" them together.

  // Best explained using this example:

  // First Byte that is received: 10101010
  // Shifting the First Byte to make space for the second one: 10101010 00000000

  // Second Byte is received: 01010101
  // ORing them: 10101010 00000000 + 01010101

  // This results in: 10101010 01010101
  // Which is actually the 16-Bit reading taken by the MPU, but due to i2c limitations, 
  // it is split up in 2-Bytes.


  // Converting to actual meaningfull values. Dividing by "131.0" due 
  // to the "+- 250 degree/sec" sensitivity settings. Check DataSheet for other sensitivities.

  cookedGyro.x = rawGyro.x / 131.0;
  cookedGyro.x = cookedGyro.x + 4.35;       // Offset calculated by averaging the readings manually

  cookedGyro.y = rawGyro.y / 131.0;
  cookedGyro.y = cookedGyro.y - 2.05;       // Offset calculated by averaging the readings manually

  cookedGyro.z = rawGyro.z / 131.0;
  cookedGyro.z = cookedGyro.z + 0.50;       // Offset calculated by averaging the readings manually

}



/////////////////////////////////////////////////////////////////////////



void leftLED_Comm ()
{
  
  sendToSlave_1 = 0;
      
  Wire.beginTransmission(9); 
  Wire.write(sendToSlave_1);  
  Wire.endTransmission();

  Wire.requestFrom(9,1);
  receiveFromSlave_1 =  Wire.read();


  if (receiveFromSlave_1 == sendToSlave_1)
  {
    
    if (stateLED_1 == LOW)
    {
      displayLED_1 = "ON";
      stateLED_1 = HIGH;
    }
    else if (stateLED_1 == HIGH)
    {
      displayLED_1 = "OFF";
      stateLED_1 = LOW;
    }

  }
  else if (receiveFromSlave_1 != sendToSlave_1)
  {
    displayLED_1 = "Com. Error";
  }
  
  
  sendToSlave_1 = 0;
  receiveFromSlave_1 = 0;

}


/////////////////////////////////////////////////////////////////////////



void rightLED_Comm ()
{

  sendToSlave_2 = 1;
      
  Wire.beginTransmission(9); 
  Wire.write(sendToSlave_2);  
  Wire.endTransmission();
  

  Wire.requestFrom(9,1);
  receiveFromSlave_2 =  Wire.read();


  if (receiveFromSlave_2 == sendToSlave_2)
  {
    
    if (stateLED_2 == LOW)
    {
      displayLED_2 = "ON";
      stateLED_2 = HIGH;
    }
    else if (stateLED_2 == HIGH)
    {
      displayLED_2 = "OFF";
      stateLED_2 = LOW;
    }

  }
  else if (receiveFromSlave_2 != sendToSlave_2)
  {
    displayLED_2 = "Com. Error";
  }


  sendToSlave_2 = 0;
  receiveFromSlave_2 = 0;

}



/////////////////////////////////////////////////////////////////////////



void drawPage_1()       // For displaying LED Status page
{
  
  u8g.setFont(u8g_font_helvB08r); 

  u8g.drawStr(45, 11, "i2c Coms");
  
  u8g.drawStr(0, 35, "Yeeloow LED:");
  u8g.setPrintPos(85, 35); 
  u8g.print(displayLED_1);
    
  u8g.drawStr(0, 52, "Gereene LED:");
  u8g.setPrintPos(85, 52); 
  u8g.print(displayLED_2);
  
}



/////////////////////////////////////////////////////////////////////////



void drawPage_2()       // For displaying Counter page
{
  
  u8g.setFont(u8g_font_6x13); 

  u8g.drawStr(20, 11, "Encoder Counter");
  
  u8g.drawStr(0, 35, "Up Counter:");
  u8g.setPrintPos(102, 35); 
  u8g.print(upCounter);
    
  u8g.drawStr(0, 52, "Down Counter:");
  u8g.setPrintPos(102, 52); 
  u8g.print(downCounter);
  
}



/////////////////////////////////////////////////////////////////////////



void drawPage_3()       // For displaying 6050 MPU Accel page
{
  
  u8g.setFont(u8g_font_6x13); 

  u8g.drawStr(15, 10, "Accelerometer");
  
  u8g.drawStr(0, 28, "X-Axis:");
  u8g.setPrintPos(60, 28); 
  u8g.print(cookedAccel.x);
    
  u8g.drawStr(0, 46, "Y-Axis:");
  u8g.setPrintPos(60, 46); 
  u8g.print(cookedAccel.y);

  u8g.drawStr(0, 64, "Z-Axis:");
  u8g.setPrintPos(60, 64); 
  u8g.print(cookedAccel.z);
  
}



/////////////////////////////////////////////////////////////////////////



void drawPage_4()       // For displaying 6050 MPU Gyro page
{
  
  u8g.setFont(u8g_font_6x13); 

  u8g.drawStr(25, 10, "Gyroscope");
  
  u8g.drawStr(0, 28, "X-Axis:");
  u8g.setPrintPos(60, 28); 
  u8g.print(cookedGyro.x);
    
  u8g.drawStr(0, 46, "Y-Axis:");
  u8g.setPrintPos(60, 46); 
  u8g.print(cookedGyro.y);

  u8g.drawStr(0, 64, "Z-Axis:");
  u8g.setPrintPos(60, 64); 
  u8g.print(cookedGyro.z);
  
}



/////////////////////////////////////////////////////////////////////////



void drawPage_5()       // For displaying Accel's Y-Axis Values and Servo's Angle
{
  
  u8g.setFont(u8g_font_6x13); 

  u8g.drawStr(15, 10, "Y-Axis and Angle");
  
  u8g.drawStr(0, 35, "Y-Axis:");
  u8g.setPrintPos(85, 35); 
  u8g.print(cookedAccel.y);
    
  u8g.drawStr(0, 60, "Servo Angle:");
  u8g.setPrintPos(85, 60); 
  u8g.print(servoAngle);
  
}



/////////////////////////////////////////////////////////////////////////



void servoStop ()       // For stopping the Servo
{
  servoAngle = servoAngleDefault;
  
  balanceServo.write(servoAngle);
}



/////////////////////////////////////////////////////////////////////////



void servoReactBalance ()          // Keeps the Platform's equilibrium steady 
{                                  // based on the Accel's Y-Axis data
  
  previousServoAngle = servoAngleDefault;
  
  if ((cookedAccel.y > 0.02) && (cookedAccel.y <= 0.95))      // Lean back from right to center
  {
    servoAngle = rangeMapper (cookedAccel.y, 0.02, 0.95, 80.0, 143.0);

    if (abs(servoAngle - previousServoAngle) >= 1.0)
    {
      balanceServo.write(servoAngle);

      previousServoAngle = servoAngle;
    }

  }

  if ((cookedAccel.y > -1.00) && (cookedAccel.y <= -0.02))    // Lean back from left to center
  {
    servoAngle = rangeMapper (cookedAccel.y, -0.02, -1.00, 80.0, 23.0);

    if (abs(servoAngle - previousServoAngle) >= 1.0)
    {
      balanceServo.write(servoAngle);

      previousServoAngle = servoAngle;
    }

  }

  if ((cookedAccel.y > -0.02) && (cookedAccel.y <= 0.02))     // Set values at center,
  {                                                           // since the platform is 
    previousServoAngle = servoAngleDefault;                   // now centered and stable
    servoAngle = servoAngleDefault;

    balanceServo.write(servoAngle);
  }

}



/////////////////////////////////////////////////////////////////////////



double rangeMapper (double value, double fromMin, double fromMax, double toMin, double toMax)     // Just a mapping functon
{                                                                                                 // but can procees type 
  double fromRange = fromMax - fromMin;                                                           // 'double' values
  double toRange = toMax - toMin;

  double converted = 0.0;

  converted = (value - fromMin) * (toRange / fromRange) + toMin;

  return (converted);
}



/////////////////////////////////////////////////////////////////////////







/////////////////////////////////////////////////////////////////////////
