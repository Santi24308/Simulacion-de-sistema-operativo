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

char* config_path;
char* puerto_escucha;
char* puerto_cpu; 
int socket_servidor;
int socket_memoria;
int socket_io;
int socket_cpu_dispatch;
int socket_cpu_interrupt;
char* ip;
char* puerto_mem;
t_log* logger_kernel;
t_config* config_kernel;
pthread_t hilo_io;
pthread_t hilo_consola;
pthread_t hilo_memoria;

sem_t sema_memoria;
sem_t sema_io;
sem_t sema_consola;

#endif