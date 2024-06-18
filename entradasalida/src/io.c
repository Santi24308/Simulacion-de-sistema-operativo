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
	while(1){
		codigoInstruccion cod = recibir_codigo(socket_kernel);
		switch (cod){
			case IO_FS_CREATE:
				break;
			case IO_FS_DELETE:
				break;
			case IO_FS_TRUNCATE:
				break;
			case IO_FS_WRITE:
				break;
			case IO_FS_READ:
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
	// para poner el \0 y evitar conflictos, sabemos que en la serializacion no se va a copiar el \0
	limite_bytes++;

	char* leido = readline("> ");
	printf("\n el sting leido tiene  %ld bytes", strlen(leido) + 1);

	void* valor_a_escribir = malloc(limite_bytes);

	memcpy(valor_a_escribir, leido, limite_bytes);
	char* palabra = valor_a_escribir;
	palabra[limite_bytes] = '\0';

	printf("\nLa palabra limitada queda: %s", palabra);

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

	printf("%s", valor_a_mostrar);
	enviar_codigo(socket_kernel, LIBRE);
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
