#ifndef SRC_MEMORIA_H_
#define SRC_MEMORIA_H_

#include <utils/conexiones.h>
#include <commons/string.h>
#include <diccionarioMem.h>

void inicializar_modulo();
void conectar();
void levantar_logger();
void levantar_config();
void conectar_cpu();
void conectar_io();
void conectar_kernel();
void atender_io();
void atender_cpu();
void atender_kernel();
void iterator(char*);
void iniciar_proceso();
void liberar_proceso();
void eliminar_instruccion(t_instruccion* instruccion);
void eliminar_proceso(t_proceso*);
t_proceso* crear_proceso(uint32_t , t_list*);
t_list* levantar_instrucciones(char* );
void escribir_parametro(int , t_instruccion* ,char*);
t_proceso* buscar_proceso(uint32_t);
void enviar_instruccion();
void terminar_programa();
int countStrings(char**);


#endif