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
	uint32_t tam = string_length(cadena);

	buffer_write_uint32(buffer,tam);

	buffer->stream = realloc(buffer->stream, buffer->size + tam);

	memcpy(buffer->stream + buffer->size, cadena , tam);
	buffer->size += tam;
}

char* buffer_read_string(t_buffer* buffer){
	uint32_t tam = buffer_read_uint32(buffer);
	char* cadena = malloc(tam + 1);

	memcpy(cadena, buffer->stream + buffer->offset, tam);
	buffer->offset += tam;

	cadena[tam] = '\0';

	return cadena;
}

// CONTEXTO DE EJECUCION
void buffer_write_registros(t_buffer* buffer, t_registro* registros){
	buffer_write_uint8(buffer, registros->AX);
	buffer_write_uint8(buffer, registros->BX);
	buffer_write_uint8(buffer, registros->CX);
	buffer_write_uint8(buffer, registros->DX);
	buffer_write_uint32(buffer, registros->EAX);
	buffer_write_uint32(buffer, registros->EBX);
	buffer_write_uint32(buffer, registros->ECX);
	buffer_write_uint32(buffer, registros->EDX);
	buffer_write_uint32(buffer, registros->PC);
	buffer_write_uint32(buffer, registros->DI);
	buffer_write_uint32(buffer, registros->SI);
}

t_registro* buffer_read_registros(t_buffer* buffer){
	t_registro* registro = malloc(sizeof(t_registro));

	registro->AX =buffer_read_uint8(buffer);
	registro->BX = buffer_read_uint8(buffer);
	registro->CX = buffer_read_uint8(buffer);
	registro->DX = buffer_read_uint8(buffer);
	registro->EAX = buffer_read_uint32(buffer);
	registro->EBX = buffer_read_uint32(buffer);
	registro->ECX = buffer_read_uint32(buffer);
	registro->EDX = buffer_read_uint32(buffer);
	registro->PC = buffer_read_uint32(buffer);
	registro->DI = buffer_read_uint32(buffer);
	registro->SI = buffer_read_uint32(buffer);

	return registro;
}

void buffer_write_cde(t_buffer* buffer, t_cde* cde_recibido){
	buffer_write_uint32(buffer, cde_recibido->pid);
	buffer_write_uint32(buffer, cde_recibido->motivo_desalojo);
	buffer_write_uint32(buffer, cde_recibido->motivo_finalizacion);

	buffer_write_registros(buffer, cde_recibido->registros);

	buffer_write_uint8(buffer, cde_recibido->ultima_instruccion->codigo);

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
	t_instruccion* ultima_instruccion_creada = calloc(1, sizeof(t_instruccion));
	
	cde->pid = buffer_read_uint32(buffer);
	cde->motivo_desalojo = buffer_read_uint32(buffer);
	cde->motivo_finalizacion = buffer_read_uint32(buffer);

	cde->registros = buffer_read_registros(buffer);

	ultima_instruccion_creada->codigo = buffer_read_uint8(buffer);

	int cantidad_parametros = cantidad_parametros_instruccion(ultima_instruccion_creada->codigo);
	// hasta aca solo lei punteros, tengo que restaurar sus datos
	// t_registro no tiene punteros por lo que no hay nada que restaurar
	// el objetivo es t_instruccion

	// leemos cada parametro de la instruccion si es que existe apoyandonos en los punteros
	// que copiamos al copiar TODA la estructura

	if(cantidad_parametros >= 1)
		ultima_instruccion_creada->parametro1 = buffer_read_string(buffer);
	
	if(cantidad_parametros >= 2)
		ultima_instruccion_creada->parametro2 = buffer_read_string(buffer);
	
	if(cantidad_parametros >= 3)
		ultima_instruccion_creada->parametro3 = buffer_read_string(buffer);

	if(cantidad_parametros >= 4)
		ultima_instruccion_creada->parametro4 = buffer_read_string(buffer);

	if(cantidad_parametros == 5)
		ultima_instruccion_creada->parametro5 = buffer_read_string(buffer);

	// por ultimo corregimos los punteros del cde
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

