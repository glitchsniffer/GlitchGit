#include <Time.h>               //Time Library
#include <DS1307RTC.h>          //DS RTC library
#include <OneWire.h>            //OneWire Library for the DS Sensors
#include <DallasTemperature.h>  //Dallas Temperature library
#include <LiquidCrystal_I2C.h>  //LCD I2C library
#include <LCD.h>                //LCD library
#include <Wire.h>               //I2C Wire library

//  ***********************************************
//  CONFIGURATION SETUP
//  ***********************************************

int Serial_Debug = 1;  //  Set to 1 to enable serial debugging
int Sample_Number = 5;  //  Set the number of samples to be taken from each sensor
int Sample_Delay = 500;  //  Sets the delay between the samples taken
int Temp_Type = 1;  //  Selects between 0 = Celcius or 1 = Farenheit

//  ****************************
//  END CONFIGURATION SETUP
//  ****************************

//  ***********************************************
//  INITIALIZE THE LCD
//  ***********************************************

// Define the I2C address of the LCD Screen
#define I2C_ADDR 0x3F

// Define the LCD Pins for the I2C
#define B_Light 3
#define En 2
#define Rw 1
#define Rs 0
#define D4 4
#define D5 5
#define D6 6
#define D7 7

//int n=1;

LiquidCrystal_I2C lcd(I2C_ADDR,En,Rw,Rs,D4,D5,D6,D7);

//  initialize custom display characters here
byte degree[8] = {
B01100,B10010,B10010,B01100,B00000,B00000,B00000,};

//  ****************************
//  END INITIALIZATION OF LCD
//  ****************************

//  ***********************************************
//  INITIALIZE THE DS18B20 TEMPERATURE SENSORS
//  ***********************************************

//  setup the DS18B20
const int ONE_WIRE_BUS[]={2};
#define TEMPERATURE_PRECISION 9
#define NUMBER_OF_BUS 1
#define POLL_TIME 500    //how long the avr will poll every sensors

//  Setup oneWire instances to communicate with any OneWire Devices
OneWire *oneWire[NUMBER_OF_BUS];

// Pass onewire reference to Dallas temperatures.
DallasTemperature *sensors[NUMBER_OF_BUS];

// arrays to hold device addresses
DeviceAddress tempDeviceAddress[8];

int numberOfDevices[NUMBER_OF_BUS];

int RTC_Status=1;

//  ****************************
//  END INITIALIZATION OF  THE DS18B20 TEMPERATURE SENSORS
//  ****************************

//  ***********************************************
//  INITIALIZE THE LM35 TEMPERATURE SENSORS
//  ***********************************************

// Define Analog pin(s) for the LM35 temp sensor(s) input
int LM35_S1_pin = 5;

// Define global variables
int LM35_S1C;  //  LM35 Sensor 1 Temp in Celcius
int LM35_S1F;  //  LM35 Sensor 1 Temp in Farenheit

//  ****************************
//  END INITIALIZATION OF  THE LM35 TEMPERATURE SENSORS
//  ****************************

//  ***********************************************
//  INITIALIZE THE RELAYS
//  ***********************************************

int Relay_Pins[] = {31, 33, 35, 37, 39, 41, 43, 45};
int Relay_Count = 8;


//  ****************************
//  END INITIALIZATION OF  THE RELAYS
//  ****************************
void setup(void)
{
	for(int relay = 0; relay < Relay_Count; relay++){pinMode(Relay_Pins[relay], OUTPUT);}
	for(int relay = 0; relay < Relay_Count; relay++){digitalWrite(Relay_Pins[relay], HIGH);}

	lcd.begin(20, 4);  //  setup the LCD's number of columns and rows
	
	lcd.createChar(1, degree);  //  init custom characters as numbers
	lcd.setBacklightPin(B_Light,POSITIVE);  //  set the backlight pin and polarity
	lcd.setBacklight(HIGH);  //  toggle the backlight on

	if (Serial_Debug == 1) Serial.begin(9600);  //  start the serial port if debugging is on
	while (!Serial) ;  //  wait untill the serial monitor opens
		
	setSyncProvider(RTC.get);  //  this function get the time from the RTC
	if (timeStatus()!=timeSet){
		RTC_Status = 0;
		Serial.println("Unable to get the RTC");}
	else{Serial.println("RTC has set the system time");}

	//  Check to see how many sensors are on the busses
	for(int i=0;i<NUMBER_OF_BUS; i++)   //search each bus one by one
	{
		oneWire[i] = new OneWire(ONE_WIRE_BUS[i]);
		sensors[i] = new DallasTemperature(oneWire[i]);
		sensors[i]->begin();
		numberOfDevices[i] = sensors[i]->getDeviceCount();
		Serial.print("Locating devices...");
		Serial.print("Found ");
		Serial.print(numberOfDevices[i], DEC);
		Serial.print(" devices on port ");
		Serial.println(ONE_WIRE_BUS[i],DEC);
		for(int j=0;j<numberOfDevices[i]; j++)
		{
			// Search the wire for address
			if(sensors[i]->getAddress(tempDeviceAddress[j], j))
			{
				Serial.print("Found device ");
				Serial.print(j, DEC);
				Serial.print(" with address: ");
				printAddress(tempDeviceAddress[j]);
				Serial.println("");

				// set the resolution to TEMPERATURE_PRECISION bit (Each Dallas/Maxim device is capable of several different resolutions)
				sensors[i]->setResolution(tempDeviceAddress[j], TEMPERATURE_PRECISION);
			}
			else{
				Serial.print("Found ghost device at ");
				Serial.print(j, DEC);
				Serial.print(" but could not detect address. Check power and cabling");
			}
		}
		delay(500);
	}
}
void loop()
{
	if (RTC_Status==1){LCD_Time_Display();}

	LM35_Sens_1();  //  Read temperature sensor 1
	
	if (RTC_Status==1){Display_Date();}

	DS18B20_Read();	  //  Read DS Temperature sensors
//	RL_Toggle();
	
}

void RL_Toggle()
{
	for (int relay = 0; relay < Relay_Count; relay++){
		digitalWrite(Relay_Pins[relay], LOW);
		lcd.setCursor(relay,3);
		lcd.print("+");
		delay(100);
//	for (int relay = 0; relay < Relay_Count; relay++){		
		digitalWrite(Relay_Pins[relay], HIGH);
		lcd.setCursor(relay,3);
		lcd.print(" ");
		delay(100);}
//	for (int relay = 0; relay < Relay_Count; relay++){
//	digitalWrite(Relay_Pins[relay], HIGH);
//	delay(100);}
}

// function to print a device address
void printAddress(DeviceAddress deviceAddress)
{
	for (uint8_t i = 0; i < 8; i++)
	{
		// zero pad the address if necessary
		if (deviceAddress[i] < 16) Serial.print("0");
		Serial.print(deviceAddress[i], HEX);
	}
}

//  function to read the LM35 sensor #1
void LM35_Sens_1()
{
	int total;  //  value of the samples added up
	int sample;  //  value of the current sample

	//  take number of readings as set in the configuration from temp sensor 1
	total = 0;
	for(int n = 0; n < Sample_Number; n++)
	{
		sample = analogRead(LM35_S1_pin);  //  read the sensor
		total = total + sample;  //  have a running total of sensor data

		//  Check if serial debugging is on and if it is print the readings to the serial port
		if (Serial_Debug == 1)
		{
			Serial.print("#");
			Serial.print(n);
			Serial.print("-");
			Serial.print(sample);
			Serial.print(" ");
		}

		delay(Sample_Delay);  //  delay between reading the sensors
	}

	total = total/Sample_Number;  //  divide by the number of samples set in the config
	LM35_S1C = (5.0*total*100)/1024.0;  //  convert the reading to Celcius
	LM35_S1F = (LM35_S1C*9)/5+32;    //  convert the reading to Farenheit

	//  Check if serial debugging is on and if it is print the readings to the serial port
	if (Serial_Debug == 1)
	{
		Serial.print("Temp = ");
		Serial.print(LM35_S1C);
		Serial.print("C : ");
		Serial.print(LM35_S1F);
		Serial.println("F");
	}
	//  print the readings to the LCD
	lcd.setCursor(13,0);
	lcd.print("RM      ");  //  Clear the display to prevent extra characters from interfering when F rolls from 3 to 2 digits
	if (LM35_S1C >= 100 || LM35_S1F >= 100){lcd.setCursor(15,0);}
		else{lcd.setCursor(16,0);}
	if (Temp_Type == 0)
	{
		lcd.print(LM35_S1C);
		lcd.write(byte(1));
		lcd.print("C");
	}
	else
	{
		lcd.print(LM35_S1F);
		lcd.write(byte(1));
		lcd.print("F");
	}
}
void LCD_Time_Display()
{
	Serial.print(timeStatus());
	Serial.println(second());
	lcd.print("           ");
	//  set cursor start depending on the number of digits in the hour
	if (hourFormat12()>=10){lcd.setCursor(0,0);}
	else {lcd.setCursor(1,0);}
	lcd.print(hourFormat12());

	lcd.print(":");

	if(minute()<10){lcd.print('0');
	lcd.print(minute());}
	else{lcd.print(minute());}
	lcd.print(":");
	
	if(second()<10){lcd.print('0');
	lcd.print(second());}
	else{lcd.print(second());}

	if(isAM()==1){lcd.print("AM");}
	else{lcd.print("PM");}
}
void Display_Date()
{
	lcd.setCursor(0,0);
	lcd.print("           ");
	lcd.setCursor(1,0);
	lcd.print(month());
	lcd.print("/");
	lcd.print(day());
	lcd.print("/");
	lcd.print(year());
}
void DS18B20_Read()
{
	int c;
	
	//  Read the DS sensors found in void setup
	for(int i=0;i<NUMBER_OF_BUS; i++)   // poll every bus
	{
		sensors[i]->begin();
		numberOfDevices[i] = sensors[i]->getDeviceCount();
		sensors[i]->requestTemperatures();
		// print the device information
		for(int j=0;j<numberOfDevices[i]; j++)    // poll devices connect to the bus
		{
			sensors[i]->getAddress(tempDeviceAddress[j], j);
			printAddress(tempDeviceAddress[j]);      //print ID
			Serial.print(";");
			
			int tempC = sensors[i]->getTempC(tempDeviceAddress[j]);
			int tempF = sensors[i]->getTempF(tempDeviceAddress[j]);
			
			if(j==0){
				c=(j+1);
				Serial.print(c);
				Serial.print(" ");}
			else{
				c=(j+1);
				lcd.setCursor(13,c);
				Serial.print(c);
				Serial.print(" ");}
			lcd.setCursor(13,c);
			lcd.print("S");  //  Clear the display to prevent extra characters from interfering when F rolls from 3 to 2 digits
			lcd.print(j+1);
			lcd.print(("   "));
			
			if (tempC >= 100 || tempF >= 100){lcd.setCursor(15,c);} 
			else{lcd.setCursor(16,c);}
				
			if(Temp_Type == 0)
			{
				lcd.print(tempC);
				lcd.write(byte(1));
				lcd.print("C");
			}
			else
			{
				lcd.print(tempF);
				lcd.write(byte(1));
				lcd.print("F");
			}

			Serial.print(tempC);                    //print temperature
			Serial.print(" ");
			Serial.print(tempF);
			Serial.println("");
		}
	}
	Serial.println();
	delay(POLL_TIME);
}









