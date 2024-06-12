#ifndef SRC_CONEXIONES_H_
#define SRC_CONEXIONES_H_

// SERVER
#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<unistd.h>
#include<netdb.h>
#include<commons/log.h>
#include<commons/collections/list.h>
#include<string.h>
#include<readline/readline.h>
#include<signal.h>
#include<stdarg.h>
#include<utils/serializacion.h>
#include<utils/instrucciones.h>
#include <commons/collections/queue.h>

// AGREGAR LOS PROTOCOLOS

// prototipo de protocolo entre memoria e IO
typedef enum{
	GUARDAR_EN_DIRECCION,
	LEER_DIRECCION,
	IO_STDIN_ESCRIBIR,
	IO_STDOUT_LEER
}mensajeIOMemoria;

typedef enum{
	PEDIDO_INSTRUCCION,
	PEDIDO_OK,
	NUMERO_MARCO_SOLICITUD,
	MOV_IN_SOLICITUD,
	MOV_OUT_SOLICITUD,
	ERROR_OUT_OF_MEMORY
}mensajeCpuMem;

typedef enum{
    EJECUTAR_PROCESO,
	ALGORITMO_PLANIFICACION,
	INTERRUPT,
	DESALOJO
} mensajeKernelCpu;

typedef enum
{
	INICIAR_PROCESO_SOLICITUD,
	INICIAR_PROCESO_OK,
	INICIAR_PROCESO_ERROR,
	FINALIZAR_PROCESO_SOLICITUD,
	FINALIZAR_PROCESO_OK
} mensajeMemoriaKernel;

typedef enum {
	LIBRE,
	DESCONEXION
}mensajeIOKernel;

//CONTEXTO DE EJECUCION CREADO PARA CONEXIONES CON KERNEL
typedef struct{
	uint32_t pid;
	uint32_t pc;
	t_registro* registros;
}t_contexto;


int crear_conexion(char* ip, char* puerto);
int iniciar_servidor(char*, t_log*);
int esperar_cliente(int, t_log*);
void terminar_conexiones(int num_sockets, ...);

/* BORRAR
// PROCESOS KERNEL
typedef enum
{
	CREAR_PROCESO,
	FINALIZAR_PROCESO
}op_kernel;

typedef struct //COMPLETAR
{
    int id;
    
}t_pagina;

typedef struct //COMPLETAR
{
    uint32_t pid;
    t_pagina* paginas;
}t_proceso_memo;


*/

#endif
