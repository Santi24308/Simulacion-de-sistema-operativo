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

	memfisica = malloc(total_espacio_memoria); // Tamaño memoria (capaz difiere)
	if (memfisica == NULL)
	{
		log_error(logger_memoria, "MALLOC FAIL para la memoria fisica!\n");
		free(memfisica);
		exit(EXIT_FAILURE);
	}

	lista_procesos = list_create();
	interfacesIO = list_create();

	pthread_mutex_init(&mutex_lista_procesos, NULL);
	pthread_mutex_init(&mutex_lista_tablas, NULL);

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

	pthread_create(&hilo_esperar_IOs, NULL, (void *)esperarIOs, NULL);
	pthread_detach(hilo_esperar_IOs);

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

void conectar_io(pthread_t *hilo_io, int *socket_io)
{
	int err = pthread_create(hilo_io, NULL, (void *)atender_io, socket_io);
	if (err != 0)
	{
		perror("Fallo la creacion de hilo para IO\n");
		return;
	}
	pthread_detach(*(hilo_io));
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
		mensajeCpuMem pedido_cpu = recibir_codigo(socket_cpu);
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
		case MOV_IN_SOLICITUD:
			t_buffer *buffer_mov_in = recibir_buffer(socket_cpu);			
			uint32_t dir_fisica_mov_in = buffer_read_uint32(buffer_mov_in);
			uint32_t bytes_mov_in = buffer_read_uint32(buffer_mov_in);
			destruir_buffer(buffer_mov_in);

			void *datos_leidos = malloc(bytes_mov_in);
			if (datos_leidos == NULL)
			{
				log_error(logger_memoria, "Error al asignar memoria para datos leídos.\n");
				break;
			}

			memcpy(datos_leidos, memfisica->espacioMemoria + dir_fisica_mov_in, bytes_mov_in);

			uint32_t dato = *(uint32_t *)datos_leidos;

			t_buffer *buffer_respuesta = crear_buffer();
			buffer_write_uint32(buffer_respuesta, dato);
			enviar_buffer(buffer_respuesta, socket_cpu);
			destruir_buffer(buffer_respuesta);
			free(datos_leidos);

			break;
		case MOV_OUT_SOLICITUD:
			t_buffer *buffer_mov_out = recibir_buffer(socket_cpu);
			uint32_t dir_fisica_mov_out = buffer_read_uint32(buffer_mov_out); // No se usa 
			uint32_t valor_a_escribir = buffer_read_uint32(buffer_mov_out);
			uint32_t bytes_mov_out = buffer_read_uint32(buffer_mov_out);
			destruir_buffer(buffer_mov_out);

			memcpy(memfisica->espacioMemoria+dir_fisica_mov_out, (void *)&valor_a_escribir, bytes_mov_out);

			break;
		case RESIZE_SOLICITUD:
			t_buffer *buffer_resize = recibir_buffer(socket_cpu);
			uint32_t pid_resize = buffer_read_uint32(buffer_resize);
			uint32_t tamanio = buffer_read_uint32(buffer_resize);
			destruir_buffer(buffer_resize);

			uint32_t marcos_libres = cantidad_marcos_libres();
			uint32_t cantidad_paginas_solicitadas = ceil(tamanio / tamanio_paginas);
			t_proceso *proceso = buscar_proceso(pid_resize);
			uint32_t tamanio_reservado = sizeof(proceso->tabla_de_paginas) * tamanio_paginas;
			uint32_t tamanio_reservado_en_paginas = sizeof(proceso->tabla_de_paginas);

			// CASO REDUCCION
			if (tamanio < tamanio_reservado)
			{
				uint32_t diferencia = tamanio_reservado_en_paginas - cantidad_paginas_solicitadas;
				log_info(logger_memoria, "PID: %d -Tamaño actual: %d -Tamaño a Reducir: %d", pid_resize, tamanio_reservado, tamanio);
				while (diferencia > 0)
				{
					uint32_t indice_pagina_a_eliminar = sizeof(list_size(proceso->tabla_de_paginas)) - 1;
					t_pagina *pagina_a_eliminar = list_get(proceso->tabla_de_paginas, indice_pagina_a_eliminar);
					destruir_pagina_y_liberar_marco(pagina_a_eliminar);
					list_remove_element(proceso->tabla_de_paginas, pagina_a_eliminar);
					diferencia--;
				}
				log_info(logger_memoria, "Se destruyeron paginas del proceso PID: %d - Tamaño %d", pid_resize, diferencia);
			}

			// CASO AMPLIACION
			else if (tamanio > tamanio_reservado)
			{
				// se chequea si el proceso ya tenia asignadas paginas previamente, en ese caso solo se crea la diferencia
				uint32_t pagina_totales_a_crear = cantidad_paginas_solicitadas - tamanio_reservado_en_paginas;

				if (marcos_libres >= pagina_totales_a_crear)
				{
					log_info(logger_memoria, "PID: %d - Tamaño Actual: %d - Tamaño a Ampliar: %d", pid_resize, tamanio_reservado, tamanio);
					while (cantidad_paginas_solicitadas != 0)
					{
						uint32_t nro_pagina = list_size(proceso->tabla_de_paginas);
						crear_pagina_y_asignar_a_pid(nro_pagina, pid_resize);
						cantidad_paginas_solicitadas--;
					}
					// log creacion
					log_info(logger_memoria, "Se crearon paginas del proceso PID: %d - Tamaño %d", pid_resize, pagina_totales_a_crear);
				}
				else
				{
					enviar_codigo(socket_cpu, ERROR_OUT_OF_MEMORY);
				}
			}

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

void atender_io(void *socket_io)
{
	int socket_interfaz_io = *((int *)socket_io);
	while (1){
		codigoInstruccion codigo = recibir_codigo(socket_interfaz_io);
		switch (codigo){
		case IO_STDIN_READ:
			ejecutar_io_stdin(socket_interfaz_io);
			break;
		case IO_STDOUT_WRITE:
			ejecutar_io_stdout(socket_interfaz_io);
			break;
		case IO_GEN_SLEEP:
			break;
		case IO_FS_CREATE:
			break;
		case IO_FS_DELETE:
			break;
		case IO_FS_TRUNCATE:
			break;
		case IO_FS_WRITE:
			break;
		case IO_FS_READ:
			break;
		default:
			break;
		}
	}
}

void ejecutar_io_stdin(int socket_interfaz_io)
{	
	uint32_t tamanio_valor;
	t_buffer *buffer = recibir_buffer(socket_interfaz_io);
	uint32_t pid = buffer_read_uint32(buffer);
	uint32_t direccion_fisica = buffer_read_uint32(buffer);
	uint32_t bytes_a_copiar = buffer_read_uint32(buffer);
	char *valor_a_escribir = buffer_read_string(buffer, &tamanio_valor); 
	destruir_buffer(buffer);

	uint32_t bytes_disp_frame = tamanio_paginas - obtener_desplazamiento_pagina(bytes_a_copiar);
	if (bytes_disp_frame >= bytes_a_copiar)
	{
		memcpy(memfisica->espacioMemoria + direccion_fisica, valor_a_escribir, bytes_a_copiar);
		return;
	}
	else
	{
		memcpy(memfisica->espacioMemoria + direccion_fisica, valor_a_escribir, bytes_disp_frame);
		// termina una pagina
		bytes_a_copiar = bytes_a_copiar - bytes_disp_frame;
		int cant_paginas_restantes = ceil(bytes_a_copiar / tamanio_paginas);
		int desplazamiento_string = bytes_disp_frame;
		while (cant_paginas_restantes > 1)
		{
			t_pagina *pagina_siguiente = obtener_pagsig_de_dirfisica(direccion_fisica, pid);
			uint32_t direccion_fisica_nueva = recalcular_direccion_fisica(pagina_siguiente);
			memcpy(memfisica->espacioMemoria + direccion_fisica_nueva, valor_a_escribir + desplazamiento_string, tamanio_paginas);
			desplazamiento_string = tamanio_paginas;
			bytes_a_copiar = bytes_a_copiar - tamanio_paginas;
			cant_paginas_restantes = ceil(bytes_a_copiar / tamanio_paginas);
		}
		t_pagina *pagina_siguiente = obtener_pagsig_de_dirfisica(direccion_fisica, pid);
		uint32_t direccion_fisica_nueva = recalcular_direccion_fisica(pagina_siguiente);
		memcpy(memfisica->espacioMemoria + direccion_fisica_nueva, valor_a_escribir + desplazamiento_string, bytes_a_copiar);
	}
}

t_pagina *obtener_pagsig_de_dirfisica(uint32_t direccion_fisica, uint32_t pid)
{
	t_proceso *proceso = buscar_proceso(pid);
	t_list *paginas = proceso->tabla_de_paginas;
	uint32_t marco_actual = obtener_numero_marco(direccion_fisica);
	int i = 0;
	bool pagina_encontrada = false;
	while (i < list_size(paginas) && !pagina_encontrada)
	{
		t_pagina *pagina = list_get(paginas, i);
		if (marco_actual == pagina->marco)
		{
			pagina_encontrada = true;
			return pagina = list_get(paginas, i + 1);
		}
		i++;
	}
	return NULL;
}

uint32_t recalcular_direccion_fisica(t_pagina *pagina)
{
	return pagina->marco * tamanio_paginas; // retorna la direccion_fisica
}
int obtener_numero_marco(int direccion_fisica)
{
	return floor(direccion_fisica / tamanio_paginas);
}

int obtener_desplazamiento_pagina(int direccion_fisica)
{
	int numero_marco = obtener_numero_marco(direccion_fisica);
	return direccion_fisica - numero_marco * tamanio_paginas;
}

void ejecutar_io_stdout(int socket_interfaz_io)
{
	t_buffer *buffer = recibir_buffer(socket_interfaz_io);
	uint32_t pid = buffer_read_uint32(buffer);
	uint32_t direccion_fisica = buffer_read_uint32(buffer);
	uint32_t bytes_a_leer = buffer_read_uint32(buffer);
	destruir_buffer(buffer);

	buffer = crear_buffer();

	char* valor_a_leer = malloc((size_t)bytes_a_leer);
	int bytes_usados = 0;
	int desplazamiento = obtener_desplazamiento_pagina(direccion_fisica);
	int bytes_restantes = bytes_a_leer; 

	// si el desplazamiento es distinto de cero leemos hasta terminar la pagina
	if (desplazamiento != 0){
		bytes_usados = tamanio_paginas - desplazamiento;
		memcpy(valor_a_leer, memfisica->espacioMemoria+direccion_fisica, bytes_usados);
		
		bytes_restantes = bytes_a_leer - bytes_usados;
		
		// caso en donde leyendo de la primer pagina ya completamos la solicitud
		if (bytes_restantes == 0) {
			buffer_write_string(buffer, valor_a_leer);
			enviar_buffer(buffer, socket_interfaz_io);
			destruir_buffer(buffer);
			free(valor_a_leer);
			return;
		}
	}

	// una vez completada la primera pagina seguimos leyendo la cantidad de bytes de una pagina completa
	// hasta que no se pueda mas

	t_pagina *pagina_siguiente = NULL;
	uint32_t direccion_fisica_nueva = UINT32_MAX;

	while (bytes_restantes != 0 && bytes_restantes >= tamanio_paginas){
		pagina_siguiente = obtener_pagsig_de_dirfisica(direccion_fisica, pid);
		direccion_fisica_nueva = recalcular_direccion_fisica(pagina_siguiente);
		memcpy(valor_a_leer+bytes_usados, memfisica->espacioMemoria+direccion_fisica_nueva, tamanio_paginas);
		
		bytes_usados += tamanio_paginas;
		bytes_restantes -= tamanio_paginas;
	}

	// si sobraron bytes leemos esa cantidad 

	if (bytes_restantes != 0){
		pagina_siguiente = obtener_pagsig_de_dirfisica(direccion_fisica, pid);
		direccion_fisica_nueva = recalcular_direccion_fisica(pagina_siguiente);
		memcpy(valor_a_leer+bytes_usados, memfisica->espacioMemoria+direccion_fisica_nueva, bytes_restantes);
	}

	buffer_write_string(buffer, valor_a_leer);
	enviar_buffer(buffer, socket_interfaz_io);
	destruir_buffer(buffer);
	free(valor_a_leer);
	return;
}

//char *reconstruir_dato(void *parte1, void *parte2, uint32_t tamanio1, uint32_t tamanio2)
//{
//}

/*Acceso a espacio de usuario: “PID: <PID> - Accion: <LEER / ESCRIBIR> - Direccion fisica: <DIRECCION_FISICA>” - Tamaño <TAMAÑO A LEER / ESCRIBIR>*/
void esperarIOs() {
	log_info(logger_memoria, "Esperando que se conecte una IO....");
	while (1){
		int socket_io = esperar_cliente(socket_servidor, logger_memoria);

		uint32_t tamanio_id;
		uint32_t tamanio_tipo;
		t_buffer *buffer = recibir_buffer(socket_io);
		char *id = buffer_read_string(buffer, &tamanio_id);
		char *tipo = buffer_read_string(buffer, &tamanio_tipo);
		destruir_buffer(buffer);

		t_interfaz *interfaz = crear_interfaz(id, tipo, socket_io);

		list_add(interfacesIO, (void *)interfaz);

		conectar_io(&interfaz->hilo_io, &interfaz->socket);

		log_info(logger_memoria, "Se conecto la IO con ID : %s  TIPO: %s", id, tipo);

		// imprimir_ios();
	}
}

// SOLUCION MOMENTANEA A VER DONDE UBICAMOS CREAR_INTERFAZ PARA NO REPETIRLO 
t_interfaz* crear_interfaz(char* nombre, char* tipo, int socket){
    t_interfaz* interfaz = malloc(sizeof(t_interfaz));
    interfaz->id = string_new();
    string_append(&interfaz->id, nombre);
    interfaz->tipo = string_new();
    string_append(&interfaz->tipo, tipo);
    interfaz->socket = socket;

    return interfaz;
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
	proceso->tabla_de_paginas = list_create();
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
	liberarMemoriaPaginacion();
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

	// buscamos por pid al proceso involucrado
	t_proceso *proceso_involucrado = buscar_proceso(pid);

	// nos metemos en la tabla de paginas del proceso
	t_pagina *pagina_solicitada = list_get(proceso_involucrado->tabla_de_paginas, nro_pagina);

	buffer = crear_buffer();
	buffer_write_uint32(buffer, pagina_solicitada->marco);
	enviar_buffer(buffer, socket_cpu);
	destruir_buffer(buffer);

	log_info(logger_memoria, "Acceso a TGP - PID: %d - Página: %d - Marco: %d", pid, pagina_solicitada->nro_pagina, pagina_solicitada->marco);
}

// Exclusivo de paginacion //

void inicializar_paginacion()
{
	cant_marcos_ppal = calculoDeCantidadMarcos();
	log_info(logger_memoria, " el espacio de memoria total es %d", total_espacio_memoria);

	log_info(logger_memoria, "Tengo %d marcos de %d bytes en memoria principal", cant_marcos_ppal, tamanio_paginas);

	// Inicializamos la memoria a cero
	//memset(memfisica, 0, total_espacio_memoria);
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
		pthread_mutex_lock(&mutex_lista_tablas);
		list_add(tabla_de_marcos, marco);
		pthread_mutex_unlock(&mutex_lista_tablas);

		log_info(logger_memoria, "Marco %d inicializado y añadido a la tabla", i);
	}

	log_info(logger_memoria, "Tabla de marcos creada con éxito");
}

void inicializarMarco(t_marco *marco)
{
	marco->bit_uso = 0;
	marco->paginaAsociada = NULL;
}

void crear_pagina_y_asignar_a_pid(int numero_pagina, int pid)
{
	t_pagina *pagina = malloc(sizeof(t_pagina));
	int numeroMarco = obtener_marco_libre();
	pagina->nro_pagina = numero_pagina;
	pagina->pid = pid;
	pagina->marco = numeroMarco;

	t_proceso *proceso = buscar_proceso(pid);
	list_add(proceso->tabla_de_paginas, pagina);

	pthread_mutex_lock(&mutex_lista_tablas);
	list_replace(tabla_de_marcos, numeroMarco, pagina);
	pthread_mutex_unlock(&mutex_lista_tablas);
}

uint32_t obtener_marco_libre()
{
	for (int i = 0; i < cantidadDeMarcos; i++)
	{
		t_pagina *pagina = list_get(tabla_de_marcos, i);
		if (pagina == NULL)
			return i;
	}
	return -1;
}

void destruir_pagina_y_liberar_marco(t_pagina *pagina)
{
	uint32_t nroMarco = pagina->marco;
	t_marco *marco = list_get(tabla_de_marcos, nroMarco);
	marco->bit_uso = 0;
	marco->paginaAsociada = NULL;
	free(pagina);
}

t_pagina *buscarPaginaPorNroYPid(uint32_t nroPag, uint32_t pid)
{

	for (int i = 0; i < list_size(tabla_de_marcos); i++)
	{
		t_pagina *pag = list_get(tabla_de_marcos, i);

		if (pag->nro_pagina == nroPag && pag->pid == pid)
			return pag;
	}

	return NULL;
}

int cantidad_marcos_libres()
{
	int j = 0;
	for (int i = 0; i < cantidadDeMarcos; i++)
	{
		t_pagina *pagina = list_get(tabla_de_marcos, i);
		if (pagina == NULL)
		{
			j++;
		}
	}
	return j;
}

// EVALUAR SI ES NECESARIO USAR ESTAS FUNCIONES EN OTRO MODULO PARA PORNERLA EN LIBRERIA COMUN//

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

void liberarMemoriaPaginacion()
{

	// free(memoriaReservada);
	pthread_mutex_lock(&mutex_lista_tablas);
	list_destroy(tabla_de_marcos);
	pthread_mutex_unlock(&mutex_lista_tablas);
}
