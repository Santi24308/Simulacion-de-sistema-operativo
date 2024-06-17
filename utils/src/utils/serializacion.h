#ifndef SRC_SERIALIZACION_H_
#define SRC_SERIALIZACION_H_

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h> // Para uintX_t
#include <string.h>
#include <sys/socket.h>
#include <utils/instrucciones.h>

typedef struct{
	uint32_t size;
	uint32_t offset;
	void* stream;
} t_buffer;

typedef struct{
	uint32_t PC;
	uint8_t AX;
	uint8_t BX;
	uint8_t CX;
	uint8_t DX;
	uint32_t EAX;
	uint32_t EBX;
	uint32_t ECX;
	uint32_t EDX;
	uint32_t SI;
	uint32_t DI;
}t_registro;

/*
Check Interrupt
En este momento, se deberá chequear si el Kernel nos envió una interrupción al PID que se está ejecutando,
 en caso afirmativo, se devuelve el Contexto de Ejecución actualizado al Kernel con motivo de la interrupción.
 Caso contrario, se descarta la interrupción.
*/
typedef enum{
	FINALIZACION_EXIT,
	FINALIZACION_ERROR,
	LLAMADA_IO,
	INTERRUPCION,
	FIN_DE_QUANTUM,
	RECURSOS,
	OUT_OF_MEMORY_ERROR,
	NO_DESALOJADO
}cod_desalojo;

typedef enum{
	SUCCESS,
	INVALID_RESOURCE,
	INVALID_INTERFACE,
	OUT_OF_MEMORY,
	INTERRUMPED_BY_USER,
	NO_FINALIZADO
}cod_finalizacion;

// CONTEXTO DE EJECUCION
// la colocamos en serializacion porque necesitamos que tanto CPU como Kernel entiendan el TDA

// con motivo se refiere a si el cde vuelve por exit, llamada a I/O, desalojo por quantum, interrupcion
// tener en cuenta que cuando haya llamada a I/O vamos a tener que cargar los datos que necesite I/O para 
// justamente atender esa llamada

// siempre la idea va a ser que cuando se desaloje se setee el motivo y se guarde la ultima instruccion
typedef struct{
	uint32_t pid;
	t_registro* registros;
	cod_desalojo motivo_desalojo;
	cod_finalizacion motivo_finalizacion;
	t_instruccion* ultima_instruccion; // se usa mas que nada para los casos en donde I/O necesite data  
}t_cde;

t_buffer* crear_buffer();
void destruir_buffer(t_buffer* buffer);

void enviar_buffer(t_buffer* buffer, int socket);
t_buffer* recibir_buffer(int socket);

void enviar_codigo(int socket_receptor, uint8_t codigo);
uint8_t recibir_codigo(int socket_emisor);

// FUNCIONES PARA CARGAR BUFFER
// UINT32
void buffer_write_uint32(t_buffer* buffer, uint32_t entero);
uint32_t buffer_read_uint32(t_buffer* buffer);

// UINT8
void buffer_write_uint8(t_buffer* buffer, uint8_t entero);
uint8_t buffer_read_uint8(t_buffer *buffer);

// STRING
void buffer_write_string(t_buffer* buffer, char* string_a_escribir);
char* buffer_read_string(t_buffer* buffer);

//REGISTROS
void buffer_write_registros(t_buffer* buffer, t_registro* cde);
t_registro* buffer_read_registros(t_buffer* buffer);

// CDE
void buffer_write_cde(t_buffer* buffer, t_cde* cde);
t_cde* buffer_read_cde(t_buffer* buffer);
void destruir_cde(t_cde* cde);

// INSTRUCCIONES
t_instruccion* buffer_read_instruccion(t_buffer* buffer);
void buffer_write_instruccion(t_buffer* buffer, t_instruccion* instruccion);
void destruir_instruccion(t_instruccion* instruccion);

#endif /* SRC_UTILS_SERIALIZACION_H_ */
