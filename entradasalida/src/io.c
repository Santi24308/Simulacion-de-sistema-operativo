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

void terminar_programa(){
	terminar_conexiones(2, socket_memoria, socket_kernel);
    if(logger_io) log_destroy(logger_io);
    if(config_io) config_destroy(config_io);
}