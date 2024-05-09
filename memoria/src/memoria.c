#include "memoria.h"

int main(int argc, char* argv[]) {

	if(argc != 2) {
		printf("ERROR: Tenés que pasar el path del archivo config de Memoria\n");
		return -1;
	}

	config_path = argv[1];

	inicializar_modulo();
	conectar();
	esperar_desconexiones();

	terminar_programa();

	return 0;
}

void inicializar_modulo(){
	levantar_logger();
	levantar_config();

	sem_init(&sema_cpu, 0, 0);
	sem_init(&sema_kernel, 0, 1);
}

void conectar(){
	puerto_escucha = config_get_string_value(config_memoria, "PUERTO_ESCUCHA");

	socket_servidor = iniciar_servidor(puerto_escucha, logger_memoria);
	if (socket_servidor == -1) {
		perror("Fallo la creacion del servidor para memoria.\n");
		exit(EXIT_FAILURE);
	}

	conectar_cpu();
	conectar_kernel();
	conectar_io();
}

void esperar_desconexiones(){
	sem_wait(&sema_cpu);
	sem_wait(&sema_kernel);
}

void levantar_logger(){
    logger_memoria = log_create("memoria.log", "MEMORIA",true, LOG_LEVEL_INFO);
	if (!logger_memoria) {
		perror("Error al iniciar logger de memoria\n");
		exit(EXIT_FAILURE);
	}
}

void levantar_config(){
	config_memoria = config_create(config_path);
	if (!config_memoria) {
		perror("Error al iniciar config de memoria\n");
		exit(EXIT_FAILURE);
	}
}

void conectar_kernel(){
	log_info(logger_memoria, "Esperando Kernel....");
    socket_kernel = esperar_cliente(socket_servidor, logger_memoria);
    log_info(logger_memoria, "Se conecto Kernel");

	int err = pthread_create(&hilo_kernel, NULL, (void *)atender_kernel, NULL);
	if (err != 0) {
		perror("Fallo la creacion de hilo para Kernel\n");
		return;
	}
	pthread_detach(hilo_kernel);
}

void conectar_io(){
    log_info(logger_memoria, "Esperando IO....");
    socket_io = esperar_cliente(socket_servidor, logger_memoria);
    log_info(logger_memoria, "Se conecto IO");

	int err = pthread_create(&hilo_io, NULL, (void *)atender_io, NULL);
	if (err != 0) {
		perror("Fallo la creacion de hilo para IO\n");
		return;
	}
	pthread_detach(hilo_io);
}

void conectar_cpu(){
    log_info(logger_memoria, "Esperando Cpu....");
    socket_cpu = esperar_cliente(socket_servidor, logger_memoria);
    log_info(logger_memoria, "Se conecto Cpu");
    	
	int err = pthread_create(&hilo_cpu, NULL, (void *)atender_cpu, NULL);
	if (err != 0) {
		perror("Fallo la creacion de hilo para CPU\n");
		return;
	}
	pthread_detach(hilo_cpu);
}

void atender_kernel(){	
	while(1){
	int cod_kernel = recibir_codigo(socket_kernel); 
	switch(cod_kernel){
		case INICIAR_PROCESO_SOLICITUD:			
			iniciar_proceso();
		break;		
		case FINALIZAR_PROCESO_SOLICITUD:		
			liberar_proceso();			
		break;		
		case -1:
			log_error(logger_memoria, "Se desconecto KERNEL");
			sem_post(&sema_kernel);
			return;		
		default:
		break;
		}
		}
	}

void atender_cpu(){		
	retardo_respuesta = config_get_string_value(config_memoria, "RETARDO_RESPUESTA");
	while(1){
	int pedido_cpu = recibir_codigo(socket_cpu);
	switch(pedido_cpu){
		case PEDIDO_INSTRUCCION:
			usleep((int)retardo_respuesta);
		//Ante un pedido de lectura, devolver el valor que se encuentra a partir de la dirección física pedida
		//Ante un pedido de escritura, escribir lo indicado a partir de la dirección física pedida. En caso satisfactorio se responderá un mensaje de ‘OK’
			enviar_instruccion();
			break;
		case -1:
			log_error(logger_memoria, "Se desconecto CPU");
			sem_post(&sema_cpu);
			return;	
		default:
			break;			
		}		
	}
}

void atender_io(){
}

void iniciar_proceso(){	
	t_buffer* buffer_recibido = recibir_buffer(socket_kernel);
	int tam = 0;
	uint32_t pid = buffer_read_uint32(buffer_recibido);
    char* path_op = buffer_read_string(buffer_recibido,tam);  //path de kernel, inst del proceso a ejecutar 
	destruir_buffer(buffer_recibido);

	t_list* lista_instrucciones = levantar_instrucciones(path_op);

	t_proceso* proceso_nuevo = crear_proceso(pid,lista_instrucciones);
		
	log_info(logger_memoria, "Creación: PID: %d - Tamaño: 0", pid);

	pthread_mutex_lock(&mutex_lista_procesos);
	list_add(lista_procesos, proceso_nuevo);
	pthread_mutex_unlock(&mutex_lista_procesos);

	enviar_codigo(socket_kernel, INICIAR_PROCESO_OK);	
}


void liberar_proceso(){
	t_buffer* buffer_recibido = recibir_buffer(socket_kernel);
	uint32_t pid = buffer_read_uint32(buffer_recibido);
	t_proceso* proceso_a_eliminar = buscar_proceso(pid);
	eliminar_proceso(proceso_a_eliminar);
	log_info(logger_memoria, "Destruccion: PID: %d - Tamaño: 0", pid);
	enviar_codigo(socket_kernel , FINALIZAR_PROCESO_OK);
}

void eliminar_proceso(t_proceso* proceso){
	list_destroy_and_destroy_elements(proceso->lista_instrucciones, (void* ) eliminar_instruccion);
	free(proceso);
}

void eliminar_instruccion(t_instruccion* instruccion){
	free(instruccion->parametro1);
	free(instruccion->parametro2);
	free(instruccion->parametro3);
	free(instruccion->parametro4);
	free(instruccion->parametro5);
	free(instruccion->codigo); //?
	free(instruccion);
}

t_proceso* crear_proceso(uint32_t pid, t_list* lista_instrucciones){
	t_proceso* proceso = malloc(sizeof(t_proceso));	
	proceso->pid = pid;		
	proceso->lista_instrucciones = lista_instrucciones;
	return proceso;
}

t_list* levantar_instrucciones(char* path_op){
	t_list* lista_instrucciones = list_create();
	//crear un proceso cuyas operaciones corresponderán al archivo de pseudocódigo pasado por parámetro
	/*void leer_y_ejecutar(char* path){
    FILE* script = fopen(path,"r");
    char* s;
    int leido = fscanf(script,"%s\n", s);
    while (leido != EOF){
        char** linea = string_split(s, " ");  // linea[0] contiene el comando y linea[1] el parametro
        ejecutar_comando_unico(linea[0], linea); // linea y palabras son lo mismo, es el resultado de split
        string_array_destroy(linea); // no se si string_split usa memoria dinamica
		leido = fscanf(script,"%s\n", s);
    }
    fclose(script);
}*/
	FILE* archivo_instrucciones = fopen(path_op, "r");
	return lista_instrucciones;
}

t_proceso* buscar_proceso(uint32_t pid)
{
	t_proceso* proceso;
	for(int i = 0; i < list_size(lista_procesos); i++){
		proceso = list_get(lista_procesos, i);
		if(proceso->pid == pid){
			return proceso;
			}
	}
}
void enviar_instruccion(){
	
	instrucciones_path = config_get_string_value(config_memoria, "PATH_INSTRUCCIONES");
	uint32_t pid;
	uint32_t pc;
		
	t_instruccion* instruccion = leer_instruccion(pc); //instruccion?
	
	t_buffer* buffer_recibido = recibir_buffer(socket_cpu);

	pid = buffer_read_uint32(buffer_recibido);
	pc = buffer_read_uint32(buffer_recibido);	
	
	pthread_mutex_lock(&mutex_lista_procesos);
	t_proceso* proceso = buscar_proceso(pid);
	pthread_mutex_unlock(&mutex_lista_procesos);

	//t_instruccion* instruccion = list_get(proceso->instrucciones, pc);
	
	/*FILE* archivo_inst = fopen( instrucciones_path, "r");
	//codigo de instruccion?
	
	fseek(archivo_inst,codigoInstruccion, SEEK_SET);
	fscanf(archivo_inst,"%s",ins_leida);*/
		
	//le envio a cpu la instruccion

	buffer_instruccion = crear_buffer();	
	buffer_write_instruccion(buffer_instruccion, instruccion);
	enviar_buffer(buffer_instruccion,socket_cpu);
	destruir_buffer(buffer_instruccion);
	//fclose(archivo_inst);
}
void terminar_programa(){
	if (logger_memoria) log_destroy(logger_memoria);
	if (config_memoria) config_destroy(config_memoria);
}

void iterator(char* value) {
	log_info(logger_memoria,"%s", value);
}