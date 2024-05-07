#ifndef SRC_DICCIONARIOMEM_H_
#define SRC_DICCIONARIOMEM_H_


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

typedef struct{
	uint32_t pid;
	t_list* instrucciones;
    uint32_t tamanio;
}t_proceso;

char* config_path;
char* instrucciones_path;
char* puerto_escucha; 
char* retardo_respuesta;
int socket_servidor;
int socket_io;
int socket_cpu;
int socket_kernel;
t_log* logger_memoria;
t_config* config_memoria;
t_list* listaGlobalProceso;

pthread_t mutex_lista_global_procesos;
pthread_t hilo_cpu;
pthread_t hilo_io;
pthread_t hilo_kernel;

sem_t sema_kernel;
sem_t sema_cpu;
sem_t sema_io;

t_buffer* buffer_instruccion;
#endif