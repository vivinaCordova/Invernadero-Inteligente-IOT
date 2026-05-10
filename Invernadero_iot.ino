#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>

// =========================
// Pines
// =========================
#define PIN_LDR 25
#define PIN_SERVO 13
#define PIN_BOTON 27
#define PIN_SWITCH_MODO 26

// =========================
// LCD
// =========================
LiquidCrystal_I2C lcd(0x27, 16, 2);

// =========================
// Servo
// =========================
Servo servoTecho;

// =========================
// Estados
// =========================
enum EstadoLuz {
  LUZ_ALTA,
  LUZ_MEDIA,
  LUZ_BAJA_ALERTA
};

EstadoLuz estadoLuz = LUZ_MEDIA;

// =========================
// Posiciones servo
// =========================
const int TECHO_ABIERTO = 160;
const int TECHO_MEDIO = 100;
const int TECHO_CERRADO = 0;

// =========================
// Umbrales LDR
// =========================
const int UMBRAL_LUZ_ALTA = 2200;
const int UMBRAL_LUZ_MEDIA = 3700;
const int CAIDA_RAPIDA = 380;

// =========================
// Variables
// =========================
int lecturaLDR = 0;
int lecturaAnterior = 0;

bool techoManualAbierto = true;
bool ultimoEstadoBoton = HIGH;

// =========================
// Temporizadores
// =========================
unsigned long tiempoLectura = 0;
unsigned long tiempoLCD = 0;
unsigned long tiempoBoton = 0;
unsigned long tiempoInicio = 0;

// =========================
// Intervalos
// =========================
const unsigned long intervaloLectura = 2000;
const unsigned long intervaloLCD = 500;
const unsigned long debounceBoton = 200;
const unsigned long tiempoSplash = 2000;

// =========================
// Control splash
// =========================
bool splashMostrado = true;

// =========================
// SETUP
// =========================
void setup() {

  Serial.begin(115200);

  analogReadResolution(12);
  

  pinMode(PIN_BOTON, INPUT_PULLUP);
  pinMode(PIN_SWITCH_MODO, INPUT_PULLUP);

  // I2C ESP32
  Wire.begin(21, 22);

  // LCD
  lcd.init();
  lcd.backlight();

  // Servo
  servoTecho.setPeriodHertz(50);
  servoTecho.attach(PIN_SERVO, 500, 2400);

  servoTecho.write(TECHO_MEDIO);

  // Lectura inicial
  lecturaAnterior = analogRead(PIN_LDR);

  // Pantalla inicio
  lcd.setCursor(0,0);
  lcd.print("Invernadero");

  lcd.setCursor(0,1);
  lcd.print("Inicializando");

  tiempoInicio = millis();
}

// =========================
// LOOP
// =========================
void loop() {

  unsigned long tiempoActual = millis();

  // =========================
  // Pantalla splash
  // =========================
  if (splashMostrado &&
      (tiempoActual - tiempoInicio >= tiempoSplash)) {

    lcd.clear();
    splashMostrado = false;
  }

  // =========================
  // Leer switch modo
  // =========================
  bool modoManual =
      digitalRead(PIN_SWITCH_MODO) == LOW;

  // =========================
  // Lectura sensor
  // =========================
  if (tiempoActual - tiempoLectura >= intervaloLectura) {

    tiempoLectura = tiempoActual;

    lecturaLDR = analogRead(PIN_LDR);

    Serial.print("LDR: ");
    Serial.println(lecturaLDR);

    if (modoManual) {

      controlarModoManual(tiempoActual);
    }
    else {

      maquinaEstadosAutomatica();
    }

    lecturaAnterior = lecturaLDR;
  }

  // =========================
  // Actualizar LCD
  // =========================
  if (!splashMostrado &&
      (tiempoActual - tiempoLCD >= intervaloLCD)) {

    tiempoLCD = tiempoActual;

    actualizarLCD(modoManual);
  }
}

// =========================
// CONTROL MANUAL
// =========================
void controlarModoManual(unsigned long tiempoActual) {

  bool estadoBoton = digitalRead(PIN_BOTON);

  // Detectar pulsación
  if (estadoBoton == LOW &&
      ultimoEstadoBoton == HIGH) {

    if (tiempoActual - tiempoBoton >= debounceBoton) {

      tiempoBoton = tiempoActual;

      techoManualAbierto =
          !techoManualAbierto;
    }
  }

  ultimoEstadoBoton = estadoBoton;

  // Movimiento servo
  if (techoManualAbierto) {

    servoTecho.write(TECHO_ABIERTO);
  }
  else {

    servoTecho.write(TECHO_CERRADO);
  }
}

// =========================
// MAQUINA DE ESTADOS
// =========================
void maquinaEstadosAutomatica() {

  switch (estadoLuz) {

    // =====================
    // DIA
    // =====================
    case LUZ_ALTA:

      servoTecho.write(TECHO_ABIERTO);

      // Luz media
      if (lecturaLDR >= UMBRAL_LUZ_ALTA &&
          lecturaLDR < UMBRAL_LUZ_MEDIA) {

        estadoLuz = LUZ_MEDIA;
      }

      // Noche
      else if (lecturaLDR >= UMBRAL_LUZ_MEDIA) {

        estadoLuz = LUZ_BAJA_ALERTA;
      }

    break;

    // =====================
    // TARDE
    // =====================
    case LUZ_MEDIA:

      servoTecho.write(TECHO_MEDIO);

      // Día
      if (lecturaLDR < UMBRAL_LUZ_ALTA) {

        estadoLuz = LUZ_ALTA;
      }

      // Noche
      else if (lecturaLDR >= UMBRAL_LUZ_MEDIA) {

        estadoLuz = LUZ_BAJA_ALERTA;
      }

    break;

    // =====================
    // NOCHE
    // =====================
    case LUZ_BAJA_ALERTA:

      servoTecho.write(TECHO_CERRADO);

      // Día
      if (lecturaLDR < UMBRAL_LUZ_ALTA) {

        estadoLuz = LUZ_ALTA;
      }

      // Tarde
      else if (lecturaLDR < UMBRAL_LUZ_MEDIA) {

        estadoLuz = LUZ_MEDIA;
      }

    break;
  }
}
// =========================
// LCD
// =========================
void actualizarLCD(bool modoManual) {

  int porcentajeLuz =
      map(lecturaLDR, 0, 4095, 0, 100);

  // NO usar lcd.clear()
  // para evitar parpadeos

  lcd.setCursor(0,0);
  lcd.print("                ");

  lcd.setCursor(0,1);
  lcd.print("                ");

  // =====================
  // MODO MANUAL
  // =====================
  if (modoManual) {

    lcd.setCursor(0,0);
    lcd.print("MANUAL ");
    lcd.print(porcentajeLuz);
    lcd.print("%");

    lcd.setCursor(0,1);

    if (techoManualAbierto) {

      lcd.print("Techo Abierto");
    }
    else {

      lcd.print("Techo Cerrado");
    }

    return;
  }

  // =====================
  // MODO AUTOMATICO
  // =====================
  switch (estadoLuz) {

    case LUZ_ALTA:

      lcd.setCursor(0,0);
      lcd.print("AUTO Luz Alta");

      lcd.setCursor(0,1);
      lcd.print("Techo Abierto");

    break;

    case LUZ_MEDIA:

      lcd.setCursor(0,0);
      lcd.print("AUTO Luz Media");

      lcd.setCursor(0,1);
      lcd.print("Techo Medio");

    break;

    case LUZ_BAJA_ALERTA:

      lcd.setCursor(0,0);
      lcd.print("ALERTA LLUVIA");

      lcd.setCursor(0,1);
      lcd.print("Techo Cerrado");

    break;
  }
}
