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

//Pines usados para el modulo LoRa RA-02 (Fijos en el Weareable EEG V1.0)
#define RST_RA  4  //RESET del RA-02 esta conectado a IO4
#define NSS     5  //NSS del RA-02 esta conectado a IO4
#define IRQ_NA 13  //La salida IO0 del RA-02 usada para indicar que llego un dato, (no esta conectada en Weareable EEG v1.0 pero se asigna IO13 que esta libre()

#define SAMPLING_FREQ 256 //En Hz, escoge la frecuencia de muestreo


//Declaracion de las funciones a utilizar en este programa
void enTouch1Pulsado();  //Funcion que se ejecuta cuando se ha tocado el touchpad 1
void enTouch2Pulsado();  //Funcion que se ejecuta cuando se ha tocado el touchpad 1
void enTouch3Pulsado();  //Funcion que se ejecuta cuando se ha tocado el touchpad 1
void filtrar();          //Funcion que filtra digitalmente la señal analoga en ADC1_7 (IO35) y la transmite por un modulo LoRa




/**
 * Funcion de inicializacion del dispositivo
 */
void setup() {
  
   //************************ Inicializacion del puerto serial
  Serial.begin(115200);  //Ajustamos el puerto serial a 115200 bits por segundo
  while (!Serial);       //Esperamos a que se inicialice el puerto serial en un bucle infinito

  //************************ Inicializacion del modulo LoRa RA-02
  setLoRa(RST_RA, NSS, IRQ_NA, 433E6);

  //************************ Inicializacion de las interrupciones de los touchpads
  setTouchCallbacks(&enTouch1Pulsado, &enTouch2Pulsado, &enTouch3Pulsado, 10000);

  //************************ Inicializacion de las interrupciones del ADC
  setADCCallbacks(&filtrar, SAMPLING_FREQ);
}


uint8_t voltajeSalida=0;         //Variable que almacena el voltaje que sera sacado por el canal DAC1
uint8_t voltajeSalidaHi=0;         //Variable que almacena el voltaje que sera sacado por el canal DAC1
uint8_t voltajeSalidaLo=0;         //Variable que almacena el voltaje que sera sacado por el canal DAC1
#define SIZE_BUF 256
uint8_t buffer[SIZE_BUF*2];  
const uint8_t * buf;
/**
 * Funcion que filtra digitalmente la señal analoga en ADC1_7 (IO35), saca la señal filtrada 
 * por el canal DAC1 y transmite dicha señal por un modulo LoRa
 */
void filtrar(){	

  /****ADC - Adquisicion de datos por el ADC1_7****/
  int adc=analogRead(35);// = local_adc1_read(7);   //Adquisicion de un dato analogo por el ADC1_7 (IO35) con resolucion de 12 bits
  static uint16_t index=0;
  /****FILTRADO - Implementacion del filtro digital****/
  //
  // Escribe tu codigo del filtro digital aqui
  //

  /****DAC - Sacando valores analogos por el canal DAC1****/
  //unsigned char voltajeSalida=0;         //Variable que almacena el voltaje que sera sacado por el canal DAC1

  //if(voltajeSalida==0) voltajeSalida=0xFF; else voltajeSalida=0x00; //Generamos una señal de la mitad de la frecuencia de muestreo en el pin IO25
  //if(index<(SIZE_BUF-1)) {index++;buffer[index]=index;} else {index=0x00; buffer[0]=0;}; //Generamos una señal diente de sierra

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

  voltajeSalida = (uint8_t)(adc >> 4);   //El modulo DAC tiene resolucion de 8 bits, asi que el valor del ADC (de 12 bits) se rota a la derecha 4 bits (divide en 16)
  if(index<(SIZE_BUF-1)) {index++; /*buffer[index]=voltajeSalida;*/} else {index=0x00; /*buffer[0]=voltajeSalida;*/}; //Generamos una señal diente de sierra
  
  for(int i=1;i<SIZE_BUF;i++){
    buffer[i-1]=buffer[i];
  }
  buffer[255]=(voltajeSalida+buffer[SIZE_BUF-1]+buffer[SIZE_BUF-2]+buffer[SIZE_BUF-3])>>2;
  //Sacar un valor de voltaje por el canal DAC1
  //dac_output_enable(DAC_CHANNEL_1);                  //Habilitamos el DAC canal 1
  //dac_output_voltage(DAC_CHANNEL_1, voltajeSalida);  //Sacamos el voltaje en el DAC canal 1
  buf=buffer;
  if(index==(SIZE_BUF-1)){
      /****TRANSMISION - Transmision de datos por LoRa****/
      LoRa.beginPacket();                  //Inicializamos el paquete a enviar
      //LoRa.print(voltajeSalida,0);       //Asi se enviaria un solo byte, en caso de querer implementar una forma de envio de datos mas optima
      //LoRa.print(String(voltajeSalida)); //Convertimos a String el valor del ADC (de 0 a 1023)
      LoRa.write(buf,(size_t)SIZE_BUF);    //Convertimos a String el valor del ADC (de 0 a 1023)
      //LoRa.print(String(adc));           //Convertimos a String el valor del ADC (de 0 a 1023)
      //LoRa.print("\r\n");                //Enviamos un enter para que sea leido por el programa graficador
      LoRa.endPacket(true);                //Finalizamos el paquete a enviar
  }
}




/**
 * Funcion que maneja la pulsacion del touchpad 1
 */
void enTouch1Pulsado(){
  Serial.println("1 presionado, valor: "+String(touchRead(TOUCH_1)));
}

/**
 * Funcion que maneja la pulsacion del touchpad 2
 */
void enTouch2Pulsado(){
  Serial.println("2 presionado, valor: "+String(touchRead(TOUCH_2)));  
}

/**
 * Funcion que maneja la pulsacion del touchpad 3
 */
void enTouch3Pulsado(){
  Serial.println("3 presionado, valor: "+String(touchRead(TOUCH_3)));  
}




/**
 * Bucle infinito principal
 */
void loop() {
 vTaskDelay(100000);
}
