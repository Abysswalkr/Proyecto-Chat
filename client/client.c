#ifdef _WIN32
  // Definir versión mínima de Windows (para inet_pton)
  #ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0600
  #endif

  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>
  #include <pthread.h> 
  #pragma comment(lib, "ws2_32.lib")

  // Redefinir close() → closesocket()
  #define close(fd) closesocket(fd)

#else
  // En Linux/Unix
  #include <unistd.h>
  #include <pthread.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "cJSON.h"


#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define RED     "\033[1;31m"
#define GREEN   "\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE    "\033[1;34m"
#define MAGENTA "\033[1;35m"
#define CYAN    "\033[1;36m"
#define WHITE   "\033[1;37m"

#define BUFFER_SIZE 2048

// Variables globales
int g_socket = 0;
char g_username[50];
int g_connected = 0;
int g_status = 0; // 0: ACTIVO, 1: OCUPADO, 2: INACTIVO

// Prototipos de funciones
void *receive_messages(void *arg);
void send_registration();
void send_broadcast(const char *message);
void send_direct_message(const char *recipient, const char *message);
void request_user_list();
void request_user_info(const char *username);
void change_status(int status);
void disconnect_client();
void display_help();
void handle_command(const char *input);
void sigint_handler(int sig);

int main(int argc, char *argv[]) {
#ifdef _WIN32
    // Inicializar WinSock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        fprintf(stderr, RED "Error: WSAStartup() fallo\n" RESET);
        exit(EXIT_FAILURE);
    }
#endif

    struct sockaddr_in serv_addr;
    pthread_t recv_thread;
    char buffer[BUFFER_SIZE];

    signal(SIGINT, sigint_handler);

    if (argc != 4) {
        printf(YELLOW "Uso: %s <nombredeusuario> <IPdelservidor> <puertodelservidor>\n" RESET, argv[0]);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    
    // Guardar el nombre de usuario
    strncpy(g_username, argv[1], sizeof(g_username) - 1);
    
    // Crear socket TCP
    if ((g_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror(RED "Error al crear socket" RESET);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[3]));

    if (inet_pton(AF_INET, argv[2], &serv_addr.sin_addr) <= 0) {
        perror(RED "Direccion invalida o no soportada" RESET);
        close(g_socket);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    
    // Conectar al servidor
    if (connect(g_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror(RED "Error al conectar al servidor" RESET);
        close(g_socket);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    
    g_connected = 1;
    printf(GREEN "Conectado al servidor %s:%s\n" RESET, argv[2], argv[3]);
    

    send_registration();
    
    if (pthread_create(&recv_thread, NULL, receive_messages, NULL) != 0) {
        perror(RED "Error al crear el hilo de recepcion" RESET);
        close(g_socket);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    pthread_detach(recv_thread); // El hilo se ejecuta de forma independiente
    
    // Mostrar los comandos disponibles
    display_help();
    
    // Bucle principal: leer y procesar comandos del usuario
    while (g_connected) {
        printf(CYAN "> " RESET);
        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL)
            break;
        // Eliminar el salto de línea
        buffer[strcspn(buffer, "\n")] = 0;
        // Procesar la entrada
        handle_command(buffer);
    }
    
    // Desconexión limpia
    disconnect_client();
    close(g_socket);

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}

// Manejador de la señal SIGINT (Ctrl+C) para desconexión limpia.
void sigint_handler(int sig) {
    (void)sig; // Evitar advertencia de variable no usada
    printf(YELLOW "\nSe recibio SIGINT. Cerrando conexion...\n" RESET);
    disconnect_client();
    close(g_socket);
#ifdef _WIN32
    WSACleanup();
#endif
    exit(0);
}

// Hilo encargado de recibir mensajes del servidor
void *receive_messages(void *arg) {
    (void)arg;
    char buffer[BUFFER_SIZE];
    
    while (g_connected) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(g_socket, buffer, BUFFER_SIZE, 0);
        
        if (bytes_received <= 0) {
            printf(RED "\nDesconectado del servidor.\n" RESET);
            g_connected = 0;
            break;
        }
        
        // Parsear el mensaje recibido en formato JSON
        cJSON *json = cJSON_Parse(buffer);
        if (json == NULL) {
            // Si el mensaje no es JSON, se ignora
            continue;
        }
        
        // Se verifica si se trata de una respuesta (OK o ERROR), de una acción o de un tipo específico
        cJSON *respuesta = cJSON_GetObjectItemCaseSensitive(json, "respuesta");
        cJSON *accion = cJSON_GetObjectItemCaseSensitive(json, "accion");
        cJSON *tipo = cJSON_GetObjectItemCaseSensitive(json, "tipo");
        
        if (respuesta && cJSON_IsString(respuesta)) {
            if (strcmp(respuesta->valuestring, "OK") == 0) {
                printf(GREEN "\nOperacion completada con exito.\n" RESET);
            } else if (strcmp(respuesta->valuestring, "ERROR") == 0) {
                cJSON *razon = cJSON_GetObjectItemCaseSensitive(json, "razon");
                if (razon && cJSON_IsString(razon)) {
                    printf(RED "\nError: %s\n" RESET, razon->valuestring);
                }
            }
        } else if (accion && cJSON_IsString(accion)) {
            // Procesar acciones según el tipo de mensaje recibido
            if (strcmp(accion->valuestring, "BROADCAST") == 0) {
                cJSON *emisor = cJSON_GetObjectItemCaseSensitive(json, "nombre_emisor");
                cJSON *mensaje = cJSON_GetObjectItemCaseSensitive(json, "mensaje");
                if (emisor && mensaje && cJSON_IsString(emisor) && cJSON_IsString(mensaje)) {
                    printf(YELLOW "\n[BROADCAST] %s: %s\n" RESET, emisor->valuestring, mensaje->valuestring);
                }
            } else if (strcmp(accion->valuestring, "DM") == 0) {
                cJSON *emisor = cJSON_GetObjectItemCaseSensitive(json, "nombre_emisor");
                cJSON *mensaje = cJSON_GetObjectItemCaseSensitive(json, "mensaje");
                if (emisor && mensaje && cJSON_IsString(emisor) && cJSON_IsString(mensaje)) {
                    printf(MAGENTA "\n[DM de %s]: %s\n" RESET, emisor->valuestring, mensaje->valuestring);
                }
            } else if (strcmp(accion->valuestring, "LISTA") == 0) {
                cJSON *usuarios = cJSON_GetObjectItemCaseSensitive(json, "usuarios");
                if (usuarios && cJSON_IsArray(usuarios)) {
                    printf(CYAN "\nUsuarios conectados:\n" RESET);
                    cJSON *usuario = NULL;
                    cJSON_ArrayForEach(usuario, usuarios) {
                        if (cJSON_IsString(usuario))
                            printf(WHITE "- %s\n" RESET, usuario->valuestring);
                    }
                }
            }
        } else if (tipo && cJSON_IsString(tipo)) {
            // Procesar tipos específicos de mensajes
            if (strcmp(tipo->valuestring, "MOSTRAR") == 0) {
                // Procesar información de usuario
                cJSON *usuario = cJSON_GetObjectItemCaseSensitive(json, "usuario");
                cJSON *direccionIP = cJSON_GetObjectItemCaseSensitive(json, "direccionIP");
                cJSON *estado = cJSON_GetObjectItemCaseSensitive(json, "estado");
                
                if (usuario && direccionIP && estado && 
                    cJSON_IsString(usuario) && cJSON_IsString(direccionIP) && cJSON_IsString(estado)) {
                    printf(BLUE "\nInformacion de usuario:\n" RESET);
                    printf("  " WHITE "Usuario:" RESET " %s\n", usuario->valuestring);
                    printf("  " WHITE "IP:" RESET " %s\n", direccionIP->valuestring);
                    printf("  " WHITE "Estado:" RESET " %s\n", estado->valuestring);
                }
            } else if (strcmp(tipo->valuestring, "ESTADO") == 0) {
                // Procesar cambio de estado
                cJSON *usuario = cJSON_GetObjectItemCaseSensitive(json, "usuario");
                cJSON *estado = cJSON_GetObjectItemCaseSensitive(json, "estado");
                
                if (usuario && estado && cJSON_IsString(usuario) && cJSON_IsString(estado)) {
                    printf(CYAN "\nUsuario %s cambió su estado a: %s\n" RESET, 
                           usuario->valuestring, estado->valuestring);
                }
            } else if (strcmp(tipo->valuestring, "SERVER_SHUTDOWN") == 0) {
                // Procesar cierre del servidor
                cJSON *mensaje = cJSON_GetObjectItemCaseSensitive(json, "mensaje");
                if (mensaje && cJSON_IsString(mensaje)) {
                    printf(RED "\n[SERVIDOR]: %s\n" RESET, mensaje->valuestring);
                    g_connected = 0; // Marcar como desconectado
                }
            }
        }
        
        cJSON_Delete(json);
        printf(CYAN "> " RESET);
        fflush(stdout);
    }
    
    return NULL;
}

/*
    Descripción:
  Envía al servidor un mensaje JSON para registrar al usuario.
  
    Entrada:
    - No recibe parámetros externos.
    
    Salida/Efectos:
    - Construye un objeto JSON con:
        "tipo": "REGISTRO"
        "usuario": valor de la variable global g_username
        "direccionIP": "0.0.0.0" (para que el servidor detecte la IP real)
    - Envía este objeto a través del socket global g_socket.
    - En caso de error en el envío, se muestra un mensaje de error.
    - No devuelve valor.

*/
void send_registration() {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "tipo", "REGISTRO");
    cJSON_AddStringToObject(json, "usuario", g_username);
    // Se envía "0.0.0.0" ya que el servidor detectará la IP real del cliente.
    cJSON_AddStringToObject(json, "direccionIP", "0.0.0.0");
    
    char *json_str = cJSON_Print(json);
    if (send(g_socket, json_str, strlen(json_str), 0) < 0) {
        perror(RED "Error al enviar registro" RESET);
    }
    
    free(json_str);
    cJSON_Delete(json);
}

/*
   Descripción:
  Envía un mensaje de difusión (broadcast) a todos los usuarios conectados.
  
    Entrada:
    - message: Cadena de texto con el mensaje a enviar.
    
    Salida/Efectos:
    - Construye un objeto JSON con:
        "accion": "BROADCAST"
        "nombre_emisor": valor de la variable global g_username
        "mensaje": contenido de message
    - Envía el objeto JSON a través de g_socket.
    - Reporta error con perror() si falla el envío.
    - No devuelve valor. 

*/
void send_broadcast(const char *message) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "accion", "BROADCAST");
    cJSON_AddStringToObject(json, "nombre_emisor", g_username);
    cJSON_AddStringToObject(json, "mensaje", message);
    
    char *json_str = cJSON_Print(json);
    if (send(g_socket, json_str, strlen(json_str), 0) < 0) {
        perror(RED "Error al enviar broadcast" RESET);
    }
    
    free(json_str);
    cJSON_Delete(json);
}

/*
    Descripción:
  Envía un mensaje directo (DM) a un usuario específico.
  
    Entrada:
    - recipient: Nombre del usuario destinatario.
    - message: Texto del mensaje.
    
    Salida/Efectos:
    - Crea un objeto JSON con:
        "accion": "DM"
        "nombre_emisor": valor de g_username
        "nombre_destinatario": valor de recipient
        "mensaje": contenido de message
    - Envía el objeto JSON mediante g_socket.
    - Si ocurre error, se muestra un mensaje usando perror().
    - No retorna valor.
*/
void send_direct_message(const char *recipient, const char *message) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "accion", "DM");
    cJSON_AddStringToObject(json, "nombre_emisor", g_username);
    cJSON_AddStringToObject(json, "nombre_destinatario", recipient);
    cJSON_AddStringToObject(json, "mensaje", message);
    
    char *json_str = cJSON_Print(json);
    if (send(g_socket, json_str, strlen(json_str), 0) < 0) {
        perror(RED "Error al enviar mensaje directo" RESET);
    }
    
    free(json_str);
    cJSON_Delete(json);
}

/*
    Descripción:
  Solicita al servidor el listado de usuarios conectados.
  
    Entrada:
    - No recibe parámetros; utiliza g_username para indicar el solicitante.
    
    Salida/Efectos:
    - Construye un objeto JSON con:
        "accion": "LISTA"
        "nombre_usuario": valor de g_username
    - Envía este objeto a través de g_socket.
    - Reporta error en caso de fallo en el envío.
    - No retorna valor.
*/

void request_user_list() {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "accion", "LISTA");
    cJSON_AddStringToObject(json, "nombre_usuario", g_username);
    
    char *json_str = cJSON_Print(json);
    if (send(g_socket, json_str, strlen(json_str), 0) < 0) {
        perror(RED "Error al solicitar lista de usuarios" RESET);
    }
    
    free(json_str);
    cJSON_Delete(json);
}

/*
    Descripción:
  Solicita al servidor información específica de un usuario.
  
    Entrada:
    - username: Nombre del usuario del que se solicita información.
    
    Salida/Efectos:
    - Crea un objeto JSON con:
        "tipo": "MOSTRAR"
        "usuario": valor de username
    - Envía el objeto JSON por g_socket.
    - Reporta error si ocurre fallo en el envío.
    - No retorna valor.
*/

void request_user_info(const char *username) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "tipo", "MOSTRAR");
    cJSON_AddStringToObject(json, "usuario", username);
    
    char *json_str = cJSON_Print(json);
    if (send(g_socket, json_str, strlen(json_str), 0) < 0) {
        perror(RED "Error al solicitar informacion de usuario" RESET);
    }
    
    free(json_str);
    cJSON_Delete(json);
}

/*
    Descripción:
  Solicita al servidor cambiar el estado del usuario (ACTIVO, OCUPADO, INACTIVO).
  
    Entrada:
    - status: Valor entero (0 para ACTIVO, 1 para OCUPADO, 2 para INACTIVO).
    
    Salida/Efectos:
    - Convierte el entero a una cadena (por ejemplo, "ACTIVO") y construye un JSON con:
        "tipo": "ESTADO"
        "usuario": g_username
        "estado": cadena correspondiente al estado
    - Envía el JSON mediante g_socket.
    - Actualiza la variable global g_status.
    - En caso de valor inválido, imprime un mensaje y no envía nada.
    - No devuelve valor.
*/

void change_status(int status) {
    const char *status_str;
    switch (status) {
        case 0:
            status_str = "ACTIVO";
            break;
        case 1:
            status_str = "OCUPADO";
            break;
        case 2:
            status_str = "INACTIVO";
            break;
        default:
            printf(YELLOW "Estado invalido. Use 0 para ACTIVO, 1 para OCUPADO, 2 para INACTIVO.\n" RESET);
            return;
    }
    
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "tipo", "ESTADO");
    cJSON_AddStringToObject(json, "usuario", g_username);
    cJSON_AddStringToObject(json, "estado", status_str);
    
    char *json_str = cJSON_Print(json);
    if (send(g_socket, json_str, strlen(json_str), 0) < 0) {
        perror(RED "Error al cambiar estado" RESET);
    }
    
    g_status = status;
    
    free(json_str);
    cJSON_Delete(json);
}

/*
   Envía una solicitud de desconexión al servidor para cerrar la sesión de forma limpia.
*/

void disconnect_client() {
    if (!g_connected) return;
    
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "tipo", "EXIT");
    cJSON_AddStringToObject(json, "usuario", g_username);
    
    char *json_str = cJSON_Print(json);
    if (send(g_socket, json_str, strlen(json_str), 0) < 0) {
        perror(RED "Error al enviar solicitud de desconexion" RESET);
    }
    
    free(json_str);
    cJSON_Delete(json);
    
    g_connected = 0;
    printf(RED "Desconectado del chat.\n" RESET);
}

/*
  Descripción:
  Muestra en consola una lista de comandos disponibles para el usuario.
  
    Entrada:
    - No recibe parámetros.
    
    Salida/Efectos:
    - Imprime en la consola una lista detallada de comandos (broadcast, dm, list, info, status, help, exit).
    - No retorna valor.  
*/


void display_help() {
    printf("\n" BLUE "========================================\n" RESET);
    printf(YELLOW BOLD "         COMANDOS DISPONIBLES           \n" RESET);
    printf(BLUE "========================================\n" RESET);
    
    printf(GREEN "/broadcast <mensaje>" RESET "    - Enviar mensaje a todos los usuarios\n");
    printf(GREEN "/dm <usuario> <mensaje>" RESET " - Enviar mensaje directo a un usuario\n");
    printf(GREEN "/list" RESET "                   - Mostrar lista de usuarios conectados\n");
    printf(GREEN "/info <usuario>" RESET "         - Mostrar informacion de un usuario\n");
    printf(GREEN "/status <0|1|2>" RESET "         - Cambiar estado (0: ACTIVO, 1: OCUPADO, 2: INACTIVO)\n");
    printf(GREEN "/help" RESET "                   - Mostrar esta ayuda\n");
    printf(GREEN "/exit" RESET "                   - Salir del chat\n");
    
    printf(BLUE "========================================\n\n" RESET);
}


/*
   Descripción:
  Procesa la entrada del usuario y ejecuta el comando correspondiente.
  
    Entrada:
    - input: Cadena de texto ingresada por el usuario. Puede ser un comando (por ejemplo, "/exit") o un mensaje a enviar.
    
    Salida/Efectos:
    - Si input coincide con un comando específico, invoca la función asociada:
        * "/exit" → disconnect_client()
        * "/help" → display_help()
        * "/list" → request_user_list()
        * "/broadcast <mensaje>" → send_broadcast()
        * "/dm <usuario> <mensaje>" → send_direct_message()
        * "/info <usuario>" → request_user_info()
        * "/status <0|1|2>" → change_status()
    - Si no coincide con ningún comando, envía el contenido como mensaje broadcast.
    - No devuelve valor. 
*/

void handle_command(const char *input) {
    if (strcmp(input, "/exit") == 0) {
        disconnect_client();
        return;
    }
    
    if (strcmp(input, "/help") == 0) {
        display_help();
        return;
    }
    
    if (strcmp(input, "/list") == 0) {
        request_user_list();
        return;
    }
    
    if (strncmp(input, "/broadcast ", 11) == 0) {
        const char *message = input + 11;
        send_broadcast(message);
        return;
    }
    
    if (strncmp(input, "/dm ", 4) == 0) {
        char recipient[50];
        const char *remain = input + 4;
        const char *space = strchr(remain, ' ');
        if (space == NULL) {
            printf(YELLOW "Uso: /dm <usuario> <mensaje>\n" RESET);
            return;
        }
        int len = (int)(space - remain);
        strncpy(recipient, remain, len);
        recipient[len] = '\0';
        const char *msg = space + 1;
        send_direct_message(recipient, msg);
        return;
    }
    
    if (strncmp(input, "/info ", 6) == 0) {
        const char *username = input + 6;
        request_user_info(username);
        return;
    }
    
    if (strncmp(input, "/status ", 8) == 0) {
        int status = atoi(input + 8);
        change_status(status);
        return;
    }
    
    // Si no se reconoce el comando, se envía el texto como mensaje broadcast.
    send_broadcast(input);
}