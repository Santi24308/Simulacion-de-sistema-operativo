#ifndef SRC_CPU_H_
#define SRC_CPU_H_

#include <utils/conexiones.h>
#include <commons/string.h>
#include <diccionarioCpu.h>


void inicializar_modulo();
void inicializarSemaforos();
void conectar();
void esperar_desconexiones();
void levantar_logger();
void levantar_config();
void terminar_programa();
void terminar_conexiones(int, ...);
void conectar_kernel();
void conectar_kernel_dispatch();
void conectar_kernel_interrupt();
bool es_bloqueante(codigoInstruccion);

void atender_kernel_dispatch();
void atender_kernel_interrupt();
void conectar_memoria();
void iterator(char* );
void ejecutar_instruccion(t_cde*, t_instruccion* );
void ejecutar_set(char*, uint32_t );
void ejecutar_sum(char*, char* );
void ejecutar_sub(char*, char *);
void ejecutar_jnz(void*, uint32_t , t_cde*);
void ejecutar_sleep(uint32_t);
void ejecutar_wait(char*);
void ejecutar_signal(char*);
void ejecutar_IO_GEN_SLEEP(uint32_t);
void copiar_ultima_instruccion(t_cde*, t_instruccion*);

void devolver_cde_a_kernel(t_cde*, t_instruccion*);
void inicializar_registros();
void ejecutar_proceso(t_cde*);
void cargar_registros(t_cde*);
void guardar_registros(t_cde*);
uint32_t buscar_valor_registroUINT32(void*);
uint8_t buscar_valor_registroUINT8(void*);
void imprimir_instruccion(t_instruccion*);


void desalojar_cde(t_cde*, t_instruccion*);
char* obtener_nombre_instruccion(t_instruccion* );
char* obtener_nombre_motivo_desalojo(cod_desalojo);

#endif