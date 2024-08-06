Simulacion del funcionamiento de un sistema operativo. Parte practica de la asignatura "sistemas operativos"  de la UTN FRBA 1C 2024.
Este protecto esta desarrollado en C y esta dividido principalmente en 4 modulos:
1- Kernel
2- CPU
3- Memoria 
4- Entrada salida

Ademas, se creo otro modulo extra para aquellas funciones y logica que repiten todos los modulos. 

ENUNCIADO: https://docs.google.com/document/d/1-AqFTroovEMcA1BfC2rriB5jsLE6SUa4mbcAox1rPec/edit

El sistema operativo presenta las siguiente caracteristicas:
* Diagrama de 5 estaods : New - Ready - Exec - Blocked - Exit
* Algoritmos del planificador de corto plazo: FIFO - Round Robin - Virtul Round Robin
* Consola interactiva en Kernel: EJECUTAR_SCRIPT -INICIAR_PROCESO -FINALIZAR_PROCESO , etc.
* Ciclo de instruccion : Fetch -Decode -Execute
* Instrucciones en codigo Asembler y registro de CPU : SET - MOV_IN - MOV_OUT - SUM- SUB -WAIT - SIGNAL ,AX , BX , EAX, EBX , etc..
* Desarrollo de MMU
* Implementacion de TLB con algoritmos : FIFO - LRU
* Esquema de paginacion simple (contigua) : tabla de paginas 
* Desarrollo de File System: Creacion de Metadata por archivo regular - tabla de procesos abiertos - archivo para el contenido de los archivos creados (bloque.dat) - creacion de bitmap- compactacion de archivos
* Asignacion de bloques contiguo 
* Desarrollo de interfaces input y output
* Desarollo de interfaz generica : retardo de tiempo unicamente
  
