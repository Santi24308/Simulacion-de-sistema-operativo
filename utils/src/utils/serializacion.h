#ifndef SRC_SERIALIZACION_H_
#define SRC_SERIALIZACION_H_

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h> // Para uintX_t
#include <string.h>
#include<sys/socket.h>

typedef struct{
	uint32_t size;
	uint32_t offset;
	void* stream;
} t_buffer;

typedef struct{
	uint32_t AX;
	uint32_t BX;
	uint32_t CX;
	uint32_t DX;
	uint32_t EAX;
	uint32_t EBX;
	uint32_t ECX;
	uint32_t EDX;
	uint32_t SI;
	uint32_t DI;
}t_registro;

// CONTEXTO DE EJECUCION
typedef struct{
	uint32_t pid;
	uint32_t pc;
	t_registro* registros;
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
char* buffer_read_string(t_buffer* buffer, uint32_t* tam);

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
