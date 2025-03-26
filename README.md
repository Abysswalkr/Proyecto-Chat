# Dependencias e Instrucciones para el Proyecto de Chat

## Dependencias de Software

### Bibliotecas y herramientas necesarias:

1. **Compilador C:**
   - GCC (GNU Compiler Collection)

2. **Bibliotecas requeridas:**
   - **pthread**: Para manejo de múltiples hilos  
     ```c
     #include <pthread.h>
     ```
   - **Sockets**: Para comunicación en red  
     ```c
     #include <sys/socket.h>
     #include <netinet/in.h>
     #include <arpa/inet.h>
     ```
   - **cJSON**: Para el procesamiento y generación de mensajes en formato JSON  
     ```c
     #include <cjson/cJSON.h>
     ```
   - **Signal**: Para manejo de señales (por ejemplo, SIGINT) y desconexión limpia  
     ```c
     #include <signal.h>
     ```

3. **Otras bibliotecas estándar:**
   - **stdio.h**: Para operaciones de entrada/salida
   - **stdlib.h**: Para funciones generales
   - **string.h**: Para manipulación de cadenas

4. **Herramientas de construcción:**
   - Make: Para compilación y gestión de proyectos

## Requisitos de Hardware/Red

1. **Cable de Red RJ45** (requerido el día de la entrega)
2. **Conexión a la infraestructura de red** montada en clase

## Instrucciones Clave del Proyecto

### Servidor

1. **Ejecución:**  
   `<nombredelservidor> <puertodelservidor>`  
   - Ejemplo: `./server/servidor 50213`

2. **Funcionalidades obligatorias:**
   - Registro de usuarios (con validación para evitar duplicados)
   - Liberación de usuarios (mecanismo de cierre de sesión y eliminación de la lista)
   - Listado de usuarios conectados
   - Obtención de información de un usuario en específico
   - Manejo de estados (ACTIVO, OCUPADO, INACTIVO) y detección de inactividad
   - Broadcasting (envío de mensajes a todos los clientes)
   - Mensajes directos (envío de mensajes privados entre usuarios)
   - Multithreading para la atención concurrente de clientes
   - Desconexión limpia, implementada también con manejo de señales (SIGINT)

### Cliente

1. **Ejecución:**  
   `<nombredelcliente> <nombredeusuario> <IPdelservidor> <puertodelservidor>`  
   - Ejemplo: `./client/client usuario_ejemplo 127.0.0.1 50213`

2. **Funcionalidades obligatorias:**
   - Registro con el servidor al conectarse
   - Interfaz de comandos en consola con ayuda (comando `/help`)
   - Envío de mensajes al chat general (broadcast)
   - Envío y recepción de mensajes directos (DM)
   - Cambio de estado (ACTIVO, OCUPADO, INACTIVO)
   - Listado de usuarios conectados
   - Visualización de información de un usuario específico
   - Desconexión limpia (incluyendo el manejo de SIGINT para cerrar con Ctrl+C)

## Formato del Protocolo JSON

Se utiliza JSON para estructurar la comunicación entre el servidor y el cliente. A continuación se muestra el formato de los principales mensajes:

### Registro de usuarios
```json
{
  "tipo": "REGISTRO",
  "usuario": "nombre_usuario",
  "direccionIP": "192.168.1.1"
}
```

### Liberación de usuarios
```json
{
  "tipo": "EXIT",
  "usuario": "nombre_usuario",
  "estado": ""
}
```

### Broadcasting
```json
{
  "accion": "BROADCAST",
  "nombre_emisor": "nombre_emisor",
  "mensaje": "mensaje"
}
```

### Mensaje directo (DM)
```json
{
  "accion": "DM",
  "nombre_emisor": "nombre_emisor",
  "nombre_destinatario": "nombre_destinatario",
  "mensaje": "mensaje"
}
```

### Listado de usuarios
```json
{
  "accion": "LISTA",
  "nombre_usuario": "nombre_usuario"
}
```

### Cambio de estado
```json
{
  "tipo": "ESTADO",
  "usuario": "nombre_usuario",
  "estado": "estado"
}
```

### Mostrar información de usuario
```json
{
  "tipo": "MOSTRAR",
  "usuario": "nombre_usuario"
}
```

## Estructura de Archivos Recomendada

```
chat_project/
├── Makefile
├── server/
│   ├── Makefile
│   ├── server.c      // Versión basada en JSON con manejo de hilos y señales
│   └── server.h
├── client/
│   ├── Makefile
│   ├── client.c      // Versión basada en JSON con interfaz de comandos y desconexión limpia
│   └── client.h
└── common/
    ├── protocol.h
    └── protocol.c
```

## Conceptos Técnicos Clave a Implementar

1. **Sockets TCP/IP**: Para la comunicación entre clientes y el servidor.
2. **Multithreading**: Manejo concurrente de conexiones mediante pthread.
3. **Mutex**: Sincronización de acceso a recursos compartidos (lista de usuarios).
4. **Manejo de estados**: Seguimiento del estado de cada usuario (ACTIVO, OCUPADO, INACTIVO).
5. **Manejo de inactividad**: Cambio automático de estado tras un periodo sin actividad.
6. **Parseo y generación de JSON**: Uso de cJSON para estructurar los mensajes.
7. **Interfaz de usuario en terminal**: Comandos para interactuar (broadcast, DM, listar, cambiar estado, ayuda y salir).
8. **Manejo de señales**: Desconexión limpia con SIGINT (Ctrl+C).

## Comandos de Instalación y Ejecución

### Instalación de Dependencias (Debian/Ubuntu/Kali)
```bash
# Actualizar repositorios
sudo apt update

# Instalar herramientas de desarrollo
sudo apt install build-essential make

# Instalar biblioteca cJSON
sudo apt install libcjson-dev
```

### Compilación del Proyecto
```bash
# Compilación completa (suponiendo que se utilice un Makefile en el directorio raíz)
make

# Compilar sólo el servidor
make -C server

# Compilar sólo el cliente
make -C client
```

O, compilando manualmente:

```bash
# Compilar el servidor (versión JSON)
gcc -o servidor server.c -lcjson -lpthread

# Compilar el cliente (versión JSON)
gcc -o client client.c -lcjson -lpthread
```

### Ejecución
```bash
# Ejecutar el servidor en el puerto 50213
./server/servidor 50213

# Ejecutar el cliente (en terminales separadas)
./client/client usuario_ejemplo 127.0.0.1 50213
```

## Uso y Pruebas

1. **Registro y Conexión:**  
   Al iniciar el cliente, se registra automáticamente con el servidor enviando un mensaje JSON con el nombre de usuario y una IP genérica (el servidor detecta la IP real).

2. **Envío de Mensajes:**  
   - **Broadcast:** Escribe un mensaje sin prefijo o utiliza `/broadcast <mensaje>` para enviarlo a todos.
   - **Mensaje Directo (DM):** Usa `/dm <usuario> <mensaje>` para enviar un mensaje privado.
   - **Listar usuarios:** Ejecuta `/list` para ver la lista de usuarios conectados.
   - **Mostrar información:** Usa `/info <usuario>` para obtener detalles de un usuario.
   - **Cambio de estado:** Emplea `/status <0|1|2>` para cambiar tu estado (0: ACTIVO, 1: OCUPADO, 2: INACTIVO).

3. **Desconexión:**  
   Utiliza `/exit` o presiona Ctrl+C (SIGINT) para una desconexión limpia del servidor.

4. **Verificación:**  
   Se recomienda probar con múltiples clientes para asegurar que la comunicación (broadcast y mensajes directos) funcione correctamente.

