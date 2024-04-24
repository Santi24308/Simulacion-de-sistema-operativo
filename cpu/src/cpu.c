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
	conectar_memoria();
	conectar_kernel();
}

void conectar_kernel(){
	puerto_escucha_dispatch = config_get_string_value(config_cpu, "PUERTO_ESCUCHA_DISPATCH");
	puerto_escucha_interrupt = config_get_string_value(config_cpu, "PUERTO_ESCUCHA_INTERRUPT");


	socket_servidor_dispatch = iniciar_servidor(puerto_escucha_dispatch, logger_cpu);
	socket_servidor_interrupt= iniciar_servidor(puerto_escucha_interrupt , logger_cpu);

	if (socket_servidor_dispatch == -1) {
		perror("Fallo la creacion del servidor para Kernel dispatch.\n");
		exit(EXIT_FAILURE);
	}

	if (socket_servidor_interrupt == -1) {
		perror("Fallo la creacion del servidor para Kernel interrupt.\n");
		exit(EXIT_FAILURE);
	}
}

void inicializar_modulo(){
	sem_init(&sema_kernel_dispatch, 0, 0);
	sem_init(&sema_kernel_interrupt, 0, 1);
	levantar_logger();
	levantar_config();
	inicializar_registros();
	pthread_mutex_init(&mutex_cde_ejecutando, NULL);
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

void inicializar_registros(){
    registros_cpu = malloc(sizeof(t_registro));
    
    registros_cpu->AX = 0;
    registros_cpu->BX = 0;
    registros_cpu->CX = 0;
    registros_cpu->DX = 0;
	registros_cpu->EAX = 0;
	registros_cpu->EBX = 0;
	registros_cpu->ECX = 0;
	registros_cpu->EDX = 0;
	registros_cpu->SI = 0;
	registros_cpu->DI = 0;
}

// CONEXIONES

void conectar_kernel_dispatch(){
	log_info(logger_cpu, "Esperando Kernel (dispatch)....");
    socket_kernel_dispatch = esperar_cliente(socket_servidor_dispatch, logger_cpu);
    log_info(logger_cpu, "Se conecto Kernel Dispatch");

	if(socket_kernel_dispatch == -1){
		log_info(logger_cpu, "Se desconecto Kernel Dispatch!!!.");
		exit(EXIT_FAILURE);
	}

	int err = pthread_create(&hilo_kernel_dispatch, NULL, (void *)atender_kernel_dispatch, NULL);
	if (err != 0) {
		perror("Fallo la creacion de hilo para Kernel\n");
		return;
	}
	pthread_detach(hilo_kernel_dispatch);
	
}

void conectar_kernel_interrupt(){
	log_info(logger_cpu, "Esperando Kernel (interrupt)....");
    socket_kernel_interrupt = esperar_cliente(socket_servidor_interrupt, logger_cpu);
    log_info(logger_cpu, "Se conecto Kernel Interrupt");

	if(socket_kernel_dispatch == -1){
		
		log_info(logger_cpu, "Se desconecto Kernel Interrupt.");
		exit(EXIT_FAILURE);
	}

	int err = pthread_create(&hilo_kernel_interrupt, NULL, (void *)atender_kernel_interrupt, NULL);
		if (err != 0) {
			perror("Fallo la creacion de hilo para Kernel\n");
			return;
		}
	pthread_detach(hilo_kernel_interrupt);

}

void atender_kernel_dispatch(){
	while(1){
		mensajeKernelCpu op_code = recibir_codigo(socket_kernel_dispatch);

		t_buffer* buffer = recibir_buffer(socket_kernel_dispatch);

		switch (op_code){
			case EJECUTAR_PROCESO:
				t_cde cde_recibido = buffer_read_cde(buffer);
				destruir_buffer(buffer);

				pthread_mutex_lock(&mutex_cde_ejecutando);
				pid_de_cde_ejecutando = cde_recibido->pid;
				pthread_mutex_unlock(&mutex_cde_ejecutando);

				ejecutar_proceso(&cde_recibido);
				break;
			default:
				break;
		}
	}
}

void ejecutar_proceso(t_cde* cde){
	cargar_registros(cde);	
	t_instruccion* instruccion_a_ejecutar;

    while(interrupion != 1 && interrupcion_consola != 1 && realizar_desalojo != 1){
        log_info(logger_cpu, "PID: %d - FETCH - Program Counter: %d", cde->pid, cde->pc);

        enviar_codigo(socket_memoria, PEDIDO_INSTRUCCION); // fetch
        t_buffer* buffer_envio = crear_buffer();
        buffer_write_uint32(buffer_envio, cde->pid);
        buffer_write_uint32(buffer_envio, cde->pc);

        enviar_buffer(buffer_envio, socket_memoria);

        destruir_buffer(buffer_envio);
        
        cde->pc++;

        t_buffer* buffer_recibido = recibir_buffer(socket_memoria);
        instruccion_a_ejecutar = buffer_read_instruccion(buffer_recibido);
        destruir_buffer(buffer_recibido);
        
        pthread_mutex_lock(&mutex_instruccion_actualizada);
        instruccion_actualizada = instruccion_a_ejecutar->codigo;
        pthread_mutex_unlock(&mutex_instruccion_actualizada);

        ejecutar_instruccion(cde, instruccion_a_ejecutar);
	}

	if(interrupcion){
		// que pasaria
	}else if (realizar_desalojo){

	}else {

	}
}

void cargar_registros(t_cde* cde){
    registros_cpu->AX = cde->registros->AX;
    registros_cpu->BX = cde->registros->BX;
    registros_cpu->CX = cde->registros->CX;
    registros_cpu->DX = cde->registros->DX;
	registros_cpu->EAX = cde->registros->EAX;
	registros_cpu->EBX = cde->registros->EBX;
	registros_cpu->ECX = cde->registros->ECX;
	registros_cpu->EDX = cde->registros->EDX;
	registros_cpu->DX = cde->registros->SI;
	registros_cpu->DX = cde->registros->DI;
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
