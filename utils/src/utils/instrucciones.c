#include "instrucciones.h"

t_instruccion* crear_instruccion(codigoInstruccion codigo, char* par1, char* par2, char* par3, char* par4, char* par5){
	t_instruccion* instruccion = malloc(sizeof(t_instruccion));
	if (!instruccion) {
		perror("No se pudo reservar memoria para instruccion.\n");
		return NULL;
	}

	// para que no se queje el compilador
	instruccion->parametro1 = NULL;
	instruccion->parametro2 = NULL;
	instruccion->parametro3 = NULL;
	instruccion->parametro4 = NULL;
	instruccion->parametro5 = NULL;
    // -----------------------------------

	instruccion->codigo = codigo;
	strcpy(instruccion->parametro1, par1);
	strcpy(instruccion->parametro2, par2);
	strcpy(instruccion->parametro3, par3);
	strcpy(instruccion->parametro4, par4);
	strcpy(instruccion->parametro5, par5);
	return instruccion;
}

uint32_t leerEnteroParametroInstruccion(int indice, t_instruccion* instr){
	char* aux = leerCharParametroInstruccion(indice, instr);
	uint32_t leido = atoi(aux);
	free(aux);
	return leido;
}

void escribirCharParametroInstruccion(int indice, t_instruccion* instr, char* string){

	int tam = string_length(string) + 1;

	switch(indice){
		case 1:
			instr->parametro1 = malloc(tam);
			strcpy(instr->parametro1, string);
			string_append(&(instr->parametro1), "\0");
			break;
		case 2:
			instr->parametro2 = malloc(tam);
			strcpy(instr->parametro2, string);
			string_append(&(instr->parametro2), "\0");
			break;
		case 3:
			instr->parametro3 = malloc(tam);
			strcpy(instr->parametro3, string);
			string_append(&(instr->parametro3), "\0");
			break;
		case 4:
			instr->parametro4 = malloc(tam);
			strcpy(instr->parametro4, string);
			string_append(&(instr->parametro4), "\0");
			break;
		case 5:
			instr->parametro5 = malloc(tam);
			strcpy(instr->parametro5, string);
			string_append(&(instr->parametro5), "\0");
			break;
	}
}


char* leerCharParametroInstruccion(int indice, t_instruccion* instr){
	char* leido;

	int tam = 0;

	switch(indice){
	case 1:
		tam = string_length(instr->parametro1);
		leido = malloc(tam);
		strcpy(leido, instr->parametro1);
		break;
	case 2:
		tam = string_length(instr->parametro2);
		leido = malloc(tam);
		strcpy(leido, instr->parametro2);
		break;
	case 3:
		tam = string_length(instr->parametro3);
		leido = malloc(tam);
		strcpy(leido, instr->parametro3);
		break;
	case 4:
		tam = string_length(instr->parametro4);
		leido = malloc(tam);
		strcpy(leido, instr->parametro4);
		break;
	case 5:
		tam = string_length(instr->parametro5);
		leido = malloc(tam);
		strcpy(leido, instr->parametro5);
		break;
	}

	return leido;
}