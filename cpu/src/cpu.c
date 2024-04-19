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
	t_list* lista_d;
	while (1) {
		int cod_kernel_dispatch = recibir_operacion(socket_kernel_dispatch);
		switch (cod_kernel_dispatch) {
			case MENSAJE:
				log_warning(logger_cpu, "Kernel (dispatch) se comunicó");
				recibir_mensaje(socket_kernel_dispatch,logger_cpu);
				break;
			case PAQUETE:
				log_warning(logger_cpu, "Kernel (dispatch) se comunicó");
				lista_d = recibir_paquete(socket_kernel_dispatch);
				log_info(logger_cpu, "Me llegaron los siguientes valores:");
				list_iterate(lista_d, (void*) iterator);
				break;
			case -1:
				log_error(logger_cpu, "Se desconecto KERNEL (dispatch)");
				sem_post(&sema_kernel_dispatch);
				return;
			default:
				break;
		}
	}
}

void atender_kernel_interrupt(){
	t_list* lista_i;
	while (1) {
		int cod_kernel_interrupt = recibir_operacion(socket_kernel_interrupt);
		switch (cod_kernel_interrupt) {
			case MENSAJE:
				log_warning(logger_cpu, "Kernel (interrupt) se comunicó");
				recibir_mensaje(socket_kernel_interrupt,logger_cpu);
				break;
			case PAQUETE:
			    log_warning(logger_cpu, "Kernel (interrupt) se comunicó");
				lista_i = recibir_paquete(socket_kernel_interrupt);
				log_info(logger_cpu, "Me llegaron los siguientes valores:");
				list_iterate(lista_i, (void*) iterator);
				break;
			case -1:
				log_error(logger_cpu, "Se desconecto KERNEL (interrupt)");
				sem_post(&sema_kernel_interrupt);
				return;
			default:
				break;
		}
	}	
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
	terminar_conexiones(1, socket_memoria);
    if(logger_cpu) log_destroy(logger_cpu);
    if(config_cpu) config_destroy(config_cpu);
}

void iterator(char* value) {
	log_info(logger_cpu,"%s", value);
}