# TP: C-Comenta

# Modo de uso
- Importante, los cuatro módulos necesitan el path de sus respectivos configs files, la manera de levantarlos es:
  ```
  ./modulo config-path
  ```
1. Levantar **Memoria**
2. Levantar **Cpu**
3. Levantar **Kernel**
4. Levantar **Entradasalida**
5. Interactuar con las consolas
6. Al finalizar apagar **Entradasalida y Kernel** por consola

# Entrega: Checkpoint 1

## Funcionalidades

- Los cuatro módulos se conectan correctamente.
- Los cuatro módulos pueden comunicarse correctamente.
- Kernel tiene consola interactiva.
- Kernel tiene la conexion dispatch e interrupt con Cpu disponible.
- Entradasalida tiene consola interactiva (detallado en comentarios).
- Kernel puede (y debe) apagarse por consola, esto implica que Cpu también se apague (detallado en comentarios).
- Entradasalida puede (y debe) apagarse por consola.

## Comentarios

- Sobre el protocolo de comunicación: consideramos que usando el formato de mensajes del TP0 podriamos demostrar la correcta funcionalidad de las conexiones, los cuatro modulos son capaces de recibir y enviar mensajes de forma correcta, tanto mensajes como paquetes.
- Consola interactiva de Entradasalida: pensamos que en esta instancia sería necesario una consola para poder demostrar la correcta comunicación entre los modulos, a futuro puede retirarse sin problemas.
- Kernel puede apagarse por consola: pensamos que si Kernel es el único modulo que puede enviarle mensajes a Cpu entonces si finaliza, finalizan ambos.
- Sabemos que las Entradasalida pueden ser una o muchas, la implementación no sería muy distinta, la idea seria hacer una lista o array de sockets y junto a un contador de cuantos fueron creados gestionar todas, incluyendo el manejo de hilos para cada conexion. Ya va a estar implementado para la siguiente entrega.
- Absolutamente todo esta listo para modificaciones y expansiones, se trató de generalizar lo más que se pudo y **reutilizamos el tp0** de la forma más óptima que supimos, estamos abiertos a mejoras.
