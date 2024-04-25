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

// AGREGAR LOS PROTOCOLOS

typedef enum{
	PEDIDO_INSTRUCCION
}mensajeCpuMem;

typedef struct {
    EJECUTAR_PROCESO
} mensajeKernelCpu;


int crear_conexion(char* ip, char* puerto);
int iniciar_servidor(char*, t_log*);
int esperar_cliente(int, t_log*);
void terminar_conexiones(int num_sockets, ...);

// PROCESOS KERNEL
typedef enum
{
	CREAR_PROCESO,
	FINALIZAR_PROCESO
}op_kernel;

typedef struct //COMPLETAR
{
    int pid;
    t_pagina* paginas;
}t_proceso_memo;

typedef struct //COMPLETAR
{
    int id;
    
}t_pagina;



#endif