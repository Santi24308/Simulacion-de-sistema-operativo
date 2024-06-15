#ifndef SRC_CPU_H_
#define SRC_CPU_H_

#include <utils/conexiones.h>
#include <commons/string.h>
#include <diccionarioCpu.h>


void inicializar_modulo();
void inicializarSemaforos();
void conectar();
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
void ejecutar_instruccion(t_instruccion* );
void ejecutar_set(char*, uint32_t );
void ejecutar_sum(char*, char* );
void ejecutar_sub(char*, char *);
void ejecutar_jnz(void*, uint32_t);
void ejecutar_sleep(uint32_t);
void copiar_ultima_instruccion(t_instruccion*);

// funciones con memoria, tlb, mmu
void ejecutar_resize(int tamanio);
void ejecutar_mov_in(char* reg_datos, char* reg_direccion);
void leer_de_dir_fisica_los_bytes(uint32_t dir_fisica, uint32_t bytes, uint32_t* valor_leido);
void ejecutar_mov_in_un_byte(char* reg_datos, char* reg_direccion);
void ejecutar_mov_in_cuatro_bytes(char* reg_datos, char* reg_direccion);
uint32_t leer_y_guardar_de_dos_paginas(uint32_t dir_logica);
uint32_t dato_reconstruido(uint32_t primera, uint32_t segunda, int bytes_primera, int bytes_segunda);
bool es_reg_de_cuatro_bytes(char* reg_datos);
void ejecutar_mov_out(char* reg_datos, char* reg_direccion);
void escribir_en_dir_fisica_los_bytes(uint32_t dir_fisica, uint32_t bytes, uint32_t valor_a_escribir);
void ejecutar_copy_string(int tamanio);
void escribir_y_guardar_en_dos_paginas(uint32_t dir_logica, uint32_t* valor);

// mmu
int obtener_numero_pagina(int direccion_logica);
int obtener_desplazamiento_pagina(int direccion_logica);
uint32_t calcular_direccion_fisica(int direccion_logica, t_cde* cde);

// tlb
void colocar_pagina_en_tlb(uint32_t pid, uint32_t nro_pagina, uint32_t marco);
void desalojar_y_agregar(t_pagina_tlb* nueva_pagina);
void* mayor_tiempo_de_ultimo_acceso(void* A, void* B);
int obtenerTiempoEnMiliSegundos(char* tiempo);
bool se_encuentra_en_tlb(uint32_t dir_logica, uint32_t* dir_fisica);

// --------------------------------

void devolver_cde_a_kernel();
void inicializar_registros();
void ejecutar_proceso();
void cargar_registros();
void guardar_registros();
uint32_t buscar_valor_registro(char*);
void imprimir_instruccion(t_instruccion*);


void desalojar_cde(t_instruccion*);
char* obtener_nombre_instruccion(t_instruccion* );
char* obtener_nombre_motivo_desalojo(cod_desalojo);

#endif