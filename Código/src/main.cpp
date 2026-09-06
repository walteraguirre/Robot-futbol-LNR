#include <Arduino.h>
#include <Bluepad32.h>

// ============================================================
// PINES ESP32 -> TB6612FNG
// ============================================================
const int PIN_PWMA = 2;
const int PIN_AIN1 = 16;
const int PIN_AIN2 = 4;

const int PIN_PWMB = 19;
const int PIN_BIN1 = 5;
const int PIN_BIN2 = 18;

const int PIN_STBY = 17;

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
// Bluepad32 entrega valores de joystick de -511 a 512.
// Zona muerta independiente por eje: X tiene mas drift que Y.
// Calibracion: mirar en el monitor serie los valores en reposo y
// poner cada zona unos 20-30 puntos por encima del maximo observado.
const int ZONA_MUERTA_X = 90;
const int ZONA_MUERTA_Y = 50;

const float FACTOR_A = 1.00;
const float FACTOR_B = 1.00;
const int VELOCIDAD_MAXIMA = 255;

// MAC del unico control permitido para ESTE robot.
// Dejar en ceros la primera vez: el ESP32 acepta cualquier control e
// imprime su MAC por serie. Copiarla aca, volver a flashear, y desde
// entonces solo ese control puede manejar este robot.

#if ROBOT == 1
const uint8_t MAC_PERMITIDA[6] = {0x90, 0xB6, 0x85, 0x12, 0x9A, 0x65};  // control 1
#elif ROBOT == 2
const uint8_t MAC_PERMITIDA[6] = {0x58, 0x10, 0x31, 0x7E, 0x52, 0x55};  // control 2
#else
const uint8_t MAC_PERMITIDA[6] = {0, 0, 0, 0, 0, 0};  // acepta cualquiera
#endif

// Puntero para guardar el control de PS5
ControllerPtr miControl = nullptr;

bool macEsCero(const uint8_t* mac) {
    for (int i = 0; i < 6; i++) if (mac[i] != 0) return false;
    return true;
}

bool macCoincide(const uint8_t* a, const uint8_t* b) {
    for (int i = 0; i < 6; i++) if (a[i] != b[i]) return false;
    return true;
}

// ============================================================
// MOTOR A (IZQUIERDO)
// ============================================================
void motorA(int velocidad) {
    velocidad = constrain(velocidad, -255, 255);
    if (velocidad > 0) {
        digitalWrite(PIN_AIN1, HIGH);
        digitalWrite(PIN_AIN2, LOW);
        ledcWrite(PWM_CHANNEL_A, velocidad);
    } else if (velocidad < 0) {
        digitalWrite(PIN_AIN1, LOW);
        digitalWrite(PIN_AIN2, HIGH);
        ledcWrite(PWM_CHANNEL_A, -velocidad);
    } else {
        digitalWrite(PIN_AIN1, LOW);
        digitalWrite(PIN_AIN2, LOW);
        ledcWrite(PWM_CHANNEL_A, 0);
    }
}

// ============================================================
// MOTOR B (DERECHO)
// ============================================================
void motorB(int velocidad) {
    velocidad = constrain(velocidad, -255, 255);
    if (velocidad > 0) {
        digitalWrite(PIN_BIN1, HIGH);
        digitalWrite(PIN_BIN2, LOW);
        ledcWrite(PWM_CHANNEL_B, velocidad);
    } else if (velocidad < 0) {
        digitalWrite(PIN_BIN1, LOW);
        digitalWrite(PIN_BIN2, HIGH);
        ledcWrite(PWM_CHANNEL_B, -velocidad);
    } else {
        digitalWrite(PIN_BIN1, LOW);
        digitalWrite(PIN_BIN2, LOW);
        ledcWrite(PWM_CHANNEL_B, 0);
    }
}

void detenerMotores() {
    motorA(0);
    motorB(0);
}

// ============================================================
// PROCESAMIENTO DE EJES
// ============================================================
// Aplica zona muerta y reescala desde el borde de la zona hasta 512.
// Asi el centro da exactamente 0 y no hay salto al salir de la zona muerta.
int procesarEje(int valor, int zonaMuerta) {
    if (abs(valor) < zonaMuerta) return 0;
    int signo = (valor > 0) ? 1 : -1;
    int magnitud = map(abs(valor), zonaMuerta, 512, 0, VELOCIDAD_MAXIMA);
    return signo * constrain(magnitud, 0, VELOCIDAD_MAXIMA);
}

// ============================================================
// EVENTOS BLUEPAD32 (PS5)
// ============================================================
void onConnectedController(ControllerPtr ctl) {
    ControllerProperties p = ctl->getProperties();
    Serial.printf("Control detectado, MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  p.btaddr[0], p.btaddr[1], p.btaddr[2],
                  p.btaddr[3], p.btaddr[4], p.btaddr[5]);

    // FILTRO POR MAC: si hay una MAC configurada y no coincide, rechazo
    if (!macEsCero(MAC_PERMITIDA) && !macCoincide(p.btaddr, MAC_PERMITIDA)) {
        Serial.println("Control NO autorizado para este robot, rechazado.");
        ctl->disconnect();
        return;
    }

    if (miControl == nullptr) {
        miControl = ctl;
        // Ya tengo mi control: no acepto conexiones nuevas
        BP32.enableNewBluetoothConnections(false);
        Serial.println();
        Serial.println("==============================");
        Serial.println("MANDO PS5 CONECTADO");
        Serial.println("==============================");
    } else {
        // Ya hay un control asignado, rechazo este
        ctl->disconnect();
    }
}

void onDisconnectedController(ControllerPtr ctl) {
    if (miControl == ctl) {
        miControl = nullptr;
        detenerMotores();
        // Vuelvo a aceptar conexiones para poder reconectar
        BP32.enableNewBluetoothConnections(true);
        Serial.println();
        Serial.println("==============================");
        Serial.println("MANDO PS5 DESCONECTADO");
        Serial.println("MOTORES DETENIDOS");
        Serial.println("==============================");
    }
}

// ============================================================
// SETUP
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(1000);

    pinMode(PIN_AIN1, OUTPUT);
    pinMode(PIN_AIN2, OUTPUT);
    pinMode(PIN_BIN1, OUTPUT);
    pinMode(PIN_BIN2, OUTPUT);
    pinMode(PIN_STBY, OUTPUT);
    digitalWrite(PIN_STBY, HIGH);

    ledcSetup(PWM_CHANNEL_A, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(PIN_PWMA, PWM_CHANNEL_A);

    ledcSetup(PWM_CHANNEL_B, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(PIN_PWMB, PWM_CHANNEL_B);

    detenerMotores();

    // Iniciar Bluepad32
    BP32.setup(&onConnectedController, &onDisconnectedController);

    // Descomentar solo en el primer flasheo si el control no conecta,
    // borra pareos viejos. Despues volver a comentar.
    // BP32.forgetBluetoothKeys();

    Serial.println();
    Serial.println("==============================");
    Serial.println("ESP32 + TB6612 + PS5 (Bluepad32)");
    Serial.println("==============================");
    Serial.println("Pon tu control de PS5 en modo emparejamiento (Create + PS)...");
}

// ============================================================
// LOOP
// ============================================================

void loop() {
    // Actualizar datos del Bluetooth
    BP32.update();

    // SEGURIDAD: SIN MANDO = MOTORES DETENIDOS
    if (!miControl || !miControl->isConnected()) {
        detenerMotores();
        delay(50);
        return;
    }

    // LEER JOYSTICK (Valores de -511 a 512)
    int ejeX = miControl->axisX();

    // En Bluepad32, arriba es negativo. Lo invertimos (arriba = positivo).
    int ejeY = -miControl->axisY();

    // ZONA MUERTA + ESCALADO A VELOCIDAD_MAXIMA (por eje)
    int avance = procesarEje(ejeY, ZONA_MUERTA_Y);
    int giro   = procesarEje(ejeX, ZONA_MUERTA_X);

    // MEZCLA DIFERENCIAL
    int velocidadA = avance + giro;
    int velocidadB = avance - giro;

    // NORMALIZAR: la rueda mas rapida no puede superar lo que
    // realmente estas empujando el joystick (el mayor entre avance y giro).
    // Recto a fondo -> ambas a 180. Diagonal -> menos. Giro puro -> pivote.
    int objetivo = max(abs(avance), abs(giro));
    int mayor = max(abs(velocidadA), abs(velocidadB));
    if (mayor > objetivo && mayor > 0) {
        velocidadA = velocidadA * objetivo / mayor;
        velocidadB = velocidadB * objetivo / mayor;
    }

    // CALIBRACION ENTRE MOTORES (al final, sobre el valor ya normalizado)
    velocidadA = (int)(velocidadA * FACTOR_A);
    velocidadB = (int)(velocidadB * FACTOR_B);

    velocidadA = constrain(velocidadA, -VELOCIDAD_MAXIMA, VELOCIDAD_MAXIMA);
    velocidadB = constrain(velocidadB, -VELOCIDAD_MAXIMA, VELOCIDAD_MAXIMA);

    // MOVER MOTORES
    motorA(velocidadA);
    motorB(velocidadB);

    // DEBUG (Solo imprime si hay movimiento para no saturar)
    if (avance != 0 || giro != 0) {
        Serial.print("X(Giro): ");
        Serial.print(ejeX);
        Serial.print(" | Y(Avance): ");
        Serial.print(ejeY);
        Serial.print(" | MotorA: ");
        Serial.print(velocidadA);
        Serial.print(" | MotorB: ");
        Serial.println(velocidadB);
    }

    delay(20);
}
/*void loop() {
    Serial.println("A adelante");  motorA(150); delay(1500); motorA(0); delay(500);
    Serial.println("A atras");     motorA(-150); delay(1500); motorA(0); delay(500);
    Serial.println("B adelante");  motorB(150); delay(1500); motorB(0); delay(500);
    Serial.println("B atras");     motorB(-150); delay(1500); motorB(0); delay(500);
}*/