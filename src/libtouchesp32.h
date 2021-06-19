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
//Configuracion de pines del touchpad y tiempo antirebote
#define TOUCH_1 27
#define TOUCH_2 14
#define TOUCH_3 12

/**
 * Funcion que inicializa las interrupciones del touchpad, se usa en el setup()
 * @param period Especifica el periodo de muestreo del touchpad en microsegundos
 */
void initTouch(int period);

/**
 * Funcion que configura las funciones manejadoras de los touch e inicializa el timer 1
 * @param t1_handler Puntero a la funcion que controla la pulsacion del touchpad 1
 * @param t2_handler Puntero a la funcion que controla la pulsacion del touchpad 2
 * @param t3_handler Puntero a la funcion que controla la pulsacion del touchpad 3
 * @param period Periodo de muestreo de los touchpad
 */
void setTouchCallbacks(void (*t1_handler)(void), void (*t2_handler)(void), void (*t3_handler)(void), int period);