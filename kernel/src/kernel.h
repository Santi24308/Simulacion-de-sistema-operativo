#ifndef SRC_KERNEL_H_
#define SRC_KERNEL_H_

#include <utils/conexiones.h>
#include <commons/string.h>
#include <diccionarioKernel.h>

void imprimir_ios();

void levantar_logger();
void levantar_config();
void terminar_programa();
void conectar_cpu_dispatch();
void conectar_cpu_interrupt();
void conectar_io();
void conectar_memoria();
void atender_io();
void atender_cpu();
void inicializar_modulo();
void consola();
void conectar();
void iterator(char*);
void conectar_consola();
// CHECKPOINT 2
void inicializarListas();
void inicializarSemaforos();
t_pcb* crear_pcb(char*);
void destruir_pcb(t_pcb*);
t_pcb* encontrar_pcb_por_pid(uint32_t, int* );
void retirar_pcb_de_su_respectivo_estado(t_pcb* );
void finalizar_pcb(t_pcb*);
void iniciar_proceso(char*);
void terminar_proceso();
void iniciar_quantum();
void controlar_tiempo_de_ejecucion();
void listar_procesos_por_estado();
void enviar_cde_a_cpu();
void ejecutar_comando_unico(char**);
void leer_y_ejecutar(char*);
void esperarIOs();
t_interfaz *obtener_interfaz_en_lista(char*, int *);
t_interfaz* crear_interfaz(char*, char*, int );
bool interfaz_valida(char*);
void finalizarProceso(uint32_t pid_string);
void despachar_pcb_a_interfaz(t_interfaz*, t_pcb*);
bool io_puede_cumplir_solicitud(char* , codigoInstruccion );
void trim_trailing_whitespace(char*);

void evaluar_io_gen_sleep(t_instruccion*);
void evaluar_signal(char*);
void evaluar_wait(char*);

void signal_recursos_asignados_pcb(t_pcb*, char*);
void liberar_recursos_pcb(t_pcb*);

void agregar_pcb_a(t_queue*, t_pcb*, pthread_mutex_t*);
t_pcb* retirar_pcb_de(t_queue*, pthread_mutex_t*);
char* obtener_elementos_cargados_en(t_queue*);
char* obtener_nombre_estado(t_estados);
t_recurso* inicializar_recurso(char*, int);
void terminar_proceso_consola(uint32_t);

char* obtener_nombre_motivo_finalizacion(cod_finalizacion);

bool instruccion_de_recursos(codigoInstruccion);
void evaluar_io(t_instruccion* );

void controlar_tiempo();

// PLANIFICACION -------------------------------------------------------------------------------------------------------------------
void iniciar_planificacion();
void detener_planificacion();

// TRANSICIONES

void enviar_de_new_a_ready();
void enviar_de_ready_a_exec();
void enviar_de_exec_a_ready();
void enviar_de_exec_a_block();
void enviar_pcb_de_block_a_ready(t_pcb*);
void enviar_pcb_de_block_a_readyPlus(t_pcb*);

void enviar_de_exec_a_finalizado();
char* obtener_nombre_estado(t_estados);
char* obtener_nombre_motivo(cod_desalojo);

// FIN TRANSICIONES 

// PLANIFICACION 
void cambiar_grado_multiprogramacion(char*);

t_pcb* retirar_pcb_de_ready_segun_algoritmo();
t_pcb* elegir_segun_fifo();
t_pcb* elegir_segun_rr();
t_pcb* elegir_segun_vrr();

int esta_proceso_en_cola_bloqueados(t_pcb*);
char* obtener_elementos_cargados_en(t_queue*);

void enviar_cde_a_cpu();
void evaluar_instruccion(t_instruccion*);
void recibir_cde_de_cpu();

void levantar_planificador_largo_plazo();
void levantar_recepcion_cde();
void levantar_planificador_corto_plazo();

// FIN PLANIFICACION

void actualizar_cde(t_cde*);
void copiar_ultima_instruccion(t_cde*, t_instruccion*);

void controlar_tiempo_de_ejecucion_VRR();
void reloj_quantum_VRR();


#endif