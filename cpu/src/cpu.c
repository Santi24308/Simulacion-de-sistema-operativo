#include "cpu.h"

int main(int argc, char* argv[]) {

	if(argc != 2) {
		printf("ERROR: Tenés que pasar el path del archivo config de CPU\n");
		return -1;
	}

	config_path = argv[1];

	inicializar_modulo();
	conectar();
	esperar_desconexiones();

    terminar_programa();

    return 0;
}

void esperar_desconexiones(){
	sem_wait(&sema_kernel_dispatch);
	sem_wait(&sema_kernel_interrupt);
}

void conectar(){
	puerto_escucha = config_get_string_value(config_cpu, "PUERTO_ESCUCHA");

	socket_servidor = iniciar_servidor(puerto_escucha, logger_cpu);
	if (socket_servidor == -1) {
		perror("Fallo la creacion del servidor para memoria.\n");
		exit(EXIT_FAILURE);
	}

	conectar_memoria();
	conectar_kernel_dispatch();
	conectar_kernel_interrupt();
}

void inicializar_modulo(){
	sem_init(&sema_kernel_dispatch, 0, 0);
	sem_init(&sema_kernel_interrupt, 0, 1);
	levantar_logger();
	levantar_config();
}

void levantar_logger(){
	logger_cpu = log_create("cpu_log.log", "CPU",true, LOG_LEVEL_INFO);
	if (!logger_cpu) {
		perror("Error al iniciar logger de cpu\n");
		exit(EXIT_FAILURE);
	}
}

void levantar_config(){
    config_cpu = config_create(config_path);
	if (!config_cpu) {
		perror("Error al iniciar config de cpu\n");
		exit(EXIT_FAILURE);
	}
}

void conectar_kernel_dispatch(){
	log_info(logger_cpu, "Esperando Kernel (dispatch)....");
    socket_kernel_dispatch = esperar_cliente(socket_servidor, logger_cpu);
    log_info(logger_cpu, "Se conecto Kernel D");

	int err = pthread_create(&hilo_kernel_dispatch, NULL, (void *)atender_kernel_dispatch, NULL);
	if (err != 0) {
		perror("Fallo la creacion de hilo para Kernel\n");
		return;
	}
	pthread_detach(hilo_kernel_dispatch);
}

void conectar_kernel_interrupt(){
	log_info(logger_cpu, "Esperando Kernel (interrupt)....");
    socket_kernel_interrupt = esperar_cliente(socket_servidor, logger_cpu);
    log_info(logger_cpu, "Se conecto Kernel I");

	int err = pthread_create(&hilo_kernel_interrupt, NULL, (void *)atender_kernel_interrupt, NULL);
	if (err != 0) {
		perror("Fallo la creacion de hilo para Kernel\n");
		return;
	}
	pthread_detach(hilo_kernel_interrupt);
}

void atender_kernel_dispatch(){
}

void atender_kernel_interrupt(){
}

void conectar_memoria(){
	ip = config_get_string_value(config_cpu, "IP");
	puerto_mem = config_get_string_value(config_cpu, "PUERTO_MEM");

    socket_memoria = crear_conexion(ip, puerto_mem);
    if (socket_memoria == -1) {
		terminar_programa();
        exit(EXIT_FAILURE);
    }
}

void terminar_programa(){
	terminar_conexiones(1, socket_memoria);
    if(logger_cpu) log_destroy(logger_cpu);
    if(config_cpu) config_destroy(config_cpu);
}

void iterator(char* value) {
	log_info(logger_cpu,"%s", value);
}