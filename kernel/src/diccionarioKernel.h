#ifndef SRC_DICCIONARIOKERNEL_H_
#define SRC_DICCIONARIOKERNEL_H_

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <commons/collections/list.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/temporal.h>
#include <commons/string.h>
#include <pthread.h>
#include <semaphore.h>
#include <commons/collections/queue.h>
#include <utils/serializacion.h>

// Diagrama de 5 estados para la planificacion de recursos
typedef enum{
	ESTADO_NULO, // le agrego el ESTADO porque NULO ya estaba definido en instrucciones
	NEW, 
	READY,
	EXEC,
	BLOCKED, // aca dice algo de multiples colas con i/o
	TERMINADO
}t_estados;

//Estructura PCB
typedef struct{
	t_cde* cde; // contiene el pc, pid y registros
	t_estados estado; 
	char* path;
    int quantum;
	bool flag_clock;
	bool fin_q;
}t_pcb;

typedef struct{
    char* nombre;
    int instancias;
	t_list* procesos_bloqueados;
    sem_t sem_recurso;
}t_recurso;


char* config_path;
char* puerto_escucha;
char* puerto_cpu_dispatch;
char* puerto_cpu_interrupt; 
int socket_servidor;
int socket_memoria;
int socket_io;
int socket_cpu_dispatch;
int socket_cpu_interrupt;
char* ip;
char* puerto_mem;
char* algoritmo;
int pid_a_asignar;
int quantum_a_asignar;
int planificacion_detenida;
t_log* logger_kernel;
t_config* config_kernel;
t_pcb* pcb_en_ejecucion;
pthread_t hilo_io;
pthread_t hilo_consola;
pthread_t hilo_memoria;

int quantum;
t_list* recursos; // lista de t_recurso*
int grado_max_multiprogramacion;

// LISTAS Y COLAS
t_list* procesos_globales;

t_queue* procesosNew;
t_queue* procesosReady;
t_queue* procesosReadyPlus;
t_queue* procesosBloqueados;
t_queue* procesosFinalizados;

// SEMAFOROS
//sem_t sema_memoria;
//sem_t sema_io;
//sem_t sema_consola;
// la idea ahora va a ser que cada modulo tenga un unico semaforo 
// que le impida terminar a menos que - pase algo que lo obligue -
// como por ejemplo en kernel: que se indique por consola 
sem_t terminar_kernel;
sem_t procesos_en_exec;
sem_t cde_recibido;
sem_t cpu_libre;
// semaforos de procesos y estados
pthread_mutex_t mutex_procesos_globales;
pthread_mutex_t mutex_new;
pthread_mutex_t mutex_ready;
pthread_mutex_t mutex_readyPlus;
pthread_mutex_t mutex_block;
pthread_mutex_t mutex_finalizados;
pthread_mutex_t mutex_exec;

pthread_mutex_t mutex_pcb_en_ejecucion;


sem_t procesos_en_new;
sem_t procesos_en_ready;
sem_t procesos_en_blocked;
sem_t procesos_en_exit;


sem_t sem_iniciar_quantum;
sem_t sem_reloj_destruido;
sem_t no_end_kernel;
sem_t grado_de_multiprogramacion;
sem_t bin_recibir_cde;
sem_t sem_liberar_archivos;


sem_t pausar_new_a_ready;
sem_t pausar_ready_a_exec;
sem_t pausar_exec_a_finalizado;
sem_t pausar_exec_a_ready;
sem_t pausar_exec_a_blocked;
sem_t pausar_blocked_a_ready;
sem_t bin_recibir_cde;


#endif