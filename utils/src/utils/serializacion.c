#include <utils/serializacion.h>

// BUFFER

t_buffer* crear_buffer(){
	t_buffer* b = malloc(sizeof(t_buffer));
	b->size = 0;
	b->stream = NULL;
	b->offset = 0;
	return b;
}

void destruir_buffer(t_buffer* buffer){
	free(buffer->stream);
	free(buffer);
	
	return;
}

void enviar_buffer(t_buffer* buffer, int socket){
    // Enviamos el tamanio del buffer
    send(socket, &(buffer->size), sizeof(uint32_t), 0);

	if (buffer->size != 0){
    	// Enviamos el stream del buffer
    	send(socket, buffer->stream, buffer->size, 0);
	}
}

t_buffer* recibir_buffer(int socket){

	t_buffer* buffer = crear_buffer();

	// Recibo el tamanio del buffer y reservo espacio en memoria
	recv(socket, &(buffer -> size), sizeof(uint32_t), MSG_WAITALL);

	if(buffer->size != 0){
		buffer -> stream = malloc(buffer -> size);

		// Recibo stream del buffer
		recv(socket, buffer -> stream, buffer -> size, MSG_WAITALL);
	}
	return buffer;
}

// Sirve para enviar cualquier enum ya que toma uint8_t, que seria el "nativo" de los enum
void enviar_codigo(int socket_receptor, uint8_t codigo){
	send(socket_receptor, &codigo, sizeof(uint8_t), 0);
}


// Sirve para recibir cualquier enum ya que toma uint8_t, que seria el "nativo" de los enum
uint8_t recibir_codigo(int socket_emisor){
	uint8_t codigo;

	recv(socket_emisor, &codigo, sizeof(uint8_t), MSG_WAITALL);

	return codigo;
}
