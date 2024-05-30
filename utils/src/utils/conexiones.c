#include "conexiones.h"

int crear_conexion(char *ip, char* puerto)
{
	struct addrinfo hints;
	struct addrinfo *server_info;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	getaddrinfo(ip, puerto, &hints, &server_info);

	int socket_cliente = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
	if (socket_cliente == -1) {
		printf("Error al crear socket cliente\n");
		return -1;
	}

	if (connect(socket_cliente, server_info->ai_addr, server_info->ai_addrlen) == -1){
		printf("Error al conectar con el servidor\n");
		freeaddrinfo(server_info);
		return -1;
	} 

	freeaddrinfo(server_info);

	return socket_cliente;
}

// SERVER ----------------------------------------------------------------------------------------

int iniciar_servidor(char* puerto, t_log* logger)
{

	int socket_servidor;

	struct addrinfo hints, *servinfo;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(NULL, puerto, &hints, &servinfo);

	bool conecto = false;

	socket_servidor = socket(servinfo->ai_family,servinfo->ai_socktype,servinfo->ai_protocol);

	if (setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) 
    	error("setsockopt(SO_REUSEADDR) failed");

	if (bind(socket_servidor, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
		perror("Fallo el bind del socket\n");
		close(socket_servidor);
	} else 
		conecto = true;

	if (!conecto) {
		free(servinfo);
		return 0;
	}

	listen(socket_servidor, SOMAXCONN);

	freeaddrinfo(servinfo);

	return socket_servidor;
}

int esperar_cliente(int socket_servidor, t_log* logger)
{
    struct sockaddr_storage cliente_addr;
    socklen_t addr_size = sizeof(cliente_addr);
	
    int socket_cliente = accept(socket_servidor, (struct sockaddr *)&cliente_addr, &addr_size);

	return socket_cliente;
}

void terminar_conexiones(int num_sockets, ...) {
  va_list args;
  va_start(args, num_sockets);

  for (int i = 0; i < num_sockets; i++) {
    int socket_fd = va_arg(args, int);
    if (socket_fd != -1) close(socket_fd);
  }

  va_end(args);
}