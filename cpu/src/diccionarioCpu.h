#ifndef SRC_DICCIONARIOCPU_H_
#define SRC_DICCIONARIOCPU_H_


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


char* puerto_escucha_dispatch; 
char* puerto_escucha_interrupt; 
int socket_servidor_dispatch;
int socket_servidor_interrupt;
char* config_path;
char* puerto_escucha; 
int socket_servidor;
int socket_kernel_dispatch;
int socket_kernel_interrupt;
int socket_memoria;
char* ip;
char* puerto_mem;
t_log* logger_cpu;
t_config* config_cpu;
pthread_t hilo_kernel_dispatch;
pthread_t hilo_kernel_interrupt;
pthread_t hilo_memoria;
t_registro* registros_cpu;
pthread_t mutex_cde_ejecutando;
pthread_t mutex_instruccion_actualizada;
pthread_t mutex_realizar_desalojo;
sem_t sema_kernel_dispatch;
sem_t sema_kernel_interrupt;
sem_t sema_memoria;
sem_t sema_ejecucion;


//Instruccion
codigoInstruccion instruccion_actualizada;

uint32_t pid_de_cde_ejecutando;

int interrupcion;
int realizar_desalojo;

#endif