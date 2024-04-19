#ifndef SRC_IO_H_
#define SRC_IO_H_

#include <utils/conexiones.h>
#include <commons/string.h>
#include <diccionarioIO.h>

void levantar_logger();
void levantar_config();
void terminar_programa();
void terminar_conexiones(int, ...);
void paquete(int);
void conectar_memoria();
void conectar_kernel();
void conectar();
void inicializar_modulo();
void consola();

#endif