#ifndef SRC_INSTRUCCIONES_H_
#define SRC_INSTRUCCIONES_H_

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include<commons/string.h>
#include <ctype.h> // Para isDigit()
#include <stdint.h> // Para uintX_t
#include <stddef.h>


typedef enum{
	SET,
    MOV_IN,
    MOV_OUT,
    SUM,
    SUB,
    JNZ,
    RESIZE,
    COPY_STRING,
    WAIT,
    SIGNAL,
    IO_GEN_SLEEP,
    IO_STDIN_READ,
    IO_STDOUT_WRITE,
    IO_FS_CREATE,
    IO_FS_DELETE,
    IO_FS_TRUNCATE,
    IO_FS_WRITE,
    IO_FS_READ,
    EXIT,
    NULO     // se usa como estado de instruccion_actualizada en cpu cuando se desaloja un proceso
}codigoInstruccion;


typedef struct{
	codigoInstruccion codigo;
	char* parametro1;
	char* parametro2;
	char* parametro3;
    char* parametro4;
    char* parametro5;
}t_instruccion;

t_instruccion* crear_instruccion(codigoInstruccion);

uint32_t leerEnteroParametroInstruccion(int indice, t_instruccion* instr);

void escribirCharParametroInstruccion(int indice, t_instruccion* instr, char* string);
char* leerCharParametroInstruccion(int indice, t_instruccion* instr);
char* obtener_nombre_instruccion(t_instruccion* instruccion);
codigoInstruccion obtener_codigo_instruccion(char* cod_char);
int cantidad_parametros_instruccion(codigoInstruccion codigo);

#endif /* SRC_UTILS_INSTRUCCIONES_H_ */
