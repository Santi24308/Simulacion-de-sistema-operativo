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
t_pcb* crear_pcb(char* path, int quantum);
void destruir_pcb(t_pcb* pcb);
void iniciarProceso(char* path, char* size, int quantum);
void terminar_proceso();
void iniciarPlanificacion();
void detenerPlanificacion();
void listar_procesos_por_estado();
void enviar_contexto_a_cpu();

void agregar_pcb_a(t_queue* cola, t_pcb* pcb_a_agregar, pthread_mutex_t* mutex);
t_pcb* retirar_pcb_de(t_queue* cola, pthread_mutex_t* mutex);
char* obtener_elementos_cargados_en(t_queue* cola);
char* obtener_nombre_estado(t_estados estado);


#endif