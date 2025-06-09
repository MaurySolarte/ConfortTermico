#include "StateMachineLib.h"
#include "AsyncTaskLib.h"
#include <LiquidCrystal.h>
#include "DHT.h"
#define DHTPIN 22    
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

int t = 0;
int h = 0;
int sensorPin = A0;   
int sensorValue = 0;  
#define RED_PIN A1
#define  GREEN_PIN A3
#define  BLUE_PIN  A2
// State Alias
void readTemp(void);
void readHum(void);
void readLuz(void);
void readAlarma(void);
void Timeout(void);
void show_amb(void);


AsyncTask Task_TEMP(200, false, readTemp);
AsyncTask Task_HUM(200, false, readHum);
AsyncTask Task_LUZ(200, false, readLuz);
AsyncTask Task_ALARMA(200, false, readAlarma);
AsyncTask Task_TIMEOUT(200, false, Timeout);

enum State
{
	MON_AMB = 0,
	MON_LUZ = 1,
	MON_ALARMA = 2
};
enum Input
{
	T = 0,
	L = 1,
	H = 2,
	Unknown = 3
};


StateMachine stateMachine(3, 5);

Input input;

// Setup the State Machine
void setupStateMachine()
{
	// Add transitions
	stateMachine.AddTransition(MON_AMB, MON_LUZ, []() { return input == T; });
	stateMachine.AddTransition(MON_LUZ, MON_AMB, []() { return input == T; });
	stateMachine.AddTransition(MON_LUZ, MON_ALARMA, []() { return input == L; });
	stateMachine.AddTransition(MON_AMB, MON_ALARMA, []() { return input == H; });
	stateMachine.AddTransition(MON_ALARMA, MON_AMB, []() { return input == T; });
	// Add actions
	stateMachine.SetOnEntering(MON_AMB, funcInitAmb);
	stateMachine.SetOnEntering(MON_LUZ, funcInitLuz);
	stateMachine.SetOnEntering(MON_ALARMA, funcInitLuzAlarma);

	stateMachine.SetOnLeaving(MON_AMB, funcFinAmb);
	stateMachine.SetOnLeaving(MON_LUZ, funcFinLuz);
	stateMachine.SetOnLeaving(MON_ALARMA, funcFinAlarma);
}

void Timeout(void){
	input = Input::T;
}

void funcInitLuz(void){
	//Serial.println("Estado luz");	
	Task_TIMEOUT.SetIntervalMillis(2000);
	Task_TIMEOUT.Start();
	Task_LUZ.Start();
	show_LUZ();
	
}

void funcFinLuz(void){
	Task_LUZ.Stop();
		digitalWrite(RED_PIN, LOW);

}

void funcInitAmb(void){		
	//Serial.println("Estado amb");
	Task_TIMEOUT.SetIntervalMillis(5000);
	Task_TIMEOUT.Start();
	Task_HUM.Start();
	Task_TEMP.Start();
	show_AMB();

}

void funcFinAmb(void){
	Task_HUM.Stop();
	Task_TEMP.Stop();
}

void funcInitLuzAlarma(void){
	//Serial.println("Estado ALARMA");
	Task_TIMEOUT.SetIntervalMillis(8000);
	Task_TIMEOUT.Start();
	Task_ALARMA.SetIntervalMillis(300);
	Task_ALARMA.Start();

}

void funcFinAlarma(void){
	Task_LUZ.Stop();
}

void readAlarma(void){
	Serial.println("Estado ALARMA");
	digitalWrite(RED_PIN, HIGH);
}


void readTemp(void){
	dht.begin();
	Serial.println("Estado TEMP");
	t = dht.readTemperature();
	if( t > 27 && h < 70){
		input = Input::T;
	}
}

void readHum(void){
	dht.begin();
	Serial.println("Estado HUMEDAD");
	h = dht.readHumidity();
	if( t > 27 && h < 70){
		input = Input::T;
	}
}

void readLuz(void){
	Serial.println("Estado luz");
	sensorValue =  analogRead(sensorPin);
	if( sensorValue < 50){
		input = Input::L;
	}
}


void show_AMB(void){
      Serial.print("Temp: ");
			Serial.println(t);
			Serial.print("Hum: ");
			Serial.println(h);
			lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Temp: Hum: ");
      lcd.setCursor(0, 1);      
      lcd.print("  ");			
      lcd.print(t);
      lcd.print("   ");
      lcd.print(h);
}

void show_LUZ(void){
			Serial.print("Luz: ");
			Serial.println(sensorValue);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Luz ");
      lcd.print(sensorValue);
}

void setup() 
{
	
	lcd.begin(16,2);
	Serial.begin(9600);
	Serial.println("Starting State Machine...");
	setupStateMachine();	
	Serial.println("Start Machine Started");
	pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
	// Initial state
	stateMachine.SetState(MON_AMB, true, true);

}

void loop() 
{
	Task_HUM.Update();
	Task_LUZ.Update();
	Task_ALARMA.Update();
	Task_TEMP.Update();
	Task_TIMEOUT.Update();
	stateMachine.Update();
	input = Input::Unknown;
}
// Auxiliar output functions that show the state debug
void outputA()
{
	Serial.println("AMB   LUZ   ALARM  ");
	Serial.println(" X          ");
	Serial.println();
}

void outputB()
{
	Serial.println("AMB   LUZ   ALARM  ");
	Serial.println("       X    ");
	Serial.println();
}

void outputC()
{
	Serial.println("AMB   LUZ   ALARM  ");
	Serial.println("              X");
	Serial.println();
}
void outputD()
{
	Serial.println("A   B   C   D");
	Serial.println("            X");
	Serial.println();
}