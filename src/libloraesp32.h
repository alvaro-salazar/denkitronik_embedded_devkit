#include <LoRa.h>
#include <SPI.h>

/**
 * Funcion que inicializa el modulo LoRa RA-02
 * @param rst_ra es el pin en donde va conectado el pin reset del RA-02
 * @param nss es el pin en donde va conectado el pin nss del RA-02
 * @param irq_na es el pin en donde va conectado el pin de irq (IO0) del RA-02 (Actualmente desconectado del Weareable EEG v1.0)
 * @param freq es la frecuencia deseada de operacion del RA-02
 */
void setLoRa(int rst_ra, int nss, int irq_na, long freq);