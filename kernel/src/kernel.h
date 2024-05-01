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

void agregar_pcb_a(t_queue* cola, t_pcb* pcb_a_agregar, pthread_mutex_t* mutex);
t_pcb* retirar_pcb_de(t_queue* cola, pthread_mutex_t* mutex);
char* obtener_elementos_cargados_en(t_queue* cola);
char* obtener_nombre_estado(t_estados estado);
t_recurso* inicializar_recurso(char* nombre_recu, int instancias_tot);


#endif