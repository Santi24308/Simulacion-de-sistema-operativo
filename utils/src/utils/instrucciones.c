#include "instrucciones.h"

t_instruccion* crear_instruccion(codigoInstruccion codigo){
	t_instruccion* instruccion = calloc(1, sizeof(t_instruccion));
	if (!instruccion) {
		perror("No se pudo reservar memoria para instruccion.\n");
		return NULL;
	}

	instruccion->codigo = codigo;

	instruccion->parametro1 = NULL;
	instruccion->parametro2 = NULL;
	instruccion->parametro3 = NULL;
	instruccion->parametro4 = NULL;
	instruccion->parametro5 = NULL;

	return instruccion;
}

uint32_t leerEnteroParametroInstruccion(int indice, t_instruccion* instr){
	char* aux = leerCharParametroInstruccion(indice, instr);
	uint32_t leido = atoi(aux);
	free(aux);
	return leido;
}

																			
void escribirCharParametroInstruccion(int indice, t_instruccion* instr, char* string){

	switch(indice){
		case 1:
			instr->parametro1 = string_new();
			string_append(&instr->parametro1, string);
			break;
		case 2:
			instr->parametro2 = string_new();
			string_append(&instr->parametro2, string);
			break;
		case 3:
			instr->parametro3 = string_new();
			string_append(&instr->parametro3, string);
			break;
		case 4:
			instr->parametro4 = string_new();
			string_append(&instr->parametro4, string);
			break;
		case 5:
			instr->parametro5 = string_new();
			string_append(&instr->parametro5, string);
			break;
	}
}


char* leerCharParametroInstruccion(int indice, t_instruccion* instr){
	char* leido;

	switch(indice){
	case 1:
		if(!instr->parametro1)
			return NULL;
		leido = string_new();
		string_append(&leido, instr->parametro1);
		break;
	case 2:
		if(!instr->parametro2)
			return NULL;
		leido = string_new();
		string_append(&leido, instr->parametro2);
		break;
	case 3:
		if(!instr->parametro3)
			return NULL;
		leido = string_new();
		string_append(&leido, instr->parametro3);
		break;
	case 4:
		if(!instr->parametro4)
			return NULL;
		leido = string_new();
		string_append(&leido, instr->parametro4);
		break;
	case 5:
		if(!instr->parametro5)
			return NULL;
		leido = string_new();
		string_append(&leido, instr->parametro5);
		break;
	}

	return leido;
}

char* obtener_nombre_instruccion(t_instruccion* instruccion){
	char* nombre = string_new();
    switch(instruccion->codigo){
        case SET:
            string_append(&nombre,"SET");
            break;
	    case SUM:
            string_append(&nombre,"SUM") ;
            break;
	    case SUB:
            string_append(&nombre,"SUB") ;
            break;
	    case JNZ:
            string_append(&nombre,"JNZ") ;
            break;
	    case RESIZE:
            string_append(&nombre,"RESIZE") ;
            break;
	    case WAIT:
            string_append(&nombre,"WAIT") ;
            break;
	    case SIGNAL:
            string_append(&nombre,"SIGNAL") ;
            break;
	    case MOV_IN:
            string_append(&nombre,"MOV_IN") ;
            break;
	    case MOV_OUT:
            string_append(&nombre,"MOV_OUT") ;
            break;
	    case COPY_STRING:
            string_append(&nombre,"COPY_STRING") ;
            break;
	    case IO_GEN_SLEEP:
            string_append(&nombre,"IO_GEN_SLEEP") ;
            break;
	    case IO_STDIN_READ:
            string_append(&nombre,"IO_STDIN_READ") ;
            break;
	    case IO_STDOUT_WRITE:
            string_append(&nombre,"IO_STDOUT_WRITE") ;
            break;
	    case IO_FS_CREATE:
            string_append(&nombre,"IO_FS_CREATE") ;
            break;
	    case IO_FS_DELETE:
            string_append(&nombre,"IO_FS_DELETE") ;
            break;
        case IO_FS_TRUNCATE:
            string_append(&nombre,"IO_FS_TRUNCATE") ;
            break;
        case IO_FS_WRITE:
            string_append(&nombre,"IO_FS_WRITE") ;
            break;
	    case IO_FS_READ:
            string_append(&nombre,"IO_FS_READ") ;
            break;
	    case EXIT:
            string_append(&nombre,"EXIT") ;
            break;
        default:
            return NULL;
            break;
    }

	return nombre;
}

codigoInstruccion obtener_codigo_instruccion(char* cod_char){
	if (strcmp(cod_char, "SET") == 0){
		return SET;
	} else if (strcmp(cod_char, "SUM") == 0) {
		return SUM;
	} else if (strcmp(cod_char, "SUB") == 0) {
		return SUB;
	} else if (strcmp(cod_char, "JNZ") == 0) {
		return JNZ;
	} else if (strcmp(cod_char, "RESIZE") == 0) {
		return RESIZE;
	} else if (strcmp(cod_char, "WAIT") == 0) {
		return WAIT;
	} else if (strcmp(cod_char, "SIGNAL") == 0) {
		return SIGNAL;
	} else if (strcmp(cod_char, "MOV_IN") == 0) {
		return MOV_IN;
	} else if (strcmp(cod_char, "MOV_OUT") == 0) {
		return MOV_OUT;
	} else if (strcmp(cod_char, "COPY_STRING") == 0) {
		return COPY_STRING;
	} else if (strcmp(cod_char, "IO_GEN_SLEEP") == 0) {
		return IO_GEN_SLEEP;
	} else if (strcmp(cod_char, "IO_STDIN_READ") == 0) {
		return IO_STDIN_READ;
	} else if (strcmp(cod_char, "IO_STDOUT_WRITE") == 0) {
		return IO_STDOUT_WRITE;
	} else if (strcmp(cod_char, "IO_FS_CREATE") == 0) {
		return IO_FS_CREATE;
	} else if (strcmp(cod_char, "IO_FS_DELETE") == 0) {
		return IO_FS_DELETE;
	} else if (strcmp(cod_char, "IO_FS_TRUNCATE") == 0) {
		return IO_FS_TRUNCATE;
	} else if (strcmp(cod_char, "IO_FS_WRITE") == 0) {
		return IO_FS_WRITE;
	} else if (strcmp(cod_char, "IO_FS_READ") == 0) {
		return IO_FS_READ;
	} else if (strcmp(cod_char, "EXIT") == 0) {
		return EXIT;
	} else {
		return NULO;
	}
}

int cantidad_parametros_instruccion(codigoInstruccion codigo){
    switch(codigo){
        case SET:
            return 2;
	    case SUM:
            return 2;
	    case SUB:
            return 2;
	    case JNZ:
            return 2;
	    case RESIZE:
            return 1;
	    case WAIT:
            return 1;
	    case SIGNAL:
            return 1;
	    case MOV_IN:
            return 2;
	    case MOV_OUT:
            return 2;
	    case COPY_STRING:
            return 1;
	    case IO_GEN_SLEEP:
            return 2;
	    case IO_STDIN_READ:
            return 3;
	    case IO_STDOUT_WRITE:
            return 3;
	    case IO_FS_CREATE:
            return 2;
	    case IO_FS_DELETE:
            return 2;
        case IO_FS_TRUNCATE:
            return 3;
        case IO_FS_WRITE:
            return 5;
	    case IO_FS_READ:
            return 5;
	    case EXIT:
            return 0;
		default:
			return 0;
    }	
}

void destruir_instruccion(t_instruccion* instruccion){
	if (!instruccion)
		return;
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