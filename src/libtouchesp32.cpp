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

//Configuracion de pines del touchpad y tiempo antirebote
#define TOUCH_1 27
#define TOUCH_2 14
#define TOUCH_3 12
#define WAIT_BOUNCE 80000

//Registra el tiempo transcurrido desde el inicio de la pulsacion de cada touchpad
unsigned long timeout1;
unsigned long timeout2;
unsigned long timeout3;

int acumulador1=0;
int acumulador2=0;
int acumulador3=0;
int valorTouch1=0;
int valorTouch2=0;
int valorTouch3=0;

//Contador de interrupciones del timer
volatile int contadorTouchInterrupt;

//Contador del total de interrupciones generadas por el timer desde el encendido
int totalTouch1Interrupt;
int promedio1=0;
int promedio2=0;
int promedio3=0;
int threshold = 40; //Variable que ajusta el umbral de deteccion del touchpad
bool touch1detected = false; //Variable que indica si se presiono el touch 1
bool touch2detected = false; //Variable que indica si se presiono el touch 2
bool touch3detected = false; //Variable que indica si se presiono el touch 3
bool is_on_touch1=false; //Variable que indica si el touch 1 esta en estado pulsado
bool is_on_touch2=false; //Variable que indica si el touch 2 esta en estado pulsado
bool is_on_touch3=false; //Variable que indica si el touch 3 esta en estado pulsado

//Puntero a una variable de tipo hw_timer_t para configurar el timer
hw_timer_t *timer2 = NULL;
portMUX_TYPE DRAM_ATTR timer2Mux = portMUX_INITIALIZER_UNLOCKED; //Variable mutex (MUTual EXclusion) para sincronizacion entre loop() y el manejador de interrupcion onTimer()

//portMUX_TYPE DRAM_ATTR timerMux = portMUX_INITIALIZER_UNLOCKED; 
TaskHandle_t complexHandlerTask;
//hw_timer_t * adcTimer = NULL; // our timer

void (*task1_handler)(void);
void (*task2_handler)(void);
void (*task3_handler)(void);
void initTouch(int freq);
void controlTouch();


/**
 * @brief Funcion que configura las funciones manejadoras de los touch e inicializa el timer 1
 * @param t1_handler Puntero a la funcion que controla la pulsacion del touchpad 1
 * @param t2_handler Puntero a la funcion que controla la pulsacion del touchpad 2
 * @param t3_handler Puntero a la funcion que controla la pulsacion del touchpad 3
 * @param period Periodo de muestreo de los touchpad
 */
void setTouchCallbacks(void (*t1_handler)(void), void (*t2_handler)(void), void (*t3_handler)(void), int period){
  task1_handler=t1_handler;
  task2_handler=t2_handler;
  task3_handler=t3_handler;
  initTouch(period);
}


/**
 * Funcion manejadora compleja
 */
void complexHandler(void *param) {
  while (true) {
    //Duerma hasta que la rutina de interrupcion ISR no de algo para hacer, o por 1 segundo
    //uint32_t tcount = ulTaskNotifyTake(pdFALSE, pdMS_TO_TICKS(1000));  
    ulTaskNotifyTake(pdFALSE, pdMS_TO_TICKS(1000));  
    controlTouch();  //Ejecutamos el controlador del touchpad
  }
}

/**
 * Funcion manejadora de la interrupcion del timer ajustado a 1000000us (1s)
 */
void IRAM_ATTR onTimer2() {
  portENTER_CRITICAL_ISR(&timer2Mux); //Usamos mutex (MUTual EXclusion) para evitar reentrada a esta rutina
  contadorTouchInterrupt++;           //Contamos el total de interrupciones dadas por el timer sin atender
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;                          //Ajustamos la prioridad de la tarea que va a ser despertada
  vTaskNotifyGiveFromISR(complexHandlerTask, &xHigherPriorityTaskWoken);  //Despertamos la tarea controlTouch
  if (xHigherPriorityTaskWoken) {     //Si la prioridad de la tarea es mayor que el de esta interrupcion
     portYIELD_FROM_ISR();            //Retorne de la interrupcion a la tarea no bloqueada
  }
  portEXIT_CRITICAL_ISR(&timer2Mux);  //Usamos mutex (MUTual EXclusion) para permitir entrada a esta rutina
}

/**
 * Funcion que inicializa las interrupciones del touchpad
 * @param period Especifica el periodo de muestreo del touchpad en microsegundos
 */
void initTouch(int period){
  xTaskCreatePinnedToCore(complexHandler, "Handler Task", 8192, NULL, 2,	&complexHandlerTask, 1);
  timer2 = timerBegin(1, 80, true);               //Configurar el timer (0: timer0, 80: divide entre 80, true: cuenta ascendente)
  timerAttachInterrupt(timer2, &onTimer2, true);  //Agregamos el manejador de interrupciones (timer: timer configurado, &onTimer: funcion manejadora, true: interrupcion por flanco)
  timerAlarmWrite(timer2, period, true);          //Configurar una alarma cada 1000us = 1ms (timer: timer configurado, 100000: 1 millon de cuentas, autoreinicie)
  timerAlarmEnable(timer2);                       //Habilita el timer configurado
}


/**
 *  Funcion controladora de las pulsaciones de los touch pad
 */
void controlTouch(){
  valorTouch1=touchRead(TOUCH_1);
  valorTouch2=touchRead(TOUCH_2);
  valorTouch3=touchRead(TOUCH_3);
  if(touch1detected){     //Se detecto una pulsacion
    if(!is_on_touch1){    //Si touch1 no estaba en estado "pulsado"
      task1_handler();    //Llamamos la funcion que maneja la pulsacion de touch 1
      is_on_touch1=true;  //Indicamos que touch 1 esta en estado "pulsado"
    }
  } else {                //Si no se detecto pulsacion
    is_on_touch1=false;   //Indicamos que touch 1 esta en estado "pulsado"    
  }

  if(touch2detected){     //Se detecto una pulsacion
    if(!is_on_touch2){    //Si touch1 no estaba en estado "pulsado"
      task2_handler();    //Llamamos la funcion que maneja la pulsacion de touch 1
      is_on_touch2=true;  //Indicamos que touch 1 esta en estado "pulsado"
    }
  } else {                //Si no se detecto pulsacion
    is_on_touch2=false;   //Indicamos que touch 1 esta en estado "pulsado"    
  }

  if(touch3detected){     //Se detecto una pulsacion
    if(!is_on_touch3){    //Si touch1 no estaba en estado "pulsado"
      task3_handler();    //Llamamos la funcion que maneja la pulsacion de touch 1
      is_on_touch3=true;  //Indicamos que touch 1 esta en estado "pulsado"
    }
  } else {                //Si no se detecto pulsacion
    is_on_touch3=false;   //Indicamos que touch 1 esta en estado "pulsado"    
  }

  if (contadorTouchInterrupt > 0) { //Revisamos si hay interrupciones del timer sin atender
    portENTER_CRITICAL(&timer2Mux); //Activamos el mutex para evitar que el manejador haga una re-entrada
    contadorTouchInterrupt--;       //Decrementamos el contador de interrupciones
    portEXIT_CRITICAL(&timer2Mux);  //Desactivamos el mutex
    acumulador1+=valorTouch1;       //Acumulamos el valor del touchpad1
    acumulador2+=valorTouch2;       //Acumulamos el valor del touchpad2
    acumulador3+=valorTouch3;       //Acumulamos el valor del touchpad3
    totalTouch1Interrupt++;
    if(totalTouch1Interrupt>=8){   //Si ya hemos tomado 16 datos
      promedio1=acumulador1>>3;     //Dividimos entre 16 para calcular el promedio (correr 4 bits a la derecha)
      promedio2=acumulador2>>3;     //Dividimos entre 16 para calcular el promedio (correr 4 bits a la derecha)
      promedio3=acumulador3>>3;     //Dividimos entre 16 para calcular el promedio (correr 4 bits a la derecha)
      totalTouch1Interrupt=0;       //Reiniciamos la cuenta de cuantas muestras se han tomado
      acumulador1=0;                //Reiniciamos el acumulador del touchpad 1
      acumulador2=0;                //Reiniciamos el acumulador del touchpad 2
      acumulador3=0;                //Reiniciamos el acumulador del touchpad 3
      if(promedio1<=70) touch1detected=true; else touch1detected=false; //Si el promedio1 es <=70 se ha pulsado el touchpad1
      if(promedio2<=70) touch2detected=true; else touch2detected=false; //Si el promedio1 es <=70 se ha pulsado el touchpad2
      if(promedio3<=70) touch3detected=true; else touch3detected=false; //Si el promedio1 es <=70 se ha pulsado el touchpad3
    }
  }
}

