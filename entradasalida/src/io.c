#include "io.h"

int main(int argc, char* argv[]) {

	if(argc != 2) {
		printf("ERROR: Tenés que pasar el path del archivo config de Entradasalida\n");
		return -1;
	}

	config_path = argv[1];

	inicializar_modulo();
	conectar();
	consola();

    terminar_programa();

    return 0;
}

void conectar(){
	conectar_kernel();
	conectar_memoria();
}

void inicializar_modulo(){
	levantar_logger();
	levantar_config();
	solicitarInformacionIO();
	inicializar_IO(nombreIO , config_io);
}

void consola(){
}

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




void terminar_programa(){
	terminar_conexiones(2, socket_memoria, socket_kernel);
    if(logger_io) log_destroy(logger_io);
    if(config_io) config_destroy(config_io);
}
