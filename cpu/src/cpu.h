#ifndef SRC_CPU_H_
#define SRC_CPU_H_

#include <utils/conexiones.h>
#include <commons/string.h>
#include <diccionarioCpu.h>

void inicializar_modulo();
void conectar();
void esperar_desconexiones();
void levantar_logger();
void levantar_config();
void terminar_programa();
void terminar_conexiones(int, ...);
void conectar_kernel();
void conectar_kernel_dispatch();
void conectar_kernel_interrupt();

void atender_kernel_dispatch();
void atender_kernel_interrupt();
void conectar_memoria();
void iterator(char* );

void inicializar_registros();
void ejecutar_proceso();
void cargar_registros(t_cde);

#endif