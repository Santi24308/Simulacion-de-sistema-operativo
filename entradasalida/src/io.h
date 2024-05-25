#ifndef SRC_IO_H_
#define SRC_IO_H_

#include <utils/conexiones.h>
#include <commons/string.h>
#include <diccionarioIO.h>
#include <stdbool.h>

// es buena idea pero en el enunciado piden que TIPO_INTERFAZ llegue 
// como string desde config
typedef enum{
    GENERICA,
    STDIN,
    STDOUT,
    DIALFS
}tipoIO;

bool chequeo_parametros(int, char**);
void levantar_logger();
void levantar_config();
void terminar_programa();
void terminar_conexiones(int, ...);
void conectar_memoria();
void conectar_kernel();
void conectar();
void inicializar_modulo();
void atender_kernel_generica();
void atender_kernel_stdin();
void atender_kernel_stdout();
void atender_kernel_dialfs();
void leer_y_enviar_a_memoria();
#endif
