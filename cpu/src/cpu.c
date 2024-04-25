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
	registros_cpu->DX = cde->registros->SI;
	registros_cpu->DX = cde->registros->DI;
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
	cde->registros->DX = registros_cpu->SI;
	cde->registros->DX = registros_cpu->DI;
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
        desalojar_cde(cde, instruccion_a_ejecutar);

	}else if (realizar_desalojo){ // con RR Y PRIORIDADES SALE POR ACA
		interrupcion = 0;  
        pthread_mutex_lock(&mutex_realizar_desalojo);
        realizar_desalojo = 0;
        pthread_mutex_unlock(&mutex_realizar_desalojo);
        if(algoritmo_planificacion == 1) // significa que es PRIORIDADES
            log_info(logger_cpu, "PID: %d - Desalojado por proceso de mayor prioridad", cde->pid);
        else if(algoritmo_planificacion == 2) // significa que es RR
            log_info(logger_cpu, "PID: %d - Desalojado por fin de Quantum", cde->pid); 
        desalojar_cde(cde, instruccion_a_ejecutar);
	}else {

	}
}

void interruptProceso(void* socket_server){
    int socket_servidor_interrupt = (int) (intptr_t) socket_server;
    
    log_info(logger_cpu, "Esperando Kernel INTERRUPT....");
    socket_kernel_interrupt = esperar_cliente(socket_servidor_interrupt);
    log_info(logger_cpu, "Se conecto el Kernel por INTERRUPT");
    
    while(1){
        // Esperar que termine de ejecutar (semaforo)
        // Chequear si le llego una interrupcion del kernel
        // Si llego una, la atiendo
        // Si no, posteo el semaforo de ejecucion

        mensajeKernelCpu op_code = recibir_codigo(socket_kernel_interrupt);
        t_buffer* buffer = recibir_buffer(socket_kernel_interrupt); // recibe pid o lo que necesite
        uint32_t pid_recibido = buffer_read_uint32(buffer);
        destruir_buffer_nuestro(buffer);
        
        switch (op_code){
            case INTERRUPT:
                // atendemos la interrupcion
                pthread_mutex_lock(&mutex_interrupcion_consola);
                interrupcion_consola = 1;
                pthread_mutex_unlock(&mutex_interrupcion_consola);
                break;
            case DESALOJO:
                // se desaloja proceso en ejecucion
                if(algoritmo_planificacion == 2 && pid_de_cde_ejecutando != pid_recibido){ // significa que el algoritmo es RR
                    break;
                }
                else if(algoritmo_planificacion == 2 && pid_de_cde_ejecutando == pid_recibido){
                    if(es_bloqueante(instruccion_actualizada)){
                        break;
                    }
                }
                pthread_mutex_lock(&mutex_realizar_desalojo);
                realizar_desalojo = 1;
                pthread_mutex_unlock(&mutex_realizar_desalojo);
                break;
            default:
                log_warning(logger_cpu, "entre al case de default");
                break;
        }
    }
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






