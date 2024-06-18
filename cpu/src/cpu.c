#include "cpu.h"

int main(int argc, char* argv[]) {

	if(argc != 2) {
		printf("ERROR: Tenés que pasar el path del archivo config de CPU\n");
		return -1;
	}

	config_path = argv[1];

	inicializar_modulo();
	conectar();

    sem_wait(&terminar_cpu);

    terminar_programa();

    return 0;
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
    conectar_kernel_dispatch();



	if (socket_servidor_interrupt == -1) {
		perror("Fallo la creacion del servidor para Kernel interrupt.\n");
		exit(EXIT_FAILURE);
	}

    conectar_kernel_interrupt();
}

void inicializar_modulo(){
	sem_init(&terminar_cpu, 0, 0);
	levantar_logger();
	levantar_config();
	inicializar_registros();
	inicializarSemaforos();
    interrupcion =0;
    interrupcion_consola = 0;
    fin_q = 0;
    cde_ejecutando = NULL;
    tlb = queue_create();
    cantidad_entradas_tlb = config_get_int_value(config_cpu, "CANTIDAD_ENTRADAS_TLB");
    algoritmo_tlb = config_get_string_value(config_cpu, "ALGORITMO_TLB");
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
    registros_cpu->PC = 0;
}

void inicializarSemaforos(){
	pthread_mutex_init(&mutex_cde_ejecutando, NULL);
	pthread_mutex_init(&mutex_desalojar, NULL);
	pthread_mutex_init(&mutex_instruccion_actualizada, NULL);
}

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

        // en caso de que se desconecte kernel NO se sigue
        // nos sirve mas que nada para tener una salida segura
        if (op_code == UINT8_MAX || !buffer) {
            sem_post(&terminar_cpu);
            return;
        }

		switch (op_code){
			case EJECUTAR_PROCESO:
				t_cde* cde_recibido = buffer_read_cde(buffer);
				destruir_buffer(buffer);

                cde_ejecutando = cde_recibido;

				pthread_mutex_lock(&mutex_cde_ejecutando);
				pid_de_cde_ejecutando = cde_recibido->pid;
				pthread_mutex_unlock(&mutex_cde_ejecutando);

				ejecutar_proceso(cde_recibido);

                cde_ejecutando = NULL;

				break;
			default:
				break;
		}
	}
}

void atender_kernel_interrupt(){
    while(1){
        mensajeKernelCpu op_code = recibir_codigo(socket_kernel_interrupt);
        t_buffer* buffer = recibir_buffer(socket_kernel_interrupt); // recibe pid o lo que necesite
        // en caso de que se desconecte kernel NO se sigue
        // nos sirve mas que nada para tener una salida segura
        if (op_code == UINT8_MAX || !buffer) {
            sem_post(&terminar_cpu);
            return;
        }
        uint32_t pid_recibido = buffer_read_uint32(buffer);
        destruir_buffer(buffer);
        
        // asumimos que puede pasar que el pid recibido sea distinto del actual
        if (pid_de_cde_ejecutando == pid_recibido) {
            switch (op_code){
                case INTERRUPT:
                    pthread_mutex_lock(&mutex_desalojar);
                    interrupcion = 1;
                    pthread_mutex_unlock(&mutex_desalojar);
                    break;
                case DESALOJO:
                    
                    if(es_bloqueante(instruccion_actualizada)) 
                        break;
                    
                    pthread_mutex_lock(&mutex_desalojar);
                    fin_q = 1;
                    pthread_mutex_unlock(&mutex_desalojar);
                    break;
                default:
                    break;
            }
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

    t_buffer* buffer = recibir_buffer(socket_memoria);
    tamanio_pagina = buffer_read_uint32(buffer);
    destruir_buffer(buffer);
}

void terminar_programa(){
	terminar_conexiones(1, socket_memoria);
    if(logger_cpu) log_destroy(logger_cpu);
    if(config_cpu) config_destroy(config_cpu);
}

void iterator(char* value) {
	log_info(logger_cpu,"%s", value);
}

uint32_t buscar_valor_registro(char* registro) {
	if (strcmp(registro, "AX") == 0)
		return registros_cpu->AX;
	else if (strcmp(registro, "BX") == 0)
		return registros_cpu->BX;
	else if (strcmp(registro, "CX") == 0)
		return registros_cpu->CX;
	else if (strcmp(registro, "DX") == 0)
		return registros_cpu->DX;
	else if (strcmp(registro, "EAX") == 0)
		return registros_cpu->EAX;
	else if (strcmp(registro, "EBX") == 0)
		return registros_cpu->EBX;
	else if (strcmp(registro, "ECX") == 0)
		return registros_cpu->ECX;
	else if (strcmp(registro, "EDX") == 0)
		return registros_cpu->EDX;
	else if (strcmp(registro, "SI") == 0)
		return registros_cpu->SI;
	else if (strcmp(registro, "DI") == 0)
		return registros_cpu->DI;

	return 0;
}

void cargar_registros(){
    registros_cpu->AX = cde_ejecutando->registros->AX;
    registros_cpu->BX = cde_ejecutando->registros->BX;
    registros_cpu->CX = cde_ejecutando->registros->CX;
    registros_cpu->DX = cde_ejecutando->registros->DX;
	registros_cpu->EAX = cde_ejecutando->registros->EAX;
	registros_cpu->EBX = cde_ejecutando->registros->EBX;
	registros_cpu->ECX = cde_ejecutando->registros->ECX;
	registros_cpu->EDX = cde_ejecutando->registros->EDX;
	registros_cpu->SI = cde_ejecutando->registros->SI;
	registros_cpu->DI = cde_ejecutando->registros->DI;
    registros_cpu->PC = cde_ejecutando->registros->PC;
}

void guardar_registros(){
    cde_ejecutando->registros->AX = registros_cpu->AX;
    cde_ejecutando->registros->BX = registros_cpu->BX;
    cde_ejecutando->registros->CX = registros_cpu->CX;
    cde_ejecutando->registros->DX = registros_cpu->DX;
	cde_ejecutando->registros->EAX = registros_cpu->EAX;
	cde_ejecutando->registros->EBX = registros_cpu->EBX;
	cde_ejecutando->registros->ECX = registros_cpu->ECX;
	cde_ejecutando->registros->EDX = registros_cpu->EDX;
	cde_ejecutando->registros->SI = registros_cpu->SI;
	cde_ejecutando->registros->DI = registros_cpu->DI;
    cde_ejecutando->registros->PC = registros_cpu->PC;
}

bool es_bloqueante(codigoInstruccion cod_instruccion){
    switch(cod_instruccion){
    case WAIT:
        return true;
        break;
    case SIGNAL:
        return true;
        break;
   	case IO_GEN_SLEEP:
        return true;
        break;
    case IO_STDIN_READ:
        return true;
        break;
	case IO_STDOUT_WRITE:
        return true;
        break;
	case IO_FS_CREATE:
        return true;
        break;
	case IO_FS_DELETE:
        return true;
        break;
	case IO_FS_TRUNCATE:
        return true;
        break;
	case IO_FS_WRITE:
        return true;
        break;
	case IO_FS_READ:
        return true;
        break;	
	case EXIT:
        return true;
        break;
    default:
        return false;
        break;
    }
}

void ejecutar_proceso(){
	cargar_registros(cde_ejecutando);	
	t_instruccion* instruccion_a_ejecutar;

    while(interrupcion != 1 && realizar_desalojo != 1 && fin_q != 1){

        enviar_codigo(socket_memoria, PEDIDO_INSTRUCCION);
        t_buffer* buffer_envio = crear_buffer();
        buffer_write_uint32(buffer_envio, cde_ejecutando->pid);
        // el PC se carga del cde a la CPU previamente en cargar registros
        buffer_write_uint32(buffer_envio, registros_cpu->PC);

        enviar_buffer(buffer_envio, socket_memoria);

        destruir_buffer(buffer_envio);
        
        log_info(logger_cpu, "PID: %d - FETCH - Program Counter: %d", cde_ejecutando->pid, registros_cpu->PC);
        
        t_buffer* buffer_recibido = recibir_buffer(socket_memoria);
        instruccion_a_ejecutar = buffer_read_instruccion(buffer_recibido);
        destruir_buffer(buffer_recibido);
        
        pthread_mutex_lock(&mutex_instruccion_actualizada);
        instruccion_actualizada = instruccion_a_ejecutar->codigo;
        pthread_mutex_unlock(&mutex_instruccion_actualizada);

        destruir_instruccion(cde_ejecutando->ultima_instruccion);
        cde_ejecutando->ultima_instruccion = instruccion_a_ejecutar;

        ejecutar_instruccion(instruccion_a_ejecutar);

        registros_cpu->PC += 1;
	}

	if(interrupcion){
        pthread_mutex_lock(&mutex_desalojar);
        fin_q = 0;
		interrupcion = 0;
        realizar_desalojo = 0;
        pthread_mutex_unlock(&mutex_desalojar);
        log_warning(logger_cpu, "PID: %d - Volviendo a kernel por instruccion %s", cde_ejecutando->pid, obtener_nombre_instruccion(instruccion_a_ejecutar));
        cde_ejecutando->motivo_desalojo = INTERRUPCION;
        desalojar_cde(instruccion_a_ejecutar);
	} else if (realizar_desalojo){ // salida por fin de quantum
        pthread_mutex_lock(&mutex_desalojar);
        fin_q = 0;
		interrupcion = 0;  
        realizar_desalojo = 0;
        pthread_mutex_unlock(&mutex_desalojar);
        log_warning(logger_cpu, "PID: %d - Desalojado por %s", cde_ejecutando->pid, obtener_nombre_motivo_desalojo(cde_ejecutando->motivo_desalojo)); 
        desalojar_cde(instruccion_a_ejecutar);
    } else if (fin_q){
        pthread_mutex_lock(&mutex_desalojar);
        fin_q = 0;
		interrupcion = 0;  
        realizar_desalojo = 0;
        pthread_mutex_unlock(&mutex_desalojar);
        log_warning(logger_cpu, "PID: %d - Desalojado por fin de Quantum", cde_ejecutando->pid); 
        cde_ejecutando->motivo_desalojo = FIN_DE_QUANTUM;
        desalojar_cde(instruccion_a_ejecutar);
    }
}

char* obtener_nombre_motivo_desalojo(cod_desalojo cod){
    switch(cod){
        case LLAMADA_IO:
            return "LLAMADA_IO";
	    case FINALIZACION_EXIT:
            return "FINALIZACION_EXIT";
	    case INTERRUPCION:
            return "INTERRUPCION";
	    case FIN_DE_QUANTUM:
            return "FIN_DE_QUANTUM";
	    case FINALIZACION_ERROR:
            return "FINALIZACION_ERROR";
        case RECURSOS:
            return "RECURSOS";
        default:
            return NULL;  // nunca deberia entrar aca, esta pueso por los warning de retorno
    }
}

void ejecutar_instruccion(t_instruccion* instruccion_a_ejecutar){
    uint32_t parametro2 = 0;
    switch(instruccion_a_ejecutar->codigo){
        case SET:
            log_info(logger_cpu, "PID: %d - Ejecutando: %s - %s %s", cde_ejecutando->pid, obtener_nombre_instruccion(instruccion_a_ejecutar), instruccion_a_ejecutar->parametro1, instruccion_a_ejecutar->parametro2);
            parametro2 = leerEnteroParametroInstruccion(2, instruccion_a_ejecutar);
            ejecutar_set(instruccion_a_ejecutar->parametro1, parametro2);
            break;
        case SUM:
            log_info(logger_cpu, "PID: %d - Ejecutando: %s - %s %s", cde_ejecutando->pid, obtener_nombre_instruccion(instruccion_a_ejecutar), instruccion_a_ejecutar->parametro1, instruccion_a_ejecutar->parametro2);
            ejecutar_sum(instruccion_a_ejecutar->parametro1, instruccion_a_ejecutar->parametro2);
            break;
        case SUB:
            log_info(logger_cpu, "PID: %d - Ejecutando: %s - %s %s", cde_ejecutando->pid, obtener_nombre_instruccion(instruccion_a_ejecutar), instruccion_a_ejecutar->parametro1, instruccion_a_ejecutar->parametro2);
            ejecutar_sub(instruccion_a_ejecutar->parametro1, instruccion_a_ejecutar->parametro2);
            break;
        case JNZ:
            log_info(logger_cpu, "PID: %d - Ejecutando: %s - %s %s", cde_ejecutando->pid, obtener_nombre_instruccion(instruccion_a_ejecutar), instruccion_a_ejecutar->parametro1, instruccion_a_ejecutar->parametro2);
            parametro2 = leerEnteroParametroInstruccion(2, instruccion_a_ejecutar);
            ejecutar_jnz(instruccion_a_ejecutar->parametro1, parametro2);
            break;
        case IO_GEN_SLEEP:
            log_info(logger_cpu, "PID: %d - Ejecutando: %s - %s %s", cde_ejecutando->pid, obtener_nombre_instruccion(instruccion_a_ejecutar), instruccion_a_ejecutar->parametro1, instruccion_a_ejecutar->parametro2);
            cde_ejecutando->motivo_desalojo = LLAMADA_IO;
            realizar_desalojo = 1;
            break;
        case MOV_IN:
            log_info(logger_cpu, "PID: %d - Ejecutando: %s - %s %s", cde_ejecutando->pid, obtener_nombre_instruccion(instruccion_a_ejecutar), instruccion_a_ejecutar->parametro1, instruccion_a_ejecutar->parametro2);
            ejecutar_mov_in(instruccion_a_ejecutar->parametro1, instruccion_a_ejecutar->parametro2);
            break;
        case MOV_OUT:
            log_info(logger_cpu, "PID: %d - Ejecutando: %s - %s %s", cde_ejecutando->pid, obtener_nombre_instruccion(instruccion_a_ejecutar), instruccion_a_ejecutar->parametro1, instruccion_a_ejecutar->parametro2);
            ejecutar_mov_out(instruccion_a_ejecutar->parametro1, instruccion_a_ejecutar->parametro2);
            break;
        case RESIZE:
            log_info(logger_cpu, "PID: %d - Ejecutando: %s - %s", cde_ejecutando->pid, obtener_nombre_instruccion(instruccion_a_ejecutar), instruccion_a_ejecutar->parametro1);
            ejecutar_resize(atoi(instruccion_a_ejecutar->parametro1));
            break;
        case WAIT:
            log_info(logger_cpu, "PID: %d - Ejecutando: %s - %s", cde_ejecutando->pid, obtener_nombre_instruccion(instruccion_a_ejecutar), instruccion_a_ejecutar->parametro1);
            cde_ejecutando->motivo_desalojo = RECURSOS;
            realizar_desalojo = 1;
            break;
        case SIGNAL:
            log_info(logger_cpu, "PID: %d - Ejecutando: %s - %s", cde_ejecutando->pid, obtener_nombre_instruccion(instruccion_a_ejecutar), instruccion_a_ejecutar->parametro1);
            cde_ejecutando->motivo_desalojo = RECURSOS;
            realizar_desalojo = 1;
            break;
        case COPY_STRING:
            log_info(logger_cpu, "PID: %d - Ejecutando: %s - %s", cde_ejecutando->pid, obtener_nombre_instruccion(instruccion_a_ejecutar), instruccion_a_ejecutar->parametro1);
            break;
        case IO_STDIN_READ:
            log_info(logger_cpu, "PID: %d - Ejecutando: %s - %s %s", cde_ejecutando->pid, obtener_nombre_instruccion(instruccion_a_ejecutar), instruccion_a_ejecutar->parametro1, instruccion_a_ejecutar->parametro2);
            cde_ejecutando->motivo_desalojo = LLAMADA_IO;
            actualizar_dirLogica_a_dirFisica(instruccion_a_ejecutar);
            realizar_desalojo = 1;
            break;
        case IO_STDOUT_WRITE: 
            log_info(logger_cpu, "PID: %d - Ejecutando: %s - %s %s %s", cde_ejecutando->pid, obtener_nombre_instruccion(instruccion_a_ejecutar), instruccion_a_ejecutar->parametro1, instruccion_a_ejecutar->parametro2, instruccion_a_ejecutar->parametro3);
            cde_ejecutando->motivo_desalojo = LLAMADA_IO;
            actualizar_dirLogica_a_dirFisica(instruccion_a_ejecutar);
            realizar_desalojo = 1;
            break;
        case IO_FS_CREATE:
            log_info(logger_cpu, "PID: %d - Ejecutando: %s - %s %s", cde_ejecutando->pid, obtener_nombre_instruccion(instruccion_a_ejecutar), instruccion_a_ejecutar->parametro1, instruccion_a_ejecutar->parametro2);
            cde_ejecutando->motivo_desalojo = LLAMADA_IO;
            realizar_desalojo = 1;
            break;
        case IO_FS_DELETE:
            log_info(logger_cpu, "PID: %d - Ejecutando: %s - %s %s", cde_ejecutando->pid, obtener_nombre_instruccion(instruccion_a_ejecutar), instruccion_a_ejecutar->parametro1, instruccion_a_ejecutar->parametro2);
            cde_ejecutando->motivo_desalojo = LLAMADA_IO;
            realizar_desalojo = 1;
            break;
        case IO_FS_TRUNCATE: 
            log_info(logger_cpu, "PID: %d - Ejecutando: %s - %s %s %s", cde_ejecutando->pid, obtener_nombre_instruccion(instruccion_a_ejecutar), instruccion_a_ejecutar->parametro1, instruccion_a_ejecutar->parametro2, instruccion_a_ejecutar->parametro3);
            cde_ejecutando->motivo_desalojo = LLAMADA_IO;
            realizar_desalojo = 1;
            break;
        case IO_FS_WRITE: 
            log_info(logger_cpu, "PID: %d - Ejecutando: %s - %s %s %s %s %s", cde_ejecutando->pid, obtener_nombre_instruccion(instruccion_a_ejecutar), instruccion_a_ejecutar->parametro1, instruccion_a_ejecutar->parametro2, instruccion_a_ejecutar->parametro3, instruccion_a_ejecutar->parametro4, instruccion_a_ejecutar->parametro5);
            cde_ejecutando->motivo_desalojo = LLAMADA_IO;
            realizar_desalojo = 1;
            break;
        case IO_FS_READ: 
            log_info(logger_cpu, "PID: %d - Ejecutando: %s - %s %s %s %s %s", cde_ejecutando->pid, obtener_nombre_instruccion(instruccion_a_ejecutar), instruccion_a_ejecutar->parametro1, instruccion_a_ejecutar->parametro2, instruccion_a_ejecutar->parametro3, instruccion_a_ejecutar->parametro4, instruccion_a_ejecutar->parametro5);
            cde_ejecutando->motivo_desalojo = LLAMADA_IO;
            realizar_desalojo = 1;
            break;
        case EXIT:
            log_info(logger_cpu, "PID: %d - Ejecutando: %s", cde_ejecutando->pid, obtener_nombre_instruccion(instruccion_a_ejecutar));
            realizar_desalojo = 1;
            cde_ejecutando->motivo_desalojo = FINALIZACION_EXIT;
            break;
        default:
            log_warning(logger_cpu, "Instruccion no reconocida");
            break;
    }
}

void actualizar_dirLogica_a_dirFisica(t_instruccion* instruccion_a_ejecutar){
    uint32_t dir_fisica = UINT32_MAX;
    bool pagina_en_tlb = se_encuentra_en_tlb(atoi(instruccion_a_ejecutar->parametro2), &dir_fisica); 
    if (!pagina_en_tlb)
        dir_fisica = calcular_direccion_fisica(atoi(instruccion_a_ejecutar->parametro2), cde_ejecutando);

    instruccion_a_ejecutar->parametro2 = uint32_to_string(dir_fisica);

    // actualizo el parametro de tamanio tambien
    uint32_t tamanio = buscar_valor_registro(instruccion_a_ejecutar->parametro3);
    instruccion_a_ejecutar->parametro3 = uint32_to_string(tamanio);
}

char* uint32_to_string(uint32_t number) {
    // Determinar el tamaño máximo necesario para almacenar el número en una cadena.
    // El tamaño máximo de un uint32_t en decimal es 10 dígitos más el carácter nulo.
    char* str = malloc(11);
    if (!str) {
        // Manejo de error de memoria (opcional).
        return NULL;
    }

    // Convertir el número a cadena.
    sprintf(str, "%u", number);

    return str;
}

void desalojar_cde(t_instruccion* instruccion_a_ejecutar){
    guardar_registros(); //cargar registros de cpu en el cde
    devolver_cde_a_kernel();
    destruir_cde(cde_ejecutando);
    
    // seteamos un valor imposible para que el sistema sepa que no hay proceso en ejecución
    pthread_mutex_lock(&mutex_cde_ejecutando);
    pid_de_cde_ejecutando = UINT32_MAX;  
    pthread_mutex_unlock(&mutex_cde_ejecutando);

    // seteamos un valor nulo para que el sistema sepa que no hay instruccion
    pthread_mutex_lock(&mutex_instruccion_actualizada);
    instruccion_actualizada = NULO;
    pthread_mutex_unlock(&mutex_instruccion_actualizada);
}

void devolver_cde_a_kernel(){
    // elimine la linea en donde se copia la ultima instruccion porque ya lo hace antes de ejecutarla apenas la obtiene de memoria
    t_buffer* buffer = crear_buffer();
    buffer_write_cde(buffer, cde_ejecutando);
    enviar_buffer(buffer, socket_kernel_dispatch);
    destruir_buffer(buffer);
}

// -------------------------------------------------
//          INSTRUCCIONES SIN MEMORIA
// -------------------------------------------------

void ejecutar_set(char* registro, uint32_t valor_recibido){
    if (strcmp(registro, "AX") == 0) 
        registros_cpu->AX = valor_recibido;   
    else if(strcmp(registro, "BX") == 0)
        registros_cpu->BX = valor_recibido;
    else if(strcmp(registro, "CX") == 0)
        registros_cpu->CX = valor_recibido;
    else if(strcmp(registro, "DX") == 0)
        registros_cpu->DX = valor_recibido;
	else if(strcmp(registro, "EAX") == 0)
        registros_cpu->EAX= valor_recibido;
	else if(strcmp(registro, "EBX") == 0)
        registros_cpu->EBX = valor_recibido;
	else if(strcmp(registro, "ECX") == 0)
        registros_cpu->ECX = valor_recibido;	
	else if(strcmp(registro, "EDX") == 0)
        registros_cpu->EDX = valor_recibido;	
	else if(strcmp(registro, "SI") == 0)
        registros_cpu->SI = valor_recibido;		
	else if(strcmp(registro, "DI") == 0)
        registros_cpu->DI = valor_recibido;	
    else
        log_error(logger_cpu, "No se reconoce el registro %s", registro);
}

void ejecutar_sum(char* reg_dest, char* reg_origen){
    uint32_t valor_registro = buscar_valor_registro(reg_origen);

    if (strcmp(reg_dest, "AX") == 0)
        registros_cpu->AX += valor_registro;
    else if(strcmp(reg_dest, "BX") == 0)
        registros_cpu->BX += valor_registro;
    else if(strcmp(reg_dest, "CX") == 0)
        registros_cpu->CX += valor_registro;
    else if(strcmp(reg_dest, "DX") == 0)
        registros_cpu->DX += valor_registro;
    else if(strcmp(reg_dest, "EAX") == 0)
        registros_cpu->EAX+= valor_registro;
	else if(strcmp(reg_dest, "EBX") == 0)
        registros_cpu->EBX += valor_registro;
	else if(strcmp(reg_dest, "ECX") == 0)
        registros_cpu->ECX += valor_registro;
	else if(strcmp(reg_dest, "EDX") == 0)
        registros_cpu->EDX += valor_registro;
    else if(strcmp(reg_dest, "SI") == 0)
        registros_cpu->SI = valor_registro;		
	else if(strcmp(reg_dest, "DI") == 0)
        registros_cpu->DI = valor_registro;	
    else
        log_warning(logger_cpu, "Registro no reconocido");
}

void ejecutar_sub(char* reg_dest, char* reg_origen){
    uint32_t valor_registro = buscar_valor_registro(reg_origen);

    if (strcmp(reg_dest, "AX") == 0)
        registros_cpu->AX -= valor_registro;
    else if(strcmp(reg_dest, "BX") == 0)
        registros_cpu->BX -= valor_registro;
    else if(strcmp(reg_dest, "CX") == 0)
        registros_cpu->CX -= valor_registro;
    else if(strcmp(reg_dest, "DX") == 0)
        registros_cpu->DX -= valor_registro;
	else if(strcmp(reg_dest, "EAX") == 0)
        registros_cpu->EAX -= valor_registro;
	else if(strcmp(reg_dest, "EBX") == 0)
        registros_cpu->EBX -= valor_registro;
	else if(strcmp(reg_dest, "ECX") == 0)
        registros_cpu->ECX -= valor_registro;
	else if(strcmp(reg_dest, "EDX") == 0)
        registros_cpu->EDX -= valor_registro;
    else if(strcmp(reg_dest, "SI") == 0)
        registros_cpu->SI = valor_registro;		
	else if(strcmp(reg_dest, "DI") == 0)
        registros_cpu->DI = valor_registro;	
    else
        log_warning(logger_cpu, "Registro no reconocido");
}

void ejecutar_jnz(void* registro, uint32_t nro_instruccion){

    if(strcmp(registro, "AX") == 0){
        if(registros_cpu->AX != 0)
           registros_cpu->PC = nro_instruccion;
    }
    else if(strcmp(registro, "BX") == 0){
        if(registros_cpu->BX != 0)
           registros_cpu->PC = nro_instruccion;
    }
    else if(strcmp(registro, "CX") == 0){
        if(registros_cpu->CX != 0)
           registros_cpu->PC = nro_instruccion;
    }
    else if(strcmp(registro, "DX") == 0){
        if(registros_cpu->DX != 0)
           registros_cpu->PC = nro_instruccion;
    }
	else if(strcmp(registro, "EAX") == 0){
        if(registros_cpu->EAX != 0)
           registros_cpu->PC = nro_instruccion;
    }
	else if(strcmp(registro, "EBX") == 0){
        if(registros_cpu->EBX != 0)
           registros_cpu->PC = nro_instruccion;
    }
	else if(strcmp(registro, "ECX") == 0){
        if(registros_cpu->ECX != 0)
           registros_cpu->PC = nro_instruccion;
    }
	else if(strcmp(registro, "EDX") == 0){
        if(registros_cpu->EDX != 0)
           registros_cpu->PC = nro_instruccion;
    }
    else if(strcmp(registro, "DI") == 0){
        if(registros_cpu->DI != 0)
           registros_cpu->PC = nro_instruccion;
    }
    else if(strcmp(registro, "SI") == 0){
        if(registros_cpu->SI != 0)
           registros_cpu->PC = nro_instruccion;
    }
    else
        log_warning(logger_cpu, "Registro no reconocido");
}

// -------------------------------------------------
//          INSTRUCCIONES CON MEMORIA
// -------------------------------------------------

// -------- RESIZE ---------------------------------
void ejecutar_resize(int tamanio){
    enviar_codigo(socket_memoria, RESIZE_SOLICITUD);
    t_buffer* buffer = crear_buffer();
    buffer_write_uint32(buffer, cde_ejecutando->pid);
    buffer_write_uint32(buffer, tamanio);
    enviar_buffer(buffer, socket_memoria);
    destruir_buffer(buffer);

    mensajeCpuMem cod = recibir_codigo(socket_memoria);
    if (cod == ERROR_OUT_OF_MEMORY) {
        realizar_desalojo = 1;
        cde_ejecutando->motivo_desalojo = OUT_OF_MEMORY_ERROR;
        return;
    }
}

// -------- MOV_IN ---------------------------------
void ejecutar_mov_in(char* reg_datos, char* reg_direccion){
    if(es_reg_de_cuatro_bytes(reg_datos)){
        ejecutar_mov_in_cuatro_bytes(reg_datos, reg_direccion);
    } else {
        ejecutar_mov_in_un_byte(reg_datos, reg_direccion);
    }
}

void leer_de_dir_fisica_los_bytes(uint32_t dir_fisica, uint32_t bytes, uint32_t* valor_leido){
    *valor_leido = 0; // limpio ante la duda la variable
    enviar_codigo(socket_memoria, MOV_IN_SOLICITUD);
    t_buffer* buffer = crear_buffer();
    buffer_write_uint32(buffer, cde_ejecutando->pid);
    buffer_write_uint32(buffer, dir_fisica);
    buffer_write_uint32(buffer, bytes); 
    enviar_buffer(buffer, socket_memoria);

    buffer = recibir_buffer(socket_memoria);
    *valor_leido = buffer_read_uint32(buffer);
    destruir_buffer(buffer);

    log_info(logger_cpu, "Lectura/Escritura Memoria: “PID: %d - Acción: LEER - Dirección Física: %d - Valor: %d.", cde_ejecutando->pid, dir_fisica, *valor_leido);
}

void ejecutar_mov_in_un_byte(char* reg_datos, char* reg_direccion){
    uint32_t dir_logica = buscar_valor_registro(reg_direccion);

    // en este caso es imposible que se traiga mas de una pagina

    uint32_t dir_fisica = UINT32_MAX;
    bool pagina_en_tlb = se_encuentra_en_tlb(dir_logica, &dir_fisica); 
    if (!pagina_en_tlb)
        dir_fisica = calcular_direccion_fisica(dir_logica, cde_ejecutando);

    uint32_t valor_leido = 0;
    leer_de_dir_fisica_los_bytes(dir_fisica, 1, &valor_leido);

    ejecutar_set(reg_datos, valor_leido);
}

void ejecutar_mov_in_cuatro_bytes(char* reg_datos, char* reg_direccion){
    uint32_t dir_logica = buscar_valor_registro(reg_direccion);

    int cant_paginas_a_traer = (obtener_desplazamiento_pagina(dir_logica) + 4 > tamanio_pagina)? 2 : 1;

    if (cant_paginas_a_traer == 1) {
        uint32_t dir_fisica = UINT32_MAX;
        bool pagina_en_tlb = se_encuentra_en_tlb(dir_logica, &dir_fisica); 
        if (!pagina_en_tlb)
            dir_fisica = calcular_direccion_fisica(dir_logica, cde_ejecutando);

        uint32_t valor_leido = 0;
        leer_de_dir_fisica_los_bytes(dir_fisica, 4, &valor_leido);

        ejecutar_set(reg_datos, valor_leido);
    } else {
        // caso 2 paginas leidas
        uint32_t valor_leido = leer_y_guardar_de_dos_paginas(dir_logica);
        ejecutar_set(reg_datos, valor_leido);
    }
}

uint32_t leer_y_guardar_de_dos_paginas(uint32_t dir_logica){
    uint32_t dir_fisica_primera = UINT32_MAX;

    bool pagina_en_tlb = se_encuentra_en_tlb(dir_logica, &dir_fisica_primera); 

    if (!pagina_en_tlb)
        dir_fisica_primera = calcular_direccion_fisica(dir_logica, cde_ejecutando);

    int cant_bytes_a_leer_primera_pag = 4 - (obtener_desplazamiento_pagina(dir_logica) + 4 - tamanio_pagina);
//                                           |---------------- saco por cuanto se paso ---------------------|
//                                     |----- la diferencia con el tamaño del registro me da cuanto tengo que leer

    // PRIMER PAGINA

    uint32_t primera_lectura = 0;
    leer_de_dir_fisica_los_bytes(dir_fisica_primera, cant_bytes_a_leer_primera_pag, &primera_lectura);

    // SEGUNDA PAGINA

    uint32_t dir_logica_segunda = dir_logica + cant_bytes_a_leer_primera_pag; // asi completo los bytes de la primera y me situo en la segunda desde el comienzo
    uint32_t dir_fisica_segunda = UINT32_MAX;

    pagina_en_tlb = se_encuentra_en_tlb(dir_logica_segunda, &dir_fisica_segunda);

    if (!pagina_en_tlb)
        dir_fisica_segunda = calcular_direccion_fisica(dir_logica_segunda, cde_ejecutando);

    int cant_bytes_a_leer_segunda_pag = 4 - cant_bytes_a_leer_primera_pag;

    uint32_t segunda_lectura = 0;
    leer_de_dir_fisica_los_bytes(dir_fisica_segunda, cant_bytes_a_leer_segunda_pag, &segunda_lectura);

    uint32_t valor_reconstruido = dato_reconstruido(primera_lectura, segunda_lectura, cant_bytes_a_leer_primera_pag, cant_bytes_a_leer_segunda_pag);

    return valor_reconstruido;
}

uint32_t dato_reconstruido(uint32_t primera, uint32_t segunda, int bytes_primera, int bytes_segunda){
    uint32_t dato_reconstruido = 0;

    void* primera_ptr = &primera;
    void* segunda_ptr = &segunda;
    void* dato_reconstruido_ptr = &dato_reconstruido;

    memcpy(dato_reconstruido_ptr, primera_ptr, (size_t)bytes_primera);
    memcpy(dato_reconstruido_ptr + (size_t)bytes_primera, segunda_ptr, (size_t)bytes_segunda);

    return dato_reconstruido;
}

bool es_reg_de_cuatro_bytes(char* reg_datos){
    if (strcmp(reg_datos, "AX") == 0)
        return false;
    else if (strcmp(reg_datos, "BX") == 0)
        return false;
    else if (strcmp(reg_datos, "CX") == 0)
        return false;
    else if (strcmp(reg_datos, "DX") == 0)
        return false;
    else   
        return true;
}

// -------- MOV_OUT ---------------------------------
void ejecutar_mov_out(char* reg_datos, char* reg_direccion){
    uint32_t dir_logica = buscar_valor_registro(reg_direccion);
    uint32_t valor_a_escribir = buscar_valor_registro(reg_datos);
    bool pagina_dividida = (obtener_desplazamiento_pagina(dir_logica) + 4 > tamanio_pagina);

    if (es_reg_de_cuatro_bytes(reg_datos)){
        // puede pasar que se quiera escribir un valor de 4 bytes y a la pagina en donde se esta parado le queden solo 2 bytes disponibles por ejemplo
        // en ese caso se escriben los 2 bytes restantes en la siguiente al comienzo de la misma
        if (pagina_dividida) {
            escribir_y_guardar_en_dos_paginas(dir_logica, &valor_a_escribir);
        } else {
            uint32_t dir_fisica = UINT32_MAX;
            bool pagina_en_tlb = se_encuentra_en_tlb(dir_logica, &dir_fisica); 
            if (!pagina_en_tlb)
                dir_fisica = calcular_direccion_fisica(dir_logica, cde_ejecutando);

            escribir_en_dir_fisica_los_bytes(dir_fisica, 4, valor_a_escribir);
        }
    } else {
        uint32_t dir_fisica = UINT32_MAX;
        bool pagina_en_tlb = se_encuentra_en_tlb(dir_logica, &dir_fisica); 
        if (!pagina_en_tlb)
            dir_fisica = calcular_direccion_fisica(dir_logica, cde_ejecutando);

        escribir_en_dir_fisica_los_bytes(dir_fisica, 1, valor_a_escribir);
    }
}

void escribir_en_dir_fisica_los_bytes(uint32_t dir_fisica, uint32_t bytes, uint32_t valor_a_escribir){
    enviar_codigo(socket_memoria, MOV_OUT_SOLICITUD);
    t_buffer* buffer = crear_buffer();
    buffer_write_uint32(buffer, cde_ejecutando->pid);
    buffer_write_uint32(buffer, dir_fisica);
    buffer_write_uint32(buffer, valor_a_escribir);
    buffer_write_uint32(buffer, bytes); 
    enviar_buffer(buffer, socket_memoria);

    log_info(logger_cpu, "Lectura/Escritura Memoria: “PID: %d - Acción: ESCRIBIR - Dirección Física: %d - Valor: %d.", cde_ejecutando->pid, dir_fisica, valor_a_escribir);
}

void escribir_y_guardar_en_dos_paginas(uint32_t dir_logica_destino, uint32_t* valor){
    void* valor_ptr = valor;
    void* valor_parte_1_ptr = malloc(2);
    void* valor_parte_2_ptr = malloc(2);

    int bytes_a_escribir_primera_pag = 4 - (obtener_desplazamiento_pagina(dir_logica_destino) + 4 - tamanio_pagina);
    int bytes_a_escribir_segunda_pag = 4 - bytes_a_escribir_primera_pag;
    
    memcpy(valor_parte_1_ptr, valor_ptr, bytes_a_escribir_primera_pag);
    memcpy(valor_parte_2_ptr, valor_ptr + bytes_a_escribir_primera_pag, bytes_a_escribir_segunda_pag);

    uint32_t valor_parte_1 = *(uint32_t*)valor_parte_1_ptr;
    uint32_t valor_parte_2 = *(uint32_t*)valor_parte_2_ptr;

    uint32_t dir_fisica_destino = UINT32_MAX;    
    bool pagina_en_tlb = se_encuentra_en_tlb(dir_logica_destino, &dir_fisica_destino); 
    if (!pagina_en_tlb)
        dir_fisica_destino = calcular_direccion_fisica(dir_logica_destino, cde_ejecutando);

    escribir_en_dir_fisica_los_bytes(dir_fisica_destino, bytes_a_escribir_primera_pag, valor_parte_1);

    // actualizo la dir, me muevo a la siguiente
    uint32_t dir_logica_segunda_pag = dir_logica_destino + bytes_a_escribir_primera_pag;

    uint32_t dir_fisica_destino_segunda_pag = UINT32_MAX;    
    pagina_en_tlb = se_encuentra_en_tlb(dir_logica_segunda_pag, &dir_fisica_destino_segunda_pag); 
    if (!pagina_en_tlb)
        dir_fisica_destino_segunda_pag = calcular_direccion_fisica(dir_logica_segunda_pag, cde_ejecutando);

    escribir_en_dir_fisica_los_bytes(dir_fisica_destino_segunda_pag, bytes_a_escribir_segunda_pag, valor_parte_2);

    free(valor_parte_1_ptr);
    free(valor_parte_2_ptr);
}

// -------- COPY_STRING ------------------------------
void ejecutar_copy_string(int tamanio){

    uint32_t dir_logica_string = buscar_valor_registro("SI");
    uint32_t dir_logica_destino = buscar_valor_registro("DI");


    // voy a ir cargando de a 4 bytes siempre y cuando se pueda, por eso se chequea que, ademas de quedar bytes
    // por usar, queden como minimo 4
    // cuando ya no queden como minimo 4 bytes voy a empezar a leer y escribir de a 1 byte hasta que termine
    int bytes_usados = 0;
    uint32_t valor_leido = 0;
    bool valor_dividido = false;
    bool pagina_dividida = false;
    bool pagina_en_tlb = false;

    while (bytes_usados < tamanio && bytes_usados + 4 < tamanio){

        // parte de LECTURA

        valor_dividido = (obtener_desplazamiento_pagina(dir_logica_string) + 4 > tamanio_pagina);
        if (valor_dividido) {
            valor_leido = leer_y_guardar_de_dos_paginas(dir_logica_string);
        } else {
            uint32_t dir_fisica_string = UINT32_MAX;    
            pagina_en_tlb = se_encuentra_en_tlb(dir_logica_string, &dir_fisica_string); 
            if (!pagina_en_tlb)
                dir_fisica_string = calcular_direccion_fisica(dir_logica_string, cde_ejecutando);            
            leer_de_dir_fisica_los_bytes(dir_fisica_string, 4, &valor_leido);
        }

        // parte de ESCRITURA

        pagina_dividida = (obtener_desplazamiento_pagina(dir_logica_destino) + 4 > tamanio_pagina);
        if (pagina_dividida) {
            escribir_y_guardar_en_dos_paginas(dir_logica_destino, &valor_leido);
        } else {
            uint32_t dir_fisica_destino = UINT32_MAX;    
            pagina_en_tlb = se_encuentra_en_tlb(dir_logica_destino, &dir_fisica_destino); 
            if (!pagina_en_tlb)
                dir_fisica_destino = calcular_direccion_fisica(dir_logica_destino, cde_ejecutando);
            escribir_en_dir_fisica_los_bytes(dir_fisica_destino, 4, valor_leido);
        }
        
        // consumimos 4 bytes de cada direccion
        // desplazamos las direcciones (chequear que el concepto este bien)
        dir_logica_destino += 4;
        dir_logica_string += 4;
        
        bytes_usados += 4;
    }

    // aca entra cuando ya no puedo tomar de a 4 bytes o porque nunca fue igual o mayor a 4 el tamanio a escribir
    while (bytes_usados < tamanio){

        // LECTURA
        uint32_t dir_fisica_string = UINT32_MAX;    
        pagina_en_tlb = se_encuentra_en_tlb(dir_logica_string, &dir_fisica_string); 
        if (!pagina_en_tlb)
            dir_fisica_string = calcular_direccion_fisica(dir_logica_string, cde_ejecutando);  
        
        leer_de_dir_fisica_los_bytes(dir_fisica_string, 1, &valor_leido);

        // ESCRITURA
        uint32_t dir_fisica_destino = UINT32_MAX;    
        pagina_en_tlb = se_encuentra_en_tlb(dir_logica_destino, &dir_fisica_destino); 
        if (!pagina_en_tlb)
            dir_fisica_destino = calcular_direccion_fisica(dir_logica_destino, cde_ejecutando);
        escribir_en_dir_fisica_los_bytes(dir_fisica_destino, 1, valor_leido);

        dir_logica_destino ++;
        dir_logica_string ++;
        
        bytes_usados ++;
    }
}

// -------------------------------------------------
//                     MMU
// -------------------------------------------------

int obtener_numero_pagina(int direccion_logica){
	return floor(direccion_logica / tamanio_pagina);
}

int obtener_desplazamiento_pagina(int direccion_logica){
	int numero_pagina = obtener_numero_pagina(direccion_logica);
	return direccion_logica - numero_pagina * tamanio_pagina;
}

uint32_t calcular_direccion_fisica(int direccion_logica, t_cde* cde){
	int nro_pagina = obtener_numero_pagina(direccion_logica);
	int desplazamiento = obtener_desplazamiento_pagina(direccion_logica);

	enviar_codigo(socket_memoria, NUMERO_MARCO_SOLICITUD);
	t_buffer* buffer = crear_buffer();
	buffer_write_uint32(buffer, nro_pagina);
	buffer_write_uint32(buffer, cde->pid);
	enviar_buffer(buffer, socket_memoria);
	destruir_buffer(buffer);

	buffer = recibir_buffer(socket_memoria);
    uint32_t nro_marco_recibido = buffer_read_uint32(buffer);
    destruir_buffer(buffer);
    log_info(logger_cpu, "PID: %d - OBTENER MARCO - Página: %d - Marco: %d", cde->pid, nro_pagina, nro_marco_recibido);

    colocar_pagina_en_tlb(cde->pid, nro_pagina, nro_marco_recibido);

    return nro_marco_recibido * tamanio_pagina + desplazamiento; // retorna la direccion_fisica
}

// -------------------------------------------------
//                     TLB
// -------------------------------------------------

void colocar_pagina_en_tlb(uint32_t pid, uint32_t nro_pagina, uint32_t marco){
    t_pagina_tlb* nueva_pagina = malloc(sizeof(t_pagina_tlb));
    if (!nueva_pagina){
        log_error(logger_cpu, "Fallo la creacion de la pagina %d del proceso %d", nro_pagina, pid);
        return;
    }
    nueva_pagina->marco = marco;
    nueva_pagina->nroPagina = nro_pagina;
    nueva_pagina->pid = pid;
    nueva_pagina->tiempo_ultimo_acceso = temporal_get_string_time("%H:%M:%S:%MS");

    if (queue_size(tlb) < cantidad_entradas_tlb) {
        queue_push(tlb, nueva_pagina);
    } else 
        desalojar_y_agregar(nueva_pagina);
}

void desalojar_y_agregar(t_pagina_tlb* nueva_pagina){
    t_pagina_tlb* pag_a_remover = NULL;

    if (strcmp(algoritmo_tlb, "FIFO")){
        pag_a_remover = queue_peek(tlb);
        free(pag_a_remover);
        queue_pop(tlb);
        queue_push(tlb, nueva_pagina);
    } else {  // LRU
        t_list* lista_tlb = tlb->elements;

        pag_a_remover = list_get_maximum(lista_tlb, mayor_tiempo_de_ultimo_acceso);
        list_remove_element(lista_tlb, pag_a_remover);
        free(pag_a_remover);
        
        list_add(lista_tlb, nueva_pagina);
    }
}

void* mayor_tiempo_de_ultimo_acceso(void* A, void* B){
    t_pagina_tlb* paginaA = A;
    t_pagina_tlb* paginaB = B;

    if (obtenerTiempoEnMiliSegundos(paginaA->tiempo_ultimo_acceso) >= obtenerTiempoEnMiliSegundos(paginaB->tiempo_ultimo_acceso))
        return paginaA;
    else 
        return paginaB;
}

int obtenerTiempoEnMiliSegundos(char* tiempo){
    char** horaNuevaDesarmada = string_split(tiempo,":");

    int horas = atoi(horaNuevaDesarmada[0]);
    int minutos = atoi(horaNuevaDesarmada[1]) + horas * 60;
    int segundos = atoi(horaNuevaDesarmada[2]) + minutos * 60;
    int miliSegundos = atoi(horaNuevaDesarmada[3]) + segundos * 1000;

    string_array_destroy(horaNuevaDesarmada);

    return miliSegundos;
}

bool se_encuentra_en_tlb(uint32_t dir_logica, uint32_t* dir_fisica){

    int nro_pag_buscada = obtener_numero_pagina(dir_logica);
    int desplazamiento = obtener_desplazamiento_pagina(dir_logica);
    int i = 0;
    bool encontrado = false;

    while (i < list_size(tlb->elements) && !encontrado) {
        t_pagina_tlb* pagina = list_get(tlb->elements, i);
        if (pagina->nroPagina == nro_pag_buscada && pagina->pid == cde_ejecutando->pid){
            log_info(logger_cpu, "PID: %d - TLB HIT - Pagina: %d", cde_ejecutando->pid, nro_pag_buscada);
            log_info(logger_cpu, "PID: %d - OBTENER MARCO - Página: %d - Marco: %d", cde_ejecutando->pid, nro_pag_buscada, pagina->marco);
            *dir_fisica = pagina->marco * tamanio_pagina + desplazamiento;
            encontrado = true;
        }
        i++;
    }

    if (!encontrado)
        log_info(logger_cpu, "PID: %d - TLB MISS - Pagina: %d", cde_ejecutando->pid, nro_pag_buscada);

    return encontrado;
}


