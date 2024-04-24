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

t_buffer* crear_buffer();
void destruir_buffer(t_buffer* buffer);

void enviar_buffer(t_buffer* buffer, int socket);
t_buffer* recibir_buffer(int socket);

void enviar_codigo(int socket_receptor, uint8_t codigo);
uint8_t recibir_codigo(int socket_emisor);

#endif /* SRC_UTILS_SERIALIZACION_H_ */
