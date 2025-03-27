#ifdef _WIN32
  // Definir versión mínima de Windows para habilitar inet_ntop
  #ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0600
  #endif

  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>
  #include <pthread.h>   // Necesario si usas pthreads-win32
  #pragma comment(lib, "ws2_32.lib")

  // Redefinir funciones POSIX a las equivalentes de Win32
  #define close(fd) closesocket(fd)
  #define sleep(x) Sleep((x)*1000)

#else
  // En Linux/Unix, las cabeceras POSIX normales
  #include <unistd.h>
  #include <pthread.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "cJSON.h"  // Asegúrate de que cJSON.h esté en tu proyecto

#define MAX_CLIENTS 100
#define BUFFER_SIZE 2048
#define DEFAULT_PORT 50213

// Estructura para usuarios
typedef struct {
    char username[50];
    char ip[INET_ADDRSTRLEN];
    int socket;
    int status;  // 0: ACTIVO, 1: OCUPADO, 2: INACTIVO
    time_t last_activity;
} user_t;

// Estructura para clientes
typedef struct {
    int socket;
    struct sockaddr_in address;
} client_t;

// Variables globales
user_t users[MAX_CLIENTS];
int user_count = 0;
pthread_mutex_t users_mutex = PTHREAD_MUTEX_INITIALIZER;

// Prototipos
void *handle_client(void *arg);
void *check_inactivity(void *arg);
int register_user(const char *username, const char *ip, int socket);
void remove_user(const char *username);
void broadcast_message(const char *sender, const char *message);
void send_direct_message(const char *sender, const char *recipient, const char *message);
void list_users(int client_socket);
void get_user_info(const char *username, int client_socket);
void change_user_status(const char *username, int status);

int main(int argc, char *argv[]) {
#ifdef _WIN32
    // Inicializar WinSock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        fprintf(stderr, "Error: WSAStartup() falló\n");
        exit(EXIT_FAILURE);
    }
#endif

    int server_fd, client_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    pthread_t thread_id, inactivity_thread;
    
    // Verificar argumentos
    int port = DEFAULT_PORT;
    if (argc > 1) {
        port = atoi(argv[1]);
    }
    
    // Crear socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Error al crear socket");
        exit(EXIT_FAILURE);
    }
    
    // Configurar socket
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt))) {
        perror("Error en setsockopt");
        exit(EXIT_FAILURE);
    }
    
    // Configurar dirección
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    // Vincular socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Error en bind");
        exit(EXIT_FAILURE);
    }
    
    // Escuchar conexiones
    if (listen(server_fd, 10) < 0) {
        perror("Error en listen");
        exit(EXIT_FAILURE);
    }
    
    printf("Servidor iniciado en el puerto %d\n", port);
    
    // Iniciar hilo para verificar inactividad
    if (pthread_create(&inactivity_thread, NULL, check_inactivity, NULL) != 0) {
        perror("Error al crear hilo de verificación de inactividad");
    } else {
        pthread_detach(inactivity_thread);
    }
    
    // Aceptar conexiones entrantes
    while (1) {
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Error en accept");
            continue;
        }
        
        // Crear estructura para el cliente
        client_t *client = malloc(sizeof(client_t));
        client->socket = client_socket;
        client->address = address;
        
        // Crear un hilo para manejar al cliente
        if (pthread_create(&thread_id, NULL, handle_client, (void *)client) != 0) {
            perror("Error al crear hilo");
            close(client_socket);
            free(client);
        } else {
            pthread_detach(thread_id);
        }
    }
    
    // Cerrar el socket del servidor
    close(server_fd);

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}

// Función para manejar conexiones de clientes
void *handle_client(void *arg) {
    client_t *client = (client_t *)arg;
    int socket_fd = client->socket;
    struct sockaddr_in address = client->address;
    char buffer[BUFFER_SIZE] = {0};
    char ip[INET_ADDRSTRLEN];

    // Obtener IP del cliente (inet_ntop está habilitado con _WIN32_WINNT >= 0x0600)
    inet_ntop(AF_INET, &(address.sin_addr), ip, INET_ADDRSTRLEN);
    printf("Nueva conexión desde %s:%d\n", ip, ntohs(address.sin_port));
    
    free(client);
    
    // Leer mensajes del cliente
    while (recv(socket_fd, buffer, BUFFER_SIZE, 0) > 0) {
        // Procesar mensaje JSON
        cJSON *json = cJSON_Parse(buffer);
        if (json == NULL) {
            fprintf(stderr, "Error en JSON\n");
            goto cleanup;
        }
        
        // Obtener tipo de mensaje
        cJSON *tipo = cJSON_GetObjectItemCaseSensitive(json, "tipo");
        cJSON *accion = cJSON_GetObjectItemCaseSensitive(json, "accion");
        
        // Procesar según tipo o acción
        if (tipo != NULL && cJSON_IsString(tipo)) {
            // Registro de usuario
            if (strcmp(tipo->valuestring, "REGISTRO") == 0) {
                cJSON *usuario = cJSON_GetObjectItemCaseSensitive(json, "usuario");
                cJSON *direccionIP = cJSON_GetObjectItemCaseSensitive(json, "direccionIP");
                
                if (usuario != NULL && cJSON_IsString(usuario) &&
                    direccionIP != NULL && cJSON_IsString(direccionIP)) {
                    
                    int result = register_user(usuario->valuestring, ip, socket_fd);
                    
                    // Responder al cliente
                    cJSON *response = cJSON_CreateObject();
                    if (result == 0) {
                        cJSON_AddStringToObject(response, "respuesta", "OK");
                    } else {
                        cJSON_AddStringToObject(response, "respuesta", "ERROR");
                        cJSON_AddStringToObject(response, "razon", "Nombre o dirección duplicado");
                    }
                    
                    char *response_str = cJSON_Print(response);
                    send(socket_fd, response_str, strlen(response_str), 0);
                    
                    free(response_str);
                    cJSON_Delete(response);
                }
            }
            // Salida de usuario
            else if (strcmp(tipo->valuestring, "EXIT") == 0) {
                cJSON *usuario = cJSON_GetObjectItemCaseSensitive(json, "usuario");
                
                if (usuario != NULL && cJSON_IsString(usuario)) {
                    remove_user(usuario->valuestring);
                    
                    // Responder OK
                    cJSON *response = cJSON_CreateObject();
                    cJSON_AddStringToObject(response, "respuesta", "OK");
                    
                    char *response_str = cJSON_Print(response);
                    send(socket_fd, response_str, strlen(response_str), 0);
                    
                    free(response_str);
                    cJSON_Delete(response);
                }
            }
            // Cambio de estado
            else if (strcmp(tipo->valuestring, "ESTADO") == 0) {
                cJSON *usuario = cJSON_GetObjectItemCaseSensitive(json, "usuario");
                cJSON *estado = cJSON_GetObjectItemCaseSensitive(json, "estado");
                
                if (usuario != NULL && cJSON_IsString(usuario) &&
                    estado != NULL && cJSON_IsString(estado)) {
                    
                    int status_code;
                    if (strcmp(estado->valuestring, "ACTIVO") == 0) {
                        status_code = 0;
                    } else if (strcmp(estado->valuestring, "OCUPADO") == 0) {
                        status_code = 1;
                    } else if (strcmp(estado->valuestring, "INACTIVO") == 0) {
                        status_code = 2;
                    } else {
                        status_code = -1;
                    }
                    
                    if (status_code >= 0) {
                        change_user_status(usuario->valuestring, status_code);
                        
                        // Responder OK
                        cJSON *response = cJSON_CreateObject();
                        cJSON_AddStringToObject(response, "respuesta", "OK");
                        
                        char *response_str = cJSON_Print(response);
                        send(socket_fd, response_str, strlen(response_str), 0);
                        
                        free(response_str);
                        cJSON_Delete(response);
                    } else {
                        // Estado inválido
                        cJSON *response = cJSON_CreateObject();
                        cJSON_AddStringToObject(response, "respuesta", "ERROR");
                        cJSON_AddStringToObject(response, "razon", "ESTADO_INVALIDO");
                        
                        char *response_str = cJSON_Print(response);
                        send(socket_fd, response_str, strlen(response_str), 0);
                        
                        free(response_str);
                        cJSON_Delete(response);
                    }
                }
            }
            // Información de usuario
            else if (strcmp(tipo->valuestring, "MOSTRAR") == 0) {
                cJSON *usuario = cJSON_GetObjectItemCaseSensitive(json, "usuario");
                
                if (usuario != NULL && cJSON_IsString(usuario)) {
                    get_user_info(usuario->valuestring, socket_fd);
                }
            }
        } else if (accion != NULL && cJSON_IsString(accion)) {
            // Broadcast
            if (strcmp(accion->valuestring, "BROADCAST") == 0) {
                cJSON *emisor = cJSON_GetObjectItemCaseSensitive(json, "nombre_emisor");
                cJSON *mensaje = cJSON_GetObjectItemCaseSensitive(json, "mensaje");
                
                if (emisor != NULL && cJSON_IsString(emisor) &&
                    mensaje != NULL && cJSON_IsString(mensaje)) {
                    
                    broadcast_message(emisor->valuestring, mensaje->valuestring);
                    
                    // Actualizar última actividad
                    pthread_mutex_lock(&users_mutex);
                    for (int i = 0; i < user_count; i++) {
                        if (strcmp(users[i].username, emisor->valuestring) == 0) {
                            users[i].last_activity = time(NULL);
                            break;
                        }
                    }
                    pthread_mutex_unlock(&users_mutex);
                }
            }
            // Mensaje directo
            else if (strcmp(accion->valuestring, "DM") == 0) {
                cJSON *emisor = cJSON_GetObjectItemCaseSensitive(json, "nombre_emisor");
                cJSON *destinatario = cJSON_GetObjectItemCaseSensitive(json, "nombre_destinatario");
                cJSON *mensaje = cJSON_GetObjectItemCaseSensitive(json, "mensaje");
                
                if (emisor != NULL && cJSON_IsString(emisor) &&
                    destinatario != NULL && cJSON_IsString(destinatario) &&
                    mensaje != NULL && cJSON_IsString(mensaje)) {
                    
                    send_direct_message(emisor->valuestring, destinatario->valuestring, mensaje->valuestring);
                    
                    // Actualizar última actividad
                    pthread_mutex_lock(&users_mutex);
                    for (int i = 0; i < user_count; i++) {
                        if (strcmp(users[i].username, emisor->valuestring) == 0) {
                            users[i].last_activity = time(NULL);
                            break;
                        }
                    }
                    pthread_mutex_unlock(&users_mutex);
                }
            }
            // Lista de usuarios
            else if (strcmp(accion->valuestring, "LISTA") == 0) {
                list_users(socket_fd);
            }
        }
        
        cJSON_Delete(json);
        memset(buffer, 0, BUFFER_SIZE);
    }
    
cleanup:
    // El cliente se desconectó, limpieza
    printf("Cliente desconectado\n");
    
    // Buscar y eliminar usuario por socket
    pthread_mutex_lock(&users_mutex);
    for (int i = 0; i < user_count; i++) {
        if (users[i].socket == socket_fd) {
            printf("Eliminando usuario: %s\n", users[i].username);
            
            // Mover el último usuario a esta posición
            if (i < user_count - 1) {
                users[i] = users[user_count - 1];
            }
            
            // Reducir conteo
            user_count--;
            break;
        }
    }
    pthread_mutex_unlock(&users_mutex);
    
    close(socket_fd);
    return NULL;
}

// Función para verificar inactividad
void *check_inactivity(void *arg) {
    (void)arg;
    while (1) {
        sleep(60); // Verificar cada minuto
        
        time_t current_time = time(NULL);
        
        pthread_mutex_lock(&users_mutex);
        
        for (int i = 0; i < user_count; i++) {
            // Si han pasado más de 5 minutos desde la última actividad
            if (users[i].status != 2 && difftime(current_time, users[i].last_activity) > 300) {
                printf("Usuario %s marcado como INACTIVO por inactividad\n", users[i].username);
                users[i].status = 2; // Marcar como INACTIVO
                
                // Notificar al usuario
                cJSON *json = cJSON_CreateObject();
                cJSON_AddStringToObject(json, "tipo", "ESTADO");
                cJSON_AddStringToObject(json, "usuario", users[i].username);
                cJSON_AddStringToObject(json, "estado", "INACTIVO");
                
                char *json_str = cJSON_Print(json);
                send(users[i].socket, json_str, strlen(json_str), 0);
                
                free(json_str);
                cJSON_Delete(json);
            }
        }
        
        pthread_mutex_unlock(&users_mutex);
    }
    
    return NULL;
}

// Función para registrar un usuario
int register_user(const char *username, const char *ip, int socket) {
    int result = 0;
    
    pthread_mutex_lock(&users_mutex);
    
    // Verificar si el nombre o IP ya existen
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0 || strcmp(users[i].ip, ip) == 0) {
            result = 1; // Error, ya existe
            break;
        }
    }
    
    // Si no existe, agregarlo
    if (result == 0 && user_count < MAX_CLIENTS) {
        strncpy(users[user_count].username, username, sizeof(users[user_count].username) - 1);
        strncpy(users[user_count].ip, ip, sizeof(users[user_count].ip) - 1);
        users[user_count].socket = socket;
        users[user_count].status = 0; // ACTIVO
        users[user_count].last_activity = time(NULL);
        user_count++;
        printf("Usuario registrado: %s (%s)\n", username, ip);
    } else if (user_count >= MAX_CLIENTS) {
        result = 1; // Error, máximo de clientes alcanzado
    }
    
    pthread_mutex_unlock(&users_mutex);
    
    return result;
}

// Función para eliminar un usuario
void remove_user(const char *username) {
    pthread_mutex_lock(&users_mutex);
    
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0) {
            // Mover el último usuario a esta posición
            if (i < user_count - 1) {
                users[i] = users[user_count - 1];
            }
            
            // Reducir conteo
            user_count--;
            printf("Usuario eliminado: %s\n", username);
            break;
        }
    }
    
    pthread_mutex_unlock(&users_mutex);
}

// Función para cambiar estado de usuario
void change_user_status(const char *username, int status) {
    pthread_mutex_lock(&users_mutex);
    
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0) {
            users[i].status = status;
            users[i].last_activity = time(NULL);
            break;
        }
    }
    
    pthread_mutex_unlock(&users_mutex);
}

// Función para transmitir mensaje a todos
void broadcast_message(const char *sender, const char *message) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "accion", "BROADCAST");
    cJSON_AddStringToObject(json, "nombre_emisor", sender);
    cJSON_AddStringToObject(json, "mensaje", message);
    
    char *json_str = cJSON_Print(json);
    
    pthread_mutex_lock(&users_mutex);
    
    for (int i = 0; i < user_count; i++) {
        send(users[i].socket, json_str, strlen(json_str), 0);
    }
    
    pthread_mutex_unlock(&users_mutex);
    
    free(json_str);
    cJSON_Delete(json);
}

// Función para mensaje directo
void send_direct_message(const char *sender, const char *recipient, const char *message) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "accion", "DM");
    cJSON_AddStringToObject(json, "nombre_emisor", sender);
    cJSON_AddStringToObject(json, "nombre_destinatario", recipient);
    cJSON_AddStringToObject(json, "mensaje", message);
    
    char *json_str = cJSON_Print(json);
    
    pthread_mutex_lock(&users_mutex);
    
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, recipient) == 0) {
            send(users[i].socket, json_str, strlen(json_str), 0);
            break;
        }
    }
    
    pthread_mutex_unlock(&users_mutex);
    
    free(json_str);
    cJSON_Delete(json);
}

// Función para listar usuarios
void list_users(int client_socket) {
    cJSON *json = cJSON_CreateObject();
    cJSON *usuarios = cJSON_CreateArray();
    
    cJSON_AddStringToObject(json, "accion", "LISTA");
    
    pthread_mutex_lock(&users_mutex);
    
    for (int i = 0; i < user_count; i++) {
        cJSON_AddItemToArray(usuarios, cJSON_CreateString(users[i].username));
    }
    
    pthread_mutex_unlock(&users_mutex);
    
    cJSON_AddItemToObject(json, "usuarios", usuarios);
    
    char *json_str = cJSON_Print(json);
    send(client_socket, json_str, strlen(json_str), 0);
    
    free(json_str);
    cJSON_Delete(json);
}

// Función para mostrar información de usuario
void get_user_info(const char *username, int client_socket) {
    cJSON *json = cJSON_CreateObject();
    int found = 0;
    
    pthread_mutex_lock(&users_mutex);
    
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0) {
            const char *status_str;
            switch (users[i].status) {
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
                    status_str = "DESCONOCIDO";
                    break;
            }
            
            cJSON_AddStringToObject(json, "tipo", "MOSTRAR");
            cJSON_AddStringToObject(json, "usuario", username);
            cJSON_AddStringToObject(json, "direccionIP", users[i].ip);
            cJSON_AddStringToObject(json, "estado", status_str);
            found = 1;
            break;
        }
    }
    
    pthread_mutex_unlock(&users_mutex);
    
    if (!found) {
        cJSON_Delete(json);
        json = cJSON_CreateObject();
        cJSON_AddStringToObject(json, "respuesta", "ERROR");
        cJSON_AddStringToObject(json, "razon", "USUARIO_NO_ENCONTRADO");
    }
    
    char *json_str = cJSON_Print(json);
    send(client_socket, json_str, strlen(json_str), 0);
    
    free(json_str);
    cJSON_Delete(json);
}
