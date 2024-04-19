# TP: C-Comenta

# Modo de uso

1. Levantar **Memoria**
2. Levantar **Cpu**
3. Levantar **Kernel**
4. Levantar **Entradasalida**

# Entrega: Checkpoint 1

## Funcionalidades

- Los 4 módulos se conectan correctamente.
- Los 4 módulos pueden comunicarse correctamente.
- Kernel tiene consola interactiva.
- Entradasalida tiene consola interactiva (detallado en comentarios).
- Kernel puede (y debe) apagarse por consola, esto implica que Cpu también se apague (detallado en comentarios).
- Entradasalida puede (y debe) apagarse por consola.

## Comentarios

- Consola interactiva de Entradasalida: pensamos que en esta instancia sería necesario una consola para poder demostrar la correcta comunicación entre los modulos, a futuro puede retirarse sin problemas.
- Kernel puede apagarse por consola: pensamos que si Kernel es el único modulo que puede enviarle mensajes a Cpu entonces si finaliza, finalizan ambos.
- Absolutamente todo esta listo para modificaciones y expansiones, se trató de generalizar lo más que se pudo y **reutilizamos el tp0** de la forma más óptima que supimos, estamos abiertos a mejoras.
