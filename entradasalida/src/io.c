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
	// Duda: en el enunciado dice que el nombre es unico en el sistema, cual seria una manera
	// de corroborar eso (en caso de ser necesario)...
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
			case -1:
				log_info(logger_io, "Se desconecto Kernel");
				return;
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
			case -1:
				log_info(logger_io, "Se desconecto Kernel");
				return;
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
			case -1:
				log_info(logger_io, "Se desconecto Kernel");
				return;
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
			case -1:
				log_info(logger_io, "Se desconecto Kernel");
				return;
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

	char* leido = readline("> ");

	void* valor_a_escribir = malloc(limite_bytes);

	memcpy(valor_a_escribir, leido, limite_bytes);

	enviar_codigo(socket_memoria, IO_STDIN_ESCRIBIR);
	buffer = crear_buffer();
	buffer_write_uint32(buffer, pid);
	buffer_write_uint32(buffer, (uint32_t)atoi(instruccion->parametro2)); // direccion
	buffer_write_string(buffer, (char*)valor_a_escribir);
	buffer_write_uint32(buffer, (uint32_t)limite_bytes);
	enviar_buffer(buffer, socket_memoria);
	destruir_buffer(buffer);
}

void ejecutar_std_out(){
	t_buffer* buffer = recibir_buffer(socket_kernel);
	uint32_t pid = buffer_read_uint32(buffer);
	t_instruccion* instruccion = buffer_read_instruccion(buffer);
	destruir_buffer(buffer);

	enviar_codigo(socket_memoria, IO_STDOUT_LEER);
	buffer = crear_buffer();
	buffer_write_uint32(buffer, pid);
	buffer_write_uint32(buffer, (uint32_t)atoi(instruccion->parametro2));
	buffer_write_uint32(buffer, (uint32_t)atoi(instruccion->parametro3));
	enviar_buffer(buffer, socket_memoria);
	destruir_buffer(buffer);

	uint32_t tam = 0;
	buffer = recibir_buffer(socket_memoria);
	char* valor_a_mostrar = buffer_read_string(buffer, &tam);
	destruir_buffer(buffer);

	printf("%s", valor_a_mostrar);
}

// esto ya no es del checkpoint 2
void leer_y_enviar_a_memoria(){
	enviar_codigo(socket_memoria, GUARDAR_EN_DIRECCION);

	// primero leo la direccion que me llego de kernel
	t_buffer* buffer_recibido = recibir_buffer(socket_kernel);
	uint32_t tamanio_direccion;
	char* direccion_memoria = buffer_read_string(buffer_recibido, &tamanio_direccion);

	char* leido = readline("> ");

	t_buffer* buffer_a_enviar = crear_buffer();
	buffer_write_string(buffer_a_enviar, direccion_memoria);
	buffer_write_string(buffer_a_enviar, leido);

	enviar_buffer(buffer_a_enviar, socket_memoria);

	free(leido);
	destruir_buffer(buffer_recibido);
	destruir_buffer(buffer_a_enviar);
}
/*
void leer_y_mostrar_resultado(){

	//falta saber cual es la direccion que quiero leer

	
	//Le aviso a memoria que quiero leer tal direccion

	enviar_codigo(socket_memoria , LEER_DIRECCION);

	//Memoria me responde con un buffer que contiene la direccion
	t_buffer* buffer_recibido = recibir_buffer(socket_memoria);
	uint32_t tamanio_direccion;
	char* direccion_memoria = buffer_read_string(buffer_recibido , &tamanio_direccion);   //deberia verificar que sea valida esa direccion (futuro refinamiento del tp)

	//leo el valor de la direccion con funcion de las common y la guardo en una variable
	char* contenido_a_imprimir  = mem_hexstring(direccion_memoria, string_length(direccion_memoria) + 1);
	//duermo 1 unidad de tiempo
	sleep(1);

	//imprimo el valor guardado de la direccion
	// podria hacerse con printf tambien pero me parecio que estaba bueno dejar el logg
	log_info(logger_io ,contenido_a_imprimir);


	//destruyo estructuras creadas
	destruir_buffer(buffer_recibido);
	free(contenido_a_imprimir);

}
*/
//--------------------------------------------------------------------------------------------------------------


void conectar_memoria(){
	ip = config_get_string_value(config_io, "IP");
	puerto_mem = config_get_string_value(config_io, "PUERTO_MEM");

    socket_memoria = crear_conexion(ip, puerto_mem);
    if (socket_memoria == -1) {
		terminar_programa();
        exit(EXIT_FAILURE);
    }
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
