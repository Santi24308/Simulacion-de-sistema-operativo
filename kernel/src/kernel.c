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
	algoritmo = config_get_string_value(config_kernel, "ALGORITMO");
	quantum = config_get_int_value(config_kernel, "QUANTUM");
	grado_max_multiprogramacion = config_get_int_value(config_kernel, "MULTIPROGRAMACION_MAX");
	char **recursos_config = config_get_array_value(config_kernel,"RECURSOS");
	char **instancias = config_get_array_value(config_kernel,"INSTANCIAS_RECURSOS");
	recursos = list_create();

	int a = 0;
	while(recursos_config[a]!= NULL){
		int num_instancias = strtol(instancias[a],NULL,10);
		t_recurso* recurso = inicializar_recurso(recursos_config[a], num_instancias);
		list_add(recursos, recurso);
		a++;
	}

    string_array_destroy(recursos_config);
    string_array_destroy(instancias);

	pid_a_asignar = 0; // para crear pcbs arranca en 0 el siguiente en 1
    planificacion_detenida = 0;
    
    inicializarListas();
    inicializarSemaforos();

    /*  POR AHORA DESCARTAMOS LA IDEA DE SETEARLE A CPU UN ALGORITMO --------

    enviar_codigo(socket_cpu_dispatch, ALGORITMO_PLANIFICACION);
    t_buffer* buffer = crear_buffer();
    // si el algoritmo es FIFO envia 0
    // si el algoritmo es VIRTUAL ROUND ROBIN envia un 1
    // si el algoritmo es ROUND ROBIN envia un 2
    if(strcmp(algoritmo, "FIFO") == 0) 
        buffer_write_uint32(buffer, 0);
    else if(strcmp(algoritmo, "VRR") == 0)
        buffer_write_uint32(buffer, 1);
    else
        buffer_write_uint32(buffer, 2);
    enviar_buffer(buffer, socket_cpu_dispatch);
    destruir_buffer(buffer);

    if(strcmp(algoritmo, "RR") == 0){
       iniciar_quantum();
    }*/

	// iniciarPlanificadores(); ¿pensar a futuro o necesario ahora?
    
    return;
}

t_recurso* inicializar_recurso(char* nombre_recu, int instancias_tot){
	t_recurso* recurso = malloc(sizeof(t_recurso));
	int tam = 0;

	while(nombre_recu[tam])
		tam++;

	recurso->nombre = malloc(tam);
	strcpy(recurso->nombre, nombre_recu);

	recurso->instancias = instancias_tot;
    recurso->procesos_bloqueados = list_create();
    sem_t sem;
    sem_init(&sem, instancias_tot, 1);
    recurso->sem_recurso = sem;
	
	return recurso;
}

void consola(){ // CONSOLA INTERACTIVA EN BASE A LINEAMIENTO E IMPLEMENTACION
	char* entrada;
    char* comando;
    char* parametro;
	//char texto[100];
	while (1) {
		printf("Ingrese comando:\n");
		printf("\tEJECUTAR_SCRIPT [PATH] -- Ejecutar script de operaciones\n");
		printf("\tINICIAR_PROCESO [PATH] -- Iniciar proceso\n");
		printf("\tFINALIZAR_PROCESO [PID] -- Finalizar proceso\n");
		printf("\tINICIAR_PLANIFICACION -- Inicializar planificacion\n");
		printf("\tDETENER_PLANIFICACION -- Detener planificacion\n");
		printf("\tMULTIPROGRAMACION [VALOR] -- Modificar multiprogramación\n");
		printf("\tPROCESO_ESTADO -- Listar procesos por estado\n");
		
        scanf("%s", entrada);

        char** palabras = string_split(entrada, " ");
        strcpy(comando, palabras[0]);

        if (strcmp(comando, "EJECUTAR_SCRIPT") == 0) {
            strcpy(parametro, palabras[1]);
            leer_y_ejecutar(parametro);
        } else {
            ejecutar_comando_unico(comando, palabras); // pasamos palabras completo para sacar el parametro cuando se pueda
        }
        string_array_destroy(palabras); // no se si string_split usa memoria dinamica
    }



        /*
		switch (c) {
			case 1:
    			printf("Se ejecuta el script de operaciones: \n");
				ejecutar_script_de_operaciones(texto,socket_cpu_dispatch); // todavia queda esto pendiente
				break;
			case 2:
				iniciar_proceso(); // ver error aca
				break;
			case 3:
    			terminar_proceso();
				break;
			case 4:
				iniciar_planificacion();
				break;
			case 5:
    			detener_planificacion();
				break;
			case 6:
				listar_procesos_por_estado(); // pendiente 
				break;	
			case 9:
				sem_post(&sema_consola);	
				sem_post(&sema_io);			
				return;
			default:
				printf("\tcodigo no reconocido!\n");
				break;
		}
        */

}

void ejecutar_comando_unico(char* comando, char** palabras){
    char* parametro;
    if (strcmp(comando, "INICIAR_PROCESO") == 0){
        strcpy(parametro, palabras[1]);
        if (!parametro || string_is_empty(parametro)) {
            printf("ERROR: Falta path para iniciar proceso, fue omitido.\n");
        }
        // accionar y ya esta cargado el parametro
    } else if (strcmp(comando, "INICIAR_PLANIFICACION") == 0) {
        //accionar
    } else if (strcmp(comando, "FINALIZAR_PROCESO ") == 0) {
        strcpy(parametro, palabras[1]);
        if (!parametro || string_is_empty(parametro)) {
            printf("ERROR: Falta id de proceso para finalizarlo, fue omitido.\n");
        }// accionar y ya esta cargado el parametro
    } else if (strcmp(comando, "DETENER_PLANIFICACION ") == 0) {
        // accionar
    } else if (strcmp(comando, "MULTIPROGRAMACION") == 0) {
        strcpy(parametro, palabras[1]);
        if (!parametro || string_is_empty(parametro)) {
            printf("ERROR: Falta el valor a asignar para multiprogramación, fue omitido.\n");
        }
        // accionar y ya esta cargado el parametro
    } else if (strcmp(comando, "PROCESO_ESTADO") == 0) {
        // accionar
    } else {
        printf("ERROR: Comando \"%s\" no reconocido, fue omitido.\n", comando);
    }
}

void leer_y_ejecutar(char* path){
    FILE* script = fopen(path,"r");
    char* s;
    int leido = fscanf(script,"%s\n", s);
    while (leido != EOF){
        char** linea = string_split(s, " ");  // linea[0] contiene el comando y linea[1] el parametro
        ejecutar_comando_unico(linea[0], linea); // linea y palabras son lo mismo, es el resultado de split
        string_array_destroy(linea); // no se si string_split usa memoria dinamica
    }
    fclose(script);
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
    pcb_creado->cde->motivo = NO_DESALOJADO;
    
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
    destruir_cde(pcb->cde); // en serializacion.c
    free(pcb);
}

t_pcb* encontrar_pcb_por_pid(uint32_t pid, int* encontrado){
    t_pcb* pcb;
    
    *(encontrado) = 0;

    for(int i = 0; i < list_size(procesos_globales); i++){
        pcb = list_get(procesos_globales, i);
        if(pcb->cde->pid == pid){
            *(encontrado) = 1;
            break;
        }
    }

    if(*(encontrado))
        return pcb;
    else
        log_warning(logger_kernel, "PCB no encontrado de PID: %d", pid);

    return NULL; // si llega aca es porque no lo encontró
}

void retirar_pcb_de_su_respectivo_estado(uint32_t pid, int* resultado){
    t_pcb* pcb_a_retirar = encontrar_pcb_por_pid(pid, resultado);

    if(resultado){
        switch(pcb_a_retirar->estado){
            case NEW:
                sem_wait(&procesos_en_new);
                pthread_mutex_lock(&mutex_new);
                list_remove_element(procesosNew->elements, pcb_a_retirar);
                pthread_mutex_unlock(&mutex_new);
                finalizar_pcb(pcb_a_retirar, "EXIT POR CONSOLA");
                break;
            case READY:
                sem_wait(&procesos_en_ready);
                pthread_mutex_lock(&mutex_ready);
                list_remove_element(procesosReady->elements, pcb_a_retirar);
                pthread_mutex_unlock(&mutex_ready);
                finalizar_pcb(pcb_a_retirar, "EXIT POR CONSOLA");
                break;
            case BLOCKED:
                sem_wait(&procesos_en_blocked);
                pthread_mutex_lock(&mutex_block);
                list_remove_element(procesosBloqueados->elements, pcb_a_retirar);
                pthread_mutex_unlock(&mutex_block);
                finalizar_pcb(pcb_a_retirar, "EXIT POR CONSOLA");
                break;
            case EXEC:
                enviar_codigo(socket_cpu_interrupt, INTERRUPT);
                
                t_buffer* buffer = crear_buffer();
                buffer_write_uint32(buffer, pcb_a_retirar->cde->pid);
                enviar_buffer(buffer, socket_cpu_interrupt);
                destruir_buffer(buffer);
                break;
            case TERMINADO:
                // un proceso desde TERMINADO no pasa a ningun lado
                break;
            default:
                log_error(logger_kernel, "Entre al default en estado nro: %d", pcb_a_retirar->estado);
                break;
        
        }
    }
    else
        log_warning(logger_kernel, "No ejecute el switch");
}

void finalizar_pcb(t_pcb* pcb_a_finalizar, char* razon){
    agregar_pcb_a(procesosFinalizados, pcb_a_finalizar, &mutex_finalizados);
    log_info(logger_kernel, "PID: %d - Estado anterior: %s - Estado actual: %s", pcb_a_finalizar->cde->pid, obtener_nombre_estado(pcb_a_finalizar->estado), obtener_nombre_estado(TERMINADO)); //OBLIGATORIO
    log_info(logger_kernel, "Finaliza el proceso %d - Motivo: %s", pcb_a_finalizar->cde->pid, razon); // OBLIGATORIO
    sem_post(&procesos_en_exit);
    if (pcb_a_finalizar->estado == READY || pcb_a_finalizar->estado == BLOCKED)
        sem_post(&grado_de_multiprogramacion); //Como se envia a EXIT, se "libera" 1 grado de multiprog
}

void iniciar_proceso(char* path, char* size, int quantum){
	t_pcb* pcb_a_new = crear_pcb(path, quantum); // creo un nuevo pcb al que le voy a cambiar el estado

    enviar_codigo(socket_memoria, INICIAR_PROCESO_SOLICITUD); //envio la solicitud a traves del socket

    t_buffer* buffer = crear_buffer();

    buffer_write_uint32(buffer, pcb_a_new->cde->pid);
    buffer_write_string(buffer, path); 
    uint32_t tamanio = atoi(size);
    buffer_write_uint32(buffer, tamanio);
	buffer_write_uint32(buffer, quantum);
	
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

void iniciar_quantum(){
    pthread_t clock_rr;
    pthread_create(&clock_rr, NULL, (void*) controlar_tiempo_de_ejecucion, NULL);
    pthread_detach(clock_rr);
}

void controlar_tiempo_de_ejecucion(){  
    while(1){
        sem_wait(&sem_iniciar_quantum);

        uint32_t pid_pcb_before_start_clock = pcb_en_ejecucion->cde->pid;
        bool flag_clock_pcb_before_start_clock = pcb_en_ejecucion->flag_clock;

        usleep(quantum * 1000);

        if(pcb_en_ejecucion != NULL)
            pcb_en_ejecucion->fin_q = true;

        if(pcb_en_ejecucion != NULL && pid_pcb_before_start_clock == pcb_en_ejecucion->cde->pid && flag_clock_pcb_before_start_clock == pcb_en_ejecucion->flag_clock){
            enviar_codigo(socket_cpu_interrupt, DESALOJO);

            t_buffer* buffer = crear_buffer();
            buffer_write_uint32(buffer, pcb_en_ejecucion->cde->pid); // lo enviamos porque interrupt recibe un buffer, pero no hacemos nada con esto
            enviar_buffer(buffer, socket_cpu_interrupt);
            destruir_buffer(buffer);
        }
        sem_post(&sem_reloj_destruido);
    }
}

void iniciar_planificacion(){
    sem_post(&pausar_new_a_ready); // libera el semaforo
    if(pausar_new_a_ready.__align == 1) // align decide si los semaforos deben bloquearse o no despues de ser liberados
        sem_wait(&pausar_new_a_ready);
    
    sem_post(&pausar_ready_a_exec);
    if(pausar_ready_a_exec.__align == 1)
        sem_wait(&pausar_ready_a_exec);
    
    sem_post(&pausar_exec_a_finalizado);
    if(pausar_exec_a_finalizado.__align == 1)
        sem_wait(&pausar_exec_a_finalizado);
    
    sem_post(&pausar_exec_a_ready);
    if(pausar_exec_a_ready.__align == 1)
        sem_wait(&pausar_exec_a_ready);
    
    sem_post(&pausar_exec_a_blocked);
    if(pausar_exec_a_blocked.__align == 1)
        sem_wait(&pausar_exec_a_blocked);
    
    sem_post(&pausar_blocked_a_ready);
    if(pausar_blocked_a_ready.__align == 1)
        sem_wait(&pausar_blocked_a_ready);
    
    planificacion_detenida = 0;
}


void detener_planificacion(){ 
    planificacion_detenida = 1;
}

// IDA Y VUELTA CON CPU

void enviar_cde_a_cpu(){
    mensajeKernelCpu codigo = EJECUTAR_PROCESO;
    enviar_codigo(socket_cpu_dispatch, codigo);

    t_buffer* buffer_dispatch = crear_buffer();
    pthread_mutex_lock(&mutex_pcb_en_ejecucion);
    buffer_write_cde(buffer_dispatch, pcb_en_ejecucion->cde);
    pthread_mutex_unlock(&mutex_pcb_en_ejecucion);

    if(strcmp(algoritmo, "RR") == 0){
        pcb_en_ejecucion->flag_clock = false;
    }

    enviar_buffer(buffer_dispatch, socket_cpu_dispatch);
    destruir_buffer(buffer_dispatch);
    sem_post(&cde_recibido);
}


void recibir_cde_de_cpu(){
    while(1){
        sem_wait(&cde_recibido);

        t_buffer* buffer = recibir_buffer(socket_cpu_dispatch);

        pthread_mutex_lock(&mutex_exec);
        // retiramos el cde anterior a ser ejecutado
        destruir_cde(pcb_en_ejecucion->cde);        
        // actualizamos con el cde post desalojo
        pcb_en_ejecucion->cde = buffer_read_cde(buffer);
        pthread_mutex_unlock(&mutex_exec);

        evaluar_instruccion(pcb_en_ejecucion->cde->ultima_instruccion);

        destruir_buffer(buffer);
    }
}

void evaluar_instruccion(t_instruccion instruccion_actual){
    // aca vamos a tener que analizar dependiendo de la ultima instruccion ejecutada que hacer
    // por ejemplo, en los llamados a I/O vamos a tener que gestionar el ida y vuelta
    // con la interfaz involucrada, eso implica ver si existe y tambien ver si puede
    // cumplir ese pedido esa I/O
}

// UTILS COLAS DE ESTADOS
void agregar_pcb_a(t_queue* cola, t_pcb* pcb_a_agregar, pthread_mutex_t* mutex){
    
    pthread_mutex_lock(mutex);
    queue_push(cola, (void*) pcb_a_agregar);
	pthread_mutex_unlock(mutex);

}

t_pcb* retirar_pcb_de(t_queue* cola, pthread_mutex_t* mutex){
    
	pthread_mutex_lock(mutex);
	t_pcb* pcb = queue_pop(cola);
	pthread_mutex_unlock(mutex);
    
	return pcb;
}

void inicializarListas(){
    procesos_globales = list_create();
    procesosNew = queue_create();
    procesosReady = queue_create();
    procesosBloqueados = queue_create();
    procesosFinalizados = queue_create();

}

void inicializarSemaforos(){ // TERMINAR DE VER
    pthread_mutex_init(&mutex_new, NULL);
    pthread_mutex_init(&mutex_ready, NULL);
    pthread_mutex_init(&mutex_block, NULL);
    pthread_mutex_init(&mutex_finalizados, NULL);
    pthread_mutex_init(&mutex_exec, NULL);
    pthread_mutex_init(&mutex_procesos_globales, NULL);

    pthread_mutex_init(&mutex_pcb_en_ejecucion, NULL);

    sem_init(&pausar_new_a_ready, 0, 0);
    sem_init(&pausar_ready_a_exec, 0, 0);
    sem_init(&pausar_exec_a_finalizado, 0, 0);
    sem_init(&pausar_exec_a_ready, 0, 0);
    sem_init(&pausar_exec_a_blocked, 0, 0);
    sem_init(&pausar_blocked_a_ready, 0, 0);

    sem_init(&sem_iniciar_quantum, 0, 0);
    sem_init(&sem_reloj_destruido, 0, 1);
    sem_init(&no_end_kernel, 0, 0);
    sem_init(&grado_de_multiprogramacion, 0, grado_max_multiprogramacion);
    sem_init(&procesos_en_new, 0, 0);
    sem_init(&procesos_en_ready, 0, 0);
    sem_init(&procesos_en_blocked, 0, 0);
    sem_init(&procesos_en_exit, 0, 0);
    sem_init(&cde_recibido, 0, 0);
}

char* obtener_nombre_estado(t_estados estado){
    switch(estado){
        case NEW:
            return "NEW";
            break;
        case READY:
            return "READY";
            break;
        case EXEC:
            return "EXEC";
            break;
        case BLOCKED:
            return "BLOCKED";
            break;
        case TERMINADO:
            return "EXIT";
            break;
        default:
            return "NULO";
            break;
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


// TANSICIONES ENTRE ESTADOS -------------------------------------------------------------------------------------------------
// las siguientes dos funciones (o dos transiciones) son las que van a bloquearse en caso
// de que llegue la orden de detener_planificacion

void enviar_de_new_a_ready(){  
    while(1){
        sem_wait(&procesos_en_new);
        // el funcionamiento de este semaforo es para controlar la cantidad de procesos ejecutandose
        // al mismo tiempo, se inicializa en la cantidad pasada por consola y la idea es:
        // descontar 1 cuando se pasa de new a ready 
        // sumar 1 cuando un proceso pasa a exit
        sem_wait(&grado_de_multiprogramacion); // si esto esta arriba del if planif detendia => anda cambiar grado de multiprog
        // habria que chequear que ande planificacion detenida para todooo x este cambio!!

        // cuando se llama a iniciar_planificacion se le va a dar el ok para seguir con 
        // el procedimiendo de enviar de new a ready
        if(planificacion_detenida == 1){ 
            sem_wait(&pausar_new_a_ready);
        }

        t_pcb* pcb_a_ready = retirar_pcb_de(procesosNew, &mutex_new);
        
        agregar_pcb_a(procesosReady, pcb_a_ready, &mutex_ready);
        pcb_a_ready->estado = READY;

        // Pedir a memoria incializar estructuras

        log_info(logger_kernel, "PID: %d - Estado anterior: %s - Estado actual: %s", pcb_a_ready->cde->pid, obtener_nombre_estado(NEW), obtener_nombre_estado(READY)); //OBLIGATORIO
        
        // avisamos al sistema que ya hay proceso en ready
        sem_post(&procesos_en_ready);
	}
}
 
void enviar_de_exec_a_finalizado(){
    // esperamos a que haya por lo menos un proceso en exec
    sem_wait(&procesos_en_exec);
    
    if(planificacion_detenida == 1){
        sem_wait(&pausar_exec_a_finalizado);
    }

	agregar_pcb_a(procesosFinalizados, pcb_en_ejecucion, &mutex_finalizados);
    pcb_en_ejecucion->estado = EXIT;

	log_info(logger_kernel, "PID: %d - Estado anterior: %s - Estado actual: %s", pcb_en_ejecucion->cde->pid, obtener_nombre_estado(EXEC), obtener_nombre_estado(TERMINADO)); //OBLIGATORIO
	
    
    log_info(logger_kernel, "Finaliza el proceso %d - Motivo: %s", pcb_en_ejecucion->cde->pid, obtener_nombre_motivo(pcb_en_ejecucion->cde->motivo)); // OBLIGATORIO
	
    pthread_mutex_lock(&mutex_pcb_en_ejecucion);
    pcb_en_ejecucion = NULL;
    pthread_mutex_unlock(&mutex_pcb_en_ejecucion);

    sem_post(&cpu_libre); // se libera el procesador
	sem_post(&procesos_en_exit); // se agrega uno a procesosExit
    sem_post(&grado_de_multiprogramacion); // Se libera 1 grado de multiprog
}

char* obtener_nombre_motivo(cod_desalojo motivo){
    switch(motivo){
        case FINALIZACION_EXIT:
            return "FINALIZACION_EXIT";
	    case FINALIZACION_ERROR:
            return "FINALIZACION_ERROR";
	    case LLAMADA_IO:
            return "LLAMADA_IO";
	    case INTERRUPCION:
            return "INTERRUPCION";
	    case FIN_DE_QUANTUM:
            return "FIN_DE_QUANTUM";
        default:
            return NULL;  // nunca deberia entrar aca, esta pueso por los warning de retorno
    }
}

void enviar_de_exec_a_ready(){
    sem_wait(&procesos_en_exec);
    if(planificacion_detenida == 1){
        sem_wait(&pausar_exec_a_ready);
    }
    agregar_pcb_a(procesosReady, pcb_en_ejecucion, &mutex_ready);
    pcb_en_ejecucion->estado = READY;
    
    log_info(logger_kernel, "PID: %d - Estado anterior: %s - Estado actual: %s", pcb_en_ejecucion->cde->pid, obtener_nombre_estado(EXEC), obtener_nombre_estado(READY));

    pthread_mutex_lock(&mutex_pcb_en_ejecucion);
    pcb_en_ejecucion = NULL;
    pthread_mutex_unlock(&mutex_pcb_en_ejecucion);
    
    sem_post(&cpu_libre); // se libera el procesador
    sem_post(&procesos_en_ready);
}

void enviar_de_exec_a_block(){
    sem_wait(&procesos_en_exec);
    if(planificacion_detenida == 1){
        sem_wait(&pausar_exec_a_blocked);
    }
    agregar_pcb_a(procesosBloqueados, pcb_en_ejecucion, &mutex_block);
    pcb_en_ejecucion->estado = BLOCKED;

    log_info(logger_kernel, "PID: %d - Estado anterior: %s - Estado actual: %s", pcb_en_ejecucion->cde->pid, obtener_nombre_estado(EXEC), obtener_nombre_estado(BLOCKED));
    
    pthread_mutex_lock(&mutex_pcb_en_ejecucion);
    pcb_en_ejecucion = NULL;
    pthread_mutex_unlock(&mutex_pcb_en_ejecucion);
    

    sem_post(&cpu_libre); // se libera el procesador
    sem_post(&procesos_en_blocked);
}

// esta funcion se usa para mandar un pcb a ready cuando se liberan las instancias del recurso/s que necesitaba
// yendo a ready porque esta listo para ser elegido para ejecutar
void enviar_pcb_de_block_a_ready(t_pcb* pcb){
    
    sem_wait(&procesos_en_blocked);
    
    if(planificacion_detenida == 1){
        sem_wait(&pausar_blocked_a_ready);
    }

    int posicion_pcb = esta_proceso_en_cola_bloqueados(pcb);

    t_pcb* pcb_a_ready = list_get(procesosBloqueados->elements,posicion_pcb);
    
    pthread_mutex_lock(&mutex_block);
    list_remove_element(procesosBloqueados->elements, pcb);
    pthread_mutex_unlock(&mutex_block);

    agregar_pcb_a(procesosReady, pcb_a_ready, &mutex_ready);
    pcb_a_ready->estado = READY;

    log_info(logger_kernel, "PID: %d - Estado anterior: %s - Estado actual: %s", pcb_a_ready->cde->pid, obtener_nombre_estado(BLOCKED), obtener_nombre_estado(READY));

    sem_post(&procesos_en_ready);
}

void enviar_de_ready_a_exec(){
    while(1){
        sem_wait(&cpu_libre);

		sem_wait(&procesos_en_ready);

        // Log obligatorio
        char* lista_pcbs_en_ready = obtener_elementos_cargados_en(procesosReady);
        log_info(logger_kernel, "Cola Ready %s: %s", algoritmo, lista_pcbs_en_ready);
        free(lista_pcbs_en_ready);

        if(planificacion_detenida == 1){
            sem_wait(&pausar_ready_a_exec);
        }

		pthread_mutex_lock(&mutex_exec);
		pcb_en_ejecucion = retirar_pcb_de_ready_segun_algoritmo();
		pthread_mutex_unlock(&mutex_exec);

		log_info(logger_kernel, "PID: %d - Estado anterior: %s - Estado actual: %s", pcb_en_ejecucion->cde->pid, obtener_nombre_estado(READY), obtener_nombre_estado(EXEC)); //OBLIGATORIO
        pcb_en_ejecucion->estado = EXEC;

		// Primer post de ese semaforo ya que se envia por primera vez
		sem_post(&procesos_en_exec); // dsps se hace el wait cuando quiero sacar de exec (x block, etc)
        
        if(strcmp(algoritmo, "RR") == 0){
            pcb_en_ejecucion->fin_q = false;
            sem_wait(&sem_reloj_destruido); // si hay un ciclo de quantum ejecutandose se espera a que termine
            sem_post(&sem_iniciar_quantum); // da comienzo a un nuevo ciclo
        }
        enviar_cde_a_cpu(); //avisarle al kernel que empiece a correr el proceso en el cpu y le mande las cosas necesarias
    }
}

char* obtener_elementos_cargados_en(t_queue* cola){
    char* aux = string_new();
    string_append(&aux,"[");
    int pid_aux;
    char* aux_2;
    for(int i = 0 ; i < list_size(cola->elements); i++){
        t_pcb* pcb = list_get(cola->elements,i);
        pid_aux = pcb->cde->pid;
        aux_2 = string_itoa(pid_aux);
        string_append(&aux, aux_2);
        free(aux_2);
        if(i != list_size(cola->elements)-1)
            string_append(&aux,", ");
    }
    string_append(&aux,"]");
    return aux;
}

int esta_proceso_en_cola_bloqueados(t_pcb* pcb){
    int posicion_pcb = -1;
    for(int i=0; i < list_size(procesosBloqueados->elements); i++){
        t_pcb* pcb_get = list_get(procesosBloqueados->elements, i);
        int pid_pcb = pcb_get->cde->pid;
        if(pcb->cde->pid == pid_pcb){
            posicion_pcb = i;
            break;
        }
    }
    return posicion_pcb;
}
// FIN TANSICIONES ENTRE ESTADOS -------------------------------------------------------------------------------------------------

// PLANIFICACION

t_pcb* elegir_segun_fifo(){
    return retirar_pcb_de(procesosReady, &mutex_ready);
}

t_pcb* elegir_segun_rr(){
    return retirar_pcb_de(procesosReady, &mutex_ready);
}

// en teoria lo unico que cambia con virtual es que prioriza a los procesos que esten en ready+
t_pcb* elegir_segun_vrr(){
    if (!queue_is_empty(procesosReadyPlus))
        return retirar_pcb_de(procesosReadyPlus, &mutex_readyPlus);
    else
        return retirar_pcb_de(procesosReady, &mutex_ready);
}

t_pcb* retirar_pcb_de_ready_segun_algoritmo(){

    if(strcmp(algoritmo, "FIFO") == 0) return elegir_segun_fifo();
    else if(strcmp(algoritmo, "RR") == 0) return elegir_segun_rr();
    else return elegir_segun_vrr(); // caso virtual rr
}
