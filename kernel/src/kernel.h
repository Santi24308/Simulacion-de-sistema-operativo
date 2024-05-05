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
// CHECKPOINT 2
void inicializarListas();
void inicializarSemaforos();
t_pcb* crear_pcb(char* path, int quantum);
void destruir_pcb(t_pcb* pcb);
t_pcb* encontrar_pcb_por_pid(uint32_t pid, int* encontrado);
void retirar_pcb_de_su_respectivo_estado(uint32_t pid, int* resultado);
void finalizar_pcb(t_pcb* pcb_a_finalizar, char* razon);
void iniciar_proceso(char* path, char* size, int quantum);
void terminar_proceso();
void iniciarPlanificacion();
void detenerPlanificacion();
void iniciar_quantum();
void controlar_tiempo_de_ejecucion();
void listar_procesos_por_estado();
void enviar_cde_a_cpu();
void ejecutar_comando_unico(char*, char**);
void leer_y_ejecutar(char*);

void agregar_pcb_a(t_queue* cola, t_pcb* pcb_a_agregar, pthread_mutex_t* mutex);
t_pcb* retirar_pcb_de(t_queue* cola, pthread_mutex_t* mutex);
char* obtener_elementos_cargados_en(t_queue* cola);
char* obtener_nombre_estado(t_estados estado);
t_recurso* inicializar_recurso(char* nombre_recu, int instancias_tot);

// PLANIFICACION -------------------------------------------------------------------------------------------------------------------

// TRANSICIONES

void enviar_de_new_a_ready();
void enviar_de_ready_a_exec();
void enviar_de_exec_a_ready();
void enviar_de_exec_a_block();
void enviar_pcb_de_block_a_ready(t_pcb*);

void enviar_de_exec_a_finalizado();
char* obtener_nombre_estado(t_estados);
char* obtener_nombre_motivo(cod_desalojo);

// FIN TRANSICIONES 

// PLANIFICACION 

t_pcb* retirar_pcb_de_ready_segun_algoritmo();
t_pcb* elegir_segun_fifo();
t_pcb* elegir_segun_rr();
t_pcb* elegir_segun_vrr();

int esta_proceso_en_cola_bloqueados(t_pcb*);
char* obtener_elementos_cargados_en(t_queue*);

void enviar_cde_a_cpu();
void evaluar_instruccion(t_instruccion);
void recibir_cde_de_cpu();
// FIN PLANIFICACION

#endif