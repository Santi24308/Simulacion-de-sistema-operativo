#include "kernel.h"

int main(int argc, char* argv[]) {

	if(argc != 2) {
		printf("ERROR: Tenés que pasar el path del archivo config de Kernel\n");
		return -1;
	}

	config_path = argv[1];

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

void consola(){ // CONSOLA INTERACTIVA EN BASE A LINEAMIENTO E IMPLEMENTACION
	int c;
	char texto[100];
	while (1) {
		printf("Ingrese comando:\n");
		printf("\t1 -- Ejecutar script de operaciones\n");
		printf("\t2 -- Iniciar proceso\n");
		printf("\t3 -- Finalizar proceso\n");
		printf("\t4 -- Inicializar planificacion\n");
		printf("\t5 -- Detener planificacion\n");
		printf("\t6 -- Listar procesos por estado\n");
		printf("\t9 -- Apagar Kernel\n");
		scanf("%d", &c);
		switch (c) {
			case 1:
    			printf("Se ejecuta el script de operaciones: \n");
				ejecutar_script_de_operaciones(texto,socket_cpu_dispatch);
				break;
			case 2:
				iniciar_proceso();
				break;
			case 3:
    			finalizar_proceso();
				break;
			case 4:
				inicializar_planificacion();
				break;
			case 5:
    			detener_planificacion();
				break;
			case 6:
				listar_procesos_por_estado();
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


///////// CHECKPOINT 2 PLANIFICACION /////////////////

t_pcb* crear_pcb(char* path, int quantum){
    t_pcb* pcb_creado = malloc(sizeof(t_pcb));
    // Asigno memoria a las estructuras
    pcb_creado->cde = malloc(sizeof(t_cde)); 
    pcb_creado->cde->registros = malloc(sizeof(t_registro));

	//Inicializo el quantum, pid y el PC
	pcb_creado->quantum = quantum_a_asignar;
    pcb_creado->cde->pid = pid_a_asignar;
    pcb_creado->cde->pc = 0;
    
	// Inicializo los registros
    pcb_creado->cde->registros->AX = 0;
    pcb_creado->cde->registros->BX = 0;
    pcb_creado->cde->registros->CX = 0;
    pcb_creado->cde->registros->DX = 0;
	pcb_creado->cde->registros->EAX = 0;
    pcb_creado->cde->registros->EBX = 0;
    pcb_creado->cde->registros->ECX = 0;
    pcb_creado->cde->registros->EDX = 0;
	pcb_creado->cde->registros->PC = 0;
    pcb_creado->cde->registros->SI = 0;
    pcb_creado->cde->registros->DI = 0;
	
    //Le asigno el path de la estructura del pcb
    pcb_creado->path = path;
	
	// Inicializa el estado
    pcb_creado->estado = NULO; 

	// Incremento su valor, para usar un nuevo PID la proxima vez que se cree un proceso
    pid_a_asignar ++;
    return pcb_creado;
}

void destruir_pcb(t_pcb* pcb){
    destruir_cde(pcb->cde); // en conexiones.c
    free(pcb);
}

void iniciarProceso(char* path, char* size, int quantum){
	t_pcb* pcb_a_new = crear_pcb(path, quantum); // creo un nuevo pcb al que le voy a cambiar el estado

    enviar_codigo(socket_memoria, INICIAR_PROCESO_SOLICITUD); //envio la solicitud a traves del socket

    t_buffer* buffer = crear_buffer();

    buffer_write_uint32(buffer, pcb_a_new->cde->pid);
    buffer_write_string(buffer, path); 
    uint32_t tamanio = atoi(size);
    buffer_write_uint32(buffer, tamanio);
	buffer_write_int(buffer, quantum);
	
    enviar_buffer(buffer, socket_memoria);

    destruir_buffer(buffer);

	//Recibo la respuesta de la memoria
    mensajeMemoriaKernel cod_op = recibir_codigo(socket_memoria);

    if(cod_op == INICIAR_PROCESO_OK){
        // Poner en new
        agregar_pcb_a(procesosNew, pcb_a_new, &mutex_new); // se agrega a la cola de procesos

        pthread_mutex_lock(&mutex_procesos_globales); // ningun otro hilo puede acceder hasta liberarse, evita problemas de concurrencia
        list_add(procesos_globales, pcb_a_new); // agrego a lista global de pcbs
        pthread_mutex_unlock(&mutex_procesos_globales);

        pcb_a_new->estado = NEW; // el estado del PCB se pone en New
        log_info(logger_kernel, "Se crea el proceso %d en NEW", pcb_a_new->cde->pid);
        sem_post(&procesos_en_new);
    }
    else if(cod_op == INICIAR_PROCESO_ERROR)
        log_info(logger_kernel, "No se pudo crear el proceso %d", pcb_a_new->cde->pid);
    
}

void terminar_proceso(){
    while(1){
        sem_wait(&procesos_en_exit); // espera a que el proceso termine

        t_pcb* pcb = retirar_pcb_de(procesosFinalizados, &mutex_finalizados); // lo saca de la cola

        pthread_mutex_lock(&mutex_procesos_globales);
	    list_remove_element(procesos_globales, pcb); // lo elimina de la global
	    pthread_mutex_unlock(&mutex_procesos_globales);
                
        liberar_recursos_pcb(pcb);
        liberar_archivos_pcb(pcb);

        // Solicitar a memoria liberar estructuras

        enviar_codigo(socket_memoria, FINALIZAR_PROCESO_SOLICITUD);

        t_buffer* buffer = crear_buffer();
        buffer_write_uint32(buffer, pcb->cde->pid);
        enviar_buffer(buffer, socket_memoria);
        destruir_buffer(buffer);

        mensajeMemoriaKernel rta_memoria = recibir_codigo(socket_memoria);

        if(rta_memoria == FINALIZAR_PROCESO_OK){
            log_info(logger_kernel, "PID: %d - Destruir PCB", pcb->cde->pid);
            destruir_pcb(pcb);
        }
        else{
            log_error(logger_kernel, "Memoria no logró liberar correctamente las estructuras");
            exit(1);
        }
        
    }
}

///////// CHECKPOINT 1 CONEXIONES Y CONSOLA /////////

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
	puerto_cpu_dispatch = config_get_string_value(config_kernel, "PUERTO_CPU_DISPATCH");

    socket_cpu_dispatch = crear_conexion(ip, puerto_cpu_dispatch);
    if (socket_cpu_dispatch == -1) {
		terminar_programa();
        exit(EXIT_FAILURE);
    }
	log_info(logger_kernel, "Conexion dispatch lista!");
}


void conectar_cpu_interrupt(){
	ip = config_get_string_value(config_kernel, "IP");
	puerto_cpu_interrupt = config_get_string_value(config_kernel, "PUERTO_CPU_INTERRUPT");

    socket_cpu_interrupt = crear_conexion(ip, puerto_cpu_interrupt);
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
    config_kernel = config_create(config_path);
	if (!config_kernel) {
		perror("Error al iniciar config de kernel\n");
		exit(EXIT_FAILURE);
	}
}

void terminar_programa(){
	terminar_conexiones(3, socket_memoria, socket_cpu_dispatch, socket_cpu_interrupt);
    if(logger_kernel) log_destroy(logger_kernel);
    if(config_kernel) config_destroy(config_kernel);
}

void iterator(char* value) {
	log_info(logger_kernel,"%s", value);
}