Un Servidor Web Simple Escrito en C

Este es un servidor web simple escrito en el lenguaje de programación C.

Utiliza un grupo de 10 conexiones para atender múltiples solicitudes de manera concurrente y realiza un seguimiento de la cantidad de datos que ha enviado, imprimiéndolos en el flujo de salida estándar.
Compilando y Usando el Sistema

En un sistema Linux, simplemente usa el makefile para compilar el servidor.

En una Mac, usa este comando para compilar el servidor:

gcc cs241server.c -o cs241server

Para ejecutar el servidor, escribe ./server en una terminal que esté en el directorio donde se encuentra el archivo ejecutable.

De manera predeterminada, el servidor se ejecuta en el puerto 2001. Así que, para probarlo, navega a:

localhost:2001 en un navegador web.