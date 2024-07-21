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

    sem_wait(&terminar_kernel);

    return 0;
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

    pthread_create(&hilo_esperar_IOs, NULL, (void*) esperarIOs, NULL);
    pthread_detach(hilo_esperar_IOs);
}

void conectar_io(pthread_t* hilo_io, int* socket_io){
	int err = pthread_create(hilo_io, NULL, (void*)atender_io, socket_io);
	if (err != 0) {
		perror("Fallo la creacion de hilo para IO\n");
		return;
	}
	pthread_detach(*(hilo_io));
}

void esperarIOs(){
    log_info(logger_kernel, "Esperando que se conecte una IO....");
    while (1){
        int socket_io = esperar_cliente(socket_servidor, logger_kernel);

        t_buffer* buffer = recibir_buffer(socket_io);
        char* nombre = buffer_read_string(buffer);
        char* tipo = buffer_read_string(buffer);        
        destruir_buffer(buffer);

        t_interfaz* interfaz = crear_interfaz(nombre, tipo, socket_io);

        list_add(interfacesIO, (void*)interfaz);

        conectar_io(&interfaz->hilo_io, &interfaz->socket);

        log_info(logger_kernel, "Se conecto la IO con ID (nombre): %s  TIPO: %s", nombre, tipo);
    }
} 

t_interfaz* crear_interfaz(char* nombre, char* tipo, int socket){
    t_interfaz* interfaz = malloc(sizeof(t_interfaz));
    interfaz->nombre = string_new();
    string_append(&interfaz->nombre, nombre);
    interfaz->tipo = string_new();
    string_append(&interfaz->tipo, tipo);
    interfaz->socket = socket;
    interfaz->ocupada = false;
    interfaz->pcb_ejecutando = NULL;
    interfaz->pcb_esperando = queue_create();

    return interfaz;
}

void setear_path_local() {
    char path[1024];
    ssize_t count = readlink("/proc/self/exe", path, sizeof(path) - 1);

    if (count == -1) {
        perror("readlink");
        return;
    }

    path[count] = '\0'; // Asegura que la cadena esté terminada en nulo

    // Obtiene el directorio del ejecutable
    char *dir = dirname(path);

    // Retrocede un directorio
    char *parent_dir = dirname(dir);

    // Reserva memoria para la cadena resultante
    size_t len = strlen(parent_dir);
    char *result = malloc(len + 1);

    strcpy(result, parent_dir); // Copia la ruta resultante a la memoria reservada

    // Elimina la última '/' si está presente
    if (len > 1 && result[len - 1] == '/') {
        result[len - 1] = '\0';
    }

    config_set_value(config_kernel, "RUTA_LOCAL", result);
    config_save(config_kernel);
}

void inicializar_modulo(){
	sem_init(&terminar_kernel, 0, 0);

	levantar_logger();
	levantar_config();
    setear_path_local();

	algoritmo = config_get_string_value(config_kernel, "ALGORITMO");
	quantum = config_get_int_value(config_kernel, "QUANTUM");
	grado_max_multiprogramacion = config_get_int_value(config_kernel, "MULTIPROGRAMACION_MAX");
	char **recursos_config = config_get_array_value(config_kernel,"RECURSOS");
	char **instancias = config_get_array_value(config_kernel,"INSTANCIAS_RECURSOS");
	recursos = list_create();

    flag_frenar_reloj = 0; // para VRR

	int a = 0;
	while(recursos_config[a]!= NULL){
		int num_instancias = strtol(instancias[a],NULL,10);
		t_recurso* recurso = inicializar_recurso(recursos_config[a], num_instancias);
		list_add(recursos, recurso);
		a++;
	}

    string_array_destroy(recursos_config);
    string_array_destroy(instancias);

	pid_a_asignar = 1; 
    planificacion_detenida = 0;
    
    inicializarListas();
    inicializarSemaforos();

    levantar_planificador_largo_plazo();
    levantar_planificador_corto_plazo();
    levantar_recepcion_cde();

    iniciar_quantum();
}

void levantar_planificador_largo_plazo(){
    int err = pthread_create(&hilo_plani_largo_new, NULL, (void*)enviar_de_new_a_ready, NULL);
	if (err != 0) {
		perror("Fallo la creacion de hilo para planificador de largo plazo NEW\n");
		return;
	}
	pthread_detach(hilo_plani_largo_new);

    err = pthread_create(&hilo_plani_largo_exit, NULL, (void*)terminar_proceso, NULL);
	if (err != 0) {
		perror("Fallo la creacion de hilo para planificador de largo plazo EXIT\n");
		return;
	}
	pthread_detach(hilo_plani_largo_exit);
}

void levantar_planificador_corto_plazo(){
    int err = pthread_create(&hilo_plani_corto, NULL, (void*)enviar_de_ready_a_exec, NULL);
	if (err != 0) {
		perror("Fallo la creacion de hilo para planificador de corto plazo\n");
		return;
	}
	pthread_detach(hilo_plani_corto);
}

void levantar_recepcion_cde(){
    int err = pthread_create(&hilo_recepcion_cde, NULL, (void*)atender_cpu, NULL);
	if (err != 0) {
		perror("Fallo la creacion de hilo para la recepcion de cde\n");
		return;
	}
	pthread_detach(hilo_recepcion_cde);
}


t_recurso* inicializar_recurso(char* nombre_recu, int instancias_tot){
	t_recurso* recurso = malloc(sizeof(t_recurso));

	recurso->nombre = string_new();
    string_append(&recurso->nombre, nombre_recu);

	recurso->instancias = instancias_tot;
    recurso->procesos_bloqueados = list_create();
    sem_t sem;
    sem_init(&sem, 1, instancias_tot);
    recurso->sem_recurso = sem;
	
	return recurso;
}

void consola(){
	while (1) {

		printf("Ingrese comando:\n");
		printf("\tEJECUTAR_SCRIPT [PATH] -- Ejecutar script de operaciones\n");
		printf("\tINICIAR_PROCESO [PATH] -- Iniciar proceso\n");
		printf("\tFINALIZAR_PROCESO [PID] -- Finalizar proceso\n");
		printf("\tINICIAR_PLANIFICACION -- Inicializar planificacion\n");
		printf("\tDETENER_PLANIFICACION -- Detener planificacion\n");
		printf("\tMULTIPROGRAMACION [VALOR] -- Modificar multiprogramación\n");
		printf("\tPROCESO_ESTADO -- Listar procesos por estado\n");

        char* entrada = readline("> ");

        char** palabras = string_split(entrada, " ");

        if (strcmp(palabras[0], "EJECUTAR_SCRIPT") == 0) {
            leer_y_ejecutar(palabras[1]);
        } else {
            ejecutar_comando_unico(palabras); // pasamos palabras completo para sacar el parametro cuando se pueda
        }
        string_array_destroy(palabras); 

        free(entrada);
    }
}

void ejecutar_comando_unico(char** palabras){
    if (strcmp(palabras[0], "INICIAR_PROCESO") == 0){
        if (!palabras[1] || string_is_empty(palabras[1])) {
            printf("ERROR: Falta path para iniciar proceso, fue omitido.\n");
            return;   // Se debe tener en cuenta que frente a un fallo en la escritura de un palabras[0] en consola el sistema debe permanecer estable sin reacción alguna.
        }
        printf("\nLlego la instruccion INICIAR_PROCESO con parametro: %s \n", palabras[1]);
        iniciar_proceso(palabras[1]);
    } else if (strcmp(palabras[0], "INICIAR_PLANIFICACION") == 0) {
        if (planificacion_detenida) {
            log_info(logger_kernel, "Se inicia la planificacion");
            iniciar_planificacion();
        }
    } else if (strcmp(palabras[0], "FINALIZAR_PROCESO") == 0) {
        if (!palabras[1] || string_is_empty(palabras[1])) {
            printf("ERROR: Falta id de proceso para finalizarlo, fue omitido.\n");
            return;
        }
        printf("\nLlego la instruccion FINALIZAR_PROCESO con parametro: %s\n", palabras[1]);
        terminar_proceso_consola(atoi(palabras[1]));
    } else if (strcmp(palabras[0], "DETENER_PLANIFICACION") == 0) {
        log_info(logger_kernel, "Se detiene la planificacion");
        detener_planificacion();
    } else if (strcmp(palabras[0], "MULTIPROGRAMACION") == 0) {
        if (!palabras[1] || string_is_empty(palabras[1])) {
            printf("ERROR: Falta el valor a asignar para multiprogramación, fue omitido.\n");
            return;
        }
        log_info(logger_kernel, "Se cambia el grado de multiprogramacion de %d a %d", grado_max_multiprogramacion, atoi(palabras[1]));
        cambiar_grado_multiprogramacion(palabras[1]);
    } else if (strcmp(palabras[0], "PROCESO_ESTADO") == 0) {
        printf("\nLlego la instruccion PROCESO_ESTADO\n");
        listar_procesos_por_estado();
    } else {
        printf("ERROR: Comando \"%s\" no reconocido, fue omitido.\n", palabras[0]);
    }
}

char* leer_linea_de_archivo(FILE *fp) {
    if (fp == NULL) {
        return NULL;
    }

    size_t size = 128; // Tamaño inicial del buffer
    size_t len = 0;    // Longitud actual de la cadena
    char *buffer = malloc(size); // Reserva de memoria para el buffer

    if (!buffer) {
        return NULL; // Si malloc falla, retorna NULL
    }

    while (fgets(buffer + len, size - len, fp)) {
        len += strlen(buffer + len);
        if (buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0'; // Reemplaza el salto de línea con un terminador nulo
            return buffer; // Retorna el buffer
        }

        size *= 2; // Duplica el tamaño del buffer si no encuentra un salto de línea
        char *new_buffer = realloc(buffer, size); // Redimensiona el buffer

        if (!new_buffer) {
            free(buffer);
            return NULL; // Si realloc falla, libera el buffer original y retorna NULL
        }

        buffer = new_buffer; // Actualiza el buffer
    }

    // Caso en el que se alcanza el final del archivo sin un salto de línea final
    if (len > 0) {
        buffer[len] = '\0'; // Añade el terminador nulo al final del buffer
        return buffer; // Retorna la última línea leída
    }

    free(buffer);
    return NULL; // Si no se lee nada, retorna NULL
}


void leer_y_ejecutar(char* path){
    char* ruta_completa = string_new();
    string_append(&ruta_completa, config_get_string_value(config_kernel, "RUTA_LOCAL"));
    string_append(&ruta_completa, path);
    string_append(&ruta_completa, ".txt");
    printf("\n ruta completa %s", ruta_completa);
    FILE* script = fopen(ruta_completa,"r");
    if (!script){
        perror("Error al abrir archivo, revisar el path.");
        return;
    }
    char* leido = leer_linea_de_archivo(script);
    int i = 0;
    while (leido){
        printf("\nSe lee la instruccion %s", leido);
        i++;
        char** linea = string_split(leido, " ");
        ejecutar_comando_unico(linea);
        string_array_destroy(linea); 
        leido = leer_linea_de_archivo(script);
    }
    printf("\nSe leyeron %d instrucciones", i);

    fclose(script);
}

// esta funcion fixea los casos en donde fgets al leer del archivo lee algo que deberia ser
// "palabra" como "palabra           ".
void trim_trailing_whitespace(char *str) {
    int len = strlen(str);
    while (len > 0 && (str[len - 1] == ' ' || str[len - 1] == '\n' || str[len - 1] == '\t')) {
        str[len - 1] = '\0';
        len--;
    }
}

void cambiar_grado_multiprogramacion(char* nuevo_grado){
    //para cambiarlo la planificacion debe estar detendida
    int grado_a_asignar = atoi(nuevo_grado);

    pthread_mutex_lock(&mutex_grado_multiprogramacion);
    // se puede cambiar el grado de multiprogramacion
    grado_de_multiprogramacion.__align = grado_a_asignar - grado_max_multiprogramacion + grado_de_multiprogramacion.__align - 1;
    sem_post(&grado_de_multiprogramacion);
    grado_max_multiprogramacion = grado_a_asignar; // el grado_max_multiprog se actualiza para siempre contener el grado maximo de multirpg actual
    pthread_mutex_unlock(&mutex_grado_multiprogramacion);
}

t_pcb* crear_pcb(char* path){
    t_pcb* pcb_creado = malloc(sizeof(t_pcb));
    // Asigno memoria a las estructuras
    pcb_creado->cde = malloc(sizeof(t_cde)); 
    pcb_creado->cde->registros = malloc(sizeof(t_registro));
    
    pcb_creado->cde->ultima_instruccion = crear_instruccion(NULO);

	//Inicializo el quantum, pid y el PC
    pcb_creado->cde->pid = pid_a_asignar; // arranca en 0 y va sumando 1 cada vez que se crea un pcb
    pcb_creado->cde->motivo_desalojo = NO_DESALOJADO;
    pcb_creado->cde->motivo_finalizacion = NO_FINALIZADO;

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
    pcb_creado->estado = NEW; 

	// Incremento su valor, para usar un nuevo PID la proxima vez que se cree un proceso
    pid_a_asignar ++;

    pcb_creado->flag_fin_q = 0;
    pcb_creado->clock = NULL;

    pcb_creado->recursos_asignados = list_create();
    pcb_creado->recursos_solicitados = list_create();

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


void retirar_pcb_de_su_respectivo_estado(t_pcb* pcb_a_retirar){
        switch(pcb_a_retirar->estado){
            case NEW:
                pcb_a_retirar->cde->motivo_finalizacion = INTERRUMPED_BY_USER;
                sem_wait(&procesos_en_new);
                pthread_mutex_lock(&mutex_new);
                list_remove_element(procesosNew->elements, pcb_a_retirar);
                pthread_mutex_unlock(&mutex_new);
                finalizar_pcb(pcb_a_retirar);
                break;
            case READY:
                pcb_a_retirar->cde->motivo_finalizacion = INTERRUMPED_BY_USER;
                sem_wait(&procesos_en_ready);
                pthread_mutex_lock(&mutex_ready);
                list_remove_element(procesosReady->elements, pcb_a_retirar);
                pthread_mutex_unlock(&mutex_ready);
                finalizar_pcb(pcb_a_retirar);
                break;
            case BLOCKED:
                pcb_a_retirar->cde->motivo_finalizacion = INTERRUMPED_BY_USER;
                sem_wait(&procesos_en_blocked);
                pthread_mutex_lock(&mutex_block);
                list_remove_element(procesosBloqueados->elements, pcb_a_retirar);
                pthread_mutex_unlock(&mutex_block);
                finalizar_pcb(pcb_a_retirar);
                break;
            case EXEC:
                enviar_codigo(socket_cpu_interrupt, INTERRUPT);
                t_buffer* buffer = crear_buffer();
                buffer_write_uint32(buffer, pcb_a_retirar->cde->pid);
                enviar_buffer(buffer, socket_cpu_interrupt);
                destruir_buffer(buffer);
                break;
            default:
                break;
    }
}

void finalizar_pcb(t_pcb* pcb_a_finalizar){
    agregar_pcb_a(procesosFinalizados, pcb_a_finalizar, &mutex_finalizados);
    log_info(logger_kernel, "PID: %d - Estado anterior: %s - Estado actual: %s", pcb_a_finalizar->cde->pid, obtener_nombre_estado(pcb_a_finalizar->estado), obtener_nombre_estado(TERMINADO)); //OBLIGATORIO
    log_info(logger_kernel, "Finaliza el proceso %d - Motivo: %s", pcb_a_finalizar->cde->pid, obtener_nombre_motivo_finalizacion(pcb_a_finalizar->cde->motivo_finalizacion)); // OBLIGATORIO
    sem_post(&procesos_en_exit);
    if (pcb_a_finalizar->estado == READY || pcb_a_finalizar->estado == BLOCKED)
        sem_post(&grado_de_multiprogramacion); //Como se envia a EXIT, se "libera" 1 grado de multiprog
}

void iniciar_proceso(char* path){
    
    t_pcb* pcb_a_new = crear_pcb(path); // creo un nuevo pcb al que le voy a cambiar el estado

    enviar_codigo(socket_memoria, INICIAR_PROCESO_SOLICITUD); //envio la solicitud a traves del socket

    t_buffer* buffer = crear_buffer();

    buffer_write_uint32(buffer, pcb_a_new->cde->pid);
    buffer_write_string(buffer, path); // el path es algo como instrucciones.txt, memoria le da la ruta completa con su carpeta
	
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

        // inicializado en New antes

        log_info(logger_kernel, "Se crea el proceso %d en NEW", pcb_a_new->cde->pid);
        sem_post(&procesos_en_new);
    }
    else if(cod_op == INICIAR_PROCESO_ERROR){
        log_info(logger_kernel, "No se pudo crear el proceso %d", pcb_a_new->cde->pid);
        destruir_pcb(pcb_a_new);
    }
 
}


void terminar_proceso_consola(uint32_t pid){
    int encontrado;
    t_pcb* pcb_a_liberar = encontrar_pcb_por_pid(pid, &encontrado);
    if (!pcb_a_liberar){
        log_error(logger_kernel, "No existe el proceso %d que intenta finalizar", pid);
        return;
    }
    
    liberar_recursos_pcb(pcb_a_liberar);
    pthread_mutex_lock(&mutex_procesos_globales);
    retirar_pcb_de_su_respectivo_estado(pcb_a_liberar);
    list_remove_element(procesos_globales, pcb_a_liberar);   // lo elimina de la global
    pthread_mutex_unlock(&mutex_procesos_globales);
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
    }
}

void iniciar_quantum(){
    pthread_t clock_rr;
    if (strcmp(algoritmo, "VRR") == 0){
        pthread_create(&clock_rr, NULL, (void*) controlar_tiempo, NULL);
        pthread_detach(clock_rr);
    } else if (strcmp(algoritmo, "RR") == 0){
        pthread_create(&clock_rr, NULL, (void*) controlar_tiempo, NULL);
        pthread_detach(clock_rr);
    }
}

void controlar_tiempo_de_ejecucion(){  
    while(1){
        sem_wait(&sem_iniciar_quantum);

        uint32_t pid_pcb_pre_clock = pcb_en_ejecucion->cde->pid;

        usleep(quantum * 1000);

        if(pcb_en_ejecucion != NULL && pid_pcb_pre_clock == pcb_en_ejecucion->cde->pid){
            pthread_mutex_lock(&mutex_fin_q_VRR);
            pcb_en_ejecucion->flag_fin_q = 1;
            pthread_mutex_unlock(&mutex_fin_q_VRR);
            enviar_codigo(socket_cpu_interrupt, DESALOJO);

            t_buffer* buffer = crear_buffer();
            buffer_write_uint32(buffer, pcb_en_ejecucion->cde->pid); 
            enviar_buffer(buffer, socket_cpu_interrupt);
            destruir_buffer(buffer);
        }
        sem_post(&sem_reloj_destruido);
    }
}

void controlar_tiempo(){
    while (1){
        sem_wait(&sem_iniciar_quantum);

        uint32_t pid_pcb_pre_clock = pcb_en_ejecucion->cde->pid;

        if (strcmp(algoritmo, "VRR") == 0){
            if (pcb_en_ejecucion->clock == NULL){
                pcb_en_ejecucion->clock = temporal_create();
                usleep(quantum * 1000);
            }else {
                // duermo lo restante y espero a que se acabe para interrumpir en caso de que se siga ejecutando
                log_warning(logger_kernel, "Vuelve a exec el pid %d con tiempo restante %ld", pid_pcb_pre_clock, (quantum - temporal_gettime(pcb_en_ejecucion->clock)));
                usleep((quantum - temporal_gettime(pcb_en_ejecucion->clock)) * 1000);
            }
        } else {
            // caso RR siempre va a dormirse una cantidad completa de quantum
            usleep(quantum * 1000);
        }

        if(pcb_en_ejecucion != NULL && pid_pcb_pre_clock == pcb_en_ejecucion->cde->pid){
            if (strcmp(algoritmo, "VRR") == 0){
                temporal_stop(pcb_en_ejecucion->clock);
                temporal_destroy(pcb_en_ejecucion->clock);
                pcb_en_ejecucion->clock = NULL;
            }
          
            pthread_mutex_lock(&mutex_fin_q_VRR);
            pcb_en_ejecucion->flag_fin_q = 1;
            pthread_mutex_unlock(&mutex_fin_q_VRR);
            enviar_codigo(socket_cpu_interrupt, DESALOJO);

            t_buffer* buffer = crear_buffer();
            buffer_write_uint32(buffer, pcb_en_ejecucion->cde->pid); 
            enviar_buffer(buffer, socket_cpu_interrupt);
            destruir_buffer(buffer);
        }
        sem_post(&sem_reloj_destruido);
    }
}

void controlar_tiempo_de_ejecucion_VRR(){
    while(1){
        sem_wait(&sem_iniciar_quantum);

        // puede pasar que el pcb este recien creado, entonces el flag es cero pero el clock es null...
        if (pcb_en_ejecucion->clock) { // el clock a esta altura solo existe si es que le sobro en su previa ejecucion
            log_warning(logger_kernel, "Entra el proceso %d con tiempo restante %ld ms", pcb_en_ejecucion->cde->pid, quantum - temporal_gettime(pcb_en_ejecucion->clock));
            temporal_resume(pcb_en_ejecucion->clock);
        } else {
            log_warning(logger_kernel, "Entra el proceso %d con tiempo restante %d", pcb_en_ejecucion->cde->pid, quantum); 

            pcb_en_ejecucion->clock = temporal_create();
        }

        reloj_quantum_VRR();

        if (pcb_en_ejecucion->clock)
            temporal_stop(pcb_en_ejecucion->clock);

        sem_post(&clock_VRR);
        sem_post(&clock_VRR_frenado);   
    }
}

void reloj_quantum_VRR(){
    while(flag_frenar_reloj != 1){

        if (temporal_gettime(pcb_en_ejecucion->clock) >= quantum){

            temporal_destroy(pcb_en_ejecucion->clock);
            pcb_en_ejecucion->clock = NULL;

            log_error(logger_kernel, "SE ENVIA INTERRUPCION POR FIN DE QUANTUM");
            enviar_codigo(socket_cpu_interrupt, DESALOJO);

            t_buffer* buffer = crear_buffer();
            buffer_write_uint32(buffer, pcb_en_ejecucion->cde->pid); 
            enviar_buffer(buffer, socket_cpu_interrupt);
            destruir_buffer(buffer);
            // eliminamos el clock para que se cree nuevamente cuando pase de ready a exec
            pthread_mutex_lock(&mutex_fin_q_VRR);
            pcb_en_ejecucion->flag_fin_q = 1;
            pthread_mutex_unlock(&mutex_fin_q_VRR);

            flag_frenar_reloj = 1;
        }
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
    
    sem_post(&pausar_blocked_a_readyPlus);
    if(pausar_blocked_a_ready.__align == 1)
        sem_wait(&pausar_blocked_a_ready);
    
    planificacion_detenida = 0;
}

void detener_planificacion(){ 
    planificacion_detenida = 1;
}

void listar_procesos_por_estado(){
    log_info(logger_kernel, "---------LISTANDO PROCESOS POR ESTADO---------");

    char* procesos_cargados_en_new = obtener_elementos_cargados_en(procesosNew);
    char* procesos_cargados_en_ready = obtener_elementos_cargados_en(procesosReady);
    char* procesos_cargados_en_blocked = obtener_elementos_cargados_en(procesosBloqueados);
    char* procesos_cargados_en_exit = obtener_elementos_cargados_en(procesosFinalizados);

    log_info(logger_kernel, "Procesos en %s: %s", obtener_nombre_estado(NEW), procesos_cargados_en_new);
    log_info(logger_kernel, "Procesos en %s: %s", obtener_nombre_estado(READY), procesos_cargados_en_ready);
    if(pcb_en_ejecucion != NULL)
        log_info(logger_kernel, "Proceso en %s: [%d]", obtener_nombre_estado(EXEC), pcb_en_ejecucion->cde->pid);
    else
        log_info(logger_kernel, "Proceso en %s: []", obtener_nombre_estado(EXEC));
    log_info(logger_kernel, "Procesos en %s: %s", obtener_nombre_estado(BLOCKED), procesos_cargados_en_blocked);
    log_info(logger_kernel, "Procesos en %s: %s",  obtener_nombre_estado(TERMINADO), procesos_cargados_en_exit);

    free(procesos_cargados_en_new);
    free(procesos_cargados_en_ready);
    free(procesos_cargados_en_blocked);
    free(procesos_cargados_en_exit);
}

void enviar_cde_a_cpu(){

    enviar_codigo(socket_cpu_dispatch, EJECUTAR_PROCESO);
    t_buffer* buffer_dispatch = crear_buffer();
    //pthread_mutex_lock(&mutex_pcb_en_ejecucion);
    buffer_write_cde(buffer_dispatch, pcb_en_ejecucion->cde);
    //pthread_mutex_unlock(&mutex_pcb_en_ejecucion);
    enviar_buffer(buffer_dispatch, socket_cpu_dispatch);
    destruir_buffer(buffer_dispatch);

    sem_post(&cpu_debe_retornar);

    if (strcmp(algoritmo, "FIFO") == 0)
        return;
        
    sem_post(&sem_iniciar_quantum);
}

void evaluar_solicitud_recurso(){
    t_buffer* buffer = recibir_buffer(socket_cpu_dispatch);
    t_instruccion* instruccion = buffer_read_instruccion(buffer);
    destruir_buffer(buffer);

    if (instruccion->codigo == SIGNAL)
        evaluar_signal(instruccion->parametro1);
    else 
        evaluar_wait(instruccion->parametro1);
    // avisamos que ya se debe recibir un nuevo mensaje
    sem_post(&cpu_debe_retornar);
}

void atender_cpu(){
    while (1) {

        sem_wait(&cpu_debe_retornar);
        log_warning(logger_kernel, "Recibo el signal para cde_recibido");
        
        mensajeKernelCpu cod = recibir_codigo(socket_cpu_dispatch);
        log_warning(logger_kernel, "Recibo el signal para cde_recibido");

        if (cod == CDE) {
            recibir_cde_de_cpu();
        } else if (cod == RECURSO_SOLICITUD) {
            evaluar_solicitud_recurso();
        }
    }
}

void recibir_cde_de_cpu(){

    t_buffer* buffer = recibir_buffer(socket_cpu_dispatch);
    t_cde* cde_recibido_de_cpu = buffer_read_cde(buffer);
    destruir_buffer(buffer);

    if (pcb_en_ejecucion->clock) temporal_stop(pcb_en_ejecucion->clock);

    log_warning(logger_kernel, "Recibo el cde del pid %d", cde_recibido_de_cpu->pid);

    // retiramos el cde anterior a ser ejecutado
    pthread_mutex_lock(&mutex_exec);
    destruir_cde(pcb_en_ejecucion->cde);
    pcb_en_ejecucion->cde = cde_recibido_de_cpu;
    pthread_mutex_unlock(&mutex_exec);

    if(pcb_en_ejecucion->cde->motivo_desalojo == INTERRUPCION){
        pcb_en_ejecucion->cde->motivo_finalizacion = INTERRUMPED_BY_USER;
        enviar_de_exec_a_finalizado();
    } else if (pcb_en_ejecucion->cde->motivo_desalojo == OUT_OF_MEMORY_ERROR){
        pcb_en_ejecucion->cde->motivo_finalizacion = OUT_OF_MEMORY;
        enviar_de_exec_a_finalizado();
    } else if (pcb_en_ejecucion->cde->motivo_desalojo == RECURSO_INVALIDO){
        pcb_en_ejecucion->cde->motivo_finalizacion = INVALID_RESOURCE;
        enviar_de_exec_a_finalizado();
    } else if (pcb_en_ejecucion->cde->motivo_desalojo == RECURSO_NO_DISPONIBLE){
        enviar_de_exec_a_block();
    } else {
        evaluar_instruccion(pcb_en_ejecucion->cde->ultima_instruccion);        
    }
}

void evaluar_instruccion(t_instruccion* ultima_instruccion){
    // las IO se evaluan de la misma manera ya que desde kernel se comportan siempre igual
    // lo unico que hacemos es validar y despachar/poner en espera y llegado el momento le enviamos la 
    // ultima instruccion a la IO con los datos necesarios
    switch (ultima_instruccion->codigo){
        case IO_GEN_SLEEP:
            evaluar_io(ultima_instruccion);
            break;
        case IO_STDIN_READ:
            evaluar_io(ultima_instruccion);
            break;
        case IO_STDOUT_WRITE:
            evaluar_io(ultima_instruccion);
            break;
        case IO_FS_CREATE:
            evaluar_io(ultima_instruccion);
            break;
        case IO_FS_TRUNCATE:
            evaluar_io(ultima_instruccion);
            break;
        case IO_FS_WRITE:
            evaluar_io(ultima_instruccion);
            break;
        case IO_FS_READ:
            evaluar_io(ultima_instruccion);
            break;
        case IO_FS_DELETE:
            evaluar_io(ultima_instruccion);
            break;
        case EXIT:
            pcb_en_ejecucion->cde->motivo_finalizacion = SUCCESS;
            enviar_de_exec_a_finalizado();
            break;
        default:    
            // aca solo entraria por fin de quantum
            enviar_de_exec_a_ready();
            break;
    }
}

void evaluar_io(t_instruccion* ultima_instruccion){
    if (!interfaz_valida(ultima_instruccion->parametro1)){
        terminar_proceso_consola(pcb_en_ejecucion->cde->pid);
        return;
    }

    int indice = -1;
    t_interfaz* interfaz_buscada = obtener_interfaz_en_lista(ultima_instruccion->parametro1, &indice);
    if (interfaz_buscada->ocupada){
        pthread_mutex_lock(&mutex_interfaz);
        queue_push(interfaz_buscada->pcb_esperando, (void*)pcb_en_ejecucion);
        pthread_mutex_unlock(&mutex_interfaz);
        
        enviar_de_exec_a_block(); // al hacer esto se avisa que la cpu esta libre y se continua con el proximo pcb
        return; 
    }

    despachar_pcb_a_interfaz(interfaz_buscada, pcb_en_ejecucion);

    enviar_de_exec_a_block();
}

void evaluar_signal(char* nombre_recurso_pedido){
    bool encontrado = false;
    bool asignado = false;
    int posicion_recurso;
    for(int i=0; i < list_size(recursos); i++){ //obtiene la posicion del recurso si existe
        t_recurso* recurso = list_get(recursos, i);
        char* nombre_recurso = recurso->nombre;
        if(strcmp(nombre_recurso, nombre_recurso_pedido) == 0){
            encontrado = true;
            posicion_recurso = i;
        }
    }

    if (!encontrado) {
        log_error(logger_kernel, "PID: %d - Solicitud de recurso inexistente, abortando proceso", pcb_en_ejecucion->cde->pid);
        enviar_codigo(socket_cpu_dispatch, RECURSO_INEXISTENTE);
        return;
    }

    for(int i=0; i < list_size(pcb_en_ejecucion->recursos_asignados); i++){ //se fija si lo tiene asignado
        t_recurso* recurso = list_get(pcb_en_ejecucion->recursos_asignados, i);
        char* nombre_recurso = recurso->nombre;
        if(strcmp(nombre_recurso, nombre_recurso_pedido) == 0){
            asignado = true;
        }
    }

    if(asignado){ // el recurso existe y lo tiene asignado
        t_recurso* recurso = list_get(recursos, posicion_recurso);
        recurso->instancias++; // segun el foro PUEDEN haber signals sin antes haber waits, por lo que las instancias pueden superar el maximo
        log_info(logger_kernel, "PID: %d - Signal: %s - Instancias: %d", pcb_en_ejecucion->cde->pid, nombre_recurso_pedido, recurso->instancias);
        
        list_remove_element(pcb_en_ejecucion->recursos_asignados, recurso); // saco el recurso porque lo libero

        if(list_size(recurso->procesos_bloqueados) > 0){ // Desbloquea al primer proceso de la cola de bloqueados del recurso
			sem_t semaforo_recurso = recurso->sem_recurso;

			sem_wait(&semaforo_recurso);

			t_pcb* pcb = list_remove(recurso->procesos_bloqueados, 0);
			sem_post(&semaforo_recurso);

            list_remove_element(pcb->recursos_solicitados, recurso); // saco el recurso que solicito el pcb y ya se le puede asignar
            
            list_add(pcb->recursos_asignados, recurso);
                        
            recurso->instancias--;

            enviar_pcb_de_block_a_ready(pcb);
		}
        enviar_codigo(socket_cpu_dispatch, RECURSO_OK);
    } else {
        log_error(logger_kernel, "PID: %d - Solicitud de recurso NO asignado al proceso, abortando proceso", pcb_en_ejecucion->cde->pid);
        enviar_codigo(socket_cpu_dispatch, RECURSO_INEXISTENTE);
    }
}

void evaluar_wait(char* nombre_recurso_pedido){
    bool encontrado = false;
    int posicion_recurso;
    for(int i=0; i < list_size(recursos); i++){
        t_recurso* recurso = list_get(recursos, i);
        char* nombre_recurso = recurso->nombre;
        if(strcmp(nombre_recurso_pedido, nombre_recurso) == 0){
            encontrado = true;
            posicion_recurso = i;
        }
    }
    if(encontrado){ 
        t_recurso* recurso = list_get(recursos, posicion_recurso);
        recurso->instancias--;
        log_info(logger_kernel, "PID: %d - Wait: %s - Instancias: %d", pcb_en_ejecucion->cde->pid, nombre_recurso_pedido, recurso->instancias);
        
        if(recurso->instancias < 0){  // Chequea si debe bloquear al proceso por falta de instancias

            sem_t semaforo_recurso = recurso->sem_recurso;
        	sem_wait(&semaforo_recurso);

        	list_add(recurso->procesos_bloqueados, pcb_en_ejecucion);
        	recurso->instancias = 0;
        	sem_post(&semaforo_recurso);

            log_info(logger_kernel, "PID: %d - Bloqueado por: %s", pcb_en_ejecucion->cde->pid, nombre_recurso_pedido);

            list_add(pcb_en_ejecucion->recursos_solicitados, recurso);

            enviar_codigo(socket_cpu_dispatch, RECURSO_SIN_INSTANCIAS);
        }
        else{
            list_add(pcb_en_ejecucion->recursos_asignados, recurso);

            enviar_codigo(socket_cpu_dispatch, RECURSO_OK);
        }
    } else { // el recurso no existe
        log_error(logger_kernel, "PID: %d - Solicitud de recurso inexistente, abortando proceso", pcb_en_ejecucion->cde->pid);
        enviar_codigo(socket_cpu_dispatch, RECURSO_INEXISTENTE);
    }
}

void despachar_pcb_a_interfaz(t_interfaz* interfaz, t_pcb* pcb){
    interfaz->pcb_ejecutando = pcb;
    enviar_codigo(interfaz->socket, pcb->cde->ultima_instruccion->codigo);
    t_buffer* buffer = crear_buffer();
    buffer_write_uint32(buffer, pcb->cde->pid);
    buffer_write_instruccion(buffer, pcb->cde->ultima_instruccion);
    enviar_buffer(buffer, interfaz->socket);
    destruir_buffer(buffer);
    interfaz->ocupada = true;
}

bool interfaz_valida(char* nombre_io_solicitada){
    codigoInstruccion codigo = NULO;
    
    // chequeo si existe
    int indice = -1; // aca se va a guardar el indice en donde esta la interfaz guardada, solo si lo encuentra va a tener un valor valido
    t_interfaz* interfaz_buscada = obtener_interfaz_en_lista(nombre_io_solicitada, &indice);
    if (!interfaz_buscada) {
   		printf("\n""\x1b[31m""La interfaz: %s que solicitó el proceso PID: %d no se encuentra en el sistema, se procede a finalizarlo.""\x1b[0m""\n", nombre_io_solicitada, pcb_en_ejecucion->cde->pid);
        return false;
    }

    // chequeo ahora si esta conectada
    int test_conexion = send(interfaz_buscada->socket, &codigo, sizeof(uint8_t), 0);
     // si hubo error..
    if(test_conexion < 0) {
        t_interfaz* interfaz_a_eliminar = list_get(interfacesIO, indice);
        queue_destroy(interfaz_a_eliminar->pcb_esperando);
        list_remove(interfacesIO, indice);

    	printf("\n""\x1b[31m""La interfaz: %s que solicitó el proceso PID: %d se encontraba en el sistema pero se desconectó, se procede a finalizarlo.""\x1b[0m""\n", nombre_io_solicitada, pcb_en_ejecucion->cde->pid);
        return false; 
    }

    // chequeo si puede satisfacer la solicitud
    if(!io_puede_cumplir_solicitud(interfaz_buscada->tipo, pcb_en_ejecucion->cde->ultima_instruccion->codigo)){
        printf("\n""\x1b[31m""La interfaz: %s que solicitó el proceso PID: %d no puede realizar la instrucción solicitada, se procede a finalizarlo.""\x1b[0m""\n", nombre_io_solicitada, pcb_en_ejecucion->cde->pid);
        return false; // en este caso no es necesario destruir la IO ya que no es su culpa, sino la del proceso
    }

    return true;
}

bool io_puede_cumplir_solicitud(char* tipo, codigoInstruccion instruccion){
    if (strcmp(tipo, "GENERICA") == 0) {
        return (instruccion == IO_GEN_SLEEP); // solo si es esa instruccion la io puede satisfacer
    } else if (strcmp(tipo, "STDIN") == 0) {
        return (instruccion == IO_STDIN_READ);
    } else if (strcmp(tipo, "STDOUT") == 0) {
        return (instruccion == IO_STDOUT_WRITE);
    } else if (strcmp(tipo, "DIALFS") == 0) {
        switch (instruccion){
        case IO_FS_CREATE:
            return true;
        case IO_FS_DELETE:
            return true;
        case IO_FS_READ:
            return true;
        case IO_FS_TRUNCATE:
            return true;
        case IO_FS_WRITE:
            return true;
        default:
            return false;
        }
    } else 
        return false;
}

t_interfaz* obtener_interfaz_en_lista(char* nombre, int* indice){
    t_interfaz* interfaz;
    
    int encontrado = 0;

    for(int i = 0; i < list_size(interfacesIO); i++){
        interfaz = list_get(interfacesIO, i);
        if(strcmp(interfaz->nombre, nombre) == 0){
            encontrado = 1;
            *(indice) = i; 
            break;
        }
    }

    if(encontrado)
        return interfaz;
    else
        return NULL;
}

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
    if (strcmp(algoritmo, "VRR") == 0)
        procesosReadyPlus = queue_create();
    procesosBloqueados = queue_create();
    procesosFinalizados = queue_create();
    interfacesIO = list_create();
}

void inicializarSemaforos(){ // TERMINAR DE VER
    pthread_mutex_init(&mutex_new, NULL);
    pthread_mutex_init(&mutex_ready, NULL);
    pthread_mutex_init(&mutex_block, NULL);
    pthread_mutex_init(&mutex_finalizados, NULL);
    pthread_mutex_init(&mutex_exec, NULL);
    pthread_mutex_init(&mutex_procesos_globales, NULL);
    pthread_mutex_init(&mutex_interfaz, NULL);
    pthread_mutex_init(&mutex_readyPlus, NULL);
    pthread_mutex_init(&mutex_frenar_reloj, NULL);
    pthread_mutex_init(&mutex_pcb_en_ejecucion, NULL);
    pthread_mutex_init(&mutex_fin_q_VRR, NULL);
    pthread_mutex_init(&mutex_grado_multiprogramacion, NULL);

    sem_init(&procesos_en_exec, 0, 0);

    sem_init(&clock_VRR_frenado, 0, 0);

    sem_init(&pausar_new_a_ready, 0, 0);
    sem_init(&pausar_ready_a_exec, 0, 0);
    sem_init(&pausar_exec_a_finalizado, 0, 0);
    sem_init(&pausar_exec_a_ready, 0, 0);
    sem_init(&pausar_exec_a_blocked, 0, 0);
    sem_init(&pausar_blocked_a_ready, 0, 0);
    sem_init(&pausar_blocked_a_readyPlus, 0, 0);

    sem_init(&clock_VRR, 0, 1);

    sem_init(&cpu_libre, 0, 1);
    sem_init(&sem_iniciar_quantum, 0, 0);
    sem_init(&sem_reloj_destruido, 0, 1);
    sem_init(&no_end_kernel, 0, 0);
    sem_init(&grado_de_multiprogramacion, 0, grado_max_multiprogramacion);
    sem_init(&procesos_en_new, 0, 0);
    sem_init(&procesos_en_ready, 0, 0);
    sem_init(&procesos_en_blocked, 0, 0);
    sem_init(&procesos_en_exit, 0, 0);
    sem_init(&cpu_debe_retornar, 0, 0);
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
        case READY_PLUS:
            return "READY+";
            break;
        default:
            return "NULO";
            break;
    }
}

int max(int a, int b) {
    return (a > b) ? a : b;
}

void atender_io(void* socket_io){
    int socket_interfaz_io = *((int*)socket_io);
    while(1) {
        t_buffer* buffer = NULL;
        char* id_interfaz = NULL;
        t_interfaz* interfaz = NULL;
        int indice = -1;
        mensajeIOKernel codigo = recibir_codigo(socket_interfaz_io);
        switch (codigo){
        case LIBRE:
            // recibo en un uint32 el id de la interfaz
            buffer = recibir_buffer(socket_interfaz_io);
            id_interfaz = buffer_read_string(buffer);
            destruir_buffer(buffer);
            // busco en la lista de interfaces
            interfaz = obtener_interfaz_en_lista(id_interfaz, &indice);
            // al pcb ejecutando lo paso a READY porque ya esta listo para continuar

            // si es VRR hay que pasarlo a la cola prioritaria
            if (strcmp(algoritmo, "VRR") == 0 && interfaz->pcb_ejecutando->clock){
                log_warning(logger_kernel, "Vuelve el proceso %d de IO con tiempo restante %d", interfaz->pcb_ejecutando->cde->pid, max(quantum - temporal_gettime(interfaz->pcb_ejecutando->clock), 0));
                enviar_pcb_de_block_a_readyPlus(interfaz->pcb_ejecutando);
            } // si existe el clock entonces le resta quantum
            else 
                enviar_pcb_de_block_a_ready(interfaz->pcb_ejecutando);
            // le cambio el estado del bool ocupada a false (esto es por si no tiene ninguno esperando)
            interfaz->ocupada = false;
            if (!queue_is_empty(interfaz->pcb_esperando)){
            // llamo a despachar al siguiente en su cola de espera
            // ¿Por que no evaluo si esta interfaz puede satisfacer la peticion? Porque ya se chequeo antes de poner el pcb en la cola
                t_pcb* pcb_siguiente = queue_pop(interfaz->pcb_esperando);
                despachar_pcb_a_interfaz(interfaz, pcb_siguiente);
            }
            break;
        case DESCONEXION: // suponemos que hay alguna manera de avisar
            // recibo el id
            buffer = recibir_buffer(socket_interfaz_io);
            id_interfaz = buffer_read_string(buffer);
            destruir_buffer(buffer);
            // la saco de la lista
            interfaz = obtener_interfaz_en_lista(id_interfaz, &indice);
            if (indice != -1)
                list_remove(interfacesIO, indice);
            // libero sus recursos
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
	logger_kernel = log_create("kernel_log.log", "KERNEL",false, LOG_LEVEL_INFO);
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

void destructor_pcb(void* pcb){
    t_pcb* pcb_a_destruir = pcb;
    temporal_destroy(pcb_a_destruir->clock);
    destruir_cde(pcb_a_destruir->cde);
    list_destroy(pcb_a_destruir->recursos_asignados);
    list_destroy(pcb_a_destruir->recursos_solicitados);
    free(pcb_a_destruir);
}

void destruir_listas(){
    list_destroy_and_destroy_elements(procesos_globales, destructor_pcb);
    queue_destroy(procesosNew);
    queue_destroy(procesosReady);
    if (strcmp(algoritmo, "VRR") == 0)
        queue_destroy(procesosReadyPlus);
    queue_destroy(procesosBloqueados);
    queue_destroy(procesosFinalizados);
}

void destruir_recursos(){
    int i = 0;
    while (i < list_size(recursos)){
        t_recurso* recurso = list_get(recursos, i);
        free(recurso->nombre);
        list_destroy(recurso->procesos_bloqueados);
        sem_destroy(&recurso->sem_recurso);
        free(recurso);
    }
    list_destroy(recursos);
}

void destruir_semaforos(){
    sem_destroy(&terminar_kernel);
    sem_destroy(&procesos_en_exec);
    sem_destroy(&cpu_debe_retornar);
    sem_destroy(&cpu_libre);
    sem_destroy(&procesos_en_new);
    sem_destroy(&procesos_en_ready);
    sem_destroy(&procesos_en_blocked);
    sem_destroy(&procesos_en_exit);
    sem_destroy(&sem_iniciar_quantum);
    sem_destroy(&sem_reloj_destruido);
    sem_destroy(&no_end_kernel);
    sem_destroy(&grado_de_multiprogramacion);
    sem_destroy(&clock_VRR);
    sem_destroy(&sem_liberar_archivos);
    sem_destroy(&pausar_new_a_ready);
    sem_destroy(&pausar_ready_a_exec);
    sem_destroy(&pausar_exec_a_finalizado);
    sem_destroy(&pausar_exec_a_ready);
    sem_destroy(&pausar_exec_a_blocked);
    sem_destroy(&pausar_blocked_a_ready);
    sem_destroy(&pausar_blocked_a_readyPlus);
    pthread_mutex_destroy(&mutex_procesos_globales);
    pthread_mutex_destroy(&mutex_new);
    pthread_mutex_destroy(&mutex_ready);
    pthread_mutex_destroy(&mutex_readyPlus);
    pthread_mutex_destroy(&mutex_block);
    pthread_mutex_destroy(&mutex_finalizados);
    pthread_mutex_destroy(&mutex_exec);
    pthread_mutex_destroy(&mutex_interfaz);
    pthread_mutex_destroy(&mutex_fin_q_VRR);
    pthread_mutex_destroy(&mutex_pcb_en_ejecucion);
    pthread_mutex_destroy(&mutex_frenar_reloj);
}

void destruir_interfaz(void* inter){
    t_interfaz* interfaz = inter;
    if (interfaz->socket != -1)
        close(interfaz->socket); // cerramos conexion
    free(interfaz->nombre);
    free(interfaz->tipo);
    queue_destroy(interfaz->pcb_esperando);
    free(interfaz);
}

void destruir_interfaces(){
    list_destroy_and_destroy_elements(interfacesIO, destruir_interfaz);
}

void terminar_programa(){
    if(logger_kernel) log_destroy(logger_kernel);
    if(config_kernel) config_destroy(config_kernel);
    destruir_recursos();
    destruir_semaforos();
    destruir_interfaces(); 
    destruir_listas();

    if (socket_cpu_dispatch != -1)
        close(socket_cpu_dispatch);
    if (socket_cpu_interrupt != -1)
        close(socket_cpu_interrupt);

    if (socket_memoria != -1)
        close(socket_memoria);
    if (socket_servidor != -1)
		close(socket_servidor);
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
	
    
    log_info(logger_kernel, "Finaliza el proceso %d - Motivo: %s", pcb_en_ejecucion->cde->pid, obtener_nombre_motivo_finalizacion(pcb_en_ejecucion->cde->motivo_finalizacion)); // OBLIGATORIO
	
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
        case RECURSO_INVALIDO:
            return "RECURSO_INVALIDO";
        case RECURSO_NO_DISPONIBLE:
            return "RECURSO_NO_DISPONIBLE";
        default:
            return NULL;  // nunca deberia entrar aca, esta pueso por los warning de retorno
    }
}

char* obtener_nombre_motivo_finalizacion(cod_finalizacion motivo){
    switch(motivo){
        case SUCCESS:
            return "SUCCESS";
	    case INVALID_RESOURCE:
            return "INVALID_RESOURCE";
	    case INVALID_INTERFACE:
            return "INVALID_INTERFACE";
	    case OUT_OF_MEMORY:
            return "OUT_OF_MEMORY";
	    case INTERRUMPED_BY_USER:
            return "INTERRUMPED_BY_USER";
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
    pcb_en_ejecucion->cde->motivo_desalojo = NO_DESALOJADO;
    
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
    char* lista_pcbs_en_blocked = obtener_elementos_cargados_en(procesosBloqueados);

    pcb_en_ejecucion->estado = BLOCKED;

    log_info(logger_kernel, "PID: %d - Estado anterior: %s - Estado actual: %s", pcb_en_ejecucion->cde->pid, obtener_nombre_estado(EXEC), obtener_nombre_estado(BLOCKED));
    
    log_info(logger_kernel, "Bloqueados %s: %s", algoritmo, lista_pcbs_en_blocked);
    free(lista_pcbs_en_blocked);
    
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
    pcb_a_ready->cde->motivo_desalojo = NO_DESALOJADO;

    log_info(logger_kernel, "PID: %d - Estado anterior: %s - Estado actual: %s", pcb_a_ready->cde->pid, obtener_nombre_estado(BLOCKED), obtener_nombre_estado(READY));

    sem_post(&procesos_en_ready);
}

void enviar_pcb_de_block_a_readyPlus(t_pcb* pcb){
    sem_wait(&procesos_en_blocked);
    
    if(planificacion_detenida == 1){
        sem_wait(&pausar_blocked_a_readyPlus);
    }

    int posicion_pcb = esta_proceso_en_cola_bloqueados(pcb);

    t_pcb* pcb_a_readyPlus = list_get(procesosBloqueados->elements,posicion_pcb);

    pthread_mutex_lock(&mutex_block);
    list_remove_element(procesosBloqueados->elements, pcb);
    pthread_mutex_unlock(&mutex_block);

    agregar_pcb_a(procesosReadyPlus, pcb_a_readyPlus, &mutex_readyPlus);
    pcb_a_readyPlus->estado = READY;
    pcb_a_readyPlus->cde->motivo_desalojo = NO_DESALOJADO;

    log_info(logger_kernel, "PID: %d - Estado anterior: %s - Estado actual: %s", pcb_a_readyPlus->cde->pid, obtener_nombre_estado(BLOCKED), obtener_nombre_estado(READY_PLUS));

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

        if (strcmp(algoritmo, "VRR") == 0) {
            char* lista_pcbs_en_readyPlus = obtener_elementos_cargados_en(procesosReadyPlus);
            log_info(logger_kernel, "Cola Ready+ (prioritaria) %s: %s", algoritmo, lista_pcbs_en_readyPlus);
            free(lista_pcbs_en_readyPlus);
        }

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
            pthread_mutex_lock(&mutex_fin_q_VRR);
            pcb_en_ejecucion->flag_fin_q = 0;
            pthread_mutex_unlock(&mutex_fin_q_VRR);
            sem_wait(&sem_reloj_destruido); // si hay un ciclo de quantum ejecutandose se espera a que termine
        }

        if((strcmp(algoritmo, "VRR") == 0)) {
            pthread_mutex_lock(&mutex_fin_q_VRR);
            pcb_en_ejecucion->flag_fin_q = 0;
            pthread_mutex_unlock(&mutex_fin_q_VRR);
            sem_wait(&sem_reloj_destruido);
            //pthread_mutex_lock(&mutex_frenar_reloj);
            //pcb_en_ejecucion->flag_fin_q = 0;
            //flag_frenar_reloj = 0;
            //pthread_mutex_unlock(&mutex_frenar_reloj);
            //sem_wait(&clock_VRR);
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

// ----------------------------------- APARTADO RECURSOS -------------------------------------------------------------------------

void liberar_recursos_pcb(t_pcb* pcb){
    t_recurso* recurso;
    for(int i = 0; i < list_size(pcb->recursos_asignados); i++){
        recurso = list_get(pcb->recursos_asignados, i);
        signal_recursos_asignados_pcb(pcb, recurso->nombre);
    }
    
    while(list_size(pcb->recursos_asignados) != 0){
        list_remove(pcb->recursos_asignados, 0);
    }

    while(list_size(pcb->recursos_solicitados) != 0){ // Aca entraria solo en el caso de que se finalice el proceso por consola
        recurso = list_remove(pcb->recursos_solicitados, 0);
        list_remove_element(recurso->procesos_bloqueados, pcb);
    }
}

void signal_recursos_asignados_pcb(t_pcb* pcb, char* nombre_recurso_pedido){
    int posicion_recurso;
    for(int i=0; i < list_size(recursos); i++){
        t_recurso* recurso = list_get(recursos, i);
        char* nombre_recurso = recurso->nombre;
        if(strcmp(nombre_recurso_pedido, nombre_recurso) == 0){
            posicion_recurso = i;
            break;
        }
    }
    t_recurso* recurso = list_get(recursos, posicion_recurso);
    recurso->instancias++; //podria considerarse chequear que no se pase de las instancias totales del recurso, pero me parecio innecesario
    log_info(logger_kernel, "PID: %d - LIBERANDO INSTANCIAS DEL RECURSO: %s - INSTANCIAS DISPONIBLES: %d", pcb->cde->pid, nombre_recurso_pedido, recurso->instancias);

    if(list_size(recurso->procesos_bloqueados) > 0){ // Desbloquea al primer proceso de la cola de bloqueados del recurso
	    sem_t semaforo_recurso = recurso->sem_recurso;

	    sem_wait(&semaforo_recurso);

		t_pcb* pcb_a_retirar = list_remove(recurso->procesos_bloqueados, 0);
		
		sem_post(&semaforo_recurso);
        
        // le asigno al pcb que se "libero" el recurso asi puede ejecutar
        recurso->instancias--;
        list_add(pcb_a_retirar->recursos_asignados, recurso);
        
        enviar_pcb_de_block_a_ready(pcb_a_retirar);
	}
}
