#include "memoria.h"

int main(void) {

	inicializar_modulo();
	conectar();
	esperar_desconexiones();

	terminar_programa();

	return 0;
}

void inicializar_modulo(){
	levantar_logger();
	levantar_config();

	sem_init(&sema_cpu, 0, 0);
	sem_init(&sema_kernel, 0, 1);
}

void conectar(){
	puerto_escucha = config_get_string_value(config_memoria, "PUERTO_ESCUCHA");

	socket_servidor = iniciar_servidor(puerto_escucha, logger_memoria);
	if (socket_servidor == -1) {
		perror("Fallo la creacion del servidor para memoria.\n");
		exit(EXIT_FAILURE);
	}

	conectar_cpu();
	conectar_kernel();
	conectar_io();
}

void esperar_desconexiones(){
	sem_wait(&sema_cpu);
	sem_wait(&sema_kernel);
}

void levantar_logger(){
    logger_memoria = log_create("memoria.log", "MEMORIA",true, LOG_LEVEL_INFO);
	if (!logger_memoria) {
		perror("Error al iniciar logger de memoria\n");
		exit(EXIT_FAILURE);
	}
}

void levantar_config(){
	config_memoria = config_create("/home/marcos/cursadaSO/miRepoTp/conexiones/memoria/memoria.config");
	if (!config_memoria) {
		perror("Error al iniciar config de memoria\n");
		exit(EXIT_FAILURE);
	}
}

void conectar_kernel(){
	log_info(logger_memoria, "Esperando Kernel....");
    socket_kernel = esperar_cliente(socket_servidor, logger_memoria);
    log_info(logger_memoria, "Se conecto Kernel");

	int err = pthread_create(&hilo_kernel, NULL, (void *)atender_kernel, NULL);
	if (err != 0) {
		perror("Fallo la creacion de hilo para Kernel\n");
		return;
	}
	pthread_detach(hilo_kernel);
}

void conectar_io(){
    log_info(logger_memoria, "Esperando IO....");
    socket_io = esperar_cliente(socket_servidor, logger_memoria);
    log_info(logger_memoria, "Se conecto IO");

	int err = pthread_create(&hilo_io, NULL, (void *)atender_io, NULL);
	if (err != 0) {
		perror("Fallo la creacion de hilo para IO\n");
		return;
	}
	pthread_detach(hilo_io);
}

void conectar_cpu(){
    log_info(logger_memoria, "Esperando Cpu....");
    socket_cpu = esperar_cliente(socket_servidor, logger_memoria);
    log_info(logger_memoria, "Se conecto Cpu");
    	
	int err = pthread_create(&hilo_cpu, NULL, (void *)atender_cpu, NULL);
	if (err != 0) {
		perror("Fallo la creacion de hilo para CPU\n");
		return;
	}
	pthread_detach(hilo_cpu);
}

void atender_kernel(){
	t_list* lista;
	while (1) {
		int cod_kernel = recibir_operacion(socket_kernel);
		switch (cod_kernel) {
			case MENSAJE:
				recibir_mensaje(socket_kernel,logger_memoria);
				break;
			case PAQUETE:
				lista = recibir_paquete(socket_kernel);
				log_info(logger_memoria, "Me llegaron los siguientes valores:");
				list_iterate(lista, (void*) iterator);
				break;
			case -1:
				log_error(logger_memoria, "Se desconecto KERNEL");
				sem_post(&sema_kernel);
				return;
			default:
				break;
		}
	}
}

void atender_io(){
	t_list* lista;
	while (1) {
		int cod_io = recibir_operacion(socket_io);
		switch (cod_io) {
				case MENSAJE:
					recibir_mensaje(socket_io, logger_memoria);
					break;
				case PAQUETE:
					lista = recibir_paquete(socket_io);
					log_info(logger_memoria, "Me llegaron los siguientes valores:");
					list_iterate(lista, (void*) iterator);
					break;
				case -1:
					log_error(logger_memoria, "Se desconecto ENTRADASALIDA");
					return;
				default:
					break;
		}
	}
}

void atender_cpu(){
	t_list* lista;
	while (1) {
		int cod_cpu = recibir_operacion(socket_cpu);
		switch (cod_cpu) {
				case MENSAJE:
					recibir_mensaje(socket_cpu,logger_memoria);
					break;
				case PAQUETE:
					lista = recibir_paquete(socket_cpu);
					log_info(logger_memoria, "Me llegaron los siguientes valores:");
					list_iterate(lista, (void*) iterator);
					break;
				case -1:
					log_error(logger_memoria, "Se desconecto CPU");
					sem_post(&sema_cpu);
					return;
				default:
					break;
			}
	}
}

void terminar_programa(){
	if (logger_memoria) log_destroy(logger_memoria);
	if (config_memoria) config_destroy(config_memoria);
}

void iterator(char* value) {
	log_info(logger_memoria,"%s", value);
}
