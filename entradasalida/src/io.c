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
				break;
			default:
				break;
		}
	}
}

void inicializar_fs(){
	tamanio_archivo_bloques = 0;
	block_size  = config_get_int_value(config_io, "BLOCK_SIZE");
    block_count = config_get_int_value(config_io,"BLOCK_COUNT");
    path_filesystem = config_get_string_value(config_io,"PATH_BASE_DIALFS");

	levantar_archivo_bitarray();
	levantar_archivo_bloques();
	lista_global_archivos_abiertos = list_create();

	bitmap = malloc(sizeof(t_bitarray));
	bitmap->size = tamanio_archivo_bitarray;
	bitmap->mode = MSB_FIRST;
	bitmap->bitarray = mmap(NULL, tamanio_archivo_bitarray, PROT_READ | PROT_WRITE, MAP_SHARED, fd_bitarray, 0);
	if (bitmap == MAP_FAILED) {
		log_info(logger_io, "error al crear el bitmap");
	}
	bloquesmap = mmap(NULL, tamanio_archivo_bloques, PROT_READ | PROT_WRITE, MAP_SHARED, fd_bloques, 0);
	if (bloquesmap == MAP_FAILED) {
		log_info(logger_io, "error al crear bloquesmap");
	}
}

void atender_kernel_dialfs(){
	inicializar_fs();
	
	//test();

	while(1){
		codigoInstruccion cod = recibir_codigo(socket_kernel);
		switch (cod){
			case IO_FS_CREATE:
				usleep(1000);
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

void test(){
	char cadena[10] = "hola";

	bitarray_set_bit(bitmap, 2);
	msync(bitmap->bitarray, bitmap->size, MS_SYNC);

	memcpy(bloquesmap+2*block_size, &cadena, 5);
	msync(bloquesmap, tamanio_archivo_bloques, MS_SYNC);
}

void atender(){
	if (strcmp(tipo, "GENERICA")==0)
		atender_kernel_generica();
	else if (strcmp(tipo, "STDIN")==0)
		atender_kernel_stdin();
	else if (strcmp(tipo, "STDOUT")==0)
		atender_kernel_stdout();
	else if (strcmp(tipo, "DIALFS")==0)
		atender_kernel_dialfs();
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
	munmap(bitmap, tamanio_archivo_bitarray);
    munmap(bloquesmap, tamanio_archivo_bloques);

	close(fd_bitarray);
	close(fd_bloques);

	terminar_conexiones(2, socket_memoria, socket_kernel);
    if(logger_io) log_destroy(logger_io);
    if(config_io) config_destroy(config_io);
}
	
/////////////////     DIALFS     /////////////////


void crear_archivo_bloques() {
    path_archivo_bloques = malloc(strlen("bloques.dat") + strlen(path_filesystem) + 1);
	if(!path_archivo_bloques) {
		log_error(logger_io, "error maloc bloques.dat");
		return;
	}
	
    sprintf(path_archivo_bloques, "%sbloques.dat", path_filesystem);
	
    int fd = open(path_archivo_bloques, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        log_error(logger_io, "Error al abrir/crear el archivo bloques.dat");
        free(path_archivo_bloques);
        return;
    }
	
    // Si el archivo es nuevo y no tiene tamaño, inicializarlo con el tamaño deseado
    if (tamanio_archivo_bloques == 0) {
        tamanio_archivo_bloques = block_count * block_size;
        if (ftruncate(fd, tamanio_archivo_bloques) == -1) {
            log_error(logger_io, "Error al establecer el tamaño del archivo bloques.dat");
            close(fd);
            free(path_archivo_bloques);
            return;
        }
    }

    close(fd);
}

void levantar_archivo_bitarray(){
	path_archivo_bitarray = malloc(strlen("bitmap.dat") + strlen(path_filesystem) + 1);
    sprintf(path_archivo_bitarray, "%sbitmap.dat", path_filesystem);

	FILE* archivo_bitarray = fopen(path_archivo_bitarray, "r");
	if (!archivo_bitarray) {
		// hay que crearlo
		archivo_bitarray = fopen(path_archivo_bitarray, "w");
		
		tamanio_archivo_bitarray = block_count / 8; // usado en mmap para mayor legibilidad

		int cantidad_bits_en_bytes = block_count / 8;
		int resto = block_count % 8;

		if(resto != 0) {
			log_error(logger_io,"La cantidad de bloques debe ser multiplo de 8");
			return;
		}
		
		char* bitmap = malloc(cantidad_bits_en_bytes); 

        memset(bitmap, 0 , cantidad_bits_en_bytes);

		t_bitarray* bitarray = bitarray_create_with_mode(bitmap, cantidad_bits_en_bytes, MSB_FIRST);
		fwrite(bitarray->bitarray,bitarray->size, 1, archivo_bitarray);
		// fwrite(bitarray->bitarray,floor(bitarray->size / 8), 1, archivo_bitmap); ->2da idea por si rompe 
		fclose(archivo_bitarray);

		fd_bitarray = open(path_archivo_bitarray, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
		if (fd_bitarray == -1){
			log_info(logger_io, "Error al levantar fd del bitarray");
			free(path_archivo_bitarray);
			return;
		}
		if (ftruncate(fd_bitarray, cantidad_bits_en_bytes) == -1) {
            log_error(logger_io, "Error al establecer el tamaño del archivo bitmap.dat");
            close(fd_bitarray);
            free(path_archivo_bitarray);
            return;
        }
	} else {
		fclose(archivo_bitarray);
		fd_bitarray = open(path_archivo_bitarray, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);

		//verifico el tamanio del archivo (en caso de levantar el modulo y ya exista)
		struct stat sbA;
		fstat(fd_bitarray, &sbA);
		tamanio_archivo_bitarray = sbA.st_size;
	}	
}

void levantar_archivo_bloques(){
    path_archivo_bloques = malloc(strlen("bloques.dat") + strlen(path_filesystem) + 1);
	if(!path_archivo_bloques) {
		log_error(logger_io, "error maloc bloques.dat");
		return;
	}
	
    sprintf(path_archivo_bloques, "%sbloques.dat", path_filesystem);

	FILE* archivo_bloques = fopen(path_archivo_bloques, "r");
	if (!archivo_bloques) {
		// hay que crearlo
		fd_bloques = open(path_archivo_bloques, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
		if (fd_bloques == -1) {
			log_error(logger_io, "Error al crear el archivo bloques.dat");
			free(path_archivo_bloques);
			return;
		}
		tamanio_archivo_bloques = block_count * block_size;
        if (ftruncate(fd_bloques, tamanio_archivo_bloques) == -1) {
            log_error(logger_io, "Error al establecer el tamaño del archivo bloques.dat");
            close(fd_bloques);
            free(path_archivo_bloques);
            return;
        }
	} else {
		fclose(archivo_bloques);
		fd_bloques = open(path_archivo_bloques, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
		//verifico el tamanio del archivo (en caso de levantar el modulo y ya exista)
		struct stat sbA;
		fstat(fd_bloques, &sbA);
		tamanio_archivo_bloques = sbA.st_size;
	}
}

void ejecutar_fs_create(){
	
	//REPETICION DE LOGICA EN TODAS LAS IOS
	t_buffer* buffer =  recibir_buffer(socket_kernel);	
	int pid = buffer_read_uint32(buffer);
    t_instruccion* instruccion = buffer_read_instruccion(buffer);
   	destruir_buffer(buffer);
	
	char* nombreArchivo = instruccion -> parametro2;

	if (!hay_espacio_suficiente()){
		log_error(logger_io, "No hay espacio suficiente para crear: %s" , nombreArchivo);
		return;
	}

	char *nombreArchivoPath = malloc(strlen(nombreArchivo) + strlen(path_filesystem) + 1);
	sprintf(nombreArchivoPath, "%s%s", path_filesystem, nombreArchivo);
	
	FILE *archivo_file = fopen(nombreArchivoPath , "w"); 	 
	t_config* metadata = crear_metadata(nombreArchivo, path_filesystem);	

	archivo_t* archivoAgregar = crear_archivo(nombreArchivo , nombreArchivoPath , metadata);

	log_info(logger_io, "DialFS - Crear Archivo: PID: %d - Crear Archivo: %s", pid, nombreArchivo);
	
	list_add(lista_global_archivos_abiertos , archivoAgregar);

	char* lista_de_archivos_abiertos = obtener_lista_archivos_abiertos(lista_global_archivos_abiertos);
	log_info(logger_io, "DialFS - Agregado a lista %s el archivo:  %s", lista_de_archivos_abiertos, nombreArchivo);

	fclose(archivo_file); // necesario para despues reabrir con open

	asignar_bloque(archivoAgregar);

	free(nombreArchivoPath);
	free(nombreArchivo);
}

void asignar_bloque(archivo_t* archivo){
	// la funcion de obtener el indice ya actualiza el bitmap
	int indice = obtener_indice_bloque_libre();
	if (indice == -1){
		log_info(logger_io, "Error al asignar bloque a archivo %s", archivo->nombre_archivo);
		return;
	}

	config_set_value(archivo->metadata, "BLOQUE_INICIAL" , string_itoa(indice));
	// el tamaño sigue siendo cero la primera vez
}

int obtener_indice_bloque_libre(){
	bool encontrado = false;
	int i = 0;
	while (!encontrado && i < bitarray_get_max_bit(bitmap)) {
		// el test bit devuelve false (0) o true(1), si esta libre tiene que dar false
		if (!bitarray_test_bit(bitmap, i)) {
			encontrado = true;
			bitarray_set_bit(bitmap, i);  // seteamos como ocupado
			if (msync(bitmap, tamanio_archivo_bitarray, MS_SYNC) == -1)
            	perror("msync fileA");
		} else
			i++;
	}

	// si el archivo de bloques esta lleno retorna -1
	return (encontrado)? i : -1;
}


bool hay_espacio_suficiente(){
	bool hay_espacio = false;
	int i = 0;
	while (i < bitarray_get_max_bit(bitmap)){
		if (!bitarray_test_bit(bitmap, i))
			hay_espacio = true;

		i++;
	}

	return hay_espacio;
}


archivo_t* crear_archivo(char* nombre, char* path, t_config* metadata){
	archivo_t* archivo_creado = malloc(sizeof(archivo_t));

	archivo_creado->nombre_archivo = string_new();
	string_append(&archivo_creado->nombre_archivo, nombre);

	archivo_creado->path = string_new();
	string_append(&archivo_creado->path, path);

	archivo_creado->metadata = metadata;

	return archivo_creado;
}

void ejecutar_fs_truncate(){
	t_buffer* buffer =  recibir_buffer(socket_kernel);	
	int pid = buffer_read_uint32(buffer);
    t_instruccion* instruccion = buffer_read_instruccion(buffer);
   	destruir_buffer(buffer);
	// por el warning
	pid = pid;

	uint32_t tamanio_solicitado = atoi(instruccion->parametro3);

	archivo_t* archivo_buscado = obtener_archivo_con_nombre(instruccion->parametro2);
	if (!archivo_buscado){
		log_error(logger_io, "El archivo solicitado para escribir no existe, terminando.");
		exit(EXIT_FAILURE);
	}
	
	int tamanio_archivo = config_get_int_value(archivo_buscado->metadata, "TAMANIO_ARCHIVO");

	if (tamanio_archivo < tamanio_solicitado)
		reducir_tamanio(archivo_buscado, tamanio_solicitado);
	else	
		ampliar_tamanio(archivo_buscado, tamanio_solicitado);

}

void ampliar_tamanio(archivo_t* archivo, uint32_t tamanio_solicitado){

	/*
		FALTA TODO EL CASO QUE NECESITA COMPACTAR PORQUE NO
		HAY BLOQUES CONTIGUOS SUFICIENTES
	*/

	int tamanio_archivo = config_get_int_value(archivo->metadata, "TAMANIO_ARCHIVO");
	int bloques_asignados_antes = ceil(tamanio_archivo / block_size);
	int bloques_a_asginar = ceil(tamanio_solicitado / block_size) - bloques_asignados_antes;
	int i = 0;
	// si tenia 4 bloques asignados, los bits 0 1 2 y 3 estaban en 1 (ocupados) 
	// a partir de la posicion 4 quiero setear en 1 hasta cumplir lo pedido
	int bit_inicial = bloques_asignados_antes;
	while (i < bloques_a_asginar){
		bitarray_set_bit(bitmap, bit_inicial);

		// limpio el espacio asignado, no se si es sumamente necesario
		// pero es prolijo y evita el uso de datos basura
		memset(bloquesmap+(bit_inicial+i) * block_size, 0, block_size);
		i ++;
	}

	msync(bitmap->bitarray, bitmap->size, MS_SYNC);
	msync(bloquesmap, tamanio_archivo_bloques, MS_SYNC);

	config_set_value(archivo->metadata, "TAMANIO_ARCHIVO", string_itoa(tamanio_solicitado));
	config_save(archivo->metadata);
}

void reducir_tamanio(archivo_t* archivo, uint32_t tamanio_solicitado){
	int bloques_a_asginar = ceil(tamanio_solicitado / block_size);
	int tamanio_archivo = config_get_int_value(archivo->metadata, "TAMANIO_ARCHIVO");
	int bloques_asignados_antes = ceil(tamanio_archivo / block_size);
	int bloque_inicial = config_get_int_value(archivo->metadata, "BLOQUE_INICIAL");
	int i = bloque_inicial;

	while (i < bloques_asignados_antes) {
		if (i >= bloques_a_asginar) {
			bitarray_clean_bit(bitmap, i);
		}
		i ++;
	}
	msync(bitmap->bitarray, bitmap->size, MS_SYNC);

	config_set_value(archivo->metadata, "TAMANIO_ARCHIVO", string_itoa(tamanio_solicitado));
	config_save(archivo->metadata);
}

void ejecutar_fs_delete(){
	t_buffer* buffer =  recibir_buffer(socket_kernel);	
	int pid = buffer_read_uint32(buffer);
    t_instruccion* instruccion = buffer_read_instruccion(buffer);
   	destruir_buffer(buffer);
	char* nombreArchivo = instruccion -> parametro2;


	//elimino de la lista global de archivos abiertos el archivo a eliminar y eliminar el archivo en si 
	bool encontrado = false;
	eliminar_archivo_de_lista( lista_global_archivos_abiertos ,nombreArchivo, &encontrado);	
	if (!encontrado) {
		destruir_buffer(buffer);
		log_info(logger_io, "El archivo buscado no existe.");
		return;
	}


	log_info(logger_io , "DialFS - Eliminar Archivo: PID: %d - Eliminar Archivo: %s " , pid, nombreArchivo);


	log_info (logger_io ,"DialFS - Inicio Compactación: PID: %d - Inicio Compactación." , pid );

	//void hacer_compactacion();
	
	log_info(logger_io , "DialFS - Fin Compactación: PID: %d - Fin Compactación. " , pid);
} 

void liberar_espacio_en_disco(int bloque_inicial, int tamanio_archivo){
	int cantidad_bloques_totales = ceil(tamanio_archivo / block_size);
	int i = 0;
	while (i < cantidad_bloques_totales){
		bitarray_clean_bit(bitmap, bloque_inicial + i);
		i++;
	}
}

void liberar_archivo(archivo_t* archivo) {
    if (archivo != NULL) {

		liberar_espacio_en_disco(config_get_int_value(archivo->metadata, "BLOQUE_INICIAL"), config_get_int_value(archivo->metadata, "TAMANIO_ARCHIVO"));
		msync(bitmap, tamanio_archivo_bitarray, MS_SYNC);

        if (archivo->nombre_archivo != NULL) {
            free(archivo->nombre_archivo);
        }
        if (archivo->path != NULL) {
            free(archivo->path);
        }
        if (archivo->metadata != NULL) {
        	config_destroy(archivo->metadata);
        }  
		
        free(archivo);
    }
}

void eliminar_archivo_de_lista(t_list* lista, char* nombreArchivo, bool* encontrado) {
    for (int i = 0; i < list_size(lista); i++) {
        archivo_t* archivo = list_get(lista, i);
        if (strcmp(archivo->nombre_archivo, nombreArchivo) == 0) {
			// hay que setear sus bits de uso en 0 
            list_remove(lista, i);
            liberar_archivo(archivo);
			*encontrado = true;
			log_info(logger_io, "Se elimina el archivo de nombre: %s", nombreArchivo);
            break;
        }
    }
}



/*			
void ejecutar_fs_truncate(){}
*/

/*
IO_FS_WRITE 
(Interfaz, Nombre Archivo, Registro Dirección, Registro Tamaño, Registro Puntero Archivo):
Esta instrucción solicita al Kernel que mediante la interfaz seleccionada,
 se lea desde Memoria la cantidad de bytes indicadas por el Registro Tamaño 
 a partir de la dirección lógica que se encuentra en el Registro Dirección 
 y se escriban en el archivo a partir del valor del Registro Puntero Archivo.
*/

/*
char* 00 30 12
bloques.dat
ptr archivoA  3
archivoA->metadata->bloqueInicial
[      _{   | 00 30 12   /             /             }_                                      ]

void* bloques   bloques+(bloqueInicial*tamañoBloque)+ptrArchivo

copias en el archivo usuario 00 30 12


*/


archivo_t* obtener_archivo_con_nombre(char* nombre){
	bool encontrado = false;
	int i = 0;
	archivo_t* archivo = NULL;

	while (!encontrado && i < list_size(lista_global_archivos_abiertos)){
		archivo = list_get(lista_global_archivos_abiertos, i);
		if (strcmp(archivo->nombre_archivo, nombre) == 0){
			encontrado = true;
		}
		i++;
	}

	return archivo;
}

void ejecutar_fs_write(){
    t_buffer* buffer = recibir_buffer(socket_kernel);
    int pid = buffer_read_uint32(buffer);
	t_instruccion* instruccion = buffer_read_instruccion(buffer);
    destruir_buffer(buffer);

    char* nombreArchivo = instruccion->parametro2;
    uint32_t registroDireccion = atoi(instruccion->parametro3);
    uint32_t registroTamanio = atoi(instruccion->parametro4);
	uint32_t registroPunteroArchivo = atoi(instruccion->parametro5);

	// se le pide a memoria lo que hay en la direccion fisica
	enviar_codigo(socket_memoria, IO_FS_WRITE);
	buffer = crear_buffer();
	buffer_write_uint32(buffer, pid);
	buffer_write_uint32(buffer, registroDireccion);
	buffer_write_uint32(buffer, registroTamanio);
	enviar_buffer(buffer, socket_memoria);
	destruir_buffer(buffer);

	// esperamos el string
	buffer = recibir_buffer(socket_memoria);
	char* contenido_a_escribir = buffer_read_string(buffer);
	destruir_buffer(buffer);

    char* nombreArchivoPath = malloc(strlen(nombreArchivo) + strlen(path_filesystem) + 1);
    sprintf(nombreArchivoPath, "%s/%s", path_filesystem, nombreArchivo);

	// se escribe en bloques.dat

	archivo_t* archivo_buscado = obtener_archivo_con_nombre(nombreArchivo);
	if (!archivo_buscado){
		log_error(logger_io, "El archivo solicitado para escribir no existe, terminando.");
		exit(EXIT_FAILURE);
	}

	int bloque_inicial_archivo = config_get_int_value(archivo_buscado->metadata, "BLOQUE_INICIAL");
	memcpy(bloquesmap+bloque_inicial_archivo*block_size+registroPunteroArchivo, contenido_a_escribir, registroTamanio);
	msync(bloquesmap, tamanio_archivo_bloques, MS_SYNC);

	// actualizamos el tamaño del archivo
	int tamanio_anterior = config_get_int_value(archivo_buscado->metadata, "TAMANIO_ARCHIVO");
	config_set_value(archivo_buscado->metadata, "TAMANIO_ARCHIVO", string_itoa(tamanio_anterior+registroTamanio));
	config_save(archivo_buscado->metadata);

    log_info(logger_io,"DialFS - Escribir Archivo: PID: %d - Escribir Archivo: %s - Tamaño a Escribir: %d - Puntero Archivo: %d", pid, nombreArchivo, registroTamanio, registroPunteroArchivo);
    
    free(nombreArchivoPath);
    free(nombreArchivo);
}


void ejecutar_fs_read(){
	t_buffer* buffer = recibir_buffer(socket_kernel);
    int pid = buffer_read_uint32(buffer);
	t_instruccion* instruccion = buffer_read_instruccion(buffer);
    destruir_buffer(buffer);

    char* nombreArchivo = instruccion->parametro2;
    uint32_t registroDireccion = atoi(instruccion->parametro3);
    uint32_t registroTamanio = atoi(instruccion->parametro4);
	uint32_t registroPunteroArchivo = atoi(instruccion->parametro5);

	archivo_t* archivo_buscado = obtener_archivo_con_nombre(nombreArchivo);
	if (!archivo_buscado){
		log_error(logger_io, "El archivo solicitado para escribir no existe, terminando.");
		exit(EXIT_FAILURE);
	}

	int bloque_inicial_archivo = config_get_int_value(archivo_buscado->metadata, "BLOQUE_INICIAL");

	char* cadena_a_escribir_en_memoria = malloc(registroTamanio*sizeof(char)+1);
	memset(cadena_a_escribir_en_memoria, 0, registroTamanio);

	memcpy(cadena_a_escribir_en_memoria, bloquesmap+bloque_inicial_archivo*block_size+registroPunteroArchivo, registroTamanio);
	cadena_a_escribir_en_memoria[registroTamanio] = '\0';

	// le enviamos a memoria todo lo necesario para que escriba

	enviar_codigo(socket_memoria, IO_FS_READ);
	buffer = crear_buffer();
	buffer_write_uint32(buffer, pid);
	buffer_write_uint32(buffer, registroDireccion);
	buffer_write_uint32(buffer, registroTamanio);
	buffer_write_string(buffer, cadena_a_escribir_en_memoria);
	enviar_buffer(buffer, socket_memoria);
	destruir_buffer(buffer);

	mensajeMemoria cod = recibir_codigo(socket_memoria);
	if (cod != OK) {
		log_error(logger_io, "Error al escribir en memoria la cadena: %s", cadena_a_escribir_en_memoria);
		exit(EXIT_FAILURE);
	}

	free(cadena_a_escribir_en_memoria);
	destruir_instruccion(instruccion);
}

//-------------------------------------------------------------------------------------


t_config* crear_metadata(char* nombreArchivo, char* path_filesystem) {
    char *metadataDirPath = malloc(strlen(path_filesystem) + strlen("metadata") + 1);
    sprintf(metadataDirPath, "%smetadata", path_filesystem);
    mkdir(metadataDirPath, 0755);  // Crear el directorio con permisos adecuados
	log_info(logger_io, "Se creo la carpeta con path %s", metadataDirPath);

    // Crear la ruta completa del archivo de metadata
    char *path_metadata = malloc(strlen(metadataDirPath) + strlen("/") + strlen(nombreArchivo) + 1);
    sprintf(path_metadata, "%s/%s", metadataDirPath, nombreArchivo);
	log_info(logger_io, "Ruta del archivo de metadata: %s", path_metadata);

    // Crear el archivo metadata
	FILE *file = fopen(path_metadata, "w");
    if (file == NULL) {
        log_error(logger_io, "No se pudo crear el archivo de metadata: %s", path_metadata);
        free(path_metadata);
        free(metadataDirPath);
        return NULL;
    }
    fclose(file);
	
    // Crear el config en arhvio metadata creado antes
    metadata = config_create(path_metadata);
    if (metadata == NULL) {
        log_error(logger_io, "No se pudo crear el metadata del archivo: %s", path_metadata);
        free(path_metadata);
        free(metadataDirPath);
        return NULL;
    }

	//Inicializacion de datos del config
	config_set_value(metadata, "BLOQUE_INICIAL", "-1"); //-1 para decir que no tiene ningun bloque asociado 
	config_set_value(metadata, "TAMANIO_ARCHIVO" , "0");
	config_save(metadata); // entedemos que guarda los cambios

	free(path_metadata);
	free(metadataDirPath);

    return metadata;
}

char* obtener_lista_archivos_abiertos(t_list* lista){
	char* aux = string_new();
    string_append(&aux,"[");
    char* aux_2;
    for(int i = 0 ; i < list_size(lista); i++){
        archivo_t* archivo = list_get(lista,i);
        aux_2 = archivo->nombre_archivo;
        string_append(&aux, aux_2);
        if(i != list_size(lista)-1)
            string_append(&aux,", ");
    }
    string_append(&aux,"]");
    return aux;
}




/*void agregar_bloque(FILE* archivo , int cantidad_bloques) {   

    if (cantidad_bloques == 1) {
        uint32_t nro_bloque = obtener_nro_bloque_libre();

        // NUEVO: Declaración de variable que falta
        t_bloque* primerBloqueLibre = NULL;

        // Seek al bloque libre obtenido
        fseek(archivo, nro_bloque * block_size, SEEK_SET);

        // Modificar el bitmap para marcar el bloque como ocupado
        modificar_BitMap(nro_bloque, 1);

        // Abrir el archivo de bloques (asumo que ya está abierto fuera de esta función)
        FILE *file = fopen("bloques.dat", "rb+");
        if (file == NULL) {
            log_error(logger_io, "Error abriendo bloques.dat");
            return;
        }

        // Escribir el número del nuevo bloque en el archivo de bloques
        fseek(file, nro_bloque * block_size, SEEK_SET);
        uint32_t nuevoBloque = nro_bloque;
        fwrite(&nuevoBloque, block_size, 1, file);

        // Cerrar el archivo de bloques
        fclose(file);
    }
} */
/*
void agregar_bloque(uint32_t bloque_inicial) {
    uint32_t nuevo_bloque = obtener_nro_bloque_libre();
    
    FILE *file = fopen("bloques.dat", "rb+");
    if (file == NULL) {
        perror("Error abriendo bloques.dat");
        return;
    }

    fseek(file, nuevo_bloque * sizeof(uint32_t), SEEK_SET);
    fwrite(&bloque_inicial, sizeof(uint32_t), 1, file); // Conectar el nuevo bloque al último bloque del archivo

    fclose(file);

    modificar_BitMap(nuevo_bloque, 1); // Marcar el nuevo bloque como ocupado en el bitmap
}

*/

/*
void cambiar_tamanio_archivo(uint32_t bloque_inicial, uint32_t nuevo_tamanio) {
    uint32_t tamanio_anterior = obtener_tamanio_archivo(bloque_inicial); // Función para obtener el tamaño actual del archivo

    if (nuevo_tamanio > tamanio_anterior) {
        // Agregar bloques adicionales si el nuevo tamaño es mayor
        int bloques_a_agregar = (nuevo_tamanio - tamanio_anterior + block_size - 1) / block_size;
        for (int i = 0; i < bloques_a_agregar; i++) {
            agregar_bloque(bloque_inicial);
        }
    } else if (nuevo_tamanio < tamanio_anterior) {
        // Sacar bloques si el nuevo tamaño es menor
        int bloques_a_quitar = (tamanio_anterior - nuevo_tamanio + block_size - 1) / block_size;
        for (int i = 0; i < bloques_a_quitar; i++) {
            sacar_bloque_archivo(bloque_inicial);
        }
    }
}*/


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
    modificar_BitMap(bloqueActual, 0);

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
void modificar_BitMap(uint32_t nroBloque, uint8_t valor){
    FILE *file = fopen("bitmap.dat", "rb+");
    if (file == NULL) {
        log_error(logger_io , "Error abriendo bitmap.dat");
        return;
    }
    if (fseek(file, nroBloque, SEEK_SET) != 0) {
        log_error(logger_io , "Error buscando en bitmap.dat");
        fclose(file);
        return;
    }
    if (fwrite(&valor, sizeof(uint8_t), 1, file) != 1) {
        log_error(logger_io, "Error escribiendo en bitmap.dat");
    }
    fclose(file);
}
//-------------------------------------------------------------------------
// Leemos del archivo bloques.dat y chequeamos si el bit esta usado o no

uint8_t leer_de_bitmap(uint32_t nroBloque) {
    FILE *file = fopen("bitmap.dat", "rb");
    if (file == NULL) {
        perror("Error abriendo bitmap.dat");
        return UINT8_MAX;
    }

    // Mover el puntero del archivo a la posición correspondiente
    if (fseek(file, nroBloque, SEEK_SET) != 0) {
        perror("Error buscando en bitmap.dat");
        fclose(file);
        return UINT8_MAX;
    }

    // Leer el valor en la posición correspondiente
    uint8_t valor;
    if (fread(&valor, sizeof(uint8_t), 1, file) != 1) {
        perror("Error leyendo de bitmap.dat");
        fclose(file);
        return UINT8_MAX;
    }

    // Cerrar el archivo
    fclose(file);

    return valor;
}
