/*
  client.c - Componente de cliente para el proyecto de chat.

  Funcionalidades:
  - Conexión al servidor y registro del usuario.
  - Interfaz de comandos por consola para enviar mensajes y comandos.
  - Envío de mensajes de broadcast y mensajes directos (DM).
  - Solicitud de lista de usuarios conectados e información de un usuario.
  - Cambio de estado (ACTIVO, OCUPADO, INACTIVO).
  - Recepción de mensajes del servidor en un hilo separado.
  - Desconexión limpia, incluyendo manejo de la señal Ctrl+C (SIGINT).

  Para compilar: gcc -o client client.c -lcjson -lpthread
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cjson/cJSON.h>
#include <signal.h>

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
    struct sockaddr_in serv_addr;
    pthread_t recv_thread;
    char buffer[BUFFER_SIZE];

    // Registrar handler para Ctrl+C (SIGINT) para desconexión limpia.
    signal(SIGINT, sigint_handler);

    // Verificar argumentos: <nombredeusuario> <IPdelservidor> <puertodelservidor>
    if (argc != 4) {
        printf("Uso: %s <nombredeusuario> <IPdelservidor> <puertodelservidor>\n", argv[0]);
        return 1;
    }
    
    // Guardar el nombre de usuario
    strncpy(g_username, argv[1], sizeof(g_username) - 1);
    
    // Crear socket TCP
    if ((g_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Error al crear socket");
        return 1;
    }
    
    // Configurar dirección del servidor
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[3]));
    
    // Convertir la IP de texto a formato binario
    if (inet_pton(AF_INET, argv[2], &serv_addr.sin_addr) <= 0) {
        perror("Dirección inválida o no soportada");
        return 1;
    }
    
    // Conectar al servidor
    if (connect(g_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Error al conectar al servidor");
        return 1;
    }
    
    g_connected = 1;
    printf("Conectado al servidor %s:%s\n", argv[2], argv[3]);
    
    // Registrar el usuario en el servidor
    send_registration();
    
    // Crear un hilo para recibir mensajes del servidor de forma asíncrona
    if (pthread_create(&recv_thread, NULL, receive_messages, NULL) != 0) {
        perror("Error al crear el hilo de recepción");
        close(g_socket);
        return 1;
    }
    pthread_detach(recv_thread); // El hilo se ejecuta de forma independiente
    
    // Mostrar los comandos disponibles
    display_help();
    
    // Bucle principal: leer y procesar comandos del usuario
    while (g_connected) {
        printf("> ");
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
    return 0;
}

// Manejador de la señal SIGINT (Ctrl+C) para desconexión limpia.
void sigint_handler(int sig) {
    (void)sig; // Evitar advertencia de variable no usada
    printf("\nSe recibió SIGINT. Cerrando conexión...\n");
    disconnect_client();
    close(g_socket);
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
            printf("\nDesconectado del servidor.\n");
            g_connected = 0;
            break;
        }
        
        // Parsear el mensaje recibido en formato JSON
        cJSON *json = cJSON_Parse(buffer);
        if (json == NULL) {
            // Si el mensaje no es JSON, se ignora
            continue;
        }
        
        // Se verifica si se trata de una respuesta (OK o ERROR) o de una acción
        cJSON *respuesta = cJSON_GetObjectItemCaseSensitive(json, "respuesta");
        cJSON *accion = cJSON_GetObjectItemCaseSensitive(json, "accion");
        
        if (respuesta && cJSON_IsString(respuesta)) {
            if (strcmp(respuesta->valuestring, "OK") == 0) {
                printf("\nOperación completada con éxito.\n");
            } else if (strcmp(respuesta->valuestring, "ERROR") == 0) {
                cJSON *razon = cJSON_GetObjectItemCaseSensitive(json, "razon");
                if (razon && cJSON_IsString(razon)) {
                    printf("\nError: %s\n", razon->valuestring);
                }
            }
        } else if (accion && cJSON_IsString(accion)) {
            // Procesar acciones según el tipo de mensaje recibido
            if (strcmp(accion->valuestring, "BROADCAST") == 0) {
                cJSON *emisor = cJSON_GetObjectItemCaseSensitive(json, "nombre_emisor");
                cJSON *mensaje = cJSON_GetObjectItemCaseSensitive(json, "mensaje");
                if (emisor && mensaje && cJSON_IsString(emisor) && cJSON_IsString(mensaje)) {
                    printf("\n[BROADCAST] %s: %s\n", emisor->valuestring, mensaje->valuestring);
                }
            } else if (strcmp(accion->valuestring, "DM") == 0) {
                cJSON *emisor = cJSON_GetObjectItemCaseSensitive(json, "nombre_emisor");
                cJSON *mensaje = cJSON_GetObjectItemCaseSensitive(json, "mensaje");
                if (emisor && mensaje && cJSON_IsString(emisor) && cJSON_IsString(mensaje)) {
                    printf("\n[DM de %s]: %s\n", emisor->valuestring, mensaje->valuestring);
                }
            } else if (strcmp(accion->valuestring, "LISTA") == 0) {
                cJSON *usuarios = cJSON_GetObjectItemCaseSensitive(json, "usuarios");
                if (usuarios && cJSON_IsArray(usuarios)) {
                    printf("\nUsuarios conectados:\n");
                    cJSON *usuario = NULL;
                    cJSON_ArrayForEach(usuario, usuarios) {
                        if (cJSON_IsString(usuario))
                            printf("- %s\n", usuario->valuestring);
                    }
                }
            }
        }
        
        cJSON_Delete(json);
        printf("> ");
        fflush(stdout);
    }
    
    return NULL;
}

// Envía el mensaje de registro al servidor para iniciar sesión.
void send_registration() {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "tipo", "REGISTRO");
    cJSON_AddStringToObject(json, "usuario", g_username);
    // Se envía "0.0.0.0" ya que el servidor detectará la IP real del cliente.
    cJSON_AddStringToObject(json, "direccionIP", "0.0.0.0");
    
    char *json_str = cJSON_Print(json);
    if (send(g_socket, json_str, strlen(json_str), 0) < 0) {
        perror("Error al enviar registro");
    }
    
    free(json_str);
    cJSON_Delete(json);
}

// Envía un mensaje broadcast a todos los usuarios conectados.
void send_broadcast(const char *message) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "accion", "BROADCAST");
    cJSON_AddStringToObject(json, "nombre_emisor", g_username);
    cJSON_AddStringToObject(json, "mensaje", message);
    
    char *json_str = cJSON_Print(json);
    if (send(g_socket, json_str, strlen(json_str), 0) < 0) {
        perror("Error al enviar broadcast");
    }
    
    free(json_str);
    cJSON_Delete(json);
}

// Envía un mensaje directo (DM) a un usuario específico.
void send_direct_message(const char *recipient, const char *message) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "accion", "DM");
    cJSON_AddStringToObject(json, "nombre_emisor", g_username);
    cJSON_AddStringToObject(json, "nombre_destinatario", recipient);
    cJSON_AddStringToObject(json, "mensaje", message);
    
    char *json_str = cJSON_Print(json);
    if (send(g_socket, json_str, strlen(json_str), 0) < 0) {
        perror("Error al enviar mensaje directo");
    }
    
    free(json_str);
    cJSON_Delete(json);
}

// Solicita al servidor la lista de usuarios conectados.
void request_user_list() {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "accion", "LISTA");
    // Se puede incluir el nombre del usuario que solicita la lista
    cJSON_AddStringToObject(json, "nombre_usuario", g_username);
    
    char *json_str = cJSON_Print(json);
    if (send(g_socket, json_str, strlen(json_str), 0) < 0) {
        perror("Error al solicitar lista de usuarios");
    }
    
    free(json_str);
    cJSON_Delete(json);
}

// Solicita al servidor información de un usuario en particular.
void request_user_info(const char *username) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "tipo", "MOSTRAR");
    cJSON_AddStringToObject(json, "usuario", username);
    
    char *json_str = cJSON_Print(json);
    if (send(g_socket, json_str, strlen(json_str), 0) < 0) {
        perror("Error al solicitar información de usuario");
    }
    
    free(json_str);
    cJSON_Delete(json);
}

// Envía una solicitud para cambiar el estado del usuario.
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
            printf("Estado inválido. Use 0 para ACTIVO, 1 para OCUPADO, 2 para INACTIVO.\n");
            return;
    }
    
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "tipo", "ESTADO");
    cJSON_AddStringToObject(json, "usuario", g_username);
    cJSON_AddStringToObject(json, "estado", status_str);
    
    char *json_str = cJSON_Print(json);
    if (send(g_socket, json_str, strlen(json_str), 0) < 0) {
        perror("Error al cambiar estado");
    }
    
    g_status = status;
    
    free(json_str);
    cJSON_Delete(json);
}

// Envía una solicitud de desconexión al servidor para cerrar la sesión de forma limpia.
void disconnect_client() {
    if (!g_connected) return;
    
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "tipo", "EXIT");
    cJSON_AddStringToObject(json, "usuario", g_username);
    
    char *json_str = cJSON_Print(json);
    if (send(g_socket, json_str, strlen(json_str), 0) < 0) {
        perror("Error al enviar solicitud de desconexión");
    }
    
    free(json_str);
    cJSON_Delete(json);
    
    g_connected = 0;
    printf("Desconectado del chat.\n");
}

// Muestra en consola los comandos disponibles para el usuario.
void display_help() {
    printf("\n=== COMANDOS DISPONIBLES ===\n");
    printf("/broadcast <mensaje>    - Enviar mensaje a todos los usuarios\n");
    printf("/dm <usuario> <mensaje> - Enviar mensaje directo a un usuario\n");
    printf("/list                 - Mostrar lista de usuarios conectados\n");
    printf("/info <usuario>       - Mostrar información de un usuario\n");
    printf("/status <0|1|2>        - Cambiar estado (0: ACTIVO, 1: OCUPADO, 2: INACTIVO)\n");
    printf("/help                 - Mostrar esta ayuda\n");
    printf("/exit                 - Salir del chat\n");
    printf("===========================\n");
}

// Procesa la entrada del usuario y ejecuta el comando correspondiente.
// Si el comando no coincide con ninguno, se interpreta como un mensaje broadcast.
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
            printf("Uso: /dm <usuario> <mensaje>\n");
            return;
        }
        int len = space - remain;
        strncpy(recipient, remain, len);
        recipient[len] = '\0';
        const char *message = space + 1;
        send_direct_message(recipient, message);
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
