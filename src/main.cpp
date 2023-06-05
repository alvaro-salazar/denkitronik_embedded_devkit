/*
 * The MIT License
 *
 * Copyright 2020 Alvaro Salazar <alvaro@denkitronik.com>.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <Arduino.h>
#include <stdint.h>
#include "libtouchesp32.h"
#include "libadcesp32.h"
#include <driver/dac.h>
#include "libloraesp32.h"
#include <Wire.h>
#include <L3G.h>
#include <TinyGPSPlus.h>

// Pines usados para el modulo LoRa RA-02 (Fijos en el Weareable EEG V1.0)
#define RST_RA 4  // RESET del RA-02 esta conectado a IO4
#define NSS 5     // NSS del RA-02 esta conectado a IO4
#define IRQ_NA 13 // La salida IO0 del RA-02 usada para indicar que llego un dato, (no esta conectada en Weareable EEG v1.0 pero se asigna IO13 que esta libre()

#define SAMPLING_FREQ 256 // En Hz, escoge la frecuencia de muestreo
#define SIZE_BUF 256      // Tamaño del buffer de datos a transmitir por LoRa

// Declaracion de las funciones a utilizar en este programa
void enTouch1Pulsado(); // Funcion que se ejecuta cuando se ha tocado el touchpad 1
void enTouch2Pulsado(); // Funcion que se ejecuta cuando se ha tocado el touchpad 1
void enTouch3Pulsado(); // Funcion que se ejecuta cuando se ha tocado el touchpad 1
void filtrar();         // Funcion que filtra digitalmente la señal analoga en ADC1_7 (IO35) y la transmite por un modulo LoRa
L3G gyro;               // Objeto que representa el giroscopio
TinyGPSPlus gps;        // Objeto que representa el GPS
void displayInfo();     // Funcion que muestra los datos del GPS


uint8_t voltajeSalida = 0;   // Variable que almacena el voltaje que sera sacado por el canal DAC1
uint8_t voltajeSalidaHi = 0; // Variable que almacena el voltaje que sera sacado por el canal DAC1
uint8_t voltajeSalidaLo = 0; // Variable que almacena el voltaje que sera sacado por el canal DAC1

uint8_t buffer[SIZE_BUF * 2]; // Buffer de datos a transmitir por LoRa
const uint8_t *buf;          // Puntero al buffer de datos a transmitir por LoRa
float lat = 0.0;  //Variables que almacenan la latitud y longitud del GPS
float lon = 0.0; 
int month = 0;  //Variables que almacenan la fecha y hora del GPS
int day = 0; 
int year = 0; 
String tiempo; //Variable que almacena la hora del GPS en formato de cadena de caracteres



/**
 * Funcion de inicializacion del dispositivo
 */
void setup()
{

  //************************ Inicializacion del puerto serial
  Serial.begin(115200); // Ajustamos el puerto serial a 115200 bits por segundo
  while (!Serial)
    ;                  // Esperamos a que se inicialice el puerto serial en un bucle infinito
  Serial2.begin(9600); // Ajustamos el puerto serial a 115200 bits por segundo
  while (!Serial2)
    ; // Esperamos a que se inicialice el puerto serial en un bucle infinito

  //************************ Inicializacion del modulo LoRa RA-02
  // setLoRa(RST_RA, NSS, IRQ_NA, 433E6);

  //************************ Inicializacion de las interrupciones de los touchpads
  setTouchCallbacks(&enTouch1Pulsado, &enTouch2Pulsado, &enTouch3Pulsado, 10000);

  //************************ Inicializacion de las interrupciones del ADC
  setADCCallbacks(&filtrar, SAMPLING_FREQ);
  pinMode(23, OUTPUT);  //Configuramos el pin IO23 como salida
  digitalWrite(23, HIGH); //Habilitamos la alimentacion del giroscopio
  Wire.begin();  //Inicializamos el bus I2C para el giroscopio (SCL=IO22, SDA=IO21)

  if (!gyro.init()) {  //Inicializamos el giroscopio
    Serial.println("Fallo al detectar el tipo de giroscopio!"); //Si no se detecta el giroscopio, se imprime un mensaje de error
    while (1); //Se queda en un bucle infinito
  }

  gyro.enableDefault(); //Habilitamos el giroscopio con la configuracion por defecto (500dps) y el filtro pasa-bajos a 100Hz (ver L3G.h)
}



void displayInfo() {
  if (gps.location.isValid()) { //Si el GPS tiene una posicion valida
    lat = gps.location.lat();   //Almacenamos la latitud y longitud en las variables correspondientes
    lon = gps.location.lng();
  } else {                      //Si el GPS no tiene una posicion valida
    lat = 1000;                 //Almacenamos la latitud y longitud en las variables correspondientes
    lon = 1000;               
  }

  if (gps.date.isValid()) {    //Si el GPS tiene una fecha valida
    month= gps.date.month();   //Almacenamos la fecha y hora en las variables correspondientes
    day = gps.date.day();      
    year = gps.date.year();
  } else {                     //Si el GPS no tiene una fecha valida (por ejemplo, si no se ha recibido la hora)
    month= 13;                 
    day = 32;
    year = 0;
  }

  if (gps.time.isValid()) {
    int hora = (int) gps.time.hour();
    int minuto = (int) gps.time.minute();
    int segundo = gps.time.second();
    int milisegundo = gps.time.centisecond();
    tiempo = String(hora) + ":" + String(minuto) + ":" + String(segundo); // Almacenamos la fecha y hora en las variables correspondientes
  }
}

/**
 * Funcion que filtra digitalmente la señal analoga en ADC1_7 (IO35), saca la señal filtrada
 * por el canal DAC1 y transmite dicha señal por un modulo LoRa
 */
void filtrar()
{

  /****ADC - Adquisicion de datos por el ADC1_7****/
  int adcX = analogRead(35); // = local_adc1_read(7);   //Adquisicion de un dato analogo por el ADC1_7 (IO35) con resolucion de 12 bits
  int adcY = analogRead(33); // = local_adc1_read(7);   //Adquisicion de un dato analogo por el ADC1_7 (IO35) con resolucion de 12 bits
  int adcZ = analogRead(32); // = local_adc1_read(7);   //Adquisicion de un dato analogo por el ADC1_7 (IO35) con resolucion de 12 bits
  static uint16_t index = 0;
  /****FILTRADO - Implementacion del filtro digital****/
  //
  // Escribe tu codigo del filtro digital aqui
  //

  /****DAC - Sacando valores analogos por el canal DAC1****/
  // unsigned char voltajeSalida=0;         //Variable que almacena el voltaje que sera sacado por el canal DAC1

  // if(voltajeSalida==0) voltajeSalida=0xFF; else voltajeSalida=0x00; //Generamos una señal de la mitad de la frecuencia de muestreo en el pin IO25
  // if(index<(SIZE_BUF-1)) {index++;buffer[index]=index;} else {index=0x00; buffer[0]=0;}; //Generamos una señal diente de sierra

  /*  voltajeSalidaHi = (uint8_t)((adc >> 8)&0x03);   //El modulo DAC tiene resolucion de 8 bits, asi que el valor del ADC (de 12 bits) se rota a la derecha 4 bits (divide en 16)
    voltajeSalidaLo = (uint8_t)(adc & 0xFF);   //El modulo DAC tiene resolucion de 8 bits, asi que el valor del ADC (de 12 bits) se rota a la derecha 4 bits (divide en 16)
    if(index==(SIZE_BUF-1)) {
        buffer[index*2+1]=voltajeSalidaHi;
        buffer[index*2]=voltajeSalidaLo;
        index=0;
    } else {
        buffer[index*2+1]=voltajeSalidaHi;
        buffer[index*2]=voltajeSalidaLo;
        index++;
    } */

  voltajeSalida = (uint8_t)(adcX >> 4); // El modulo DAC tiene resolucion de 8 bits, asi que el valor del ADC (de 12 bits) se rota a la derecha 4 bits (divide en 16)
  if (index < (SIZE_BUF - 1))
  {
    index++; /*buffer[index]=voltajeSalida;*/
  }
  else
  {
    index = 0x00; /*buffer[0]=voltajeSalida;*/
  };              // Generamos una señal diente de sierra

  for (int i = 1; i < SIZE_BUF; i++)
  {
    buffer[i - 1] = buffer[i];
  }
  buffer[255] = (voltajeSalida + buffer[SIZE_BUF - 1] + buffer[SIZE_BUF - 2] + buffer[SIZE_BUF - 3]) >> 2;
  // Sacar un valor de voltaje por el canal DAC1
  // dac_output_enable(DAC_CHANNEL_1);                  //Habilitamos el DAC canal 1
  // dac_output_voltage(DAC_CHANNEL_1, voltajeSalida);  //Sacamos el voltaje en el DAC canal 1
  buf = buffer;
  // Serial.println(buf[0]);

  gyro.read();  //Leemos el giroscopio

  // Quite el comentario de la siguiente linea para imprimir los datos por el puerto serial para verlos en el monitor serial del computador (a 115200 bits por segundo) en formato de tabla separados por tabuladores (\t) y saltos de linea (\n) al final de cada linea (\r) 
  //Serial.println(String(adcX) + "\t" + String(adcY) + "\t" + String(adcZ) + "\t" + String(gyro.g.x) + "\t" + String(gyro.g.y) + "\t" + String(gyro.g.z) + "\t" + String(lat) + "\t" + String(lon) + "\t" + String(month) + "\t" + String(day) + "\t" + String(year) + "\t"+ String(tiempo) );
  
  // Quite el comentario de la siguiente linea para ver los datos del giroscopio y acelerometro para verlo en el SerialPlot
  Serial.println(String(adcX) + "\t" + String(adcY) + "\t" + String(adcZ) + "\t" + String(gyro.g.x) + "\t" + String(gyro.g.y) + "\t" + String(gyro.g.z));
}

/**
 * Funcion que maneja la pulsacion del touchpad 1
 */
void enTouch1Pulsado()
{
  Serial.println("1 presionado, valor: " + String(touchRead(TOUCH_1)));
}

/**
 * Funcion que maneja la pulsacion del touchpad 2
 */
void enTouch2Pulsado()
{
  Serial.println("2 presionado, valor: " + String(touchRead(TOUCH_2)));
}

/**
 * Funcion que maneja la pulsacion del touchpad 3
 */
void enTouch3Pulsado()
{
  Serial.println("3 presionado, valor: " + String(touchRead(TOUCH_3)));
}

/**
 * Bucle infinito principal
 */
void loop()
{
  //Toma lecturas del GPS por el puerto serial 2
  while (Serial2.available() > 0)
    if (gps.encode(Serial2.read()))
      displayInfo(); //Funcion que muestra los datos del GPS

  if (millis() > 5000 && gps.charsProcessed() < 10)
  {
    Serial.println(F("No se detecto el GPS: revise el cableado."));
    while (true)
      ;
  }

  vTaskDelay(10000);
}
