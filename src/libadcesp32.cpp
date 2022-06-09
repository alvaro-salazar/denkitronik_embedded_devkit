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
#include "Arduino.h"
#include <soc/sens_reg.h>
#include <soc/sens_struct.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include <stdint.h>
#include "esp_types.h"
#include "driver/adc.h"
//#include "soc/efuse_periph.h"
#include "esp_err.h"
#include "assert.h"
#include "esp_adc_cal.h"

hw_timer_t *timerADC = NULL;
portMUX_TYPE DRAM_ATTR timerADCMux = portMUX_INITIALIZER_UNLOCKED;
TaskHandle_t complexHandlerADCTask;

void initAdc(uint32_t samplingFreq);

int IRAM_ATTR local_adc1_read(int channel);
void (*task_adc_handler)(void);


int IRAM_ATTR local_adc1_read(int channel) {
	uint16_t adc_value;
	SENS.sar_meas_start1.sar1_en_pad = (1 << channel); // Solo un canal es seleccionado
	while (SENS.sar_slave_addr1.meas_status != 0);
	SENS.sar_meas_start1.meas1_start_sar = 0;
	SENS.sar_meas_start1.meas1_start_sar = 1;
	while (SENS.sar_meas_start1.meas1_done_sar == 0);
	adc_value = SENS.sar_meas_start1.meas1_data_sar;
	return adc_value;
}


void complexHandlerADC(void *param) {
	while (true) {
		// Duerme hasta que la ISR nos de algo para hacer, o por 1 segundo

		ulTaskNotifyTake(pdFALSE, pdMS_TO_TICKS(100000));
		task_adc_handler();
	}
}


/**
 * Funcion manejadora del timer 3 usada para adquirir un dato del ADC
 */
void IRAM_ATTR onTimerADC() {  //Colocar el manejador de la interrupcion en la seccion de memoria IRAM
	portENTER_CRITICAL_ISR(&timerADCMux);  //Un mutex proteje al manejador de reentradas (lo cual no debe suceder, pero por si acaso)
	
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;  //Bandera indicadora de que la tarea de mas prioridad no esta en ejecucion.
	vTaskNotifyGiveFromISR(complexHandlerADCTask, &xHigherPriorityTaskWoken); 
	if (xHigherPriorityTaskWoken) {
		portYIELD_FROM_ISR();
	}
	portEXIT_CRITICAL_ISR(&timerADCMux);
}



/**
 * Funcion que configura las funciones manejadoras de los touch e inicializa el timer 1
 * @param t1_handler Puntero a la funcion que controla la pulsacion del touchpad 1
 * @param samplingFreq Periodo de muestreo de los touchpad
 */
void setADCCallbacks(void (*t1_handler)(void), int samplingFreq){
  task_adc_handler=t1_handler;
  initAdc(samplingFreq);
}


/**
 * Funcion que inicializa las interrupciones del ADC
 * @param samplingFreq Especifica la frecuencia de muestreo del ADC en Hz
 */
void initAdc(uint32_t samplingFreq) {
	uint32_t contador = 1000000 / samplingFreq; //Calculo del numero de cuentas
	float realFreq=1/(float)contador;
	xTaskCreatePinnedToCore(complexHandlerADC, "ADC Handler", 8192, NULL,  1, &complexHandlerADCTask, 0);
    while(!Serial);
	Serial.println("Inicializacion del ADC " + String(contador/1000)+ " ms de interrupcion de timerADC, frecuencia de muestreo = "+String(realFreq*1000000)+"Hz");
	// Adquiere un puntero al timer a usar
	timerADC = timerBegin(3, 80, true);  // Timer 3, divisor del reloj del sistema entre 80, conteo ascendente
	timerAttachInterrupt(timerADC, &onTimerADC, true); // Timer a adjuntar la interrupcion, Manejador de la interrupcion asignado al timerADC, interrupcion por flanco de subida del timerADC
	timerAlarmWrite(timerADC, contador, true); // Objeto timerADC a usar, contador de interrupciones, recargar despues de terminar (corre indefinidamente)
	timerAlarmEnable(timerADC); // Activamos el timerADC
	analogRead(35); //Leemos el puerto analogo IO35 (ADC1_CHANNEL_7) para inicializarlo

#define V_REF 1100  // Voltaje de referencia del ADC en milivoltios

    // Configura el ADC
    adc1_config_width(ADC_WIDTH_12Bit);
    adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_11db);

    // Calcula las caracteristicas del ADC por ejemplo factores ganancia de y offset
    esp_adc_cal_characteristics_t characteristics;
    //esp_adc_cal_get_characteristics(V_REF, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, &characteristics);
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, V_REF, &characteristics);
    uint32_t voltaje=0;
    // Lee el ADC y obtiene el resultado en mV
    uint32_t err = esp_adc_cal_get_voltage(ADC_CHANNEL_0, &characteristics, &voltaje);
    printf("%d mV\n", voltaje);
}