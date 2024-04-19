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

char* puerto_escucha; 
int socket_servidor;
int socket_io;
int socket_cpu;
int socket_kernel;
t_log* logger_memoria;
t_config* config_memoria;
pthread_t hilo_cpu;
pthread_t hilo_io;
pthread_t hilo_kernel;

sem_t sema_kernel;
sem_t sema_cpu;
sem_t sema_io;

#endif