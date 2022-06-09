#include <Arduino.h>
#include <LoRa.h>


/**
 * Funcion que inicializa el modulo LoRa RA-02
 * @param rst_ra es el pin en donde va conectado el pin reset del RA-02
 * @param nss es el pin en donde va conectado el pin nss del RA-02
 * @param irq_na es el pin en donde va conectado el pin de irq (IO0) del RA-02 (Actualmente desconectado del Weareable EEG v1.0)
 * @param freq es la frecuencia deseada de operacion del RA-02
 */
void setLoRa(int rst_ra, int nss, int irq_na, long freq){
//Ajuste de los pines usados por el modulo RA-02
  pinMode(rst_ra, OUTPUT);    //Configuramos IO4 como salida para manejar el pin de reset
  pinMode(nss, OUTPUT);       //Configuramos IO5 como salida para manejar el pin de seleccion de esclavo
  digitalWrite(rst_ra, HIGH); //Ponemos reset=1 para que se ejecute
  digitalWrite(nss, LOW);     //Ponemos nss=0 para seleccionar el esclavo (el RA-02)
  //Ajustamos los pines del LoRa
  LoRa.setPins(nss, rst_ra, irq_na);  //Configuramos los pines del modulo LoRa para que la libreria los maneje
  //Inicializamos el modulo a 433MHz (posible ajustarlo de 410 a 525MHz)
  if (!LoRa.begin(freq)) {           //Configuramos el modulo RA-02 a 433MHz
    Serial.println("Inicializacion del modulo LORA RA-02 fallida!");
    while (1);                        //Si no se pudo inicializar se detiene el programa en este bucle infinito
  }
  delay(200); //Retardo que espera la estabilizacion del modulo RA-02
  Serial.println("Inicializando dispositivo .::Weareable EKG::.");
  //Envio de un string de prueba (no es necesario)
  //LoRa.beginPacket();                //Inicializamos un paquete a enviar
  //LoRa.print(".::Weareable EEG::."); //Enviamos un string
  //LoRa.endPacket();    
}