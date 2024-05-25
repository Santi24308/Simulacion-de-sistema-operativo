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


int socket_servidor_dispatch;
int socket_servidor_interrupt;
int socket_servidor;
int socket_kernel_dispatch;
int socket_kernel_interrupt;
int socket_memoria;

char* puerto_escucha_dispatch; 
char* puerto_escucha_interrupt; 
char* config_path;
char* puerto_escucha; 
char* ip;
char* puerto_mem;


t_log* logger_cpu;
t_config* config_cpu;

pthread_t hilo_kernel_dispatch;
pthread_t hilo_kernel_interrupt;
pthread_t hilo_memoria;

t_registro* registros_cpu;

pthread_mutex_t mutex_cde_ejecutando;
pthread_mutex_t mutex_interrupcion;
pthread_mutex_t mutex_realizar_desalojo;
pthread_mutex_t mutex_instruccion_actualizada;


//sem_t sema_kernel_dispatch;
//sem_t sema_kernel_interrupt;
// la idea ahora va a ser que cada modulo tenga un unico semaforo 
// que le impida terminar a menos que - pase algo que lo obligue -
// en el caso de cpu va a terminar si alguna de las conexiones
// con kernel termina, ya que no tiene sentido seguir
sem_t terminar_cpu;
sem_t sema_memoria;
sem_t sema_ejecucion;


//Instruccion
codigoInstruccion instruccion_actualizada;

uint32_t pid_de_cde_ejecutando;
uint32_t algoritmo_planificacion;

int interrupcion;
int realizar_desalojo;
int interrupcion_consola ;

#endif
