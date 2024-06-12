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
void buscar_y_eliminar_proceso(uint32_t);
void enviar_instruccion();
void terminar_programa();
codigoInstruccion obtener_codigo_instruccion(char*);
void trim_trailing_whitespace(char*);
void imprimir_instruccion(t_instruccion*);
void imprimir_pids();
void eliminar_lista_instrucciones(t_list*);
t_proceso* buscar_proceso(uint32_t pid);
void inicializar_paginacion();
int calculoDeCantidadMarcos();
void crear_tabla_global_de_marcos();
void inicializarMarco(t_marco*);
void crear_pagina_y_asignar_a_pid(int, int);
char* asignarMemoriaBits(int);
int bitsToBytes(int);
void liberarMemoriaPaginacion();
void devolver_nro_marco();
uint32_t obtener_marco_libre();
int cantidad_marcos_libres();
void crear_pagina_y_asignar_a_pid(int , int);
void destruir_pagina_y_liberar_marco(t_pagina* );
t_list* interfacesIO;




#endif