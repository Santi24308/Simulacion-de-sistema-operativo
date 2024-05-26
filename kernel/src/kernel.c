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

    terminar_programa();

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
    while (1){
        log_info(logger_kernel, "Esperando que se conecte una IO....");
        int socket_io = esperar_cliente(socket_servidor, logger_kernel);

        uint32_t tamanio_nombre;
        uint32_t tamanio_tipo;
        t_buffer* buffer = recibir_buffer(socket_io);
        char* nombre = buffer_read_string(buffer, &tamanio_nombre);
        char* tipo = buffer_read_string(buffer, &tamanio_tipo);        
        destruir_buffer(buffer);

        t_interfaz* interfaz = crear_interfaz(nombre, tipo, socket_io);

        list_add(interfacesIO, (void*)interfaz);

        conectar_io(&interfaz->hilo_io, &interfaz->socket);

        log_info(logger_kernel, "Se conecto la IO con ID (nombre): %s  TIPO: %s", nombre, tipo);

        imprimir_ios();
    }
} 

void imprimir_ios(){
    int cantidad = list_size(interfacesIO);
    int i = 0;
    printf("\nLas IOs conectadas hasta el momento son:\n");
    while (i<cantidad){
        t_interfaz* interfaz = list_get(interfacesIO, i);
        printf("\n\tNOMBRE:  %s,  TIPO:  %s", interfaz->nombre, interfaz->tipo);
        i++;
    }
    printf("\n\n");
}

t_interfaz* crear_interfaz(char* nombre, char* tipo, int socket){
    t_interfaz* interfaz = malloc(sizeof(t_buffer));
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

void inicializar_modulo(){
	sem_init(&terminar_kernel, 0, 0);

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
        printf("\x1b[31m""\tFINALIZAR_SISTEMA -- Finalizar todo el sistema, todos los modulos\n""\x1b[0m""\n");

        char* entrada = readline("> ");

        char** palabras = string_split(entrada, " ");
        printf("\nLA CANTIDAD DE PALABRAS ENCONTRADAS ES: %i", string_array_size(palabras));

        printf("\n\tINSTRUCCION: %s\n", palabras[0]);
        if (palabras[1])
            printf("\n\tPARAMETRO: %s\n", palabras[1]);

        if(strcmp(palabras[0], "FINALIZAR_SISTEMA") == 0){
            sem_post(&terminar_kernel);
            free(entrada);
            return;
        }

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
        printf("\n\tLlego la instruccion INICIAR_PROCESO con parametro: %s \n", palabras[1]);
        //iniciar_proceso(palabras[1]);
    } else if (strcmp(palabras[0], "INICIAR_PLANIFICACION") == 0) {
        if (planificacion_detenida) {
            printf("\n\tLlego la instruccion INICIAR_PLANIFICACION\n");
            //iniciar_planificacion();
        }
    } else if (strcmp(palabras[0], "FINALIZAR_PROCESO") == 0) {
        if (!palabras[1] || string_is_empty(palabras[1])) {
            printf("ERROR: Falta id de proceso para finalizarlo, fue omitido.\n");
            return;
        }
        printf("\n\tLlego la instruccion FINALIZAR_PROCESO con parametro: %s\n", palabras[1]);
        //finalizarProceso(atoi(palabras[1]));
    } else if (strcmp(palabras[0], "DETENER_PLANIFICACION") == 0) {
        printf("\n\tLlego la instruccion DETENER_PLANIFICACION\n");
        //detener_planificacion();
    } else if (strcmp(palabras[0], "MULTIPROGRAMACION") == 0) {
        if (!palabras[1] || string_is_empty(palabras[1])) {
            printf("ERROR: Falta el valor a asignar para multiprogramación, fue omitido.\n");
            return;
        }
        printf("\n\tLlego la instruccion MULTIPROGRAMACION con parametro: %s\n", palabras[1]);
        //grado_max_multiprogramacion = atoi(palabras[1]);
    } else if (strcmp(palabras[0], "PROCESO_ESTADO") == 0) {
        printf("\n\tLlego la instruccion PROCESO_ESTADO\n");
        //listar_procesos_por_estado();
    } else {
        printf("ERROR: Comando \"%s\" no reconocido, fue omitido.\n", palabras[0]);
    }
}

void leer_y_ejecutar(char* path){
    FILE* script = fopen(path,"r");
    if (!script){
        perror("Error al abrir archivo, revisar el path.");
        return;
    }
    char leido[200];

    while (fgets(leido, 200, script) != NULL && !feof(script)){
        trim_trailing_whitespace(leido);
        char** linea = string_split(leido, " ");
        ejecutar_comando_unico(linea);
        string_array_destroy(linea); 
        sleep(1);
    }

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

///////// CHECKPOINT 2 PLANIFICACION /////////////////

t_pcb* crear_pcb(char* path){
    t_pcb* pcb_creado = malloc(sizeof(t_pcb));
    // Asigno memoria a las estructuras
    pcb_creado->cde = malloc(sizeof(t_cde)); 
    pcb_creado->cde->registros = malloc(sizeof(t_registro));

	//Inicializo el quantum, pid y el PC
	pcb_creado->quantum = quantum;  // el valor de quantum ya esta seteado
    pcb_creado->cde->pid = pid_a_asignar; // arranca en 0 y va sumando 1 cada vez que se crea un pcb
    pcb_creado->cde->pc = 0;
    pcb_creado->cde->motivo = NO_DESALOJADO;
    pcb_creado->fin_q = false;
    pcb_creado->flag_clock = false;

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

void iniciar_proceso(char* path){
	t_pcb* pcb_a_new = crear_pcb(path); // creo un nuevo pcb al que le voy a cambiar el estado

    enviar_codigo(socket_memoria, INICIAR_PROCESO_SOLICITUD); //envio la solicitud a traves del socket

    t_buffer* buffer = crear_buffer();

    buffer_write_uint32(buffer, pcb_a_new->cde->pid);
    buffer_write_string(buffer, path); 
	
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

void finalizarProceso(uint32_t pid_string){
    // Antes de hacer finalizar proceso, hay que ver que como desde todos los estados podes pasar a exit, quiza conviene 
    // tener el estado en el pcb, para saber en que lista lo sacas? 
    // Aunque sin este parametro se puede hacer quiza es un poco mas rebuscado?

    int resultado = 0;
    retirar_pcb_de_su_respectivo_estado(pid_string, &resultado);    
    // detectar deadlock se llama en menu.c
    return;
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
    if (instruccion_actual.codigo == IO_GEN_SLEEP) {
        if (!interfaz_valida(instruccion_actual.parametro1)){
            finalizarProceso(pcb_en_ejecucion->cde->pid);
            return;
        }

        int indice = -1;
        t_interfaz* interfaz_buscada = obtener_interfaz_en_lista(instruccion_actual.parametro1, &indice);
        if (interfaz_buscada->ocupada){
            queue_push(interfaz_buscada->pcb_esperando, (void*)pcb_en_ejecucion);
            enviar_de_exec_a_block(); // al hacer esto se avisa que la cpu esta libre y se continua con el proximo pcb
            return; 
        }
      
        despachar_pcb_a_interfaz(interfaz_buscada, pcb_en_ejecucion); 
        
        enviar_de_exec_a_block();
    }

}

void despachar_pcb_a_interfaz(t_interfaz* interfaz, t_pcb* pcb){
    enviar_codigo(interfaz->socket, pcb->cde->ultima_instruccion.codigo);
    t_buffer* buffer = crear_buffer();
    buffer_write_instruccion(buffer, &(pcb->cde->ultima_instruccion));
    enviar_buffer(buffer, interfaz->socket);
    destruir_buffer(buffer);
}

bool interfaz_valida(char* nombre_io_solicitada){
    codigoInstruccion codigo = NULO;
    
    // chequeo si existe
    int indice = -1; // aca se va a guardar el indice en donde esta la interfaz guardada, solo si lo encuentra va a tener un valor valido
    t_interfaz* interfaz_buscada = obtener_interfaz_en_lista(nombre_io_solicitada, &indice);
    if (!interfaz_buscada) 
        return false;

    // chequeo ahora si esta conectada
    int test_conexion = send(interfaz_buscada->socket, &codigo, sizeof(uint8_t), 0);
     // si hubo error..
    if(test_conexion < 0) {
        t_interfaz* interfaz_a_eliminar = list_get(interfacesIO, indice);
        queue_destroy(interfaz_a_eliminar->pcb_esperando);
        list_remove(interfacesIO, indice);
        return false; 
    }

    // chequeo si puede satisfacer la solicitud
    if(!io_puede_cumplir_solicitud(interfaz_buscada->tipo, pcb_en_ejecucion->cde->ultima_instruccion.codigo))
        return false; // en este caso no es necesario destruir la IO ya que no es su culpa, sino la del proceso

    return true;
}

bool io_puede_cumplir_solicitud(char* tipo, codigoInstruccion instruccion){
    if (strcmp(tipo, "GEN") == 0) {
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
    interfacesIO = list_create();
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

void atender_io(void* socket_io){
    int socket_interfaz_io = *((int*)socket_io);
    while(1) {
        t_buffer* buffer = NULL;
        char* id_interfaz = NULL;
        t_interfaz* interfaz = NULL;
        uint32_t tamanio = 0;
        int indice = -1;
        mensajeIOKernel codigo = recibir_codigo(socket_interfaz_io);
        switch (codigo){
        case LIBRE:
            // recibo en un uint32 el id de la interfaz
            buffer = recibir_buffer(socket_interfaz_io);
            id_interfaz = buffer_read_string(buffer, &tamanio);
            destruir_buffer(buffer);
            // busco en la lista de interfaces
            interfaz = obtener_interfaz_en_lista(id_interfaz, &indice);
            // al pcb ejecutando lo paso a READY porque ya esta listo para continuar
            enviar_pcb_de_block_a_ready(interfaz->pcb_ejecutando);
            // le cambio el estado del bool ocupada a false (esto es por si no tiene ninguno esperando)
            interfaz->ocupada = false;
            if (!queue_is_empty(interfaz->pcb_esperando)){
            // llamo a despachar al siguiente en su cola de espera
            // ¿Por que no evaluo si esta interfaz puede satisfacer la peticion? Porque ya se chequeo antes de poner el pcb en la cola
                t_pcb* pcb_siguiente = queue_pop(interfaz->pcb_esperando);
                despachar_pcb_a_interfaz(interfaz, pcb_siguiente);
                interfaz->pcb_ejecutando = pcb_siguiente;
            // y vuelve a estar ocupada
                interfaz->ocupada = true;
            }
            break;
        case DESCONEXION: // suponemos que hay alguna manera de avisar
            // recibo el id
            buffer = recibir_buffer(socket_interfaz_io);
            id_interfaz = buffer_read_string(buffer, &tamanio);
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
