#include "kernel.h"

int main(void) {

	inicializar_modulo();
	conectar();
	consola();
	esperar_desconexiones();

    terminar_programa();

    return 0;
}

void esperar_desconexiones(){
	sem_wait(&sema_io);
	sem_wait(&sema_consola);
}

void conectar(){
	puerto_escucha = config_get_string_value(config_kernel, "PUERTO_ESCUCHA");

	socket_servidor = iniciar_servidor(puerto_escucha, logger_kernel);
	if (socket_servidor == -1) {
		perror("Fallo la creacion del servidor para memoria.\n");
		exit(EXIT_FAILURE);
	}

	conectar_cpu_dispatch();
	conectar_cpu_interrupt();
	conectar_memoria();
	conectar_io();
}

void inicializar_modulo(){
	sem_init(&sema_consola, 0, 0);
	sem_init(&sema_io, 0, 0);

	levantar_logger();
	levantar_config();
}

void consola(){
	int c;
	char texto[100];
	while (1) {
		printf("Ingrese comando:\n");
		printf("\t1 -- Enviar mensaje a Cpu - Dispatch\n");
		printf("\t2 -- Enviar paquete a Cpu - Dispatch\n\n");
		printf("\t3 -- Enviar mensaje a Cpu - Interrupt\n");
		printf("\t4 -- Enviar paquete a Cpu - Interrupt\n\n");
		printf("\t5 -- Enviar mensaje a Memoria\n");
		printf("\t6 -- Enviar paquete a Memoria\n\n");
		printf("\t9 -- Apagar Kernel\n");
		scanf("%d", &c);
		switch (c) {
			case 1:
    			printf("Ingresa un mensaje: \n");
    			scanf("%s", texto);
				enviar_mensaje(texto,socket_cpu_dispatch);
				break;
			case 2:
				paquete(socket_cpu_dispatch);
				break;
			case 3:
    			printf("Ingresa un mensaje: \n");
    			scanf("%s", texto);
				enviar_mensaje(texto,socket_cpu_interrupt);
				break;
			case 4:
				paquete(socket_cpu_interrupt);
				break;
			case 5:
    			printf("Ingresa un mensaje: \n");
    			scanf("%s", texto);
				enviar_mensaje(texto,socket_memoria);
				break;
			case 6:
				paquete(socket_memoria);
				break;	
			case 9:
				sem_post(&sema_consola);	
				sem_post(&sema_io);			
				return;
			default:
				printf("\tcodigo no reconocido!\n");
				break;
		}
	}
}

void conectar_consola(){
	int err = pthread_create(&hilo_consola, NULL, (void*)consola, NULL);
	if (err != 0) {
		perror("Fallo la creacion de hilo para IO\n");
		return;
	}
	pthread_detach(hilo_consola);
}

void conectar_io(){
    log_info(logger_kernel, "Esperando IO....");
    socket_io = esperar_cliente(socket_servidor, logger_kernel);
    log_info(logger_kernel, "Se conecto IO");

	int err = pthread_create(&hilo_io, NULL, (void *)atender_io, NULL);
	if (err != 0) {
		perror("Fallo la creacion de hilo para IO\n");
		return;
	}
	pthread_detach(hilo_io);
}

void atender_io(){
	t_list* lista;
	while (1) {
		int cod_io = recibir_operacion(socket_io);		
		switch (cod_io) {
			case MENSAJE:
				recibir_mensaje(socket_io,logger_kernel);
				break;
			case PAQUETE:
				lista = recibir_paquete(socket_io);
				log_info(logger_kernel, "Me llegaron los siguientes valores:");
				list_iterate(lista, (void*) iterator);
				break;
			case -1:
				log_error(logger_kernel, "Se desconecto ENTRADASALIDA");
				sem_post(&sema_io);
				return;
			default:
				break;
		}
	}
}

void conectar_memoria(){
	ip = config_get_string_value(config_kernel, "IP");
	puerto_mem = config_get_string_value(config_kernel, "PUERTO_MEM");

    socket_memoria = crear_conexion(ip, puerto_mem);
    if (socket_memoria == -1) {
		terminar_programa();
        exit(EXIT_FAILURE);
    }
}

void conectar_cpu_dispatch(){
	ip = config_get_string_value(config_kernel, "IP");
	puerto_cpu = config_get_string_value(config_kernel, "PUERTO_CPU");

    socket_cpu_dispatch = crear_conexion(ip, puerto_cpu);
    if (socket_cpu_dispatch == -1) {
		terminar_programa();
        exit(EXIT_FAILURE);
    }
	log_info(logger_kernel, "Conexion dispatch lista!");
}

void conectar_cpu_interrupt(){
	ip = config_get_string_value(config_kernel, "IP");
	puerto_cpu = config_get_string_value(config_kernel, "PUERTO_CPU");

    socket_cpu_interrupt = crear_conexion(ip, puerto_cpu);
    if (socket_cpu_interrupt == -1) {
		terminar_programa();
        exit(EXIT_FAILURE);
    }
	log_info(logger_kernel, "Conexion interrupt lista!");
}

void levantar_logger(){
	logger_kernel = log_create("kernel_log.log", "KERNEL",true, LOG_LEVEL_INFO);
	if (!logger_kernel) {
		perror("Error al iniciar logger de kernel\n");
		exit(EXIT_FAILURE);
	}
}

void levantar_config(){
    config_kernel = config_create("/home/marcos/cursadaSO/miRepoTp/conexiones/kernel/kernel.config");
	if (!config_kernel) {
		perror("Error al iniciar config de kernel\n");
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
	terminar_conexiones(3, socket_memoria, socket_cpu_dispatch, socket_cpu_interrupt);
    if(logger_kernel) log_destroy(logger_kernel);
    if(config_kernel) config_destroy(config_kernel);
}

void iterator(char* value) {
	log_info(logger_kernel,"%s", value);
}