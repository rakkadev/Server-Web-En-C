#include <sys/socket.h>    // Proporciona funciones y definiciones necesarias para trabajar con sockets. Contiene funciones como socket(), bind(), listen(), accept(), send(), recv(), etc. para la creación y manejo de conexiones de red.
#include <sys/types.h>      // Define tipos de datos utilizados en funciones del sistema, como el tipo `ssize_t`, `size_t`, `pid_t`, entre otros. Se usa generalmente en combinación con otras librerías del sistema.
#include <arpa/inet.h>      // Proporciona funciones y macros para manipular direcciones IP y conversiones entre formatos de red e IPv4/IPv6, como inet_pton() y inet_ntoa().
#include <unistd.h>         // Define las constantes y funciones relacionadas con el sistema operativo, como `read()`, `write()`, `close()`, `fork()`, `getpid()`, y otras operaciones relacionadas con el sistema de archivos y procesos.
#include <signal.h>         // Define las constantes y funciones relacionadas con el manejo de señales en el sistema operativo, como `signal()` para manejar señales como SIGINT, SIGTERM, etc.
#include <stdlib.h>         // Contiene funciones de propósito general, como `malloc()`, `free()`, `exit()`, `atoi()`, y otras para manejo de memoria dinámica, conversión de tipos y terminación del programa.
#include <stdio.h>          // Define funciones para la entrada y salida estándar, como `printf()`, `scanf()`, `fopen()`, `fclose()`, `fread()`, y `fwrite()`.
#include <string.h>         // Proporciona funciones para manipular cadenas de caracteres, como `strlen()`, `strcmp()`, `memcpy()`, `strcpy()`, entre otras.
#include <errno.h>          // Proporciona una variable global `errno` que contiene el código de error de las últimas funciones del sistema. Se utiliza para verificar si una función ha fallado y diagnosticar el error correspondiente.
#include <fcntl.h>          // Define constantes para manipular archivos, como `O_RDONLY`, `O_WRONLY`, `O_RDWR`, `O_CREAT`, y `O_APPEND`, y proporciona funciones como `open()`, `fcntl()`, y `creat()` para operaciones de archivos.
#include <sys/mman.h>       // Contiene funciones relacionadas con la memoria mapeada, como `mmap()` para mapear archivos en memoria y `munmap()` para liberar la memoria mapeada.
#include <sys/types.h>      // Define tipos de datos que se utilizan en varias funciones del sistema, como `pid_t`, `size_t`, `uid_t`, entre otros. Aunque aparece dos veces en la lista, su función es la misma en ambas inclusiones.
#include <sys/stat.h>       // Define estructuras y funciones relacionadas con los atributos de archivos, como `stat()`, `fstat()`, `chmod()`, `chown()`, y `umask()`, que permiten obtener información sobre archivos y cambiar sus permisos.


#define PORT 2001  // Puerto en el que el servidor escuchará las conexiones
#define LISTENQ 10  // Número máximo de conexiones en la cola de escucha

int list_s;  // Socket de escucha

// Estructura para almacenar el código de retorno HTTP y la ruta del archivo a enviar al cliente
typedef struct {
    int returncode;
    char *filename;
} httpRequest;

// Encabezados HTTP para diferentes códigos de estado
const char *header200 = "HTTP/1.0 200 OK\nServer: CS241Serv v0.1\nContent-Type: text/html\n\n";
const char *header400 = "HTTP/1.0 400 Bad Request\nServer: CS241Serv v0.1\nContent-Type: text/html\n\n";
const char *header404 = "HTTP/1.0 404 Not Found\nServer: CS241Serv v0.1\nContent-Type: text/html\n\n";

// Función para manejar errores, muestra el mensaje y termina el programa
void handle_error(const char *msg) {
    perror(msg);  // Imprime el error
    exit(EXIT_FAILURE);  // Termina el programa con estado de error
}

// Función para leer el mensaje enviado por el cliente
char *getMessage(int fd) {
    FILE *sstream = fdopen(fd, "r");  // Convierte el descriptor de archivo en un stream
    if (!sstream) handle_error("Error al abrir el descriptor de archivo en getMessage()");

    size_t size = 1;
    char *block = malloc(size);  // Asigna memoria para almacenar el mensaje
    if (!block) handle_error("Error al asignar memoria a block en getMessage");
    *block = '\0';

    char *tmp = malloc(size);  // Asigna memoria para almacenar temporalmente las líneas
    if (!tmp) handle_error("Error al asignar memoria a tmp en getMessage");
    *tmp = '\0';

    int oldsize = 1;
    int end;
    while ((end = getline(&tmp, &size, sstream)) > 0) {  // Lee línea por línea
        if (strcmp(tmp, "\r\n") == 0) break;  // Si la línea es un salto de línea, termina la lectura
        block = realloc(block, size + oldsize);  // Reasigna memoria para el bloque
        if (!block) handle_error("Error al reasignar memoria en getMessage");
        oldsize += size;
        strcat(block, tmp);  // Agrega la nueva línea al bloque
    }

    free(tmp);
    return block;  // Retorna el mensaje completo
}

// Función para enviar un mensaje al cliente a través del socket
int sendMessage(int fd, const char *msg) {
    return write(fd, msg, strlen(msg));  // Escribe el mensaje en el socket
}

// Función para obtener el nombre del archivo solicitado por el cliente
char *getFileName(const char *msg) {
    char *file = malloc(strlen(msg) + 1);  // Asigna memoria para el nombre del archivo
    if (!file) handle_error("Error al asignar memoria a file en getFileName");

    sscanf(msg, "GET %s HTTP/1.1", file);  // Extrae el archivo solicitado del mensaje

    char *base = malloc(strlen(file) + 18);  // Asigna espacio para la ruta completa (directorio + archivo)
    if (!base) handle_error("Error al asignar memoria a base en getFileName");

    strcpy(base, "public_html");  // Directorio base donde se almacenan los archivos
    strcat(base, file);  // Concatenamos el archivo solicitado al directorio base
    free(file);

    return base;  // Retorna la ruta completa del archivo
}

// Función para analizar la solicitud HTTP y determinar el código de respuesta y la ruta del archivo
httpRequest parseRequest(const char *msg) {
    httpRequest ret;
    char *filename = getFileName(msg);  // Obtiene la ruta del archivo solicitado

    // Verifica si el archivo solicitado intenta acceder fuera de la carpeta pública (por ejemplo, usando "..")
    if (strstr(filename, "..")) {
        ret.returncode = 400;  // Solicitud inválida (Bad Request)
        ret.filename = "400.html";  // Archivo de error 400
    } else if (strcmp(filename, "public_html/") == 0) {
        ret.returncode = 200;  // Solicitud válida, archivo index.html
        ret.filename = "public_html/index.html";  // Página principal
    } else if (access(filename, F_OK) != -1) {
        ret.returncode = 200;  // El archivo existe y es válido
        ret.filename = filename;
    } else {
        ret.returncode = 404;  // El archivo no existe (Not Found)
        ret.filename = "404.html";  // Archivo de error 404
    }

    return ret;  // Retorna el código de respuesta y la ruta del archivo
}

// Función para enviar el contenido de un archivo al cliente
int printFile(int fd, const char *filename) {
    FILE *read = fopen(filename, "r");  // Abre el archivo solicitado para lectura
    if (!read) handle_error("Error al abrir el archivo en printFile");

    struct stat st;
    stat(filename, &st);  // Obtiene el tamaño del archivo
    int totalsize = st.st_size;

    char *temp = malloc(1);  // Asigna memoria temporal para leer el archivo
    if (!temp) handle_error("Error al asignar memoria a temp en printFile");

    size_t size = 1;
    int end;
    while ((end = getline(&temp, &size, read)) > 0) {  // Lee el archivo línea por línea
        sendMessage(fd, temp);  // Envía cada línea al cliente
    }

    sendMessage(fd, "\n");  // Añade un salto de línea al final del archivo
    free(temp);
    fclose(read);

    return totalsize;  // Retorna el tamaño total del archivo enviado
}

// Función para manejar la limpieza al presionar Ctrl-C (por ejemplo, cerrar sockets y liberar recursos)
void cleanup(int sig) {
    printf("Limpiando conexiones y saliendo.\n");
    if (close(list_s) < 0) handle_error("Error al cerrar el socket de escucha");
    shm_unlink("/sharedmem");  // Elimina la memoria compartida
    exit(EXIT_SUCCESS);  // Termina el programa
}

// Función para imprimir el encabezado HTTP correspondiente al código de estado
int printHeader(int fd, int returncode) {
    const char *header;
    switch (returncode) {
        case 200: header = header200; break;  // Código 200: OK
        case 400: header = header400; break;  // Código 400: Bad Request
        case 404: header = header404; break;  // Código 404: Not Found
        default: return 0;  // Si el código no es válido, no enviamos encabezado
    }
    sendMessage(fd, header);  // Envía el encabezado HTTP al cliente
    return strlen(header);  // Retorna el tamaño del encabezado enviado
}

// Función para registrar la cantidad total de bytes enviados en la memoria compartida
int recordTotalBytes(int bytes_sent, sharedVariables *mempointer) {
    pthread_mutex_lock(&mempointer->mutexlock);  // Bloquea el acceso a la memoria compartida para evitar conflictos
    mempointer->totalbytes += bytes_sent;  // Suma los bytes enviados al total acumulado
    pthread_mutex_unlock(&mempointer->mutexlock);  // Desbloquea el acceso a la memoria compartida
    return mempointer->totalbytes;  // Retorna el total de bytes enviados
}

int main() {
    int conn_s;
    short int port = PORT;
    struct sockaddr_in servaddr;

    signal(SIGINT, cleanup);  // Configura el manejo de la señal SIGINT (Ctrl-C) para limpiar los recursos

    list_s = socket(AF_INET, SOCK_STREAM, 0);  // Crea el socket de escucha
    if (list_s < 0) handle_error("Error al crear el socket de escucha");

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);  // Escucha en todas las interfaces
    servaddr.sin_port = htons(port);

    if (bind(list_s, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0)
        handle_error("Error al enlazar el socket");

    if (listen(list_s, LISTENQ) == -1) handle_error("Error al escuchar");

    // Creamos y mapeamos la memoria compartida para registrar los bytes enviados
    shm_unlink("/sharedmem");
    int sharedmem = shm_open("/sharedmem", O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if (sharedmem == -1) handle_error("Error al abrir la memoria compartida");

    ftruncate(sharedmem, sizeof(sharedVariables));  // Establece el tamaño de la memoria compartida
    sharedVariables *mempointer = mmap(NULL, sizeof(sharedVariables), PROT_READ | PROT_WRITE, MAP_SHARED, sharedmem, 0);
    if (mempointer == MAP_FAILED) handle_error("Error al mapear la memoria compartida");

    pthread_mutex_init(&mempointer->mutexlock, NULL);  // Inicializa el mutex
    mempointer->totalbytes = 0;  // Inicializa el contador de bytes enviados

    int addr_size = sizeof(servaddr);
    int children = 0;
    pid_t pid;

    while (1) {
        if (children <= 10) {
            pid = fork();  // Crea un nuevo proceso hijo para manejar la conexión
            children++;
        }

        if (pid == -1) handle_error("Error en fork");

        if (pid == 0) {
            while (1) {
                conn_s = accept(list_s, (struct sockaddr *)&servaddr, &addr_size);  // Acepta la conexión entrante
                if (conn_s == -1) handle_error("Error al aceptar la conexión");

                char *header = getMessage(conn_s);  // Obtiene el mensaje HTTP del cliente
                httpRequest details = parseRequest(header);  // Procesa la solicitud HTTP
                free(header);

                // Imprime el encabezado y el archivo solicitado al cliente
                int headersize = printHeader(conn_s, details.returncode);
                int pagesize = printFile(conn_s, details.filename);
                int totaldata = recordTotalBytes(headersize + pagesize, mempointer);  // Registra los bytes enviados

                printf("El proceso %d sirvió una solicitud de %d bytes. Total de bytes enviados: %d\n", getpid(), headersize + pagesize, totaldata);
                close(conn_s);  // Cierra la conexión
            }
        }
    }

    return EXIT_SUCCESS;  // Termina el programa correctamente
}
