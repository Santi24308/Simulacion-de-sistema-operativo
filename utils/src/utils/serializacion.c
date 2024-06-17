#include "serializacion.h"

// BUFFER

t_buffer* crear_buffer(){
	t_buffer* b = calloc(1, sizeof(t_buffer));
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

	// si recv no puede recibir nada, quiere decir que se desconecto el socket y retorna -1
	if (recv(socket, &(buffer -> size), sizeof(uint32_t), MSG_WAITALL) == -1){
		destruir_buffer(buffer);
		return NULL;   // chequear si fue NULL va a ser un buen flag para saber que se desconecto el socket
	}
	if(buffer->size != 0){
		buffer -> stream = calloc(1, buffer -> size);

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

	if (recv(socket_emisor, &codigo, sizeof(uint8_t), MSG_WAITALL) == -1){
		return UINT8_MAX;   // chequear si fue UINT8_MAX va a ser un buen flag para saber que se desconecto el socket
	}
	return codigo;
}


// UINT32
void buffer_write_uint32(t_buffer* buffer, uint32_t entero){
	buffer->stream = realloc(buffer->stream, buffer->size + sizeof(uint32_t));

	memcpy(buffer->stream + buffer->size, &entero, sizeof(uint32_t));
	buffer->size += sizeof(uint32_t);
}

uint32_t buffer_read_uint32(t_buffer* buffer){
	uint32_t entero;

	memcpy(&entero, buffer->stream + buffer->offset, sizeof(uint32_t));
	buffer->offset += sizeof(uint32_t);

	return entero;
}

// UINT8
void buffer_write_uint8(t_buffer* buffer, uint8_t entero){
	buffer->stream = realloc(buffer->stream, buffer->size + sizeof(uint8_t));

	memcpy(buffer->stream + buffer->size, &entero, sizeof(uint8_t));
	buffer->size += sizeof(uint8_t);
}

uint8_t buffer_read_uint8(t_buffer* buffer){
	uint8_t entero;

	memcpy(&entero, buffer->stream + buffer->offset, sizeof(uint8_t));
	buffer->offset += sizeof(uint8_t);

	return entero;
}

// STRING

void buffer_write_string(t_buffer* buffer, char* cadena){

	// aca hay que tener en cuenta que tenemos que calcular el tamaño
	// de la cadena pero considerando que strlen no cuenta el \0 por lo que
	// tenemos que sumarle 1 obligatoriamente para que no haya problemas en 
	// en reservado de memoria ya que si bien el \0 no cuenta como tamaño de palabra
	// si cuenta como espacio fisico, 1 byte.
	
	uint32_t tam = strlen(cadena) + 1;

	buffer_write_uint32(buffer,tam);

	buffer->stream = realloc(buffer->stream, buffer->size + tam);

	memcpy(buffer->stream + buffer->size, cadena , tam);
	buffer->size += tam;
}

char* buffer_read_string(t_buffer* buffer){
	uint32_t tam = buffer_read_uint32(buffer);
	// si la cadena era [h,o,l,a,\0] ya contemplamos que el tamaño real fisico es 5
	// en buffer_write_string
	char* cadena = malloc(tam);

	// memcpy va a copiar el \0 ya que lo esta contemplando porque en la escritura lo
	// agregamos
	memcpy(cadena, buffer->stream + buffer->offset, tam);
	buffer->offset += tam;

	return cadena;
}

// CONTEXTO DE EJECUCION
void buffer_write_cde(t_buffer* buffer, t_cde* cde_recibido){
	buffer->stream = realloc(buffer->stream, buffer->size + sizeof(t_cde));
	memcpy(buffer->stream + buffer->size, cde_recibido, sizeof(t_cde));
	buffer->size += sizeof(t_cde);

	buffer->stream = realloc(buffer->stream, buffer->size + sizeof(t_registro));
	memcpy(buffer->stream + buffer->size, cde_recibido->registros, sizeof(t_registro));
	buffer->size += sizeof(t_registro);

	buffer->stream = realloc(buffer->stream, buffer->size + sizeof(t_instruccion));
	memcpy(buffer->stream + buffer->size, cde_recibido->ultima_instruccion, sizeof(t_instruccion));
	buffer->size += sizeof(t_instruccion);

	// copiamos cada parametro de la instruccion ya que la copia de estructura solo copia
	// punteros pero no su contenido
	if (cde_recibido->ultima_instruccion->parametro1)
		buffer_write_string(buffer, cde_recibido->ultima_instruccion->parametro1);
	if (cde_recibido->ultima_instruccion->parametro2)
		buffer_write_string(buffer, cde_recibido->ultima_instruccion->parametro2);
	if (cde_recibido->ultima_instruccion->parametro3)
		buffer_write_string(buffer, cde_recibido->ultima_instruccion->parametro3);
	if (cde_recibido->ultima_instruccion->parametro4)
		buffer_write_string(buffer, cde_recibido->ultima_instruccion->parametro4);
	if (cde_recibido->ultima_instruccion->parametro5)
		buffer_write_string(buffer, cde_recibido->ultima_instruccion->parametro5);
	// en la lectura se va a consultar si existe el puntero de la estructura, entonces
	// si existe el puntero (!= NULL) quiere decir que se encuentra escrito en el buffer
	// de manera ordenada de 1 a 5
}

t_cde* buffer_read_cde(t_buffer* buffer){
	t_cde* cde = calloc(1, sizeof(t_cde));
	t_registro* registros_creados = calloc(1, sizeof(t_registro));
	t_instruccion* ultima_instruccion_creada = calloc(1, sizeof(t_instruccion));
	
	memcpy(cde, buffer->stream + buffer->offset, sizeof(t_cde));
	buffer->offset += sizeof(t_cde);

	memcpy(registros_creados, buffer->stream + buffer->offset, sizeof(t_registro));
	buffer->offset += sizeof(t_registro);

	memcpy(ultima_instruccion_creada, buffer->stream + buffer->offset, sizeof(t_instruccion));
	buffer->offset += sizeof(t_instruccion);

	// hasta aca solo lei punteros, tengo que restaurar sus datos
	// t_registro no tiene punteros por lo que no hay nada que restaurar
	// el objetivo es t_instruccion

	// leemos cada parametro de la instruccion si es que existe apoyandonos en los punteros
	// que copiamos al copiar TODA la estructura
	if (ultima_instruccion_creada->parametro1)
		ultima_instruccion_creada->parametro1 = buffer_read_string(buffer);
	if (ultima_instruccion_creada->parametro2)
		ultima_instruccion_creada->parametro2 = buffer_read_string(buffer);
	if (ultima_instruccion_creada->parametro3)
		ultima_instruccion_creada->parametro3 = buffer_read_string(buffer);
	if (ultima_instruccion_creada->parametro4)
		ultima_instruccion_creada->parametro4 = buffer_read_string(buffer);
	if (ultima_instruccion_creada->parametro5)
		ultima_instruccion_creada->parametro5 = buffer_read_string(buffer);

	// por ultimo corregimos los punteros del cde
	cde->registros = registros_creados;
	cde->ultima_instruccion = ultima_instruccion_creada;

	return cde;
}

void destruir_cde(t_cde* cde){
    free(cde->registros);
	destruir_instruccion(cde->ultima_instruccion);
    free(cde);
}

// INSTRUCCION
void buffer_write_instruccion(t_buffer* buffer, t_instruccion* instruccion){
	buffer_write_uint8(buffer, instruccion->codigo);
	if (instruccion->parametro1 != NULL)
		buffer_write_string(buffer, instruccion->parametro1);

	if (instruccion->parametro2 != NULL)
		buffer_write_string(buffer, instruccion->parametro2);

	if (instruccion->parametro3 != NULL)
		buffer_write_string(buffer, instruccion->parametro3);

	if (instruccion->parametro4 != NULL)
		buffer_write_string(buffer, instruccion->parametro4);

	if (instruccion->parametro5 != NULL)
		buffer_write_string(buffer, instruccion->parametro5);
}

t_instruccion* buffer_read_instruccion(t_buffer* buffer){
	t_instruccion* instr = malloc(sizeof(t_instruccion));
	instr->parametro1 = NULL;
	instr->parametro2 = NULL;
	instr->parametro3 = NULL;
	instr->parametro4 = NULL;
	instr->parametro5 = NULL;

	instr->codigo = buffer_read_uint8(buffer);
	
	int cantidad_parametros = cantidad_parametros_instruccion(instr->codigo);

	if(cantidad_parametros == 0)
		return instr;
	
	if(cantidad_parametros >= 1)
		instr->parametro1 = buffer_read_string(buffer);
	
	if(cantidad_parametros >= 2)
		instr->parametro2 = buffer_read_string(buffer);
	
	if(cantidad_parametros >= 3)
		instr->parametro3 = buffer_read_string(buffer);

	if(cantidad_parametros >= 4)
		instr->parametro4 = buffer_read_string(buffer);

	if(cantidad_parametros == 5)
		instr->parametro5 = buffer_read_string(buffer);

	return instr;
}

void destruir_instruccion(t_instruccion* instruccion){
	if (instruccion->parametro1) 
		free(instruccion->parametro1);
	if (instruccion->parametro2)
		free(instruccion->parametro2);
	if (instruccion->parametro3)
		free(instruccion->parametro3);
	if (instruccion->parametro4)
		free(instruccion->parametro4);
	if (instruccion->parametro5)
		free(instruccion->parametro5);
	free(instruccion);
}