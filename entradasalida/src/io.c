#include "io.h"

int main(int argc, char* argv[]) {

	// se analiza si se puede seguir o no ejecutando
	if(!chequeo_parametros(argc, argv)) return -1;
	sem_init(&terminar_io, 0, 0);

	inicializar_modulo();

	// las interfaces tienen tipo y de eso me doy cuenta en tiempo de ejecucion, por lo que es
	// necesario que main sepa preparar todo en base al tipo que toco (obtenido desde config)
	conectar();
	sem_wait(&terminar_io);

    terminar_programa();

    return 0;
}

bool chequeo_parametros(int argc, char** argv){
	// I/O debe recibir el NOMBRE y el CONFIG
	
	if(argc != 3) {
		printf("\x1b[31m""ERROR: Tenés que pasar el nombre de I/O y el path del archivo config de Entradasalida""\x1b[0m""\n");
		return false;
	}

	// asignamos el nombre
	nombreIO = string_new();
	string_append(&nombreIO, argv[1]);

	// asignamos config
	config_path = string_new();
	string_append(&config_path, argv[2]);

	return true;
}

void conectar(){
	conectar_kernel();
	if (strcmp(tipo, "GENERICA") != 0)
		conectar_memoria();
}

void inicializar_modulo(){
	levantar_logger();
	levantar_config();

	// seteamos el tipo de la interfaz 
	tipo = string_new();
	string_append(&tipo, config_get_string_value(config_io, "TIPO_INTERFAZ"));
}

void atender_kernel_generica(){
	while(1){
		codigoInstruccion cod = recibir_codigo(socket_kernel);
		switch (cod){
			case IO_GEN_SLEEP:
				t_buffer* buffer = recibir_buffer(socket_kernel);
				uint32_t pid = buffer_read_uint32(buffer);
				t_instruccion* instruccion_recibida = buffer_read_instruccion(buffer);
				destruir_buffer(buffer);
				int tiempo_unidad_trabajo = config_get_int_value(config_io, "TIEMPO_UNIDAD_TRABAJO");
				int cantidad_recibida = leerEnteroParametroInstruccion(2, instruccion_recibida);
				usleep(tiempo_unidad_trabajo * cantidad_recibida * 1000);
				log_info(logger_io, "PID: %d - Operacion: %s", pid, obtener_nombre_instruccion(instruccion_recibida));
				enviar_codigo(socket_kernel, LIBRE);
				buffer = crear_buffer();
				buffer_write_string(buffer, nombreIO);
				enviar_buffer(buffer, socket_kernel);
				destruir_buffer(buffer);
				break;
			case NULO: // lo usamos para testear que siga conectada desde kernel 
				break;
			default:
				break;
		}
	}
}

void atender_kernel_stdin(){
	while(1){
		codigoInstruccion cod = recibir_codigo(socket_kernel);
		switch (cod){
			case IO_STDIN_READ:
					ejecutar_std_in();
					//leer_y_enviar_a_memoria();
				break;
			default:
				break;
		}
	}
}

void atender_kernel_stdout(){
	while(1){
		codigoInstruccion cod = recibir_codigo(socket_kernel);
		switch (cod){
			case IO_STDOUT_WRITE:
				ejecutar_std_out();
				//leer_y_mostrar_resultado();
				break;
			default:
				break;
		}
	}
}
/*
void atender_kernel_dialfs(){
	
	crear_archivo_bloques();
	crear_archivo_bitmap();
	
	while(1){
		codigoInstruccion cod = recibir_codigo(socket_kernel);
		switch (cod){
			case IO_FS_CREATE:
				usleep(1000);//ver de config?
				ejecutar_fs_create();
				break;
			case IO_FS_DELETE:
				usleep(1000);
				ejecutar_fs_delete();
				break;
			case IO_FS_TRUNCATE:
				usleep(1000);
				ejecutar_fs_truncate();
				break;
			case IO_FS_WRITE:
				usleep(1000);
				ejecutar_fs_write();
				break;
			case IO_FS_READ:
				usleep(1000);
				ejecutar_fs_read();
				break;
			default:
				break;
		}
	}
}
*/

void atender(){
	if (strcmp(tipo, "GENERICA")==0)
		atender_kernel_generica();
	else if (strcmp(tipo, "STDIN")==0)
		atender_kernel_stdin();
	else if (strcmp(tipo, "STDOUT")==0)
		atender_kernel_stdout();
	//else if (strcmp(tipo, "DIALFS")==0)
	//	atender_kernel_dialfs();
	else  
		log_error(logger_io, "El tipo de I/O indicado en config es incorrecto, terminando programa...");
}

void ejecutar_std_in(){
	t_buffer* buffer = recibir_buffer(socket_kernel);
	uint32_t pid = buffer_read_uint32(buffer);
	t_instruccion* instruccion = buffer_read_instruccion(buffer);
	destruir_buffer(buffer);


	int limite_bytes = atoi(instruccion->parametro3);
	printf("\nLimite de bytes  %d", limite_bytes);

	char* leido = readline("> ");
	printf("\n el sting leido tiene  %ld bytes", strlen(leido) + 1);

	// para poner el \0 y evitar conflictos, sabemos que en la serializacion no se va a copiar el \0
	void* valor_a_escribir = malloc(limite_bytes + 1);

	memcpy(valor_a_escribir, leido, limite_bytes);
	char* palabra = valor_a_escribir;
	palabra[limite_bytes] = '\0';

	printf("\nLa palabra limitada queda: %s", palabra);
	printf("\nEn bytes queda:");
	mem_hexdump(palabra, limite_bytes);


	enviar_codigo(socket_memoria, IO_STDIN_READ);
	buffer = crear_buffer();
	buffer_write_uint32(buffer, pid);
	buffer_write_uint32(buffer, (uint32_t)atoi(instruccion->parametro2)); // direccion
	buffer_write_uint32(buffer, (uint32_t)limite_bytes);
	buffer_write_string(buffer, palabra);
	enviar_buffer(buffer, socket_memoria);
	destruir_buffer(buffer);

	mensajeMemoria cod = recibir_codigo(socket_memoria);
	if (cod != OK){
		// contemplar error
	}

	enviar_codigo(socket_kernel, LIBRE);
	buffer = crear_buffer();
	buffer_write_string(buffer, nombreIO);
	enviar_buffer(buffer, socket_kernel);
	destruir_buffer(buffer);
}

void ejecutar_std_out(){
	t_buffer* buffer = recibir_buffer(socket_kernel);
	uint32_t pid = buffer_read_uint32(buffer);
	t_instruccion* instruccion = buffer_read_instruccion(buffer);
	destruir_buffer(buffer);

	enviar_codigo(socket_memoria, IO_STDOUT_WRITE);
	buffer = crear_buffer();
	buffer_write_uint32(buffer, pid);
	buffer_write_uint32(buffer, (uint32_t)atoi(instruccion->parametro2));
	buffer_write_uint32(buffer, (uint32_t)atoi(instruccion->parametro3));
	enviar_buffer(buffer, socket_memoria);
	destruir_buffer(buffer);

	buffer = recibir_buffer(socket_memoria);
	char* valor_a_mostrar = buffer_read_string(buffer);
	destruir_buffer(buffer);

	printf("\t\nEl string que llego es: %s\n", valor_a_mostrar);

	enviar_codigo(socket_kernel, LIBRE);
	buffer = crear_buffer();	
	buffer_write_string(buffer, nombreIO);
	enviar_buffer(buffer, socket_kernel);
	destruir_buffer(buffer);
}

void conectar_memoria(){
	ip = config_get_string_value(config_io, "IP");
	puerto_mem = config_get_string_value(config_io, "PUERTO_MEM");

    socket_memoria = crear_conexion(ip, puerto_mem);
    if (socket_memoria == -1) {
		terminar_programa();
        exit(EXIT_FAILURE);
    }

	t_buffer* buffer= crear_buffer();
	buffer_write_string(buffer, nombreIO);
	buffer_write_string(buffer, tipo);
	enviar_buffer(buffer,socket_memoria);
	destruir_buffer(buffer);
}

void conectar_kernel(){
	ip = config_get_string_value(config_io, "IP");
	puerto_kernel = config_get_string_value(config_io, "PUERTO_KERNEL");

    socket_kernel = crear_conexion(ip, puerto_kernel);
    if (socket_kernel == -1) {
		terminar_programa();
        exit(EXIT_FAILURE);
    }

	t_buffer* buffer = crear_buffer();
	buffer_write_string(buffer, nombreIO);
	buffer_write_string(buffer, tipo);
	enviar_buffer(buffer, socket_kernel);
	destruir_buffer(buffer);

	int err = pthread_create(&hilo_kernel, NULL, (void *)atender, NULL);
	if (err != 0) {
		perror("Fallo la creacion de hilo para Kernel\n");
		return;
	}
	pthread_detach(hilo_kernel);

}

void levantar_logger(){
	char* nombreLog = string_new();
	string_append(&nombreLog, nombreIO);
	string_append(&nombreLog, "_log.log");

	logger_io = log_create(nombreLog, "IO",true, LOG_LEVEL_INFO);
	if (!logger_io) {
		perror("Error al iniciar logger de IO\n");
		exit(EXIT_FAILURE);
	}
}

void levantar_config(){
    config_io = config_create(config_path);
	if (!config_io) {
		perror("Error al iniciar config de IO\n");
		exit(EXIT_FAILURE);
	}
}

void terminar_programa(){
	terminar_conexiones(2, socket_memoria, socket_kernel);
    if(logger_io) log_destroy(logger_io);
    if(config_io) config_destroy(config_io);

	
/////////////////     DIALFS     /////////////////

/*
// CPU -> KERNEL -> IO ( y la logica de FS)

//kernel le manda al FS que tiene que ejecuta
void ejecutar_fs_create(nombreArchivo){
//t_buffer buffer =  recibir_buffer(socket_kernel);	
}
				
void ejecutar_fs_delete(){}
			
void ejecutar_fs_truncate(){}
			
void ejecutar_fs_write(){}
				
void ejecutar_fs_read(){}
*/

//-------------------------------------------------------------------------------------
/*
void crear_archivo_bloques(){
	block_size  = config_get_int_value(config_io, "BLOCK_SIZE");
	block_count = config_get_int_value(config_io,"BLOCK_COUNT");
	
// Función para inicializar y escribir el archivo bloques.dat
    FILE *fp = fopen("bloques.dat", "w");  // Abrir el archivo en modo escritura binaria
    if (fp == NULL) {
        perror("Error al abrir el archivo bloques.dat");
        return;   
	}

	t_bloque* bloque_vacio = malloc(sizeof(t_bloque));
		if(bloque_vacio == NULL){
		log_error(logger_io , "Error malloc en crear bloque");
		return -1;
		}

    memset(bloque_vacio->bloque, 0, block_size);  // Inicializar un bloque vacío con ceros

    for (int i = 0; i < block_count; ++i) {
        fwrite(&bloque_vacio->bloque, block_size, 1, fp);  // Escribir el bloque vacío en el archivo
    }

    fclose(fp); // Cerrar el archivo después de escribir todos los bloques
	free(bloque_vacio);
}

void crear_archivo_bitmap(){
	// Función para inicializar y escribir el archivo bloques.dat
    FILE *fp = fopen("bitmap.dat", "w");  // Abrir el archivo en modo escritura binaria
    if (fp == NULL) {
        perror("Error al abrir el archivo bloques.dat");
        return;   
	}
	//char* array_bits = crear_array_bits();
	t_bitarray bitarray = bitarray_create(0,block_count); //struct de commons. 0??
	
    fwrite(bitarray, sizeof(t_bitarray), 1, fp);  // Escribir el bloque vacío en el archivo
    
    fclose(fp); // Cerrar el archivo después de escribir todos los bloques
}
*/
//-------------------------------------------------------------------------------------

//Cambio de tamanio del archivo: chequeamos si es mayor o menos el nuevo
// tamanio al tamanio anterior, si es mayor el tamanio se agrega el bloque
// al archivo y se escribe el archivo bitmap. En caso contrario, se elimina
// el bloque y se actualiza el bit map

/*

void cambiar_tamanio_archivo(char* nombreArchivo, uint32_t nuevo_tamanio){
    char* ruta_fcb_buscado = obtener_ruta_archivo(nombreArchivo);
    t_config* fcb_buscado = config_create(ruta_fcb_buscado);
    char* tamanio_a_aplicar;
    tamanio_a_aplicar = string_itoa(nuevo_tamanio);

    uint32_t tamanio_anterior = config_get_int_value(fcb_buscado, "TAMANIO_ARCHIVO");
    uint32_t bloque_inicial = config_get_int_value(fcb_buscado, "BLOQUE_INICIAL"); 

    config_set_value(fcb_buscado, "TAMANIO_ARCHIVO", tamanio_a_aplicar);
    config_save_in_file(fcb_buscado, ruta_fcb_buscado);

    if(nuevo_tamanio > tamanio_anterior){
        for(int i = 0; i < (nuevo_tamanio - tamanio_anterior) / config_file_system.tam_bloque; i++){
            agregar_bloque_archivo(bloque_inicial);
			escribir_BitMap(uint32_t nroBloque, uint8_t valor);
        }
    }
    else{
        for(int i = 0; (tamanio_anterior - nuevo_tamanio) / config_file_system.tam_bloque; i++)
            sacar_bloque_archivo(bloque_inicial);
			escribir_BitMap(uint32_t nroBloque, uint8_t valor);
			
    }

    config_destroy(fcb_buscado);
    free(ruta_fcb_buscado);
    free(tamanio_a_aplicar);
}

//-------------------------------------------------------------------------------------

void agregar_bloque_archivo(uint32_t bloqueInicial) {
    uint32_t primerBloqueLibre = obtener_primer_bloque_libre();
    if (primerBloqueLibre == UINT32_MAX) {
        printf("No hay bloques libres disponibles.\n");
        return;
    }

    // Marcar el primer bloque libre como ocupado
    escribir_BitMap(primerBloqueLibre, 1);

    // Abrir el archivo de bloques
    FILE *file = fopen("bloques.dat", "rb+");
    if (file == NULL) {
        perror("Error abriendo bloques.dat");
        return;
    }

    // Escribir el número del nuevo bloque en el último bloque del archivo
    fseek(file, bloqueInicial * sizeof(uint32_t), SEEK_SET);
    uint32_t nuevoBloque = primerBloqueLibre;
    fwrite(&nuevoBloque, sizeof(uint32_t), 1, file);

    // Cerrar el archivo
    fclose(file);
}
//---------------------------------------

void sacar_bloque_archivo(uint32_t bloqueInicial) {
    FILE *file = fopen("bloques.dat", "rb+");
    if (file == NULL) {
        perror("Error abriendo bloques.dat");
        return;
    }

    // Encontrar el último bloque del archivo
    uint32_t bloqueActual = bloqueInicial;
    uint32_t bloqueAnterior = UINT32_MAX;
    uint32_t siguienteBloque;

    while (1) {
        fseek(file, bloqueActual * sizeof(uint32_t), SEEK_SET);
        fread(&siguienteBloque, sizeof(uint32_t), 1, file);
        if (siguienteBloque == UINT32_MAX) {
            break;
        }
        bloqueAnterior = bloqueActual;
        bloqueActual = siguienteBloque;
    }

    // Marcar el último bloque como libre
    actualizarBitMap(bloqueActual, 0);

    // Actualizar el penúltimo bloque para marcar el final del archivo
    if (bloqueAnterior != UINT32_MAX) {
        fseek(file, bloqueAnterior * sizeof(uint32_t), SEEK_SET);
        uint32_t finArchivo = UINT32_MAX;
        fwrite(&finArchivo, sizeof(uint32_t), 1, file);
    }

    // Cerrar el archivo
    fclose(file);
}

//-------------------------------------------------------------------------------------
// Escribimos en el archivo bloques.dat y pisamos con "valor" en el nro de bloque correspondiente

void escribir_BitMap(uint32_t nroBloque, uint8_t valor){
    FILE *file = fopen("bitmap.dat", "rb+");
    if (file == NULL) {
        perror("Error abriendo bitmap.dat");
        return;
    }
    // Mover el puntero del archivo a la posición correspondiente
    if (fseek(file, nroBloque, SEEK_SET) != 0) {
        perror("Error buscando en bitmap.dat");
        fclose(file);
        return;
    }
    // Escribir el nuevo valor en la posición correspondiente
    if (fwrite(&valor, sizeof(uint8_t), 1, file) != 1) {
        perror("Error escribiendo en bitmap.dat");
    }

    // Cerrar el archivo
    fclose(file);
}
//-------------------------------------------------------------------------
// Leemos del archivo bloques.dat y chequeamos si el bit esta usado o no

uint8_t leer_de_bitmap(uint32_t nroBloque) {
    FILE *file = fopen("bitmap.dat", "rb");
    if (file == NULL) {
        perror("Error abriendo bitmap.dat");
        return;
    }

    // Mover el puntero del archivo a la posición correspondiente
    if (fseek(file, nroBloque, SEEK_SET) != 0) {
        perror("Error buscando en bitmap.dat");
        fclose(file);
        return;
    }

    // Leer el valor en la posición correspondiente
    uint8_t valor;
    if (fread(&valor, sizeof(uint8_t), 1, file) != 1) {
        perror("Error leyendo de bitmap.dat");
        fclose(file);
        return;
    }

    // Cerrar el archivo
    fclose(file);

    return valor;
}

//---------------------------------------------------
uint32_t obtener_nro_bloque_libre(){
    for(int nroBloque = 0; nroBloque < config_file_system.cantidad_bloques_total; nroBloque++){
        uint8_t bloqueOcupado = leer_de_bitmap(nroBloque);
        if(!bloqueOcupado)
            return nroBloque;
    }
}
*/


}
