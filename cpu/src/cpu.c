#include "cpu.h"

int main(int argc, char* argv[]) {

	if(argc != 2) {
		printf("ERROR: Tenés que pasar el path del archivo config de CPU\n");
		return -1;
	}

	config_path = argv[1];

	inicializar_modulo();
	conectar();
	//esperar_desconexiones();

    sem_wait(&terminar_cpu);

    terminar_programa();

    return 0;
}
/*
void esperar_desconexiones(){
	sem_wait(&sema_kernel_dispatch);
	sem_wait(&sema_kernel_interrupt);
}
*/
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
	sem_init(&terminar_cpu, 0, 0);
	levantar_logger();
	levantar_config();
	inicializar_registros();
	inicializarSemaforos();
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

void inicializarSemaforos(){
	pthread_mutex_init(&mutex_cde_ejecutando, NULL);
	pthread_mutex_init(&mutex_interrupcion_consola, NULL);
	pthread_mutex_init(&mutex_realizar_desalojo, NULL);
	pthread_mutex_init(&mutex_instruccion_actualizada, NULL);
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

        // en caso de que se desconecte kernel NO se sigue
        // nos sirve mas que nada para tener una salida segura
        if (op_code == UINT8_MAX || !buffer) {
            sem_post(&terminar_cpu);
            return;
        }

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
        if (cde->pid == pid_recibido) {
            switch (op_code){
                case INTERRUPT:
                    pthread_mutex_lock(&mutex_interrupcion);
                    interrupcion = 1;
                    pthread_mutex_unlock(&mutex_interrupcion);
                    break;
                case DESALOJO:
                    // caso FIFO: no existe desalojo, kernel NUNCA deberia mandar este mensaje si esta seteado fifo como algoritmo
                    // caso RR: desaloja solo por quantum, si llega este mensaje es porque SI o SI hay que sacar por quantum
                    // caso VRR: desaloja solo por quantum, si llega este mensaje es porque SI o SI hay que sacar por quantum
                    
                    // este es el unico chequeo que quizas es necesario considerar (a confirmar) ya que si la instruccion es bloqueante
                    // se desaloja solo el cde y - en la practica se considera que si se termina el quantum pero JUSTO llego a la rafaga I/O
                    // se deja completar la misma y quien lo desaloja es I/O -
                    if(es_bloqueante(instruccion_actualizada)) break;
                    
                    pthread_mutex_lock(&mutex_realizar_desalojo);
                    realizar_desalojo = 1;
                    pthread_mutex_unlock(&mutex_realizar_desalojo);
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
}

void terminar_programa(){
	terminar_conexiones(1, socket_memoria);
    if(logger_cpu) log_destroy(logger_cpu);
    if(config_cpu) config_destroy(config_cpu);
}

void iterator(char* value) {
	log_info(logger_cpu,"%s", value);
}



// FUNCIONES AUXILIARES DE INSTRUCCIONES

uint8_t buscar_valor_registro8(void* registro){
	uint8_t valorLeido;

	if(strcmp(registro, "AX") == 0)
		valorLeido = registros_cpu->AX;
	else if(strcmp(registro, "BX") == 0)
		valorLeido = registros_cpu->BX;
	else if(strcmp(registro, "CX") == 0)
		valorLeido = registros_cpu->CX;
	else if(strcmp(registro, "DX") == 0)
		valorLeido = registros_cpu->DX;

	return valorLeido;
}

uint32_t buscar_valor_registro32(void* registro){
	uint32_t valorLeido;
	if(strcmp(registro, "EAX") == 0)
		valorLeido = registros_cpu->EAX;
	else if(strcmp(registro, "EBX") == 0)
		valorLeido = registros_cpu->EBX;
	else if(strcmp(registro, "ECX") == 0)
		valorLeido = registros_cpu->ECX;
	else if(strcmp(registro, "EDX") == 0)
		valorLeido = registros_cpu->EDX;
	else if(strcmp(registro, "SI") == 0)
		valorLeido = registros_cpu->SI;
	else if(strcmp(registro, "DI") == 0)
		valorLeido = registros_cpu->DI;

	return valorLeido;
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
	registros_cpu->SI = cde->registros->SI;
	registros_cpu->DI = cde->registros->DI;
}

void guardar_cde(t_cde* cde){
    cde->registros->AX = registros_cpu->AX;
    cde->registros->BX = registros_cpu->BX;
    cde->registros->CX = registros_cpu->CX;
    cde->registros->DX = registros_cpu->DX;
	cde->registros->EAX = registros_cpu->EAX;
	cde->registros->EBX = registros_cpu->EBX;
	cde->registros->ECX = registros_cpu->ECX;
	cde->registros->EDX = registros_cpu->EDX;
	cde->registros->SI = registros_cpu->SI;
	cde->registros->DI = registros_cpu->DI;
}

bool es_bloqueante(codigoInstruccion instruccion_actualizada){
    switch(instruccion_actualizada){
    case WAIT:
        return true;
        break;
    case SIGNAL:
        return true;
        break;
   	case IO_GEN_SLEEP :
        return false;
        break;
    case IO_STDIN_READ:
        return true;
        break;
	case IO_STDOUT_WRITE  :
        return false;
        break;
	case IO_FS_CREATE  :
        return false;
        break;
	case IO_FS_DELETE  :
        return false;
        break;
	case IO_FS_TRUNCATE  :
        return false;
        break;
	case IO_FS_WRITE   :
        return false;
        break;
	case IO_FS_READ    :
        return false;
        break;	
	case EXIT:
        return true;
        break;
    default:
        return false;
        break;
    }
}

void ejecutar_proceso(t_cde* cde){
	cargar_registros(cde);	
	t_instruccion* instruccion_a_ejecutar;

    while(interrupcion != 1 && realizar_desalojo != 1){
        log_info(logger_cpu, "PID: %d - FETCH - Program Counter: %d", cde->pid, cde->pc);

        enviar_codigo(socket_memoria, PEDIDO_INSTRUCCION); // fetch
        t_buffer* buffer_envio = crear_buffer();
        buffer_write_uint32(buffer_envio, cde->pid);
        buffer_write_uint32(buffer_envio, cde->pc);

        enviar_buffer(buffer_envio, socket_memoria);

        destruir_buffer(buffer_envio);
        
        // mientras estemos en un mismo proceso las instrucciones se encuentran ubicadas en memoria SECUENCIALMENTE
        // por lo que se puede ir moviendo el program counter en 1 para tener la direccion de la siguiente instruccion
        // PERO cuando se termine el proceso YA NO se puede asegurar esto, por lo que si o si se pide la direccion nueva a memoria
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
		interrupcion = 0;
        pthread_mutex_lock(&mutex_realizar_desalojo);
        realizar_desalojo = 0;
        pthread_mutex_unlock(&mutex_realizar_desalojo);
        log_info(logger_cpu, "PID: %d - Volviendo a kernel por instruccion %s", cde->pid, obtener_nombre_instruccion(instruccion_a_ejecutar));
        cde->motivo = INTERRUPCION;
        desalojar_cde(cde, instruccion_a_ejecutar);
	}else if (realizar_desalojo){ // salida por fin de quantum
		interrupcion = 0;  
        pthread_mutex_lock(&mutex_realizar_desalojo);
        realizar_desalojo = 0;
        pthread_mutex_unlock(&mutex_realizar_desalojo);
        log_info(logger_cpu, "PID: %d - Desalojado por fin de Quantum", cde->pid); 
        cde->motivo = FIN_DE_QUANTUM;
        desalojar_cde(cde, instruccion_a_ejecutar);
    }
}

char* obtener_nombre_instruccion(t_instruccion* instruccion){
    switch(instruccion->codigo){
        case SET:
            return "SET";
            break;
	    case SUM:
            return "SUM";
            break;
	    case SUB:
            return "SUB";
            break;
	    case JNZ:
            return "JNZ";
            break;
	    case RESIZE:
            return "RESIZE";
            break;
	    case WAIT:
            return "WAIT";
            break;
	    case SIGNAL:
            return "SIGNAL";
            break;
	    case MOV_IN:
            return "MOV_IN";
            break;
	    case MOV_OUT:
            return "MOV_OUT";
            break;
	    case COPY_STRING:
            return "COPY_STRING";
            break;
	    case IO_GEN_SLEEP:
            return "IO_GEN_SLEEP";
            break;
	    case IO_STDIN_READ:
            return "IO_STDIN_READ";
            break;
	    case IO_STDOUT_WRITE:
            return "IO_STDOUT_WRITE";
            break;
	    case IO_FS_CREATE:
            return "IO_FS_CREATE";
            break;
	    case IO_FS_DELETE:
            return "IO_FS_DELETE";
            break;
        case IO_FS_TRUNCATE:
            return "IO_FS_TRUNCATE";
            break;
        case IO_FS_WRITE:
            return "IO_FS_WRITE";
            break;
	    case IO_FS_READ:
            return "IO_FS_READ";
            break;
	    case EXIT:
            return "EXIT";
            break;
        default:
            return "Instruccion desconocida";
            break;
    }
}

void ejecutar_instruccion(t_cde* cde, t_instruccion* instruccion_a_ejecutar){
    switch(instruccion_a_ejecutar->codigo){
        case SET:
            log_info(logger_cpu, "PID: %d - Ejecutando: %s - %s %s", cde->pid, obtener_nombre_instruccion(instruccion_a_ejecutar), instruccion_a_ejecutar->parametro1, instruccion_a_ejecutar->parametro2);
            break;
        case SUM:
            log_info(logger_cpu, "PID: %d - Ejecutando: %s - %s %s", cde->pid, obtener_nombre_instruccion(instruccion_a_ejecutar), instruccion_a_ejecutar->parametro1, instruccion_a_ejecutar->parametro2);
            break;
        case SUB:
            log_info(logger_cpu, "PID: %d - Ejecutando: %s - %s %s", cde->pid, obtener_nombre_instruccion(instruccion_a_ejecutar), instruccion_a_ejecutar->parametro1, instruccion_a_ejecutar->parametro2);
            break;
        case MOV_IN:
            log_info(logger_cpu, "PID: %d - Ejecutando: %s - %s %s", cde->pid, obtener_nombre_instruccion(instruccion_a_ejecutar), instruccion_a_ejecutar->parametro1, instruccion_a_ejecutar->parametro2);
            break;
        case MOV_OUT:
            log_info(logger_cpu, "PID: %d - Ejecutando: %s - %s %s", cde->pid, obtener_nombre_instruccion(instruccion_a_ejecutar), instruccion_a_ejecutar->parametro1, instruccion_a_ejecutar->parametro2);
            break;
        case RESIZE:
            log_info(logger_cpu, "PID: %d - Ejecutando: %s - %s", cde->pid, obtener_nombre_instruccion(instruccion_a_ejecutar), instruccion_a_ejecutar->parametro1);
            break;
        case JNZ:
            log_info(logger_cpu, "PID: %d - Ejecutando: %s - %s %s", cde->pid, obtener_nombre_instruccion(instruccion_a_ejecutar), instruccion_a_ejecutar->par1, instruccion_a_ejecutar->parametro2);
            break;
        case WAIT:
            log_info(logger_cpu, "PID: %d - Ejecutando: %s - %s", cde->pid, obtener_nombre_instruccion(instruccion_a_ejecutar), instruccion_a_ejecutar->parametro1);
            break;
        case SIGNAL:
            log_info(logger_cpu, "PID: %d - Ejecutando: %s - %s", cde->pid, obtener_nombre_instruccion(instruccion_a_ejecutar), instruccion_a_ejecutar->parametro1);
            break;
        case COPY_STRING:
            log_info(logger_cpu, "PID: %d - Ejecutando: %s - %s", cde->pid, obtener_nombre_instruccion(instruccion_a_ejecutar), instruccion_a_ejecutar->parametro1);
            break;
        case IO_GEN_SLEEP:
            log_info(logger_cpu, "PID: %d - Ejecutando: %s - %s %s", cde->pid, obtener_nombre_instruccion(instruccion_a_ejecutar), instruccion_a_ejecutar->parametro1, instruccion_a_ejecutar->par2);
            break;
        case IO_STDIN_READ:
            log_info(logger_cpu, "PID: %d - Ejecutando: %s - %s %s", cde->pid, obtener_nombre_instruccion(instruccion_a_ejecutar), instruccion_a_ejecutar->parametro1, instruccion_a_ejecutar->parametro2);
            break;
        case IO_STDOUT_WRITE: 
            log_info(logger_cpu, "PID: %d - Ejecutando: %s - %s %s %s", cde->pid, obtener_nombre_instruccion(instruccion_a_ejecutar), instruccion_a_ejecutar->parametro1, instruccion_a_ejecutar->parametro2, instruccion_a_ejecutar->parametro3);
            break;
        case IO_FS_CREATE:
            log_info(logger_cpu, "PID: %d - Ejecutando: %s - %s %s", cde->pid, obtener_nombre_instruccion(instruccion_a_ejecutar), instruccion_a_ejecutar->parametro1, instruccion_a_ejecutar->parametro2);
            break;
        case IO_FS_DELETE:
            log_info(logger_cpu, "PID: %d - Ejecutando: %s - %s %s", cde->pid, obtener_nombre_instruccion(instruccion_a_ejecutar), instruccion_a_ejecutar->parametro1, instruccion_a_ejecutar->parametro2);
            break;
        case IO_FS_TRUNCATE: 
            log_info(logger_cpu, "PID: %d - Ejecutando: %s - %s %s %s", cde->pid, obtener_nombre_instruccion(instruccion_a_ejecutar), instruccion_a_ejecutar->parametro1, instruccion_a_ejecutar->parametro2, instruccion_a_ejecutar->parametro3);
            break;
        case IO_FS_WRITE: 
            log_info(logger_cpu, "PID: %d - Ejecutando: %s - %s %s %s %s %s", cde->pid, obtener_nombre_instruccion(instruccion_a_ejecutar), instruccion_a_ejecutar->parametro1, instruccion_a_ejecutar->parametro2, instruccion_a_ejecutar->parametro3, instruccion_a_ejecutar->parametro4, instruccion_a_ejecutar->parametro5);
            break;
        case IO_FS_READ: 
            log_info(logger_cpu, "PID: %d - Ejecutando: %s - %s %s %s %s %s", cde->pid, obtener_nombre_instruccion(instruccion_a_ejecutar), instruccion_a_ejecutar->parametro1, instruccion_a_ejecutar->parametro2, instruccion_a_ejecutar->parametro3, instruccion_a_ejecutar->parametro4, instruccion_a_ejecutar->parametro5);
            break;
        case EXIT:
            log_info(logger_cpu, "PID: %d - Ejecutando: %s", cde->pid, obtener_nombre_instruccion(instruccion_a_ejecutar));
        default:
            log_warning(logger_cpu, "Instruccion no reconocida");
            break;
    }
}

void desalojar_cde(t_cde* cde, t_instruccion* instruccion_a_ejecutar){
    // cde no puede devolver en su estructura ningun motivo de desalojamiento
    guardar_cde(cde); //cargar registros de cpu en el cde
    devolver_cde_a_kernel(cde, instruccion_a_ejecutar);
    destruir_cde(cde);
    
    // seteamos un valor imposible para que el sistema sepa que no hay proceso en ejecución
    pthread_mutex_lock(&mutex_cde_ejecutando);
    pid_de_cde_ejecutando = UINT32_MAX;  
    pthread_mutex_unlock(&mutex_cde_ejecutando);

    // seteamos un valor nulo para que el sistema sepa que no hay instruccion
    pthread_mutex_lock(&mutex_instruccion_actualizada);
    instruccion_actualizada = NULO;
    pthread_mutex_unlock(&mutex_instruccion_actualizada);

    destruir_instruccion(instruccion_a_ejecutar);
}

void devolver_cde_a_kernel(t_cde* cde, t_instruccion* instruccion_a_ejecutar){

    t_buffer* buffer = crear_buffer();
    buffer_write_cde(buffer, cde);
    buffer_write_instruccion(buffer, instruccion_a_ejecutar);

    enviar_buffer(buffer, socket_kernel_dispatch);
    destruir_buffer(buffer);
}

// FUNCIONES INSTRUCCIONES

// AX,BX,CX,DX es uint8, pero el resto es uint32. Cuando se la llama, hay que pasarle
// el registro y cargar el parametro que corresponda

void ejecutar_set(char* registro, uint32_t valor_recibido){

    if(strcmp(registro, "AX") == 0) registros_cpu->AX = (uint8_t)valor_recibido;   
    else if(strcmp(registro, "BX") == 0)
        registros_cpu->BX = (uint8_t) valor_recibido;
    else if(strcmp(registro, "CX") == 0)
        registros_cpu->CX = (uint8_t) valor_recibido;
    else if(strcmp(registro, "DX") == 0)
        registros_cpu->DX = (uint8_t) valor_recibido;
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
    uint32_t valor_reg_origen = buscar_valor_registro(reg_origen);
    
    if(strcmp(reg_dest, "AX") == 0)
        registros_cpu->AX += (uint8_t) valor_reg_origen;
    else if(strcmp(reg_dest, "BX") == 0)
        registros_cpu->BX += (uint8_t) valor_reg_origen;
    else if(strcmp(reg_dest, "CX") == 0)
        registros_cpu->CX += (uint8_t) valor_reg_origen;
    else if(strcmp(reg_dest, "DX") == 0)
        registros_cpu->DX += (uint8_t) valor_reg_origen;
    else if(strcmp(reg_dest, "EAX") == 0)
        registros_cpu->EAX+= valor_reg_origen;
	else if(strcmp(reg_dest, "EBX") == 0)
        registros_cpu->EBX += valor_reg_origen;
	else if(strcmp(reg_dest, "ECX") == 0)
        registros_cpu->ECX += valor_reg_origen;
	else if(strcmp(reg_dest, "EDX") == 0)
        registros_cpu->EDX += valor_reg_origen;
    else
        log_warning(logger_cpu, "Registro no reconocido");
}

void ejecutar_sub(char* reg_dest, char* reg_origen){
    uint32_t valor_reg_origen= buscar_valor_registro(reg_origen);

    if(strcmp(reg_dest, "AX") == 0)
        registros_cpu->AX -= (uint8_t) valor_reg_origen;
    else if(strcmp(reg_dest, "BX") == 0)
        registros_cpu->BX -= (uint8_t) valor_reg_origen;
    else if(strcmp(reg_dest, "CX") == 0)
        registros_cpu->CX -= (uint8_t) valor_reg_origen;
    else if(strcmp(reg_dest, "DX") == 0)
        registros_cpu->DX -= (uint8_t) valor_reg_origen;

	else if(strcmp(reg_dest, "EAX") == 0)
        registros_cpu->EAX -= valor_reg_origen;
	else if(strcmp(reg_dest, "EBX") == 0)
        registros_cpu->EBX -= valor_reg_origen;
	else if(strcmp(reg_dest, "ECX") == 0)
        registros_cpu->ECX -= valor_reg_origen;
	else if(strcmp(reg_dest, "EDX") == 0)
        registros_cpu->EDX -= valor_reg_origen;
    else
        log_warning(logger_cpu, "Registro no reconocido");
}

void ejecutar_jnz(void* registro, uint32_t nro_instruccion, t_cde* cde){

    if(strcmp(registro, "AX") == 0){
        if(registros_cpu->AX != 0)
            cde->pc = (uint8_t) nro_instruccion;
    }
    else if(strcmp(registro, "BX") == 0){
        if(registros_cpu->BX != 0)
            cde->pc = (uint8_t) nro_instruccion;
    }
    else if(strcmp(registro, "CX") == 0){
        if(registros_cpu->CX != 0)
            cde->pc = (uint8_t) nro_instruccion;
    }
    else if(strcmp(registro, "DX") == 0){
        if(registros_cpu->DX != 0)
            cde->pc = (uint8_t) nro_instruccion;
    }
	else if(strcmp(registro, "EAX") == 0){
        if(registros_cpu->EAX != 0)
            cde->pc = nro_instruccion;
    }
	else if(strcmp(registro, "EBX") == 0){
        if(registros_cpu->EBX != 0)
            cde->pc = nro_instruccion;
    }
	else if(strcmp(registro, "ECX") == 0){
        if(registros_cpu->ECX != 0)
            cde->pc = nro_instruccion;
    }
	else if(strcmp(registro, "EDX") == 0){
        if(registros_cpu->EDX != 0)
            cde->pc = nro_instruccion;
    }
    else
        log_warning(logger_cpu, "Registro no reconocido");
}


// los parametros a recibir son una interfaz y el tiempo a realizar un sleep en esa interfaz
// NO TERMINADO
void ejecutar_IO_GEN_SLEEP(uint32_t tiempo){ //devolver cde al kernel con la cant de segundos que el proceso se va a bloquear
    interrupcion = 1;
}






