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

int block_size;
int block_size;

//-------------------------------------------------
//ESTRUCTURA DE LOS ARCHIVOS DE DIAL FS

typedef struct {    
    t_metadata archivoMetadata;
}t_archivo;

typedef struct {
    void* bloque; 
}t_bloque;


/*
typedef struct{  

    //t_list archivos_en_fs;
}t_fileSystem;

/*
indice del array de bits
bloques | bitmap  | archivo
0       |1        | A
1       |1        | A (segun el tamaño de A)
2       |1        | B
3       |0        | 
4       |1        | C
*/
//-------------------------------------------------


t_log* logger_io;
t_config* config_io;
pthread_t hilo_kernel;

sem_t sema_memoria;
sem_t sema_kernel;
sem_t terminar_io;

#endif
