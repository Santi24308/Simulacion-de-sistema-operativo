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
#include <math.h>
#include <alloca.h>

typedef struct{
	uint32_t pid;	
	t_list* lista_instrucciones;
	t_list* tabla_de_paginas;
}t_proceso;

typedef struct{
	void* espacioMemoria;
} t_memoria_fisica;

typedef struct{
    int pid;
    int marco_ppal;
    bool bitPresencia; // bit de presencia    
    bool bitModificado; // bit de modificado
    //int tiempo_uso;
    //int tamanioDisponible;
    //int fragInterna; // mepa q no
    char*  ultimaReferencia; //se puede usar un temporal 
}t_pagina;

typedef struct{
   int bit_uso; // bit de uso // Estado del marco (libre, ocupado)
  t_pagina* paginaAsociada; // Puntero a la página asignada al marco (si está ocupado) y NULL si no esta asociado 
} t_marco;


char* config_path;
char* instrucciones_path;
char* puerto_escucha; 
char* retardo_respuesta;
int socket_servidor;
int socket_io;
int socket_cpu;
int socket_kernel;
int cant_marcos_ppal;
int total_espacio_memoria;
int tamanio_paginas;
int cantidadDeMarcos;
t_log* logger_memoria;
t_config* config_memoria;
t_list* lista_procesos;
t_list* tabla_de_marcos;

pthread_mutex_t mutex_lista_procesos;
pthread_mutex_t mutexListaTablas;
pthread_mutex_t mutexBitmapMP;
pthread_t hilo_cpu;
pthread_t hilo_io;
pthread_t hilo_kernel;

sem_t terminar_memoria;

t_buffer* buffer_instruccion;
#endif
