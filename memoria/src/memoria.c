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
		case CREAR_PROCESO:
			t_proceso_memo* proceso_kernel = crear_proceso();			
		break;
		case FINALIZAR_PROCESO:
			liberar_proceso(proceso_kernel);		
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
	int pedido_cpu = recibir_codigo(socket_cpu);
	retardo_respuesta = config_get_string_value(config_memoria, "RETARDO_RESPUESTA");

	switch(pedido_cpu){
		case PEDIDO_INSTRUCCION:
		usleep((int)retardo_respuesta);
		enviar_instruccion();
		default:
			break;
	}
		
}

void enviar_instruccion(){
	
	instrucciones_path = config_get_string_value(config_memoria, "PATH_INSTRUCCIONES");
	uint32_t pid;
	uint32_t pc;
	
	char* ins_leida;
	t_instruccion* instruccion;
	/*cpu pide instruccion	
	busco la instruccion en el archivo de instrucciones (ENUM)*/

	t_buffer* buffer_recibido = recibir_buffer(socket_cpu);
	//para paginas
	pid = buffer_read_uint32(buffer_recibido);
	pc = buffer_read_uint32(buffer_recibido);	
	FILE* archivo_inst = fopen( instrucciones_path, "r");
	//pc? codigo de instruccion?
	
	fseek(archivo_inst,codigoInstruccion, SEEK_SET);
	fscanf(archivo_inst,"%s",ins_leida);
	//indice?
	escribirCharParametroInstruccion(1, instruccion, ins_leida);
	
	//le envio a cpu la instruccion
	buffer_instruccion = crear_buffer();
	buffer_write_instruccion(buffer_instruccion, instruccion);

	enviar_buffer(buffer_instruccion,socket_cpu);
	destruir_buffer(buffer_instruccion);
	fclose(archivo_inst);
}

void atender_io(){
}

void terminar_programa(){
	if (logger_memoria) log_destroy(logger_memoria);
	if (config_memoria) config_destroy(config_memoria);
}

void iterator(char* value) {
	log_info(logger_memoria,"%s", value);
}

t_proceso_memo* crear_proceso();
//int pid = proceso_kernel->pid;
//int paginas= proceso_kernel->paginas;
//log_info(logger_memoria,"PID: <%d>  - Tamaño: <%d> ", pid , paginas);

void liberar_proceso(t_proceso_memo* proceso);