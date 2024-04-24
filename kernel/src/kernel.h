#ifndef SRC_KERNEL_H_
#define SRC_KERNEL_H_

#include <utils/conexiones.h>
#include <commons/string.h>
#include <diccionarioKernel.h>

void levantar_logger();
void levantar_config();
void terminar_programa();
void conectar_cpu_dispatch();
void conectar_cpu_interrupt();
void conectar_io();
void conectar_memoria();
void atender_io();
void inicializar_modulo();
void consola();
void conectar();
void esperar_desconexiones();

void iterator(char*);


#endif