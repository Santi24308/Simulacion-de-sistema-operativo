#ifndef SRC_DICCIONARIOIO_H_
#define SRC_DICCIONARIOIO_H_


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

char* tipo;
char* nombreIO;
char* config_path;
char* ip;
char* puerto_mem;
char* puerto_kernel;
int socket_memoria;
int socket_kernel;
int id_interfaz;  // necesario para que kernel pueda guiarse despues
t_log* logger_io;
t_config* config_io;
pthread_t hilo_kernel;

sem_t sema_memoria;
sem_t sema_kernel;
sem_t terminar_io;

#endif