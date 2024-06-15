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
	
	uint32_t tam = strlen(cadena) + 1;

	buffer_write_uint32(buffer,tam);

	buffer->stream = realloc(buffer->stream, buffer->size + tam);

	memcpy(buffer->stream + buffer->size, cadena , tam);
	buffer->size += tam;
}

char* buffer_read_string(t_buffer* buffer, uint32_t* tam){
	(*tam) = buffer_read_uint32(buffer);
	char* cadena = malloc(tam);

	memcpy(cadena, buffer->stream + buffer->offset, (*tam));
	buffer->offset += (*tam);

	return cadena;
}

// CONTEXTO DE EJECUCION
void buffer_write_cde(t_buffer* buffer, t_cde* cde_recibido){
	buffer->stream = realloc(buffer->stream, buffer->size + sizeof(t_cde));
	memcpy(buffer->stream + buffer->size, cde_recibido, sizeof(t_cde));
	buffer->size += sizeof(t_cde);

	buffer->stream = realloc(buffer->stream, buffer->size + sizeof(t_registro));
	memcpy(buffer->stream + buffer->size, cde_recibido->registros, sizeof(t_registro));
	buffer->size += sizeof(t_cde);

	// copiamos instruccion
	buffer_write_uint8(buffer, cde_recibido->ultima_instruccion->codigo);
	buffer_write_string(buffer, cde_recibido->ultima_instruccion->parametro1);
	buffer_write_string(buffer, cde_recibido->ultima_instruccion->parametro2);
	buffer_write_string(buffer, cde_recibido->ultima_instruccion->parametro3);
	buffer_write_string(buffer, cde_recibido->ultima_instruccion->parametro4);
	buffer_write_string(buffer, cde_recibido->ultima_instruccion->parametro5);
}

t_cde* buffer_read_cde(t_buffer* buffer){
	t_cde* cde = calloc(1, sizeof(t_cde));
	t_registro* registros_creados = calloc(1, sizeof(t_registro));
	t_instruccion* ultima_instruccion_creada = calloc(1, sizeof(t_instruccion));
	
	memcpy(cde, buffer->stream + buffer->offset, sizeof(t_cde));
	buffer->offset += sizeof(t_cde);

	memcpy(registros_creados, buffer->stream + buffer->offset, sizeof(t_registro));
	buffer->offset += sizeof(t_registro);

	uint32_t tam;
	// leemos instruccion
	ultima_instruccion_creada->codigo = buffer_read_uint8(buffer);
	ultima_instruccion_creada->parametro1 = buffer_read_string(buffer, &tam);
	ultima_instruccion_creada->parametro2 = buffer_read_string(buffer, &tam);
	ultima_instruccion_creada->parametro3 = buffer_read_string(buffer, &tam);
	ultima_instruccion_creada->parametro4 = buffer_read_string(buffer, &tam);
	ultima_instruccion_creada->parametro5 = buffer_read_string(buffer, &tam);

	cde->registros = registros_creados;
	cde->ultima_instruccion = ultima_instruccion_creada;

	return cde;
}

void destruir_cde(t_cde* cde){
    free(cde->registros);
    free(cde);
}

// INSTRUCCION
void buffer_write_instruccion(t_buffer* buffer, t_instruccion* instruccion){
	buffer_write_uint8(buffer, instruccion->codigo);
	if (instruccion->parametro1 != NULL)
		buffer_write_string(buffer, instruccion->parametro1);
	else
		buffer_write_string(buffer, "");
	
	if (instruccion->parametro2 != NULL)
		buffer_write_string(buffer, instruccion->parametro2);
	else
		buffer_write_string(buffer, "");
	
	if (instruccion->parametro3 != NULL)
		buffer_write_string(buffer, instruccion->parametro3);
	else
		buffer_write_string(buffer, "");

	if (instruccion->parametro4 != NULL)
		buffer_write_string(buffer, instruccion->parametro4);
	else
		buffer_write_string(buffer, "");

	if (instruccion->parametro5 != NULL)
		buffer_write_string(buffer, instruccion->parametro5);
	else
		buffer_write_string(buffer, "");
}

t_instruccion* buffer_read_instruccion(t_buffer* buffer){
	t_instruccion* instr = malloc(sizeof(t_instruccion));
	instr->parametro1 = NULL;
	instr->parametro2 = NULL;
	instr->parametro3 = NULL;
	instr->parametro4 = NULL;
	instr->parametro5 = NULL;

	uint32_t tam;

	instr->codigo = buffer_read_uint8(buffer);
	
	instr->parametro1 = buffer_read_string(buffer, &tam);
	instr->parametro2 = buffer_read_string(buffer, &tam);
	instr->parametro3 = buffer_read_string(buffer, &tam);
	instr->parametro4 = buffer_read_string(buffer, &tam);
	instr->parametro5 = buffer_read_string(buffer, &tam);

	return instr;
}

void destruir_instruccion(t_instruccion* instruccion){
	free(instruccion->parametro1);
	free(instruccion->parametro2);
	free(instruccion->parametro3);
	free(instruccion->parametro4);
	free(instruccion->parametro5);
	free(instruccion);
}


// Registros

void buffer_write_registros(t_buffer* buffer, t_registro* registro_recibido){
	buffer->stream = realloc(buffer->stream, buffer->size + sizeof(t_registro));

	memcpy(buffer->stream + buffer->size, registro_recibido, sizeof(t_registro));
	buffer->size += sizeof(t_registro);
}

t_registro* buffer_read_registros(t_buffer* buffer){
	t_registro* registro = calloc(1, sizeof(t_registro));
	
	memcpy(registro, buffer->stream + buffer->offset, sizeof(t_registro));
	buffer->offset += sizeof(t_registro);
	return registro;
}

void buffer_write_registros_old(t_buffer* buffer, t_registro* registros){
	buffer_write_uint8(buffer, registros->AX);
	buffer_write_uint8(buffer, registros->BX);
	buffer_write_uint8(buffer, registros->CX);
	buffer_write_uint8(buffer, registros->DX);
	buffer_write_uint32(buffer, registros->EAX);
	buffer_write_uint32(buffer, registros->EBX);
	buffer_write_uint32(buffer, registros->ECX);
	buffer_write_uint32(buffer, registros->EDX);
	buffer_write_uint32(buffer, registros->PC);
	buffer_write_uint32(buffer, registros->SI);
	buffer_write_uint32(buffer, registros->DI);
}

t_registro* buffer_read_registros_old(t_buffer* buffer){
	t_registro* regis = malloc(sizeof(t_registro));

	regis->AX =buffer_read_uint8(buffer);
	regis->BX = buffer_read_uint8(buffer);
	regis->CX = buffer_read_uint8(buffer);
	regis->DX = buffer_read_uint8(buffer);
	regis->EAX = buffer_read_uint32(buffer);
	regis->EBX = buffer_read_uint32(buffer);
	regis->ECX = buffer_read_uint32(buffer);
	regis->EDX = buffer_read_uint32(buffer);
	regis->PC = buffer_read_uint32(buffer);
	regis->SI = buffer_read_uint32(buffer);
	regis->DX = buffer_read_uint32(buffer);

	return regis;
}