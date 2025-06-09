#include "StateMachineLib.h"
#include "AsyncTaskLib.h"
#include <LiquidCrystal.h>
#include "DHT.h"
#include <LiquidCrystal.h>
#include <Keypad.h>

//LECTURA TEMPERATURA, HUMEDAD
#define DHTPIN 22    
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
int t = 0;
int h = 0;

//PANTALLA LCD
const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

//TECLADO
const byte ROWS = 4; //four rows
const byte COLS = 4; //three columns
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6', 'B'},
  {'7','8','9', 'C'},
  {'*','0','#', 'D'}
};
byte rowPins[ROWS] = {30, 32, 34, 36}; //connect to the row pinouts of the keypad
byte colPins[COLS] = {38, 40, 42, 44}; //connect to the column pinouts of the keypad
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );


//CONTRASEÑA
int bandera = 0;
char passwordSys [4] = {'2', '0', '2', '5'};
char passwordUser [4];
int i = 0;
int intentos = 3;

//LECTURA LUZ
int sensorPin = A0;   
int sensorValue = 0;  
//PINES RGB
#define RED_PIN A1
#define  GREEN_PIN A3
#define  BLUE_PIN  A2
//BUZZER
const int buzzerPin = 7;//the buzzer pin attach to

// State Alias

void readAMB(void);
void readTemp(void);
void readHum(void);
void readLuz(void);
void readAlarma(void);
void Timeout(void);
void show_AMB(void);
void pedirClave(void);

AsyncTask Task_AMB(2000, true, readAMB);
AsyncTask Task_TEMP(1000, false, readTemp);
AsyncTask Task_HUM(2000, false, readHum);
AsyncTask Task_LUZ(2000, false, readLuz);
AsyncTask Task_ALARMA(5000, true, readAlarma);
AsyncTask Task_TIMEOUT(200, false, Timeout);
AsyncTask Task_CLAVE(200, true, pedirClave);



enum State
{
  PASS = 0,
	MON_AMB = 1,	
	MON_ALARMA = 2,
  MON_BLOQUEO = 3,
  MON_ALT = 4,
  MON_BAJ = 5
	
};
enum Input
{
  C = 0,
	AMB = 1,	
	ALM = 2,
  T = 3,
  BLQ = 4,
  ALT = 5,
  BAJ = 6,
	Unknown = 7
};


StateMachine stateMachine(3, 5);

Input input;

// Setup the State Machine
void setupStateMachine()
{
	// Add transitions
  stateMachine.AddTransition(PASS, MON_AMB, []() { return input == AMB; });
  stateMachine.AddTransition(PASS, MON_BLOQUEO, []() { return input == BLQ; });
	stateMachine.AddTransition(MON_AMB, MON_ALARMA, []() { return input == ALM;});
  stateMachine.AddTransition(MON_AMB, MON_ALT, []() { return input == ALT;});
  stateMachine.AddTransition(MON_AMB, MON_BAJ, []() { return input == BAJ;});
  stateMachine.AddTransition(MON_ALT, MON_AMB, []() { return input == T;});
  stateMachine.AddTransition(MON_BAJ, MON_AMB, []() { return input == T;});
	stateMachine.AddTransition(MON_ALARMA, MON_AMB, []() { return input == T; });
  stateMachine.AddTransition(MON_ALARMA, MON_BLOQUEO, []() { return input == BLQ; });
  stateMachine.AddTransition(MON_BLOQUEO, PASS, []() { return input == C; });
	// Add actions
  stateMachine.SetOnEntering(PASS, funcInitClave);
	stateMachine.SetOnEntering(MON_AMB, funcInitAmb);
	stateMachine.SetOnEntering(MON_ALARMA, funcInitLuzAlarma);
  
  stateMachine.SetOnLeaving(PASS, funcFinClave);
	stateMachine.SetOnLeaving(MON_AMB, funcFinAmb);
	stateMachine.SetOnLeaving(MON_ALARMA, funcFinAlarma);
}


//INICIAR TAREAS

void funcInitClave(void) {
  Task_CLAVE.Start();
}

void Timeout(void){
	input = Input::T;
}

void funcInitAmb(void){		
  
  Task_AMB.Start();
	// Task_LUZ.Start();
	// Task_HUM.Start();
	// Task_TEMP.Start();
	
}

//FINALIZAR TAREAS
void funcFinClave(void) {
  Task_CLAVE.Stop();
  lcd.clear();
}


void funcFinAmb(void){
  Task_AMB.Stop();
	// Task_HUM.Stop();
	// Task_TEMP.Stop();
	// Task_LUZ.Stop();
}

void funcInitLuzAlarma(void){
	//Serial.println("Estado ALARMA");
	Task_TIMEOUT.SetIntervalMillis(3000);
	Task_TIMEOUT.Start();
	//Task_ALARMA.SetIntervalMillis(300);
	Task_ALARMA.Start();
}

void funcFinAlarma(void){
	Task_LUZ.Stop();
	digitalWrite(RED_PIN, LOW);
}

//LECTURAS
void pedirClave(void){

	char key = keypad.getKey();
  
   if(bandera == 0){
    lcd.setCursor(0, 0);
    lcd.print("Ingrese clave");
   }
    

  if (key && i < 4) {    
    Serial.println(key);
    lcd.setCursor(i, 1);
    lcd.print("*");
    passwordUser[i] = key;
    i++;
  }


  if (i == 4 && bandera == 0) {
      lcd.clear();  
      lcd.setCursor(0, 1);
    if (strncmp(passwordUser,passwordSys,4) == 0){
      digitalWrite(GREEN_PIN, HIGH);
      lcd.print("Correcta");
      bandera = 1;
      delay(2000);
      lcd.clear();        
      digitalWrite(GREEN_PIN, LOW);
      input = Input::AMB;
    }
    else{
      digitalWrite(BLUE_PIN, HIGH);
      lcd.print("Incorrecta");  
      intentos--;
      delay(2000);
      lcd.clear();
      i = 0;
      digitalWrite(BLUE_PIN, LOW);
      if(intentos == 0){
        digitalWrite(RED_PIN, HIGH);
        lcd.print("Bloqueado");  
        delay(2000);
        lcd.clear();
        digitalWrite(RED_PIN, LOW);
         input = Input::BLQ;
    }
      
  }

  }
}

void readAMB(void){
	Serial.println("Estado AMB");
	t = dht.readTemperature();
  //Serial.println("Estado HUMEDAD");
	h = dht.readHumidity();
	sensorValue =  analogRead(sensorPin);
	if( t > 40 or sensorValue < 50){
		input = Input::ALM;
	}	
	show_AMB();
}

void readTemp(void){
	Serial.println("Estado TEMP");
	// t = dht.readTemperature();
  // //Serial.println("Estado HUMEDAD");
	// h = dht.readHumidity();
	// sensorValue =  analogRead(sensorPin);
	// if( t > 40 or sensorValue < 10){
	// 	input = Input::ALM;
	// }	
	
}

void readHum(void){	
	Serial.println("Estado HUMEDAD");
	h = dht.readHumidity();
	if( t > 40){
		input = Input::ALM;
	}
}

void readLuz(void){	
	Serial.println("Estado luz");
	sensorValue =  analogRead(sensorPin);
	if( sensorValue < 10){
		input = Input::ALM;
	}
	
}

void readAlarma(void){
	Serial.println("Estado ALARMA");
  //readBuzzer();
	digitalWrite(RED_PIN, HIGH);
  delay(800);
  digitalWrite(RED_PIN, LOW);
  delay(300);
}

void readBuzzer(void){
  for(int i = 200;i <= 800;i++)   //frequence loop from 200 to 800
  {
    tone(7,i);   //in pin7 generate a tone,it frequence is i
    delay(5);    //wait for 5 milliseconds   
  }
  //delay(4000);   //wait for 4 seconds on highest frequence
  for(int i = 800;i >= 200;i--)  //frequence loop from 800 downto 200
  {
    tone(7,i);  //in pin7 generate a tone,it frequence is i
    delay(10);  //delay 10ms
  }
}

void show_AMB(void){
      
      lcd.setCursor(0, 0);
      lcd.print("Temp: Hum:  Luz:");
      lcd.setCursor(0, 1);      
      lcd.print("  ");			
      lcd.print(t);
      lcd.print("   ");
      lcd.print(h);			
      lcd.print("   ");
      lcd.print(sensorValue);
			Serial.println(sensorValue);      
}


void setup() 
{
  input = Input::Unknown;
	dht.begin();
	lcd.begin(16,2);
  pinMode(buzzerPin,OUTPUT);//set buzzerPin as OUTPUT
	Serial.begin(9600);
	Serial.println("Starting State Machine...");
	setupStateMachine();	
	Serial.println("Start Machine Started");
	pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
	stateMachine.SetState(PASS, true, true);
	dht.begin();

}

void loop() 
{
  Task_AMB.Update();
  Task_CLAVE.Update();
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