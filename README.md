A continuación se muestra un ejemplo actualizado del README.md que refleja las modificaciones realizadas para que el proyecto se compile y ejecute en Windows, incluyendo las instrucciones para compilar, ejecutar y conectar a los usuarios.

---

```markdown
# Proyecto de Chat - Versión Windows

Este proyecto de chat implementa un sistema de mensajería en C utilizando sockets, multithreading y JSON para la comunicación entre clientes y el servidor. Se han realizado modificaciones para que el proyecto se compile y ejecute nativamente en Windows usando MinGW, Winsock y pthreads (para Windows).

## Dependencias de Software

### Herramientas y Librerías Necesarias

1. **MinGW**  
   - Asegúrate de tener MinGW instalado y configurado en tu PATH.
   - Se usará `gcc` para compilar el código.

2. **Librería cJSON**  
   - El proyecto incluye (o debe incluir) los archivos `cJSON.c` y `cJSON.h` (o bien, la carpeta CJSON).  
   - Alternativamente, puedes descargar cJSON desde [https://github.com/DaveGamble/cJSON](https://github.com/DaveGamble/cJSON).

3. **Pthreads para Windows**  
   - Se utiliza pthreads para la gestión de hilos. Si tu MinGW ya lo incluye, no es necesario nada adicional; de lo contrario, instala una implementación como [pthreads-win32](https://sourceforge.net/projects/pthreads4w/).

4. **Winsock (ws2_32)**  
   - La API de sockets de Windows (Winsock) está integrada, pero debes enlazar la librería `ws2_32` al compilar.

## Configuración de Visual Studio Code (Opcional)

Si usas VS Code, la carpeta `.vscode` en el repositorio contiene archivos de configuración para facilitar el IntelliSense y la depuración:

- **.vscode/c_cpp_properties.json:** Configura la ruta del compilador y las rutas de inclusión.
- **.vscode/settings.json:** Opciones generales para el entorno de desarrollo.
- **.vscode/launch.json:** Configuraciones para depurar tanto el servidor como el cliente.

## Estructura del Proyecto

```
chat_project/
├── cJSON.c
├── cJSON.h
├── server.c       // Componente del servidor (para Windows)
├── client.c       // Componente del cliente (para Windows)
├── README.md
└── .vscode/
    ├── c_cpp_properties.json
    ├── settings.json
    └── launch.json
```

## Compilación en Windows

Abre una terminal (por ejemplo, PowerShell) y, desde la carpeta del proyecto, ejecuta los siguientes comandos:

- **Para compilar el servidor:**

  ```
  gcc server.c cJSON.c -o server.exe -lpthread -lws2_32
  ```

- **Para compilar el cliente:**

  ```
  gcc client.c cJSON.c -o client.exe -lpthread -lws2_32
  ```

> **Nota:**  
> Si usas PowerShell, para ejecutar el ejecutable desde la carpeta actual, antepone `.\` al nombre del archivo (por ejemplo, `.\server.exe`).

## Ejecución del Proyecto

1. **Levantar el Servidor:**  
   En una terminal, ejecuta:
   
   ```
   .\server.exe 50213
   ```
   
   Esto levantará el servidor en el puerto 50213.

2. **Conectar Clientes:**  
   En terminales separadas, ejecuta para cada cliente (por ejemplo, para "usuario1" y "usuario2"):

   ```
   .\client.exe usuario1 127.0.0.1 50213
   ```
   
   y
   
   ```
   .\client.exe usuario2 127.0.0.1 50213
   ```

   Cada cliente se conectará al servidor que está corriendo en `127.0.0.1` en el puerto 50213.

## Uso del Chat

Una vez conectados, los usuarios pueden interactuar utilizando comandos:

- **Broadcast:**  
  Escribe un mensaje sin prefijo o utiliza:
  
  ```
  /broadcast <mensaje>
  ```
  
- **Mensaje Directo (DM):**  
  Para enviar un mensaje privado, utiliza:
  
  ```
  /dm <usuario> <mensaje>
  ```

- **Listar Usuarios:**  
  Para ver la lista de usuarios conectados:
  
  ```
  /list
  ```

- **Información de Usuario:**  
  Para obtener información de un usuario:
  
  ```
  /info <usuario>
  ```

- **Cambio de Estado:**  
  Cambia tu estado (0 para ACTIVO, 1 para OCUPADO, 2 para INACTIVO) usando:
  
  ```
  /status <0|1|2>
  ```

- **Salir:**  
  Para desconectarte del chat:
  
  ```
  /exit
  ```
  
  O simplemente presiona Ctrl+C.

## Consideraciones Finales

- Asegúrate de tener instaladas todas las dependencias (MinGW, cJSON, pthreads para Windows).  
- Si usas VS Code, abre la carpeta del proyecto y utiliza las configuraciones de `.vscode` para compilar y depurar.  
- Los comandos para compilar y ejecutar se deben correr desde la carpeta donde se encuentran los archivos fuente.

Con estos pasos, tu proyecto de chat se podrá compilar y ejecutar en Windows sin necesidad de una máquina virtual.

---

```

Este README actualizado refleja todos los cambios realizados para la compilación y ejecución en Windows, junto con los comandos necesarios y la estructura del proyecto.

