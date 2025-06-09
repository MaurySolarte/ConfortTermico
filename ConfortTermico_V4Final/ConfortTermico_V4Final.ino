#include "StateMachineLib.h"
#include "AsyncTaskLib.h"
#include <LiquidCrystal.h>
#include "DHT.h"
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
const byte ROWS = 4; 
const byte COLS = 4; 
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6', 'B'},
  {'7','8','9', 'C'},
  {'*','0','#', 'D'}
};
byte rowPins[ROWS] = {30, 33, 34, 36};
byte colPins[COLS] = {38, 40, 42, 44};
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
const int buzzerPin = 7;
//RELAY / VENTILADOR
#define RELAY_PIN A5 
int uso = 0;
int uso2 = 2;


//RFID
const int RST_PIN = 32;        // Pin 9 para el reset del RC522
const int SS_PIN = 53;        // Pin 10 para el SS (SDA) del RC522
MFRC522 mfrc522(SS_PIN, RST_PIN);   // Crear instancia del MFRC522
byte validKey1[4]; 
byte validKey2[4];

// Definición de funciones auxiliares

/*
@brief Imprime un arreglo de caracteres.
*/
void printArray(byte *buffer, byte bufferSize);

/*
@brief Compara si dos arreglos de caracteres son iguales.
@return Equivalencia de los arreglos. True = equivalentes, false = diferentes
*/

bool isEqualArray(byte* arrayA, byte* arrayB, int length);

// Definición de Funciones utilizadas por las tareas

/*
@brief Lectura del valor de la temperatura, humedad y luz obtenidos por medio del sensor DHT
*/
void readAMB(void);
/*
@brief Enciende durante 800ms y apaga durante 200ms el led rojo. Si bloqueos = 0, entonces input = BLQ
*/
void readAlarma(void);

/*
@brief Pone la variable input = T
*/
void Timeout(void);

/*
@brief Muestra en la pantalla LCD las lecturas del sensor DHT
*/
void show_AMB(void);

/*
@brief Pide a través de la pantalla LCD una contraseña que se ingresa por medio del Keypad.
*/
void pedirClave(void);

/*
@brief Enciende durante 200ms y apaga durante 100ms el led rojo, si se presiona " * " en el Keypad, entonces input = C
*/
void readBloqueo(void);

/*
@brief Acciona el actuador RELAY para encender un ventilador.
*/
void readVentilador(void);

/*
@brief Lectura de tarjeta detectada por medio de RFID
*/
void readRFID(void);

/*
@brief Enciende un buzzer.
*/
void readBuzzer(void);

//Tareas Asíncronas
AsyncTask Task_AMB(800, true, readAMB);
AsyncTask Task_ALARMA(100, true, readAlarma);
AsyncTask Task_TIMEOUT(200, false, Timeout);
AsyncTask Task_CLAVE(20, true, pedirClave);
AsyncTask Task_BLOQUEO(200, true, readBloqueo);
AsyncTask Task_VENTILADOR(500, false, readVentilador);
AsyncTask Task_READRFID(1, true, readRFID);


//ENUMERACIONES DFSM
// Alias de los estados
enum State
{
  PASS = 0,
	MON_AMB = 1,	
	MON_ALARMA = 2,
  MON_BLOQUEO = 3,
  MON_ALT = 4,
  MON_BAJ = 5
	
};
//Alias de las instrucciones
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

//MÁQUINA DE ESTADOS FINITOS
StateMachine stateMachine(6, 10);

Input input;

// CONFIGURACIÓN DE LA MÁQUINA DE ESTADOS FINITOS
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
	// ACCIONES
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
  Task_CLAVE.Start();
}

void Timeout(void){
  input = Input::T;
  Serial.println("timeout");
	  
}

void funcInitAmb(void){		  
  Serial.println("Estado AMB");
  lcd.clear();
  Task_AMB.Start();
  Task_READRFID.Start();
	
}

void funcInitLuzAlarma(void){
  Serial.println("Estado ALARMA");
  if( t > 40){
    lcd.print("TEMPERATURA ALTA");          
  }else if( sensorValue < 50){
    lcd.print("MUCHA LUZ");
  }
  bloqueos --;

	Task_TIMEOUT.SetIntervalMillis(3000);
	Task_TIMEOUT.Start();
  Task_ALARMA.Start();
  readBuzzer();
	
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

      printArray(validKey1, mfrc522.uid.size);
      printArray(validKey2, mfrc522.uid.size);
      mfrc522.PICC_HaltA();
    }
  }
}


void readAMB(void){
	t = dht.readTemperature();
	h = dht.readHumidity();
	sensorValue =  analogRead(sensorPin);
  
	if( t > 40 or sensorValue < 50){
		input = Input::ALM;
	}	
	show_AMB();
  
}

// Encender Led Alarma
void readAlarma(void){
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
// Encender Buzzer
void readBuzzer(void){
  for(int i = 200;i <= 800;i++)   
  {
    tone(7,i);   
  }
  
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
      }else if(!(isEqualArray(mfrc522.uid.uidByte, validKey1, 4)) && !(isEqualArray(mfrc522.uid.uidByte, validKey2, 4))){
        lcd.clear();
        Serial.println("Tarjeta 1 invalida");
        lcd.print("Tarjeta invalida");
        delay(2000);
      }

      if (isEqualArray(mfrc522.uid.uidByte, validKey2, 4)){
        Serial.println("Tarjeta 2 valida");
        input = Input::BAJ;
      }else if(!(isEqualArray(mfrc522.uid.uidByte, validKey2, 4)) && !(isEqualArray(mfrc522.uid.uidByte, validKey1, 4))){
        lcd.clear();
        Serial.println("Tarjeta 2 invalida");
        lcd.print("Tarjeta invalida");
        delay(2000);
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

// Salida por pantalla de lecturas
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
}

//FUNCIÓN PARA IMPRIMIR UN ARREGLO
void printArray(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}
// FUNCIÓN PARA COMPARAR ARREGLOS
bool isEqualArray(byte* arrayA, byte* arrayB, int length)
{
  for (int index = 0; index < length; index++)
  {
    if (arrayA[index] != arrayB[index]) return false;
  }
  return true;
}



void setup() 
{
  pinMode(RELAY_PIN, OUTPUT);
  SPI.begin();     
  mfrc522.PCD_Init();
	dht.begin();
	lcd.begin(16,2);
  pinMode(buzzerPin,OUTPUT);
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
	Task_ALARMA.Update();
	Task_TIMEOUT.Update();
  Task_BLOQUEO.Update();
  Task_VENTILADOR.Update();
  Task_READRFID.Update();
	stateMachine.Update();

}
