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

// SERVER
typedef enum
{
	MENSAJE,
	PAQUETE,
	APAGAR_SISTEMA
}op_code;

//CLIENT
typedef struct
{
	int size;
	void* stream;
} t_buffer;

typedef struct
{
	op_code codigo_operacion;
	t_buffer* buffer;
} t_paquete;

//SERVER
void* recibir_buffer(int*, int);

int iniciar_servidor(char*, t_log*);
int esperar_cliente(int, t_log*);
t_list* recibir_paquete(int);
void recibir_mensaje(int, t_log*);
int recibir_operacion(int);

//CLIENT
int crear_conexion(char* ip, char* puerto);
void enviar_mensaje(char* mensaje, int socket_cliente);
t_paquete* crear_paquete(void);
void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio);
void enviar_paquete(t_paquete* paquete, int socket_cliente);
void eliminar_paquete(t_paquete* paquete);

void terminar_conexiones(int num_sockets, ...);

void enviar_codigo(int socket_receptor, int codigo);
int recibir_codigo(int socket_emisor);

#endif