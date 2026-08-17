#include <Arduino.h>
#include <PS4Controller.h>

// ============================================================
// PINES ESP32 -> TB6612FNG
// ============================================================

// Motor A
const int PIN_PWMA = 2;
const int PIN_AIN1 = 4;
const int PIN_AIN2 = 15;

// Motor B
const int PIN_PWMB = 5;
const int PIN_BIN1 = 16;
const int PIN_BIN2 = 17;

// Standby
const int PIN_STBY = 18;

// ============================================================
// PWM
// ============================================================

const int PWM_FREQ = 10000;
const int PWM_RESOLUTION = 8;

const int PWM_CHANNEL_A = 0;
const int PWM_CHANNEL_B = 1;

// ============================================================
// CONFIGURACION CONTROL
// ============================================================

// Zona muerta del joystick
const int ZONA_MUERTA = 15;

// Limite de velocidad.
// Para primeras pruebas recomiendo 150-180.
// Maximo posible = 255.
const int VELOCIDAD_MAXIMA = 180;

// ============================================================
// MOTOR A
// velocidad: -255 a +255
// ============================================================

void motorA(int velocidad)
{
    velocidad = constrain(velocidad, -255, 255);

    if (velocidad > 0)
    {
        digitalWrite(PIN_AIN1, HIGH);
        digitalWrite(PIN_AIN2, LOW);

        ledcWrite(PWM_CHANNEL_A, velocidad);
    }
    else if (velocidad < 0)
    {
        digitalWrite(PIN_AIN1, LOW);
        digitalWrite(PIN_AIN2, HIGH);

        ledcWrite(PWM_CHANNEL_A, -velocidad);
    }
    else
    {
        digitalWrite(PIN_AIN1, LOW);
        digitalWrite(PIN_AIN2, LOW);

        ledcWrite(PWM_CHANNEL_A, 0);
    }
}

// ============================================================
// MOTOR B
// velocidad: -255 a +255
// ============================================================

void motorB(int velocidad)
{
    velocidad = constrain(velocidad, -255, 255);

    if (velocidad > 0)
    {
        digitalWrite(PIN_BIN1, HIGH);
        digitalWrite(PIN_BIN2, LOW);

        ledcWrite(PWM_CHANNEL_B, velocidad);
    }
    else if (velocidad < 0)
    {
        digitalWrite(PIN_BIN1, LOW);
        digitalWrite(PIN_BIN2, HIGH);

        ledcWrite(PWM_CHANNEL_B, -velocidad);
    }
    else
    {
        digitalWrite(PIN_BIN1, LOW);
        digitalWrite(PIN_BIN2, LOW);

        ledcWrite(PWM_CHANNEL_B, 0);
    }
}

// ============================================================
// DETENER
// ============================================================

void detenerMotores()
{
    motorA(0);
    motorB(0);
}

// ============================================================
// EVENTOS PS4
// ============================================================

void onConnect()
{
    Serial.println();
    Serial.println("==============================");
    Serial.println("MANDO PS4 CONECTADO");
    Serial.println("==============================");
}

void onDisconnect()
{
    Serial.println();
    Serial.println("==============================");
    Serial.println("MANDO PS4 DESCONECTADO");
    Serial.println("MOTORES DETENIDOS");
    Serial.println("==============================");

    detenerMotores();
}

// ============================================================
// SETUP
// ============================================================

void setup()
{
    Serial.begin(115200);
    delay(1000);

    // --------------------------------------------------------
    // Pines motores
    // --------------------------------------------------------

    pinMode(PIN_AIN1, OUTPUT);
    pinMode(PIN_AIN2, OUTPUT);

    pinMode(PIN_BIN1, OUTPUT);
    pinMode(PIN_BIN2, OUTPUT);

    pinMode(PIN_STBY, OUTPUT);

    digitalWrite(PIN_STBY, HIGH);

    // --------------------------------------------------------
    // PWM
    // --------------------------------------------------------

    ledcSetup(PWM_CHANNEL_A, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(PIN_PWMA, PWM_CHANNEL_A);

    ledcSetup(PWM_CHANNEL_B, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(PIN_PWMB, PWM_CHANNEL_B);

    detenerMotores();

    // --------------------------------------------------------
    // PS4
    // --------------------------------------------------------

    PS4.attachOnConnect(onConnect);
    PS4.attachOnDisconnect(onDisconnect);

    // MISMA MAC QUE CONFIGURASTE CON SIXAXIS PAIR TOOL
    PS4.begin("1a:2b:3c:01:01:01");

    Serial.println();
    Serial.println("==============================");
    Serial.println("ESP32 + TB6612 + PS4");
    Serial.println("==============================");
    Serial.println("Esperando mando PS4...");
}

// ============================================================
// LOOP
// ============================================================

void loop()
{
    // ========================================================
    // SEGURIDAD: SIN MANDO = MOTORES DETENIDOS
    // ========================================================

    if (!PS4.isConnected())
    {
        detenerMotores();
        delay(50);
        return;
    }

    // ========================================================
    // LEER JOYSTICK
    // ========================================================

    int ejeX = PS4.LStickX();
    int ejeY = PS4.LStickY();

    // Normalmente:
    //
    // X -> -128 izquierda / +127 derecha
    // Y -> -128 abajo     / +127 arriba
    //
    // Queremos que arriba sea positivo.
    // Dependiendo del mando puede que esto ya sea asi.
    //
    int avance = ejeY;
    int giro = ejeX;

    // ========================================================
    // ZONA MUERTA
    // ========================================================

    if (abs(avance) < ZONA_MUERTA)
        avance = 0;

    if (abs(giro) < ZONA_MUERTA)
        giro = 0;

    // ========================================================
    // ESCALAR -128...127 A VELOCIDAD_MAXIMA
    // ========================================================

    avance = map(avance, -128, 127,
                 -VELOCIDAD_MAXIMA,
                  VELOCIDAD_MAXIMA);

    giro = map(giro, -128, 127,
               -VELOCIDAD_MAXIMA,
                VELOCIDAD_MAXIMA);

    // ========================================================
    // MEZCLA DIFERENCIAL
    // ========================================================
    //
    // Adelante:
    // A = +
    // B = +
    //
    // Girar derecha:
    // A aumenta
    // B disminuye
    //
    // Girar izquierda:
    // A disminuye
    // B aumenta
    //
    // ========================================================

    int velocidadA = avance + giro;
    int velocidadB = avance - giro;

    // ========================================================
    // LIMITAR
    // ========================================================

    velocidadA = constrain(
        velocidadA,
        -VELOCIDAD_MAXIMA,
         VELOCIDAD_MAXIMA
    );

    velocidadB = constrain(
        velocidadB,
        -VELOCIDAD_MAXIMA,
         VELOCIDAD_MAXIMA
    );

    // ========================================================
    // MOVER MOTORES
    // ========================================================

    motorA(velocidadA);
    motorB(velocidadB);

    // ========================================================
    // DEBUG
    // ========================================================

    Serial.print("X: ");
    Serial.print(ejeX);

    Serial.print(" | Y: ");
    Serial.print(ejeY);

    Serial.print(" | A: ");
    Serial.print(velocidadA);

    Serial.print(" | B: ");
    Serial.println(velocidadB);

    delay(20);
}