#include "StateMachineLib.h"
#include "AsyncTaskLib.h"
#include <LiquidCrystal.h>
#include "DHT.h"
#include <LiquidCrystal.h>
#include <Keypad.h>
#include <SPI.h>
#include <MFRC522.h>

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
byte rowPins[ROWS] = {30, 33, 34, 36}; //connect to the row pinouts of the keypad
byte colPins[COLS] = {38, 40, 42, 44}; //connect to the column pinouts of the keypad
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );


//CONTRASEÑA
int bandera = 0;
char passwordSys [4] = {'2', '0', '2', '5'};
char passwordUser [4];
int i = 0;
int intentos = 3;
int bloqueos = 3;

//LECTURA LUZ
int sensorPin = A0;   
int sensorValue = 0;  
//PINES RGB
#define RED_PIN A1
#define  GREEN_PIN A3
#define  BLUE_PIN  A2
//BUZZER
const int buzzerPin = 7;//the buzzer pin attach to
//RELAY / VENTILADOR
#define RELAY_PIN A5 // ESP32 pin GIOP27, which connects to the IN pin of relay
int uso = 0;
int uso2 = 2;
bool isEqualArray(byte* arrayA, byte* arrayB, int length)
{
  for (int index = 0; index < length; index++)
  {
    if (arrayA[index] != arrayB[index]) return false;
  }
  return true;
}

//RFID
const int RST_PIN = 32;        // Pin 9 para el reset del RC522
const int SS_PIN = 53;        // Pin 10 para el SS (SDA) del RC522
MFRC522 mfrc522(SS_PIN, RST_PIN);   // Crear instancia del MFRC522
byte validKey1[4]; 
byte validKey2[4];

// State Alias

void readAMB(void);
// void readTemp(void);
// void readHum(void);
// void readLuz(void);
void readAlarma(void);
void Timeout(void);
void show_AMB(void);
void pedirClave(void);
void readBloqueo(void);
void readTarjeta(void);
void readLlavero(void);
void readVentilador(void);
void readRFID(void);


AsyncTask Task_AMB(800, true, readAMB);
// AsyncTask Task_TEMP(1000, false, readTemp);
// AsyncTask Task_HUM(2000, false, readHum);
// AsyncTask Task_LUZ(2000, false, readLuz);
AsyncTask Task_ALARMA(100, true, readAlarma);
AsyncTask Task_TIMEOUT(200, false, Timeout);
AsyncTask Task_CLAVE(20, true, pedirClave);
AsyncTask Task_BLOQUEO(200, true, readBloqueo);
AsyncTask Task_VENTILADOR(500, false, readVentilador);
AsyncTask Task_READRFID(1, true, readRFID);

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


StateMachine stateMachine(6, 10);

Input input;

// Setup the State Machine
void setupStateMachine()
{
	// Add transitions
  stateMachine.AddTransition(PASS, MON_AMB, []() { return input == AMB; });
  stateMachine.AddTransition(PASS, MON_BLOQUEO, []() { return input == BLQ; });
	stateMachine.AddTransition(MON_AMB, MON_ALARMA, []() { return input == ALM; });
  stateMachine.AddTransition(MON_AMB, MON_ALT, []() { return input == ALT; });
  stateMachine.AddTransition(MON_AMB, MON_BAJ, []() { return input == BAJ; });
  stateMachine.AddTransition(MON_ALT, MON_AMB, []() { return input == T; });
  stateMachine.AddTransition(MON_BAJ, MON_AMB, []() { return input == T; });
	stateMachine.AddTransition(MON_ALARMA, MON_AMB, []() { return input == T; });
  stateMachine.AddTransition(MON_ALARMA, MON_BLOQUEO, []() { return input == BLQ; });
  stateMachine.AddTransition(MON_BLOQUEO, PASS, []() { return input == C; });
	// Add actions
  stateMachine.SetOnEntering(PASS, funcInitClave);
	stateMachine.SetOnEntering(MON_AMB, funcInitAmb);
	stateMachine.SetOnEntering(MON_ALARMA, funcInitLuzAlarma);
  stateMachine.SetOnEntering(MON_BLOQUEO, funcInitBloqueo);
  stateMachine.SetOnEntering(MON_ALT, funcInitVentilador);
  stateMachine.SetOnEntering(MON_BAJ, []() {
    Task_TIMEOUT.SetIntervalMillis(5000);
	  Task_TIMEOUT.Start();
    lcd.print("PMV Bajo");          
    digitalWrite(RED_PIN, HIGH);
    });
  
  stateMachine.SetOnLeaving(PASS, funcFinClave);
	stateMachine.SetOnLeaving(MON_AMB, funcFinAmb);
	stateMachine.SetOnLeaving(MON_ALARMA, funcFinAlarma);
  stateMachine.SetOnLeaving(MON_BLOQUEO, funcFinBloqueo);
  stateMachine.SetOnLeaving(MON_ALT, funcFinVentilador);
  stateMachine.SetOnLeaving(MON_BAJ, []() {
    digitalWrite(RED_PIN, LOW);
    lcd.clear();
    });
}


//INICIAR TAREAS

void funcInitClave(void) {
  bandera = 0;
  i = 0;
  intentos = 3;
  bloqueos = 3;
  Serial.println("estado clave");
  //Serial.println(input);
  Task_CLAVE.Start();
}

void Timeout(void){
  input = Input::T;
  Serial.println("timeout");
  //Serial.println(input);
	  
}

void funcInitAmb(void){		  
  Serial.println("Estado AMB");
  lcd.clear();
  //Serial.println(input);
  Task_AMB.Start();
  Task_READRFID.Start();
	// Task_LUZ.Start();
	// Task_HUM.Start();
	// Task_TEMP.Start();
	
}

void funcInitLuzAlarma(void){
  Serial.println("Estado ALARMA");
  if( t > 40){
    lcd.print("TEMPERATURA ALTA");          
  }else if( sensorValue < 50){
    lcd.print("POCA LUZ");
  }
  bloqueos --;
  //Serial.println(input);
	//Serial.println("Estado ALARMA");
	Task_TIMEOUT.SetIntervalMillis(3000);
	Task_TIMEOUT.Start();
  Task_ALARMA.Start();
  readBuzzer();
	//Task_ALARMA.SetIntervalMillis(300);
	
}

void funcInitBloqueo(void){
  Serial.println("Estado BLOQUEO");
  Task_BLOQUEO.Start();
  lcd.print("Bloqueado");
  lcd.setCursor(0,1);
  lcd.print("Presione *");
}

void funcInitVentilador(void){
  Task_TIMEOUT.SetIntervalMillis(5000);
	Task_TIMEOUT.Start();
  Serial.println("Estado Alto");
  lcd.print("PMV ALTO");          
  Task_VENTILADOR.Start();
}

//FINALIZAR TAREAS
void funcFinClave(void) {
  Serial.println("SALIR Estado clave");
  Task_CLAVE.Stop();
  lcd.clear();
}


void funcFinAmb(void){
  Serial.println("SALIR Estado AMB");
  Task_AMB.Stop();
  Task_READRFID.Stop();
  lcd.clear();
	// Task_HUM.Stop();
	// Task_TEMP.Stop();
	// Task_LUZ.Stop();
}



void funcFinAlarma(void){
  Serial.println("SALIR Estado ALARMA");
	Task_ALARMA.Stop();  
  lcd.clear();
	digitalWrite(RED_PIN, LOW);
  noTone(7);

}

void funcFinBloqueo(void){
  Serial.println("SALIR Estado bloqueo");
  Task_BLOQUEO.Stop();
  lcd.clear();
  digitalWrite(RED_PIN, LOW);

}

void funcFinVentilador(void){
  Serial.println("SALIR Estado ALTO");
  Task_VENTILADOR.Stop();
  lcd.clear();
  digitalWrite(RELAY_PIN, LOW);
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
        delay(1000);
        lcd.clear();
        digitalWrite(RED_PIN, LOW);
         input = Input::BLQ;
      }
      
   }

  
  }
  if (mfrc522.PICC_IsNewCardPresent())
  {
    //Seleccionamos una tarjeta
    if (mfrc522.PICC_ReadCardSerial())
    {      
      if(uso2 >= 0){
        uso2--;
      }
      if(uso == 0){
        for(int i = 0; i < 4; i++){
        validKey1[i] = mfrc522.uid.uidByte[i];  
        Serial.println("llenar");
        uso++;        
        Serial.println(uso2);
        }
      }
      if(uso2 == 0){
        for(int i = 0; i < 4; i++){
          validKey2[i] = mfrc522.uid.uidByte[i];
          Serial.println("llenar2");
          uso2++;        
        }
      }

      //Serial.print(F("Card UID:"));
      printArray(validKey1, mfrc522.uid.size);
      printArray(validKey2, mfrc522.uid.size);
      mfrc522.PICC_HaltA();
    }
  }
}

void readAMB(void){
	// Serial.println("Estado AMB");
	t = dht.readTemperature();
  //Serial.println("Estado HUMEDAD");
	h = dht.readHumidity();
	sensorValue =  analogRead(sensorPin);
  
	if( t > 40 or sensorValue < 50){
		input = Input::ALM;
	}	
	show_AMB();
  
}

void readAlarma(void){
	// Serial.println("Estado ALARMA");
  
  digitalWrite(RED_PIN, HIGH);
  delay(800);
  digitalWrite(RED_PIN, LOW);
  delay(300);
  Serial.println(bloqueos);
  Serial.println(input);
  if (bloqueos <= 0){
    Serial.println("bloq");
    input = Input::BLQ;
  }
	
}

void readBuzzer(void){
  for(int i = 200;i <= 800;i++)   //frequence loop from 200 to 800
  {
    tone(7,i);   //in pin7 generate a tone,it frequence is i
  }
  //delay(4000);   //wait for 4 seconds on highest frequence
  
}


bool ledBloqueo = false;
void readBloqueo(void){
  digitalWrite(RED_PIN, ledBloqueo ? LOW : HIGH);
  ledBloqueo = !ledBloqueo;
  Task_BLOQUEO.SetIntervalMillis(ledBloqueo ? 200 : 100);          
        
  char key = keypad.getKey();
  Serial.println(key);
  Serial.println(input);
  if(key == '*'){
    input = Input::C;
  }

}

void readRFID(void){
  Serial.println("leyendo");
  if (mfrc522.PICC_IsNewCardPresent())
  {
    //Seleccionamos una tarjeta
    if (mfrc522.PICC_ReadCardSerial())
    {      
      // Comparar ID con las claves válidas
      if (isEqualArray(mfrc522.uid.uidByte, validKey1, 4)){
        Serial.println("Tarjeta 1 valida");
        input = Input::ALT;
      }else{
        Serial.println("Tarjeta 1 invalida");
      }
      if (isEqualArray(mfrc522.uid.uidByte, validKey2, 4)){
        Serial.println("Tarjeta 2 valida");
        input = Input::BAJ;
      }else{
        Serial.println("Tarjeta 2 invalida");
      }
      // Finalizar lectura actual
      mfrc522.PICC_HaltA();
    }
  }
}

void readVentilador(void){
  Serial.println("Prendió ventilador");
  digitalWrite(RELAY_PIN, HIGH);
}

void show_AMB(void){
      lcd.clear();  
      lcd.setCursor(0, 0);
      lcd.print("Temp: Hum:  Luz:");
      lcd.setCursor(0, 1);          
      lcd.print("  ");			
      lcd.print(t);
      lcd.print("   ");
      lcd.print(h);			
      lcd.print("   ");
      lcd.print(sensorValue);
			//Serial.println(sensorValue);      
}
//RFID FUNCION
void printArray(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}


void setup() 
{
  //input = Input::Unknown;
  pinMode(RELAY_PIN, OUTPUT);
  SPI.begin();      //Función que inicializa SPI
  mfrc522.PCD_Init();
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
	stateMachine.SetState(PASS, false, true);
	dht.begin();

}

void loop() 
{
  Task_AMB.Update();
  Task_CLAVE.Update();
	// Task_HUM.Update();
	// Task_LUZ.Update();
	Task_ALARMA.Update();
	// Task_TEMP.Update();
	Task_TIMEOUT.Update();
  Task_BLOQUEO.Update();
  Task_VENTILADOR.Update();
  Task_READRFID.Update();
	stateMachine.Update();
	//input = Input::Unknown;

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