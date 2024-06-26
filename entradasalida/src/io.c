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

void atender_kernel_dialfs(){
	
	block_size  = config_get_int_value(config_io, "BLOCK_SIZE");
    block_count = config_get_int_value(config_io,"BLOCK_COUNT");
    path_filesystem = config_get_string_value(config_io,"PATH_BASE_DIALFS");

    crear_archivo_bloques();
    crear_archivo_bitmap();
	inicializar_listas();
	
	while(1){
		codigoInstruccion cod = recibir_codigo(socket_kernel);
		switch (cod){
			case IO_FS_CREATE:
				usleep(1000);//ver de config?
				ejecutar_fs_create();
				break;
			case IO_FS_DELETE:
				usleep(1000);
				//ejecutar_fs_delete();
				break;
			case IO_FS_TRUNCATE:
				usleep(1000);
				//ejecutar_fs_truncate();
				break;
			case IO_FS_WRITE:
				usleep(1000);
				ejecutar_fs_write();
				break;
			case IO_FS_READ:
				usleep(1000);
				//ejecutar_fs_read();
				break;
			default:
				break;
		}
	}
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
	terminar_conexiones(2, socket_memoria, socket_kernel);
    if(logger_io) log_destroy(logger_io);
    if(config_io) config_destroy(config_io);
}
	
/////////////////     DIALFS     /////////////////
/*
void crear_archivo_bloques(){
    char *archivo_bloques = malloc(strlen("bloques.dat") + strlen(path_filesystem) + 1);
    sprintf(archivo_bloques, "%sbloques.dat", path_filesystem);
// Función para inicializar y escribir el archivo bloques.dat

    FILE *file = fopen(archivo_bloques, "r");
    if (file == NULL) {
         file = fopen(archivo_bloques, "wb");  // Abrir el archivo en modo escritura binaria
           if (file == NULL) {
            log_error(logger_io,"Error al abrir el archivo bloques.dat");
            return;
        }
    }
    fclose(file);
} */

void crear_archivo_bloques() {
    char *archivo_bloques = malloc(strlen("bloques.dat") + strlen(path_filesystem) + 1);
	if(archivo_bloques) log_error(logger_io, "error maloc bloque.dat");
	
    sprintf(archivo_bloques, "%sbloques.dat", path_filesystem);

    //creo el
    int fd = open(archivo_bloques, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        log_error(logger_io, "Error al abrir/crear el archivo bloques.dat");
        free(archivo_bloques);
        return;
    }
	
    // Si el archivo es nuevo y no tiene tamaño, inicializarlo con el tamaño deseado
    if (tamanio_archivo_bloqueDat == 0) {
        tamanio_archivo_bloqueDat = block_count * block_size;
        if (ftruncate(fd, tamanio_archivo_bloqueDat) == -1) {
            log_error(logger_io, "Error al establecer el tamaño del archivo bloques.dat");
            close(fd);
            free(archivo_bloques);
            return;
        }
    }

    // Mapear el archivo en memoria
    void *map = mmap(NULL, tamanio_archivo_bloqueDat, PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        log_error(logger_io, "Error al mapear el archivo bloques.dat");
        close(fd);
        free(archivo_bloques);
        return;
    }

    // Aquí puedes manipular el contenido del archivo a través del puntero 'map'
	
    close(fd);
    free(archivo_bloques);
}

void crear_archivo_bitmap(){

    char *archivo_bitmap = malloc(strlen("bitmap.dat") + strlen(path_filesystem) + 1);
    sprintf(archivo_bitmap, "%sbitmap.dat", path_filesystem);

    FILE* file = fopen(archivo_bitmap, "r");
    if (file == NULL) {
        file = fopen(archivo_bitmap, "wb");  // Abrir el archivo en modo escritura binaria
        if (file == NULL) {
               log_error(logger_io ,"Error al abrir el archivo bitmap.dat");
            return;
        }
        int tamanioEnBits = floor(block_count / 8);

        char* bitmap = malloc(block_count); //que pasa si no es multiplo de 8? se asume que siempre sera multiplo de 8

        memset(bitmap, 0 , tamanioEnBits);
        t_bitarray* bitarray = bitarray_create(bitmap,block_count); 
            fwrite(bitarray->bitarray,floor(bitarray->size / 8), 1, file);
    }

    fclose(file); 
}

void inicializar_listas(){

	lista_global_archivos_abiertos = list_create();
	lista_global_archivos_cerrados = list_create();

	return; 
}



void ejecutar_fs_create(){
	
	//REPETICION DE LOGICA EN TODAS LAS IOS
	t_buffer* buffer =  recibir_buffer(socket_kernel);	
	int pid = buffer_read_uint32(buffer);
    t_instruccion* instruccion = buffer_read_instruccion(buffer);
   	destruir_buffer(buffer);
	
	char* nombreArchivo = instruccion -> parametro2;

	char *nombreArchivoPath = malloc(strlen(nombreArchivo) + strlen(path_filesystem) + 1);
	sprintf(nombreArchivoPath, "%s/%s", path_filesystem, nombreArchivo);
	
	FILE *archivo = fopen(nombreArchivoPath , "w"); 	 
	metadata_t* metadata = crear_metadata(nombreArchivo, path_filesystem);	

	archivo_t* archivoAgregar = crear_archivo(nombreArchivoPath , archivo , metadata);

	log_info(logger_io, "DialFS - Crear Archivo: PID: %d - Crear Archivo: %s", pid, nombreArchivo);
	
	list_add(lista_global_archivos_abiertos , archivoAgregar);

	char* lista_de_archivos_abiertos = obtener_lista_archivos_abiertos(lista_global_archivos_abiertos);
	log_info(logger_io, "DialFS - Agregado a lista %s el archivo:  %s", lista_global_archivos_abiertos, nombreArchivo);

	fclose(archivo);
	free(nombreArchivoPath);
	free(nombreArchivo);
}

archivo_t* crear_archivo(char* nombre, FILE* archivo, metadata_t* metadata){
	archivo_t* archivo_creado = malloc(sizeof(archivo_t));

	archivo_creado->nombre_archivo = string_new();
	string_append(archivo_creado->nombre_archivo, nombre);

	archivo_creado->archivo = archivo;
	archivo_creado->metadata_del_mismo = metadata;

	return archivo_creado;
}

void ejecutar_fs_delete(){
	t_buffer* buffer =  recibir_buffer(socket_kernel);	
	int pid = buffer_read_uint32(buffer);
    t_instruccion* instruccion = buffer_read_instruccion(buffer);
   	destruir_buffer(buffer);
	char* nombreArchivo = instruccion -> parametro2;

	//elimino de la lista global de archivos abiertos el archivo a eliminar
	eliminar_archivo_de_lista( lista_global_archivos_abiertos ,nombreArchivo);	


	//libero los campos del t_archivo con el nombre del archivo (hacer funcion)


	//agrego el archivo eliminado a la lista de archivo eliminado
	list_add(lista_global_archivos_cerrados , nombreArchivo);
	char* lista_de_archivos_cerrado = obtener_lista_archivos_abiertos(lista_global_archivos_cerrados);
	log_info(logger_io, "DialFS - Agregado a lista %s el archivo:  %s", lista_global_archivos_cerrados, nombreArchivo);


	void hacer_compactacion();
	
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





void ejecutar_fs_write(){
    t_buffer* buffer = recibir_buffer(socket_kernel);
    int pid = buffer_read_uint32(buffer);
	t_instruccion* instruccion = buffer_read_instruccion(buffer);
    destruir_buffer(buffer);

    char* nombreArchivo = instruccion->parametro2;
    uint32_t registroDireccion = instruccion->parametro3;
    uint32_t registroTamanio = instruccion->parametro4;
	uint32_t registroPunteroArchivo = instruccion->parametro5;

    char* nombreArchivoPath = malloc(strlen(nombreArchivo) + strlen(path_filesystem) + 1);
    sprintf(nombreArchivoPath, "%s/%s", path_filesystem, nombreArchivo);

    FILE* archivo = fopen(nombreArchivoPath, "r+");
    if (archivo == NULL) {
        perror("Error abriendo archivo para escritura");
        free(nombreArchivoPath);
        return;
    }

    fseek(archivo, registroTamanio, SEEK_SET);

    if (fwrite(buffer->stream, registroPunteroArchivo, 1, archivo) != 1) {
        perror("Error escribiendo en archivo");
        fclose(archivo);
        free(nombreArchivoPath);
        return;
    }

    fclose(archivo);


    log_info(logger_io,"DialFS - Escribir Archivo: PID: %d - Escribir Archivo: %s - Tamaño a Escribir: %d - Puntero Archivo: %d", pid, nombreArchivo, registroTamanio, registroPunteroArchivo);
    
    free(nombreArchivoPath);
    free(nombreArchivo);
}

/*
void ejecutar_fs_read(){}
*/
//-------------------------------------------------------------------------------------


metadata_t* crear_metadata(char* nombreArchivo, char* path_filesystem) {
    char *metadataDirPath = malloc(strlen(path_filesystem) + strlen("metadata") + 1);
    sprintf(metadataDirPath, "%smetadata", path_filesystem);
    mkdir(metadataDirPath, 0755);  // Crear el directorio con permisos adecuados
	log_info(logger_io, "Se creo la carpeta con path %s", metadataDirPath);

    // Crear la ruta completa del archivo de metadata
    char *nombreMetadata = malloc(strlen(metadataDirPath) + strlen("/") + strlen(nombreArchivo) + 1);
    sprintf(nombreMetadata, "%s/%s", metadataDirPath, nombreArchivo);
	log_info(logger_io, "Ruta del archivo de metadata: %s", nombreMetadata);


    // Crear el archivo metadata
	FILE *file = fopen(nombreMetadata, "w");
    if (file == NULL) {
        log_error(logger_io, "No se pudo crear el archivo de metadata: %s", nombreMetadata);
        free(nombreMetadata);
        free(metadataDirPath);
        return;
    }
    fclose(file);
	
    // Crear el config en arhvio metadata creado antes
    metadata = config_create(nombreMetadata);
    if (metadata == NULL) {
        log_error(logger_io, "No se pudo crear el metadata del archivo: %s", nombreMetadata);
        free(nombreMetadata);
        free(metadataDirPath);
        return;
    }

	//Inicializacion de datos del config
	config_set_value(metadata, "BLOQUE_INICIAL", "-1"); //-1 para decir que no tiene ningun bloque asociado 
	config_set_value(metadata, "TAMANIO_ARCHIVO" , "0");
	config_save(metadata); // entedemos que guarda los cambios

	// crear el archivo_t
	// nuevo archivo t;
	// archivo->nombre = nombreArchivo;
	// 

	metadata_t* metadata_a_crear = malloc(sizeof(metadata_t));
	metadata_a_crear->archivo = file;
	metadata_a_crear->metadata_archivo = metadata;

	free(nombreMetadata);
	free(metadataDirPath);

    return metadata_a_crear;
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



uint32_t obtener_nro_bloque_libre(){
    for(uint32_t nroBloque = 0; nroBloque < block_count; nroBloque++){
        uint8_t bloqueOcupado = leer_de_bitmap(nroBloque);
        if(!bloqueOcupado)
            return nroBloque;
    }
	
	return -1;
}

//-------------------------------------------------------------------------------------
// Escribimos en el archivo bloques.dat y pisamos con "valor" en el nro de bloque correspondiente

void modificar_BitMap(uint32_t nroBloque, uint8_t valor){
    FILE *file = fopen("bitmap.dat", "rb+");
    if (file == NULL) {
        perror("Error abriendo bitmap.dat");
        return;
    }
    if (fseek(file, nroBloque, SEEK_SET) != 0) {
        perror("Error buscando en bitmap.dat");
        fclose(file);
        return;
    }
    if (fwrite(&valor, sizeof(uint8_t), 1, file) != 1) {
        perror("Error escribiendo en bitmap.dat");
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
