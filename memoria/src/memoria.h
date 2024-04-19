#ifndef SRC_MEMORIA_H_
#define SRC_MEMORIA_H_

#include <utils/conexiones.h>
#include <commons/string.h>
#include <diccionarioMem.h>

void inicializar_modulo();
void conectar();
void esperar_desconexiones();
void levantar_logger();
void levantar_config();
void terminar_programa();
void conectar_cpu();
void conectar_io();
void conectar_kernel();
void atender_io();
void atender_cpu();
void atender_kernel();
void iterator(char*);

#endif