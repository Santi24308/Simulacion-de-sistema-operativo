#ifndef SRC_IO_H_
#define SRC_IO_H_

#include <utils/conexiones.h>
#include <commons/string.h>
#include <commons/bitarray.h>
#include <diccionarioIO.h>
#include <stdbool.h>
#include <math.h>

// es buena idea pero en el enunciado piden que TIPO_INTERFAZ llegue 
// como string desde config
/* typedef enum{
    GENERICA,
    STDIN,
    STDOUT,
    DIALFS
}tipoIO; */

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
void leer_y_mostrar_resultado();

void ejecutar_std_in();
void ejecutar_std_out();

void ejecutar_fs_create();				
void ejecutar_fs_delete();			
void ejecutar_fs_truncate();			
void ejecutar_fs_write();				
void ejecutar_fs_read();

void crear_archivo_bloques();
void crear_archivo_bitmap();
void crear_metadata(char*, char*);
void agregar_bloque(uint32_t );
void sacar_bloque_archivo(uint32_t );
void cambiar_tamanio_archivo(uint32_t , uint32_t );
uint32_t obtener_nro_bloque_libre();
void modificar_BitMap(uint32_t , uint8_t);
uint8_t leer_de_bitmap(uint32_t );
char* obtener_lista_archivos_abiertos(t_list* );

#endif
