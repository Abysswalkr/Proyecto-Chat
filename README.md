# Dependencias e Instrucciones para el Proyecto de Chat

## Dependencias de Software

### Bibliotecas y herramientas necesarias:

1. **Compilador C:**
   - GCC (GNU Compiler Collection)

2. **Bibliotecas requeridas:**
   - **pthread**: Para manejar múltiples hilos
     ```c
     #include <pthread.h>
     ```
   - **socket**: Para la comunicación en red
     ```c
     #include <sys/socket.h>
     #include <netinet/in.h>
     #include <arpa/inet.h>
     ```
   - **cJSON**: Para procesamiento de JSON
     ```c
     #include <cjson/cJSON.h>
     ```

3. **Otras bibliotecas estándar:**
   - **stdio.h**: Para operaciones de entrada/salida
   - **stdlib.h**: Para funciones generales
   - **string.h**: Para manipulación de strings

4. **Herramientas de construcción:**
   - Make: Para compilación y gestión

## Requisitos de Hardware/Red

1. **Cable de Red RJ45** (mencionado explícitamente para el día de la entrega)
2. **Conexión a la infraestructura de red** que se montará en clase

## Instrucciones Clave del Proyecto

### Servidor

1. **Ejecución:** `<nombredelservidor> <puertodelservidor>`
   - Puerto especificado: 50213

2. **Funcionalidades obligatorias:**
   - Registro de usuarios (validación de duplicados)
   - Liberación de usuarios (cierre de sesión)
   - Listado de usuarios conectados
   - Obtención de información de usuario específico
   - Manejo de status (ACTIVO, OCUPADO, INACTIVO)
   - Broadcasting (mensajes globales)
   - Mensajes directos (privados)
   - Multithreading para manejo de conexiones

### Cliente

1. **Ejecución:** `<nombredelcliente> <nombredeusuario> <IPdelservidor> <puertodelservidor>`

2. **Funcionalidades obligatorias:**
   - Chatear con todos los usuarios (broadcasting)
   - Enviar/recibir mensajes directos
   - Cambiar de status
   - Listar usuarios conectados
   - Mostrar información de usuario específico
   - Ayuda (comandos disponibles)
   - Salir (cerrar conexión)

## Formato del Protocolo JSON

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
│   ├── server.c
│   └── server.h
├── client/
│   ├── Makefile
│   ├── client.c
│   └── client.h
└── common/
    ├── protocol.h
    └── protocol.c
```

## Conceptos Técnicos Clave a Implementar

1. **Sockets TCP/IP**: Para comunicación cliente-servidor
2. **Multithreading**: Para manejar múltiples clientes concurrentemente
3. **Mutex**: Para sincronización de acceso a recursos compartidos
4. **Manejo de estados**: Seguimiento del estado de usuarios
5. **Parseo y generación de JSON**: Para formatear mensajes
6. **Interfaz de usuario en terminal**: Para interacción con el cliente

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
# Compilación completa
make

# Compilar sólo servidor
make -C server

# Compilar sólo cliente
make -C client
```

### Ejecución
```bash
# Ejecutar servidor (puerto 50213)
./server/server 50213

# Ejecutar cliente
./client/client usuario_ejemplo 127.0.0.1 50213
```
