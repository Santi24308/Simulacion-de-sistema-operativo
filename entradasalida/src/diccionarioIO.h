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
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

t_list* lista_global_archivos_abiertos;

typedef struct {  // no hace falta que tenga el FILE ya que no los vamos a mantener abiertos
	char* nombre_archivo; // se usa de ID
    t_config* metadata;
}archivo_t;

// se usan globales y se crean una unica vez, cuando arranca el modulo
// ya que segun la info, msync, se encarga de mantener actualizado el mapeo 
t_bitarray* bitmap;
void* bloquesmap; 
void* bloquesmap_paralelo;  // este se usa para cuando compactamos, se recrea el void* compactado y se escribe sobre el original con memcpy

int fd_bitarray;
int fd_bloques;

char* path_archivo_bloques;
size_t tamanio_archivo_bloques;
char* path_archivo_bitarray;
size_t tamanio_archivo_bitarray;

char* tipo;
char* nombreIO;
char* config_path;
char* ip;
char* puerto_mem;
char* puerto_kernel;
int socket_memoria;
int socket_kernel;

int block_count;
int block_size;
char* path_filesystem;

t_config* metadata;

t_log* logger_io;
t_config* config_io;
pthread_t hilo_kernel;

// declaracion de semaforos
sem_t terminar_io;

#endif
