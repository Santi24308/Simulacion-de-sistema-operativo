#include "io.h"

int main(void) {

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
	int c;
	while (1) {
		char texto[100];
		printf("Ingrese comando:\n");
		printf("\t1 -- Enviar mensaje a Kernel \n");
		printf("\t2 -- Enviar paquete a Kernel \n\n");
		printf("\t3 -- Enviar mensaje a Memoria \n");
		printf("\t4 -- Enviar paquete a Memoria \n\n");
		printf("\t9 -- Apagar IO\n");
		scanf("%d", &c);
		switch (c) {
			case 1:
    			printf("Ingresa un mensaje: \n");
    			scanf("%s", texto);
				enviar_mensaje(texto,socket_kernel);
				break;
			case 2:
				paquete(socket_kernel);
				break;
			case 3:
				printf("Ingresa un mensaje: \n");
    			scanf("%s", texto);
				enviar_mensaje(texto, socket_memoria);
				break;
			case 4:
				paquete(socket_memoria);
				break;
			case 9:
				return;
			default:
				printf("\tcodigo no reconocido!\n");
				break;
		}
	}
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
    config_io = config_create("/home/marcos/cursadaSO/miRepoTp/conexiones/entradasalida/io.config");
	if (!config_io) {
		perror("Error al iniciar config de IO\n");
		exit(EXIT_FAILURE);
	}
}

void paquete(int conexion){
	char* leido;
	t_paquete* paquete = crear_paquete();

	while(1) {
		printf("agregar lineas al paquete...\n");
		leido = readline("> ");
		if (string_is_empty(leido)) {
			free(leido);
			break;
		}

		agregar_a_paquete(paquete, leido, strlen(leido)+1);
		free(leido);
	}

	enviar_paquete(paquete, conexion);

	eliminar_paquete(paquete);
}

void terminar_programa(){
	terminar_conexiones(2, socket_memoria, socket_kernel);
    if(logger_io) log_destroy(logger_io);
    if(config_io) config_destroy(config_io);
}