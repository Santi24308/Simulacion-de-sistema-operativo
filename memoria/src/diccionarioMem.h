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

bool sistema_funcionando;

typedef struct{
	uint32_t pid;	
	t_list* lista_instrucciones;
	t_list* tabla_de_paginas;
}t_proceso;

typedef struct{
    int nro_pagina;
    int pid;
    int marco; 
}t_pagina;

typedef struct{
  int bit_uso; // bit de uso 
  t_pagina* paginaAsociada; // Puntero a la página asignada al marco (si está ocupado) y NULL si no esta asociado 
} t_marco;

typedef struct{
	char* id;
	char* tipo;
	int socket;
	pthread_t hilo_io;
}t_interfaz;

void* memoria_fisica;

char* config_path;
char* instrucciones_path;
char* puerto_escucha; 
char* retardo_respuesta;
int socket_servidor;
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

t_list* interfacesIO;

pthread_mutex_t mutex_lista_procesos;
pthread_mutex_t mutex_lista_tablas;
pthread_t hilo_cpu;
pthread_t hilo_io;
pthread_t hilo_esperar_IOs;
pthread_t hilo_kernel;

sem_t terminar_memoria;

t_buffer* buffer_instruccion;
#endif
