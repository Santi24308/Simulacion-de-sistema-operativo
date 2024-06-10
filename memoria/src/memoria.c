#include "memoria.h"

int main(int argc, char *argv[])
{

	if (argc != 2)
	{
		printf("ERROR: Tenés que pasar el path del archivo config de Memoria\n");
		return -1;
	}

	config_path = argv[1];

	inicializar_modulo();
	inicializar_paginacion();
	conectar();

	sem_wait(&terminar_memoria);

	terminar_programa();

	return 0;
}

void inicializar_modulo()
{
	levantar_logger();
	levantar_config();

	lista_procesos = list_create();

	pthread_mutex_init(&mutex_lista_procesos, NULL);
	sem_init(&terminar_memoria, 0, 0);
}

void conectar()
{
	puerto_escucha = config_get_string_value(config_memoria, "PUERTO_ESCUCHA");

	socket_servidor = iniciar_servidor(puerto_escucha, logger_memoria);
	if (socket_servidor == -1)
	{
		perror("Fallo la creacion del servidor para memoria.\n");
		exit(EXIT_FAILURE);
	}

	conectar_cpu();
	conectar_kernel();
	// conectar_io();
	sem_wait(&terminar_memoria);
}

void levantar_logger()
{
	logger_memoria = log_create("memoria.log", "MEMORIA", true, LOG_LEVEL_INFO);
	if (!logger_memoria)
	{
		perror("Error al iniciar logger de memoria\n");
		exit(EXIT_FAILURE);
	}
}

void levantar_config()
{
	config_memoria = config_create(config_path);
	if (!config_memoria)
	{
		perror("Error al iniciar config de memoria\n");
		exit(EXIT_FAILURE);
	}
}

void conectar_kernel()
{
	log_info(logger_memoria, "Esperando Kernel....");
	socket_kernel = esperar_cliente(socket_servidor, logger_memoria);
	log_info(logger_memoria, "Se conecto Kernel");

	int err = pthread_create(&hilo_kernel, NULL, (void *)atender_kernel, NULL);
	if (err != 0)
	{
		perror("Fallo la creacion de hilo para Kernel\n");
		return;
	}
	pthread_detach(hilo_kernel);
}

void conectar_io()
{
	log_info(logger_memoria, "Esperando IO....");
	socket_io = esperar_cliente(socket_servidor, logger_memoria);
	log_info(logger_memoria, "Se conecto IO");

	int err = pthread_create(&hilo_io, NULL, (void *)atender_io, NULL);
	if (err != 0)
	{
		perror("Fallo la creacion de hilo para IO\n");
		return;
	}
	pthread_detach(hilo_io);
}

void conectar_cpu()
{
	log_info(logger_memoria, "Esperando Cpu....");
	socket_cpu = esperar_cliente(socket_servidor, logger_memoria);
	log_info(logger_memoria, "Se conecto Cpu");

	// tiene que enviar al CPU el tamanio de pagina para que tenga con que laburar la MMU
	t_buffer *buffer = crear_buffer();
	buffer_write_uint32(buffer, tamanio_paginas); // tamanio_paginas = config_get_int_value(config_memoria, "TAM_PAGINA" );(esta declarado mas abajo pero para que sepamos de donde sale)
	enviar_buffer(buffer, socket_cpu);
	destruir_buffer(buffer);

	int err = pthread_create(&hilo_cpu, NULL, (void *)atender_cpu, NULL);
	if (err != 0)
	{
		perror("Fallo la creacion de hilo para CPU\n");
		return;
	}
	pthread_detach(hilo_cpu);
}

void atender_kernel()
{
	while (1)
	{
		int cod_kernel = recibir_codigo(socket_kernel);
		switch (cod_kernel)
		{
		case INICIAR_PROCESO_SOLICITUD:
			iniciar_proceso();
			break;
		case FINALIZAR_PROCESO_SOLICITUD:
			liberar_proceso();
			break;
		case -1:
			log_error(logger_memoria, "Se desconecto KERNEL");
			sem_post(&terminar_memoria);
			return;
		default:
			break;
		}
	}
}

void atender_cpu()
{
	retardo_respuesta = config_get_string_value(config_memoria, "RETARDO_RESPUESTA");
	while (1)
	{
		int pedido_cpu = recibir_codigo(socket_cpu);
		switch (pedido_cpu)
		{
		case PEDIDO_INSTRUCCION:
			usleep(atoi(retardo_respuesta));
			// Ante un pedido de lectura, devolver el valor que se encuentra a partir de la dirección física pedida
			// Ante un pedido de escritura, escribir lo indicado a partir de la dirección física pedida. En caso satisfactorio se responderá un mensaje de OK
			enviar_instruccion();
			break;
		case NUMERO_MARCO_SOLICITUD:
			devolver_nro_marco();
			break;
		case -1:
			log_error(logger_memoria, "Se desconecto CPU");
			sem_post(&terminar_memoria);
			return;
		default:
			break;
		}
	}
}

void atender_io()
{
}

void iniciar_proceso()
{ // ver atender io de kernel
	t_buffer *buffer_recibido = recibir_buffer(socket_kernel);
	uint32_t tam = 0;
	uint32_t pid = buffer_read_uint32(buffer_recibido);

	char *nombre_archivo = buffer_read_string(buffer_recibido, &tam); // path de kernel, inst del proceso a ejecutar

	destruir_buffer(buffer_recibido);

	char *ruta_completa = string_new();
	string_append(&ruta_completa, config_get_string_value(config_memoria, "PATH_INSTRUCCIONES"));
	string_append(&ruta_completa, nombre_archivo);

	t_list *lista_instrucciones = levantar_instrucciones(ruta_completa);
	if (!lista_instrucciones)
		return;

	t_proceso *proceso_nuevo = crear_proceso(pid, lista_instrucciones);

	log_info(logger_memoria, "Creación: PID: %d", pid);

	pthread_mutex_lock(&mutex_lista_procesos);
	list_add(lista_procesos, proceso_nuevo);
	pthread_mutex_unlock(&mutex_lista_procesos);

	enviar_codigo(socket_kernel, INICIAR_PROCESO_OK);
}

void liberar_proceso()
{
	printf("\nProcesos en el sistema antes de eliminar");
	imprimir_pids();
	t_buffer *buffer_recibido = recibir_buffer(socket_kernel);
	uint32_t pid = buffer_read_uint32(buffer_recibido);
	buscar_y_eliminar_proceso(pid);
	printf("\nProcesos en el sistema DESPUES de eliminar");
	imprimir_pids();
	log_info(logger_memoria, "Destruccion: PID: %d", pid);
	enviar_codigo(socket_kernel, FINALIZAR_PROCESO_OK);
}

void imprimir_pids()
{
	int i = 0;
	printf("\n");
	while (i < list_size(lista_procesos))
	{
		t_proceso *proceso = list_get(lista_procesos, i);
		printf("\tPROCESO PID: %i\n", proceso->pid);
		i++;
	}
	printf("\n");
}

void eliminar_instruccion(t_instruccion *instruccion)
{
	free(instruccion->parametro1);
	free(instruccion->parametro2);
	free(instruccion->parametro3);
	free(instruccion->parametro4);
	free(instruccion->parametro5);
	free(instruccion);
}

t_proceso *crear_proceso(uint32_t pid, t_list *lista_instrucciones)
{
	t_proceso *proceso = malloc(sizeof(t_proceso));
	proceso->pid = pid;
	proceso->lista_instrucciones = lista_instrucciones;
	// Apenas arranca va a ser 0, despues se hace el resize
	// proceso->cantMaxMarcos = tamanio / config_memoria.tam_pagina;
	return proceso;
}

// para que no haya error el archivo tiene que terminar con un salto de linea
t_list *levantar_instrucciones(char *path_op)
{
	t_list *lista_instrucciones = list_create();
	FILE *archivo_instrucciones = fopen(path_op, "r");
	if (!archivo_instrucciones)
	{
		printf("\nNO SE PUDO ABRIR EL ARCHIVO DE INSTRUCCIONES!!\n");
		list_destroy(lista_instrucciones);
		return NULL;
	}
	t_instruccion *instruccion;

	char leido[200];

	while (fgets(leido, 200, archivo_instrucciones) != NULL && !feof(archivo_instrucciones))
	{
		trim_trailing_whitespace(leido);
		char **linea = string_split(leido, " ");

		if (!string_is_empty(linea[0]))
		{

			instruccion = crear_instruccion(obtener_codigo_instruccion(linea[0]));

			int i = 1;
			while (linea[i])
			{
				escribirCharParametroInstruccion(i, instruccion, linea[i]);
				i++;
			}

			list_add(lista_instrucciones, instruccion);
		}

		string_array_destroy(linea);
	}

	fclose(archivo_instrucciones);

	return lista_instrucciones;
}
/*
void imprimir_instruccion(t_instruccion* instruccion){
	printf("\tINSTRUCCION LEIDA: %s \n", obtener_nombre_instruccion(instruccion));
	int i = 1;
	while(i <= 5){
		char* par = leerCharParametroInstruccion(i, instruccion);
		if (par)
			printf("\tPARAMETRO %i: %s\n", i, leerCharParametroInstruccion(i, instruccion));
		i++;
	}
}*/

// esta funcion fixea los casos en donde fgets al leer del archivo lee algo que deberia ser
// "palabra" como "palabra           ".
void trim_trailing_whitespace(char *str)
{
	int len = strlen(str);
	while (len > 0 && (str[len - 1] == ' ' || str[len - 1] == '\n' || str[len - 1] == '\t'))
	{
		str[len - 1] = '\0';
		len--;
	}
}

void buscar_y_eliminar_proceso(uint32_t pid)
{
	t_proceso *proceso = NULL;
	for (int i = 0; i < list_size(lista_procesos); i++)
	{
		proceso = list_get(lista_procesos, i);
		if (proceso->pid == pid)
		{
			eliminar_lista_instrucciones(proceso->lista_instrucciones);
			free(proceso);
			list_remove(lista_procesos, i);
			return;
		}
	}
	log_warning(logger_memoria, "Proceso no encontrado, no hay cambios.");
}

void eliminar_lista_instrucciones(t_list *lista)
{
	int i = 0;
	while (i < list_size(lista))
	{
		t_instruccion *instruccion = list_get(lista, i);
		eliminar_instruccion(instruccion);
		list_remove(lista, i);
		i++;
	}
	list_destroy(lista);
}

void enviar_instruccion()
{

	uint32_t pid;
	uint32_t pc;

	t_buffer *buffer_recibido = recibir_buffer(socket_cpu);

	pid = buffer_read_uint32(buffer_recibido);
	pc = buffer_read_uint32(buffer_recibido);

	pthread_mutex_lock(&mutex_lista_procesos);
	t_proceso *proceso = buscar_proceso(pid);
	pthread_mutex_unlock(&mutex_lista_procesos);

	t_instruccion *instruccion = list_get(proceso->lista_instrucciones, pc); // mutex?

	buffer_instruccion = crear_buffer();
	buffer_write_instruccion(buffer_instruccion, instruccion);
	enviar_buffer(buffer_instruccion, socket_cpu);
	destruir_buffer(buffer_instruccion);
}

void terminar_programa()
{
	if (logger_memoria)
		log_destroy(logger_memoria);
	if (config_memoria)
		config_destroy(config_memoria);
}

void iterator(char *value)
{
	log_info(logger_memoria, "%s", value);
}
t_proceso *buscar_proceso(uint32_t pid)
{
	t_proceso *proceso = malloc(sizeof(t_proceso));
	for (int i = 0; i < list_size(lista_procesos); i++)
	{
		proceso = list_get(lista_procesos, i);
		if (proceso->pid == pid)
		{
			return proceso;
		}
	}
	return NULL;
}

////// CHECK 3 /////

void devolver_nro_marco()
{
	t_buffer *buffer = recibir_buffer(socket_cpu);
	uint32_t nro_pagina = buffer_read_uint32(buffer);
	uint32_t pid = buffer_read_uint32(buffer);
	destruir_buffer(buffer);
	/*
	t_pagina* pagina = existePageFault(nro_pagina, pid); //Hacer existePageFault : devuelve un puntero
	if(pagina == NULL)
		enviar_codigo(socket_cpu, PAGE_FAULT);
	else{
		log_info(logger_memoria, "Acceso a TGP - PID: %d - Página: %d - Marco: %d", pid, pagina->nroPagina, pagina->nroMarco);
		// revisar el tema de que la pagina no tiene su nroPagina
		// revisar el tema de que la pagina no tiene su nroMarco, es marcoPpal
		enviar_codigo(socket_cpu, NUMERO_MARCO_OK);
		buffer = crear_buffer();
		buffer_write_uint32(buffer, pagina->marco_ppal);
		enviar_buffer(buffer, socket_cpu);
		destruir_buffer(buffer);
	} */
}

// Exclusivo de paginacion //

void inicializar_paginacion()
{
	cant_marcos_ppal = calculoDeCantidadMarcos();
	log_info(logger_memoria, " el espacio de memoria total es %d", total_espacio_memoria);

	t_memoria_fisica* memfisica = malloc(total_espacio_memoria); // Tamaño memoria (capaz difiere)
	if (memfisica == NULL)
	{
		log_error(logger_memoria, "MALLOC FAIL para la memoria fisica!\n");
		free(memfisica);
		exit(EXIT_FAILURE);
	}

	log_info(logger_memoria, "Tengo %d marcos de %d bytes en memoria principal", cant_marcos_ppal, tamanio_paginas);

	// Inicializamos la memoria a cero
	memset(memfisica, 0, total_espacio_memoria);
	/*
	memoriaReservada = asignarMemoriaBits(cant_marcos_ppal); // bitmap por cada frame

	if(memoriaReservada == NULL){
		log_error(logger_memoria ,"MALLOC FAIL para la memoria reservada!\n");
	}
	memset(data,0,cant_marcos_ppal/8);
	marcos_ocupados_ppal = bitarray_create_with_mode(data, cant_marcos_ppal/8, MSB_FIRST);
	*/

	crear_tabla_global_de_marcos();
}

int calculoDeCantidadMarcos()
{

	tamanio_paginas = config_get_int_value(config_memoria, "TAM_PAGINA");
	total_espacio_memoria = config_get_int_value(config_memoria, "TAM_MEMORIA");
	cantidadDeMarcos = total_espacio_memoria / tamanio_paginas;

	return cantidadDeMarcos;
}

void crear_tabla_global_de_marcos()
{
	int cantidadDeMarcos = calculoDeCantidadMarcos();
	if (cantidadDeMarcos <= 0)
	{
		log_error(logger_memoria, "cantidad de marcos calculada es inválida");
		return;
	}

	t_marco *marco;
	tabla_de_marcos = list_create();
	log_info(logger_memoria, "Inicializando tabla de marcos con %d marcos", cantidadDeMarcos);

	for (int i = 0; i < cantidadDeMarcos; i++)
	{
		marco = (t_marco *)malloc(sizeof(t_marco));
		if (marco == NULL)
		{
			log_error(logger_memoria, "error en la creacion del marco %d", i);
			break; // Si malloc falla, salimos del ciclo para evitar un bucle infinito
		}
		inicializarMarco(marco);
		list_add(tabla_de_marcos, marco);
		log_info(logger_memoria, "Marco %d inicializado y añadido a la tabla", i);
	}

	log_info(logger_memoria, "Tabla de marcos creada con éxito");
}

void inicializarMarco(t_marco *marco)
{
	marco->bit_uso = 0;
	marco->paginaAsociada = NULL;
}

t_pagina *crear_pagina(int numero_pagina , int pid)
{
	t_pagina *pagina = malloc(sizeof(t_pagina));
	int numeroMarco = obtener_marco_libre();	
	pagina->nro_pagina = numero_pagina;
	pagina->pid = pid;
	pagina->marco = numeroMarco;
	pagina->bitPresencia = true;
	pagina->bitModificado = false;
	pagina->ultimaReferencia = temporal_get_string_time("%H:%M:%S:%MS");

	t_proceso *proceso = buscar_proceso(pid);
	list_add(proceso->tabla_de_paginas, pagina);

	//mutex tabla_de_marcos??
	list_replace(tabla_de_marcos, numeroMarco,pagina);	

	// EN CASO DE COMUNICACION CON CPU
	/*enviar_codigo(socket A VER , CREAR_PAGINA_SOLICITUD);
	t_buffer* buffer = crear_buffer();
	buffer_write_uint32(buffer, nroPag);
	buffer_write_uint32(buffer, pagina->pidCreador);
	enviar_buffer(buffer, socket A VER);
	destruir_buffer_nuestro(buffer);
	*/
	return pagina;
}


int32_t obtener_marco_libre(){
	for(int i = 0; i < cantidadDeMarcos; i++){
		t_pagina* pagina = list_get(tabla_de_marcos, i);
		if(pagina == NULL)
			return i;
	}
}
void atender_page_fault(){//caso TLB Miss	
	t_buffer* buffer = recibir_buffer(socket_cpu);
	uint32_t nro_pagina = buffer_read_uint32(buffer);
	uint32_t pid = buffer_read_uint32(buffer);
	destruir_buffer_nuestro(buffer);

	if(!hay_marcos_libres())
	{
		liberar_marco();
	}		

	t_pagina* pagAPedir = buscarPaginaPorNroYPid(nro_pagina, pid);

	// no existe entonces es una nueva
	if(pagAPedir == NULL){
		//void* dirInicio = memoriaPrincipal + (nroMarco * config_memoria.tam_pagina);
		
		pagAPedir = crear_pagina(nro_pagina, pid);
		log_info(logger_memoria, "SWAP IN - PID: %d - Marco: %d - Page In: %d-%d", pid, nroMarco, pid, nro_pagina);
				
		sem_post(&sem_pagina_cargada);
	}
	else{
		//ojo
	}
	sem_wait(&sem_pagina_cargada);
	enviar_codigo(socket_kernel, PAGE_FAULT_OK);
}

void liberar_marco()
{	
	t_pagina*  pagina = elegir_pagina_a_matar();//laburo de cpu
	if(pagina->bitModificado){
		/*///de sofi///
		enviar_codigo(socket_file_system, ACTUALIZAR_PAGINA_SOLICITUD);		
		buffer = crear_buffer_nuestro();
		buffer_write_uint32(buffer, pag_a_matar->posSwap);
		buffer_write_pagina(buffer, pag_a_matar->direccionInicio, config_memoria.tam_pagina);
		enviar_buffer(buffer, socket_file_system);
		destruir_buffer_nuestro(buffer);*/
	}	
	pagina->bitPresencia = false;	
	uint32_t nroMarco = pagina->marco;
	vaciar_marco(nroMarco);
	/*Creación / destrucción de Tabla de Páginas: “PID: <PID> - Tamaño: <CANTIDAD_PAGINAS>”*/
} 

t_pagina* elegir_pagina_a_matar() ////CPU////
{	
	enviar_codigo(socket_cpu, LIBERAR_MARCO); //revisar
	t_buffer *buffer_recibido = recibir_buffer(socket_cpu);
	uint32_t pid = buffer_read_uint32(buffer_recibido);
	uint32_t nro_pagina = buffer_read_uint32(buffer_recibido);
	t_pagina* pagina = buscarPaginaPorNroYPid(nro_pagina, pid);	
	destruir_buffer(buffer_recibido);
	return pagina;
}
void vaciar_marco(nro_marco){		
	t_marco* marco = list_get(tabla_de_marcos,nro_marco);
	marco->bit_uso = 0;
	marco->paginaAsociada = NULL;
}

t_pagina* buscarPaginaPorNroYPid(uint32_t nroPag, uint32_t pid){

	for(int i = 0; i < list_size(tabla_de_marcos); i++){
		t_pagina* pag = list_get(tabla_de_marcos, i);

		if(pag->nro_pagina == nroPag && pag->pid == pid)
			return pag;
	}

	return NULL;
}

// EVALUAR SI ES NECESARIO USAR ESTAS FUNCIONES EN OTRO MODULO PARA PORNERLA EN LIBRERIA COMUN//
char *asignarMemoriaBits(int bits)
{
	char *aux;
	int bytes;
	bytes = bitsToBytes(bits);
	// printf("BYTES: %d\n", bytes);
	aux = malloc(bytes);
	memset(aux, 0, bytes);
	return aux;
}

int bitsToBytes(int bits)
{
	int bytes;
	if (bits < 8)
		bytes = 1;
	else
	{
		double c = (double)bits;
		bytes = ceil(c / 8.0);
	}
	return bytes;
}
// EVALUAR SI ES NECESARIO USAR ESTAS FUNCIONES EN OTRO MODULO PARA PORNERLA EN LIBRERIA COMUN//

void liberarMemoriaPaginacion()
{ // esto hay que agregarlo en nuestro terminar_programa

	// bitarray_destroy(frames_ocupados_ppal);
	// free(memoriaReservada);
	pthread_mutex_lock(&mutexListaTablas);
	list_destroy(tabla_de_marcos);
	pthread_mutex_unlock(&mutexListaTablas);
}

//// FUNCIONES QUE MUY PROBABLEMENTE USEMOS.
/*
void escribir_pagina(uint32_t posEnMemoria, void* pagina){
	memcpy(memoriaPrincipal + posEnMemoria, pagina, config_memoria.tam_pagina);
}

void* leer_pagina(uint32_t posEnMemoria){
	return (memoriaPrincipal + posEnMemoria);
}

void setModificado(t_pagina* pagina){
	pagina->modificado = 1;
}
int pagina_presente(t_pagina* pagina){
	return (pagina->presencia == 1);
}
bool lugar_disponible(int paginasNecesarias){ // ver si es necesario
	int cantFramesLibresPpal = memoriaDisponiblePag();
	if(cantFramesLibresPpal>= paginasNecesarias){
		return 1;
	}else{
		return 0;
	}
}
int memoriaDisponiblePag(int mem){
	int espaciosLibres = 0;
	int desplazamiento = 0;
	if(mem){
		while(desplazamiento < cant_marcos_ppal){
			pthread_mutex_lock(&mutexBitmapMP);
			if(bitarray_test_bit(frames_ocupados_ppal,desplazamiento) == 0){
				espaciosLibres++;
			}
			pthread_mutex_unlock(&mutexBitmapMP);
			desplazamiento++;
		}
	}
	return espaciosLibres;
}
*/
