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

void avisar_finalizacion_a_kernel(){
	enviar_codigo(socket_kernel, LIBRE);
	t_buffer* buffer = crear_buffer();	
	buffer_write_string(buffer, nombreIO);
	enviar_buffer(buffer, socket_kernel);
	destruir_buffer(buffer);
}

void atender_kernel_generica(){
	while(1){
		codigoInstruccion cod = recibir_codigo(socket_kernel);
		if (cod == UINT8_MAX){
			return;
		}
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
				avisar_finalizacion_a_kernel();
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
		if (cod == UINT8_MAX){
			return;
		}
		switch (cod){
			case IO_STDIN_READ:
					ejecutar_std_in();
					avisar_finalizacion_a_kernel();
				break;
			default:
				break;
		}
	}
}

void atender_kernel_stdout(){
	while(1){
		codigoInstruccion cod = recibir_codigo(socket_kernel);
		if (cod == UINT8_MAX){
			return;
		}
		switch (cod){
			case IO_STDOUT_WRITE:
				ejecutar_std_out();
				avisar_finalizacion_a_kernel();
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
	lista_global_archivos_abiertos = list_create();
	levantar_archivo_bitarray();
	levantar_archivo_bloques();
	levantar_archivos_creados();


	bitmap = malloc(sizeof(t_bitarray));
	bitmap->size = tamanio_archivo_bitarray;
	bitmap->mode = MSB_FIRST;
	bitmap->bitarray = mmap(NULL, tamanio_archivo_bitarray, PROT_READ | PROT_WRITE, MAP_SHARED, fd_bitarray, 0);
	if (bitmap == MAP_FAILED) {
		//log_info(logger_io, "error al crear el bitmap");
	}
	bloquesmap = mmap(NULL, tamanio_archivo_bloques, PROT_READ | PROT_WRITE, MAP_SHARED, fd_bloques, 0);
	if (bloquesmap == MAP_FAILED) {
		//log_info(logger_io, "error al crear bloquesmap");
	}
}

void atender_kernel_dialfs(){
	inicializar_fs();
	int unidad_tiempo_trabajo = config_get_int_value(config_io, "TIEMPO_UNIDAD_TRABAJO");
	//test();

	while(1){
		codigoInstruccion cod = recibir_codigo(socket_kernel);
		if (cod == UINT8_MAX){
			return;
		}
		switch (cod){
			case IO_FS_CREATE:
				usleep(unidad_tiempo_trabajo * 1000);
				ejecutar_fs_create();
				avisar_finalizacion_a_kernel();
				break;
			case IO_FS_DELETE:
				usleep(unidad_tiempo_trabajo * 1000);
				ejecutar_fs_delete();
				avisar_finalizacion_a_kernel();
				break;
			case IO_FS_TRUNCATE:
				usleep(unidad_tiempo_trabajo * 1000);
				ejecutar_fs_truncate();
				avisar_finalizacion_a_kernel();
				break;
			case IO_FS_WRITE:
				usleep(unidad_tiempo_trabajo * 1000);
				ejecutar_fs_write();
				avisar_finalizacion_a_kernel();
				break;
			case IO_FS_READ:
				usleep(unidad_tiempo_trabajo * 1000);
				ejecutar_fs_read();
				avisar_finalizacion_a_kernel();
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

	log_info(logger_io, "PID: %d - Operacion: %s", pid, obtener_nombre_instruccion(instruccion));


	int limite_bytes = atoi(instruccion->parametro3);
	printf("\nLimite de bytes  %d", limite_bytes);

	char* leido = readline("> ");
	printf("\n el string leido tiene  %ld bytes", strlen(leido) + 1);

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
}

void ejecutar_std_out(){
	t_buffer* buffer = recibir_buffer(socket_kernel);
	uint32_t pid = buffer_read_uint32(buffer);
	t_instruccion* instruccion = buffer_read_instruccion(buffer);
	destruir_buffer(buffer);

	log_info(logger_io, "PID: %d - Operacion: %s", pid, obtener_nombre_instruccion(instruccion));

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
}

void conectar_memoria(){
	ip = config_get_string_value(config_io, "IP_MEMORIA");
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
	ip = config_get_string_value(config_io, "IP_KERNEL");
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

void setear_path_base_dial(){
    char path[1024];
    ssize_t count = readlink("/proc/self/exe", path, sizeof(path) - 1);

    if (count == -1) {
        perror("readlink");
        return;
    }

    path[count] = '\0'; // Asegura que la cadena esté terminada en nulo

    // Obtiene el directorio del ejecutable
    char *dir = dirname(path);

    // Retrocede un directorio
    char *parent_dir = dirname(dir);

    // Reserva memoria para la cadena resultante
    size_t len = strlen(parent_dir);
    char *result = malloc(len + 1);

    strcpy(result, parent_dir); // Copia la ruta resultante a la memoria reservada

    // Elimina la última '/' si está presente
    if (len > 1 && result[len - 1] == '/') {
        result[len - 1] = '\0';
    }

	// hasta aca tenemos el path local
	// le sumamos /src/baseDIALFS

	char* ruta_completa = malloc(sizeof(char) * 150);

	string_append(&ruta_completa, result);
	string_append(&ruta_completa, "/src/baseDIALFS/");

    config_set_value(config_io, "PATH_BASE_DIALFS", ruta_completa);
    config_save(config_io);

	free(ruta_completa);
}


void levantar_config(){
    config_io = config_create(config_path);
	if (!config_io) {
		perror("Error al iniciar config de IO\n");
		exit(EXIT_FAILURE);
	}

	setear_path_base_dial();

	printf("\nIngrese IP de Memoria:\n");
    char* ip_memo = readline("> ");
    config_set_value(config_io, "IP_MEMORIA", ip_memo);
    config_save(config_io);

	printf("\nIngrese IP de Kernel:\n");
    char* ip_kernel = readline("> ");
    config_set_value(config_io, "IP_KERNEL", ip_kernel);
    config_save(config_io);
}

void destruir_archivo(void* arch){
	archivo_t* archivo = arch;

	config_destroy(archivo->metadata);
	free(archivo->nombre_archivo);
	free(archivo);
}

void terminar_programa(){
	munmap(bitmap->bitarray, tamanio_archivo_bitarray);
    munmap(bloquesmap, tamanio_archivo_bloques);
	free(bloquesmap);
	bitarray_destroy(bitmap);

	close(fd_bitarray);
	close(fd_bloques);

    if(logger_io) log_destroy(logger_io);
    if(config_io) config_destroy(config_io);

	free(nombreIO);
	free(tipo);
	free(config_path);

	sem_destroy(&sema_memoria);
	sem_destroy(&sema_kernel);
	sem_destroy(&terminar_io);

	list_destroy_and_destroy_elements(lista_global_archivos_abiertos, destruir_archivo);

	if (socket_kernel != -1)
		close(socket_kernel);
	if (socket_memoria != -1)
		close(socket_memoria);
}
	
/////////////////     DIALFS     /////////////////

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
			//log_error(logger_io,"La cantidad de bloques debe ser multiplo de 8");
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
			//log_info(logger_io, "Error al levantar fd del bitarray");
			free(path_archivo_bitarray);
			return;
		}
		if (ftruncate(fd_bitarray, cantidad_bits_en_bytes) == -1) {
            //log_error(logger_io, "Error al establecer el tamaño del archivo bitmap.dat");
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
		log_error(logger_io, "error malloc bloques.dat");
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


void levantar_archivos(const char *path) {
    DIR *dir;
    struct dirent *entry;

    if ((dir = opendir(path)) == NULL) {
        //perror("opendir() error");
        return;
    } else {
        //printf("Contenido del directorio: %s\n", path);
        while ((entry = readdir(dir)) != NULL) {
            // Ignorar "." y ".."
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                char fullpath[1024];
                snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);
                printf("%s\n", fullpath);
				crear_archivo_desde_path(fullpath,entry->d_name);
            }
        }
        closedir(dir);
    }
}

void crear_archivo_desde_path(char* path, char* nombre_archivo){
	char* ruta_completa = string_new();
	string_append(&ruta_completa, path);
	string_append(&ruta_completa, "/");
	string_append(&ruta_completa, nombre_archivo);
	t_config* config_archivo = config_create(path);
	archivo_t* archivo = malloc(sizeof(archivo_t));
	archivo->metadata = config_archivo;
	archivo->nombre_archivo = string_new();
	string_append(&archivo->nombre_archivo, nombre_archivo);
	list_add(lista_global_archivos_abiertos, archivo);
	//log_info(logger_io, "Ruta Obtenida: %s  Nombre Archivo: %s",ruta_completa, nombre_archivo);
	free(ruta_completa);
}

void levantar_archivos_creados(){
    char initial_path[1024];
    if (getcwd(initial_path, sizeof(initial_path)) == NULL) {
        perror("getcwd() error");
        return;
    }
	// Cambiar el directorio actual al directorio padre ("..")
    if (chdir("..") != 0) {
        //perror("chdir(..) error");
        return;
    }

    // Cambiar al directorio "src"
    if (chdir("src") != 0) {
        perror("chdir(src) error");
        return;
    }

    // Cambiar al directorio "baseDIALFS"
    if (chdir("baseDIALFS") != 0) {
        perror("chdir(baseDIALFS) error");
        return;
    }
	// Cambiar al directorio "metadata"
    if (chdir("metadata") != 0) {
        perror("chdir(metadata) error");
        return;
    }
	char current_path[1024];
    if (getcwd(current_path, sizeof(current_path)) == NULL) {
        perror("getcwd() error");
        return;
    }

    // Listar archivos en el directorio actual
    levantar_archivos(current_path);

    // Volver al directorio inicial
    if (chdir(initial_path) != 0) {
        perror("chdir() error");
        return;
    }
    return;
}


void ejecutar_fs_create(){
	
	//REPETICION DE LOGICA EN TODAS LAS IOS
	t_buffer* buffer =  recibir_buffer(socket_kernel);	
	int pid = buffer_read_uint32(buffer);
    t_instruccion* instruccion = buffer_read_instruccion(buffer);
   	destruir_buffer(buffer);

	log_info(logger_io, "PID: %d - Operacion: %s", pid, obtener_nombre_instruccion(instruccion));
	char* nombreArchivo = instruccion -> parametro2;
	
	log_info(logger_io, "PID: %d - Crear Archivo: %s", pid, nombreArchivo);

	if (!hay_espacio_suficiente()){
		log_error(logger_io, "No hay espacio suficiente para crear: %s" , nombreArchivo);
		return;
	}
	
	t_config* metadata = crear_metadata(nombreArchivo, path_filesystem);	

	archivo_t* archivoAgregar = crear_archivo(nombreArchivo, metadata);

	//log_info(logger_io, "DialFS - Crear Archivo: PID: %d - Crear Archivo: %s", pid, nombreArchivo);
	
	list_add(lista_global_archivos_abiertos , archivoAgregar);

	//log_info(logger_io, "DialFS - Agregado a lista %s el archivo:  %s", lista_de_archivos_abiertos, nombreArchivo);

	asignar_bloque(archivoAgregar);

	free(nombreArchivo);
}

void asignar_bloque(archivo_t* archivo){
	// la funcion de obtener el indice ya actualiza el bitmap
	int indice = obtener_indice_bloque_libre();
	if (indice == -1){
		//log_info(logger_io, "Error al asignar bloque a archivo %s", archivo->nombre_archivo);
		return;
	}

	config_set_value(archivo->metadata, "BLOQUE_INICIAL" , string_itoa(indice));
	config_save(archivo->metadata);
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
			if (msync(bitmap->bitarray, bitmap->size, MS_SYNC) == -1) perror("Error en la sincronizacion");
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


archivo_t* crear_archivo(char* nombre, t_config* metadata){
	archivo_t* archivo_creado = malloc(sizeof(archivo_t));

	archivo_creado->nombre_archivo = string_new();
	string_append(&archivo_creado->nombre_archivo, nombre);

	archivo_creado->metadata = metadata;

	return archivo_creado;
}

void ejecutar_fs_truncate(){
	t_buffer* buffer =  recibir_buffer(socket_kernel);	
	int pid = buffer_read_uint32(buffer);
    t_instruccion* instruccion = buffer_read_instruccion(buffer);
   	destruir_buffer(buffer);

	log_info(logger_io, "PID: %d - Operacion: %s", pid, obtener_nombre_instruccion(instruccion));
	uint32_t tamanio_solicitado = atoi(instruccion->parametro3);
	log_info(logger_io, "PID: %d - Truncar Archivo: %s - Tamaño: %d", pid, instruccion->parametro2, tamanio_solicitado);
	

	archivo_t* archivo_buscado = obtener_archivo_con_nombre(instruccion->parametro2);
	if (!archivo_buscado){
		//log_error(logger_io, "El archivo solicitado para escribir no existe, terminando.");
		exit(EXIT_FAILURE);
	}
	
	int tamanio_archivo = config_get_int_value(archivo_buscado->metadata, "TAMANIO_ARCHIVO");


	//log_info(logger_io , "Cantidad de bloques del archivo: %s antes de modificacion: %d" ,archivo_buscado->nombre_archivo, tamanio_archivo );


	if (tamanio_solicitado < tamanio_archivo){

		reducir_tamanio(archivo_buscado, tamanio_solicitado);
		//int tamanio_archivo_actualizado = config_get_int_value(archivo_buscado->metadata, "TAMANIO_ARCHIVO");
		//log_info(logger_io , "Cantidad de bloques del archivo : %s  despues de la reduccion : %d " , archivo_buscado->nombre_archivo, tamanio_archivo_actualizado);
	}
	else {

		ampliar_tamanio(archivo_buscado, tamanio_solicitado, pid);
		//int tamanio_archivo_actualizado = config_get_int_value(archivo_buscado->metadata, "TAMANIO_ARCHIVO");
		//log_info(logger_io , "Cantidad de bloques del archivo : %s  despues de la ampliacion : %d " , archivo_buscado->nombre_archivo, tamanio_archivo_actualizado);
		//log_info(logger_io , "PID: %d - Truncar archivo : %s " ,pid, archivo_buscado->nombre_archivo);

	}

}

bool hay_bloques_contiguos_al_archivo(uint32_t cantidad, int a_partir_de_indice){

	int limite_array = bitarray_get_max_bit(bitmap);
	int bloques_contiguos_disponibles = 0;
	bool bit_en_uno = false; // este se usa para el caso en donde me tope con un bit 1 y tenga que frenar el conteo
	int i = a_partir_de_indice;

	while (i < limite_array && bloques_contiguos_disponibles != cantidad && !bit_en_uno) {
		if (!bitarray_test_bit(bitmap, i)){
			bloques_contiguos_disponibles++;
		} else {
			bit_en_uno = true; // en este caso ya no hay mas contiguos
		}
		i++;
	} 

	return (bloques_contiguos_disponibles == cantidad);
}

// incluyendo libres dispersos
bool hay_bloques_necesarios(uint32_t cantidad){

	int limite_array = bitarray_get_max_bit(bitmap);
	int bloques_disponibles = 0;
	
	int i = 0;

	while (i < limite_array) {
		if (!bitarray_test_bit(bitmap, i)){
			bloques_disponibles++;
		} 
		i++;
	} 

	return (bloques_disponibles >= cantidad);
}

// --------------------------------------------------------------------------------------------------------------------------------------------------

int recrear_bloquesmap(archivo_t* archivo_a_ampliar, uint32_t cantidad_bloques_nuevos){
	int i = 0;
	archivo_t* archivo_lista = NULL;
	int tamanio_archivo = 0;
	int bloque_inicial = 0;
	int bloques_asignados_antes = 0;

	int ultimo_indice_disponible = 0;
	int offset_bloquesmap = 0;
	int offset_bloquesmap_paralelo = 0;
	
	while (i < list_size(lista_global_archivos_abiertos)){
		archivo_lista = list_get(lista_global_archivos_abiertos, i);

		if (strcmp(archivo_lista->nombre_archivo, archivo_a_ampliar->nombre_archivo) != 0){
			tamanio_archivo = config_get_int_value(archivo_lista->metadata, "TAMANIO_ARCHIVO");
			bloque_inicial = config_get_int_value(archivo_lista->metadata, "BLOQUE_INICIAL");
			bloques_asignados_antes = 1;
			if (tamanio_archivo != 0)
				bloques_asignados_antes = ceil((float)tamanio_archivo / (float)block_size);

			config_set_value(archivo_lista->metadata, "BLOQUE_INICIAL", string_itoa(ultimo_indice_disponible));
			config_save(archivo_lista->metadata);

			offset_bloquesmap += bloque_inicial * block_size;
			memcpy(bloquesmap_paralelo + offset_bloquesmap_paralelo, bloquesmap + offset_bloquesmap, bloques_asignados_antes * block_size);
			offset_bloquesmap_paralelo += bloques_asignados_antes * block_size;

			// como siempre se copian contiguos el ultimo indice va a ser la cantidad copiada (por arrancar de 0)
			ultimo_indice_disponible += bloques_asignados_antes;
			//ultimo_indice_disponible += offset_bloquesmap_paralelo / block_size;
		}
		
		i++;
	}

	// copiamos el archivo ampliado ahora
	tamanio_archivo = config_get_int_value(archivo_a_ampliar->metadata, "TAMANIO_ARCHIVO");
	bloque_inicial = config_get_int_value(archivo_a_ampliar->metadata, "BLOQUE_INICIAL");
	bloques_asignados_antes = 1;
	if (tamanio_archivo != 0)
		bloques_asignados_antes = ceil((float)tamanio_archivo / (float)block_size);

	config_set_value(archivo_a_ampliar->metadata, "BLOQUE_INICIAL", string_itoa(ultimo_indice_disponible));
	config_save(archivo_a_ampliar->metadata);

	offset_bloquesmap += bloque_inicial * block_size;
	memcpy(bloquesmap_paralelo + offset_bloquesmap_paralelo, bloquesmap + offset_bloquesmap, bloques_asignados_antes * block_size);

	ultimo_indice_disponible += bloques_asignados_antes + cantidad_bloques_nuevos;

	// ya que el indice de bloques es el mismo que el indice de bits lo reutilizamos
	// el ultimo indice disponible tiene que contar a los nuevos bloques para 
	// que el bitmap se pueda actualizar

	return ultimo_indice_disponible;
}

void compactar_y_asignar(archivo_t* archivo, uint32_t tamanio_solicitado, uint32_t pid){
	log_info(logger_io, "PID: %d - Inicio Compactacion", pid);

	// la idea es construir un nuevo void* para despues solamente pisar el actual
	// y usar msync para actualizar, esto no requiere remapeo ni nada
	bloquesmap_paralelo = malloc(block_count*block_size);
	memset(bloquesmap_paralelo, 0, block_count*block_size);

	// ahora hay que empezar a llenar el paralelo
	int indice_ultimo_bit_disponible = recrear_bloquesmap(archivo, tamanio_solicitado);

	// ahora el void* paralelo esta cargado, solo queda pisar los datos del anterior
	memcpy(bloquesmap, bloquesmap_paralelo, block_count*block_size);
	msync(bloquesmap, tamanio_archivo_bloques, MS_SYNC);

	// listo el bloquesmap ahora resta actualizar el bitmap
	// usamos el ultimo bit disponible para poner a todos los bits previos a ese en 1 y el resto en 0
	//printf("\nultimo indice disponible %d\n", indice_ultimo_bit_disponible);
	int i = 0;
	while (i < bitarray_get_max_bit(bitmap)){
		if (i < indice_ultimo_bit_disponible) 
			bitarray_set_bit(bitmap, i);
		else	
			bitarray_clean_bit(bitmap, i);
		i++;
	}
	msync(bitmap->bitarray, bitmap->size, MS_SYNC);

	free(bloquesmap_paralelo);

	// hacemos el retraso pedido
	int retraso_compactacion = config_get_int_value(config_io, "RETRASO_COMPACTACION");
	usleep(retraso_compactacion * 1000);
	log_info(logger_io, "PID: %d - Fin Compactacion", pid);
}

// --------------------------------------------------------------------------------------------------------------------------------------------------
void ampliar_tamanio(archivo_t* archivo, uint32_t tamanio_solicitado, uint32_t pid){
	int bloque_inicial = config_get_int_value(archivo->metadata, "BLOQUE_INICIAL");
	int tamanio_archivo = config_get_int_value(archivo->metadata, "TAMANIO_ARCHIVO");
	// para cubrir el caso en que un archivo tenga asignado un bloque pero el tamaño sea 0
	int bloques_asignados_antes = 1;
	if (tamanio_archivo != 0)
		bloques_asignados_antes = ceil((float)tamanio_archivo / (float)block_size);
	
	// si tenia 4 bloques asignados, los bits 0 1 2 y 3 estaban en 1 (ocupados) 
	// a partir de la posicion 4 quiero buscar libres
	
	int bloques_a_asignar = ceil((float)tamanio_solicitado / (float)block_size) - bloques_asignados_antes;
	if (bloques_a_asignar == 0){
		config_set_value(archivo->metadata, "TAMANIO_ARCHIVO", string_itoa(tamanio_solicitado));
		config_save(archivo->metadata);
		return;
	}		
	
	if (hay_bloques_contiguos_al_archivo(bloques_a_asignar, bloque_inicial + bloques_asignados_antes)){
		int i = 0;
		int bit_inicial = bloque_inicial + bloques_asignados_antes;
		while (i < bloques_a_asignar){
			bitarray_set_bit(bitmap, bit_inicial+i);

			// limpio el espacio asignado, no se si es sumamente necesario
			// pero es prolijo y evita el uso de datos basura
			memset(bloquesmap+(bit_inicial+i) * block_size, 0, block_size);
			i ++;
		}
	} else if (hay_bloques_necesarios(bloques_a_asignar)){
		//log_info(logger_io , "PID: %s Inicio de compactacion" , pid);
		compactar_y_asignar(archivo, bloques_a_asignar, pid);
		//log_info(logger_io , "PID: %s Fin de compactacion" , pid);

	} else {
		//log_error(logger_io, "No hay espacio suficiente para ampliar el archivo de %d bloques a %d bloques", bloques_asignados_antes, bloques_a_asignar);
		return;
	}

	msync(bitmap->bitarray, bitmap->size, MS_SYNC);
	msync(bloquesmap, tamanio_archivo_bloques, MS_SYNC);

	config_set_value(archivo->metadata, "TAMANIO_ARCHIVO", string_itoa(tamanio_solicitado));
	config_save(archivo->metadata);
}

// --------------------------------------------------------------------------------------------------------------------------------------------------
void reducir_tamanio(archivo_t* archivo, uint32_t tamanio_solicitado){
	int bloques_a_asignar = ceil(tamanio_solicitado / block_size);
	int tamanio_archivo = config_get_int_value(archivo->metadata, "TAMANIO_ARCHIVO");
	int bloques_asignados_antes = ceil(tamanio_archivo / block_size);
	if (bloques_a_asignar == bloques_asignados_antes)
		return;
	int bloque_inicial = config_get_int_value(archivo->metadata, "BLOQUE_INICIAL");
	int cantidad_bloques_asignados = 0;
	int j = 0;

	while (j < bloques_asignados_antes) {
		if (cantidad_bloques_asignados >= bloques_a_asignar) {
			bitarray_clean_bit(bitmap, bloque_inicial + cantidad_bloques_asignados);
		}
		cantidad_bloques_asignados++;
		j++;
	}
	
	msync(bitmap->bitarray, bitmap->size, MS_SYNC);

	config_set_value(archivo->metadata, "TAMANIO_ARCHIVO", string_itoa(tamanio_solicitado));
	config_save(archivo->metadata);
}


// --------------------------------------------------------------------------------------------------------------------------------------------------
void ejecutar_fs_delete(){
	t_buffer* buffer =  recibir_buffer(socket_kernel);	
	int pid = buffer_read_uint32(buffer);
    t_instruccion* instruccion = buffer_read_instruccion(buffer);
   	destruir_buffer(buffer);
	char* nombreArchivo = instruccion -> parametro2;

	log_info(logger_io, "PID: %d - Operacion: %s", pid, obtener_nombre_instruccion(instruccion));
	log_info(logger_io, "PID: %d - Eliminar Archivo: %s", pid, nombreArchivo);


	//elimino de la lista global de archivos abiertos el archivo a eliminar y eliminar el archivo en si 
	bool encontrado = false;
	eliminar_archivo_de_lista( lista_global_archivos_abiertos ,nombreArchivo, &encontrado);	
	if (!encontrado) {
		destruir_buffer(buffer);
		//log_info(logger_io, "El archivo buscado no existe.");
		return;
	}
} 

void liberar_espacio_en_disco(int bloque_inicial, int tamanio_archivo){
	int cantidad_bloques_totales = 1;
	if (tamanio_archivo != 0)
		cantidad_bloques_totales = ceil((float)tamanio_archivo / (float)block_size);
	int i = 0;
	while (i < cantidad_bloques_totales){
		bitarray_clean_bit(bitmap, bloque_inicial + i);
		i++;
	}
}

void liberar_archivo(archivo_t* archivo) {
    if (archivo != NULL) {

		liberar_espacio_en_disco(config_get_int_value(archivo->metadata, "BLOQUE_INICIAL"), config_get_int_value(archivo->metadata, "TAMANIO_ARCHIVO"));
		msync(bitmap->bitarray, tamanio_archivo_bitarray, MS_SYNC);

        if (archivo->nombre_archivo != NULL) {
            free(archivo->nombre_archivo);
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
			remove(archivo->metadata->path);
			// hay que setear sus bits de uso en 0 
            list_remove(lista, i);
            liberar_archivo(archivo);
			*encontrado = true;
			//log_info(logger_io, "Se elimina el archivo de nombre: %s", nombreArchivo);
            break;
        }
    }
}

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

	log_info(logger_io, "PID: %d - Operacion: %s", pid, obtener_nombre_instruccion(instruccion));

    char* nombreArchivo = instruccion->parametro2;
    uint32_t registroDireccion = atoi(instruccion->parametro3);
    uint32_t registroTamanio = atoi(instruccion->parametro4);
	uint32_t registroPunteroArchivo = atoi(instruccion->parametro5);
    log_info(logger_io,"PID: %d - Escribir Archivo: %s - Tamaño a Escribir: %d - Puntero Archivo: %d", pid, nombreArchivo, registroTamanio, registroPunteroArchivo);

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

	//log_info(logger_io, "Me llego de memoria la cadena: %s", contenido_a_escribir);

	// se escribe en bloques.dat

	archivo_t* archivo_buscado = obtener_archivo_con_nombre(nombreArchivo);
	if (!archivo_buscado){
		log_error(logger_io, "El archivo solicitado para escribir no existe, terminando.");
		exit(EXIT_FAILURE);
	}

	int bloque_inicial_archivo = config_get_int_value(archivo_buscado->metadata, "BLOQUE_INICIAL");
	memcpy(bloquesmap+bloque_inicial_archivo*block_size+registroPunteroArchivo, contenido_a_escribir, registroTamanio);
	msync(bloquesmap, tamanio_archivo_bloques, MS_SYNC);

    
    free(nombreArchivo);
}

void ejecutar_fs_read(){
	t_buffer* buffer = recibir_buffer(socket_kernel);
    int pid = buffer_read_uint32(buffer);
	t_instruccion* instruccion = buffer_read_instruccion(buffer);
    destruir_buffer(buffer);

	log_info(logger_io, "PID: %d - Operacion: %s", pid, obtener_nombre_instruccion(instruccion));

    char* nombreArchivo = instruccion->parametro2;
    uint32_t registroDireccion = atoi(instruccion->parametro3);
    uint32_t registroTamanio = atoi(instruccion->parametro4);
	uint32_t registroPunteroArchivo = atoi(instruccion->parametro5);
    log_info(logger_io,"PID: %d - Leer Archivo: %s - Tamaño a Escribir: %d - Puntero Archivo: %d", pid, nombreArchivo, registroTamanio, registroPunteroArchivo);

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

	//log_info(logger_io, "La cadena a escribir en memoria es: %s", cadena_a_escribir_en_memoria);

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

t_config* crear_metadata(char* nombreArchivo, char* path_filesystem) {
    char *metadataDirPath = malloc(strlen(path_filesystem) + strlen("metadata") + 1);
    sprintf(metadataDirPath, "%smetadata", path_filesystem);
    mkdir(metadataDirPath, 0755);  // Crear el directorio con permisos adecuados
	//log_info(logger_io, "Se creo la carpeta con path %s", metadataDirPath);

    // Crear la ruta completa del archivo de metadata
    char *path_metadata = malloc(strlen(metadataDirPath) + strlen("/") + strlen(nombreArchivo) + 1);
    sprintf(path_metadata, "%s/%s", metadataDirPath, nombreArchivo);
	//log_info(logger_io, "Ruta del archivo de metadata: %s", path_metadata);

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
