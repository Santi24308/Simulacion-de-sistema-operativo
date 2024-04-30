#include "io.h"

int main(int argc, char* argv[]) {

	// se analiza si se puede seguir o no ejecutando
	if(!chequeo_parametros(argc, argv)) return -1;

	inicializar_modulo();

	// las interfaces tienen tipo y de eso me doy cuenta en tiempo de ejecucion, por lo que es
	// necesario que main sepa preparar todo en base al tipo que toco (obtenido desde config)
	conectar();

    terminar_programa();

    return 0;
}

int chequeo_parametros(int argc, char** argv){
	// I/O debe recibir el NOMBRE y el CONFIG
	// Duda: en el enunciado dice que el nombre es unico en el sistema, cual seria una manera
	// de corroborar eso (en caso de ser necesario)...
	if(argc != 3) {
		printf("\x1b[31m""ERROR: Tenés que pasar el nombre de I/O y el path del archivo config de Entradasalida""\x1b[0m""\n");
		return -1;
	}

	// asignamos el nombre
	strcpy(nombreIO, argv[1]);
	// asignamos config
	config_path = argv[2];

	return 0;
}

void conectar(){
	conectar_kernel();
	conectar_memoria();
}

void inicializar_modulo(){
	levantar_logger();
	levantar_config();
	// PROPONGO: usando la funcion chequeo_parametros que uso antes en main estas dos lineas que siguen no harian falta
	// por otro lado, la I/O se va a dar cuenta de su tipo a la hora de atender
	
	//solicitarInformacionIO();
	//inicializar_IO(nombreIO , config_io);
}

void atender_kernel_generica(){
	int tut = config_get_int_value(config_io, "TIEMPO_UNIDAD_TRABAJO");
	while(1){
		codigoInstruccion cod = recibir_codigo(socket_kernel);
		switch (cod){
			case IO_GEN_SLEEP:
				sleep(tut);
				break;	
			case -1:
				log_info(logger_io, "Se desconecto Kernel");
				return;
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
					leer_y_enviar_a_memoria();
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
	// IMPORTANTE: el tipo se va a preguntar una sola vez en todo el programa ya que las I/O 
	// no pueden cambiar
	tipo = config_get_string_value(config_io, "TIPO_INTERFAZ");
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
	// notar que aca llamo a atender de manera generica ya que es esa funcion
	// la encargada de derivar
	int err = pthread_create(&hilo_kernel, NULL, (void *)atender, NULL);
	if (err != 0) {
		perror("Fallo la creacion de hilo para Kernel\n");
		return;
	}
	pthread_detach(hilo_kernel);
}

void levantar_logger(){
	logger_io = log_create("io_log.log", "IO",true, LOG_LEVEL_INFO);
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

/*
solicitarInformacionIO(){
	//PODRIA MANEJAR ESTO CON HILOS(?)
	solicitarNombreIO();
	solicitarTipoDeIO();

}

void solicitarNombreIO(){
	log_info(logger_io, "Ingresar nombre de la interfaz. \n");
	fgets(nombreIO, sizeof(nombreIO), stdin); 
}

void solicitarTipoDeIO(){
	int c;
		printf("Ingrese tipo de interfaz:\n");
			printf("\t1 -- Interfaz Generica -\n");
			printf("\t2 -- Interfaz STDIN-\n\n");
			printf("\t3 -- Interfaz STDOUT- \n");
			printf("\t4 -- Interfaz DialFS \n\n");
			scanf("%d", &c);

			if(c != 1 && c != 2 && c != 3 && c!=4){
				log_error(logger_io , "No es una entrada valida");
				exit(EXIT_FAILURE);
			}

			switch (c) {
				case 1:	
					config_set_value(config_io , "TIPO_INTERFAZ" , "GENERICA");
					break;
				case 2:
					config_set_value(config_io , "TIPO_INTERFAZ" , "STDIN");
					break;
				case 3:
					config_set_value(config_io , "TIPO_INTERFAZ" , "STDOUT");
					break;
				case 4:
					config_set_value(config_io , "TIPO_INTERFAZ" , "DIALFS");
					break;
				default:
					log_error(logger_io, "No existe una interfaz de ese tipo")
					exit(EXIT_FAILURE);
					break;
}
}

void inicializar_IO(char* nombre , t_config* config_io){
	config_set_value(config_io , "NOMBRE" , nombre);

	while(1){
		tiposIO tipoInterfaz = config_get_string_value(config_io , "TIPO_INTERFAZ");

		switch (tipoInterfaz)
		{
		case GENERICA:
		//definir unidad de trabajo para interfaces genericas 
			atender_interfazGenercia();
			break;
		case STDIN:
		//definir unidad de trabajo para interfaces entrada
			atender_interfazEntrada();
			break;
		case STDOUT:
		//definir unidad de trabajo para interfaces salida
			atender_interfazSalida(socket_memoria);
			break;
		case DIALFS:
		//definir unidad de trabajo para interfaces file sytem
			atender_fileSystem(socket_memoria);
			break;
		default:
			log_error(logger_io,"No existe esa IO");
			break;
		}
	}

}

void atender_interfazGenercia(){

	while(1){
		//recibir instruccion de kernel
		//t_instruccion* codigoInstruccion = recibir_codigo(socket_kernel);

		switch (codigoInstruccion){
		case IO_GEN_SLEEP:
			
			int unidadDeTrabajo = config_get_int_value(config_io , "TIEMPO_UNIDAD_TRABAJO");

			esperar(unidadDeTrabajo); 

			break;
		default:
			log_error(logger_io , "No puedo ejecutar esa intruccion")
			break;
		}
	}

}

//desarrollar
void esperar(*int tiempo){}

*/


void terminar_programa(){
	terminar_conexiones(2, socket_memoria, socket_kernel);
    if(logger_io) log_destroy(logger_io);
    if(config_io) config_destroy(config_io);
}
