# Proyecto 1: Chat Multiusuario estilo IRC con Protocolo de Texto

Este proyecto implementa un sistema de chat multiusuario concurrente utilizando sockets TCP en C++. Permite la comunicación entre usuarios mediante canales y mensajes privados, incorpora buzones persistentes para usuarios desconectados y ofrece una puerta HTTP para consultar el estado del chat.

## 1. Dependencias

Para compilar y ejecutar el proyecto se requiere:

- Sistema operativo Linux.
- Compilador `g++` con soporte para C++17.
- Herramienta `make`.
- Bibliotecas estándar de C++ y sockets POSIX.

## 2. Compilación

Desde la carpeta raíz del proyecto, ejecutar:

```bash
make
```

Esto compila el código fuente y genera los ejecutables `servidor` y `cliente`.

## 3. Ejecución

### Servidor

Formato de ejecución:

```bash
./servidor --chat <puerto_chat> --http <puerto_http> --buzones <carpeta>
```

Ejemplo:

```bash
./servidor --chat 5000 --http 8080 --buzones ./buzones
```

El servidor escucha las conexiones del chat en el puerto 5000 y las peticiones HTTP en el puerto 8080.

La carpeta `buzones` se utiliza para almacenar los mensajes privados enviados a usuarios desconectados.

### Cliente

Formato de ejecución:

```bash
./cliente <host> <puerto>
```

Ejemplo:

```bash
./cliente localhost 5000
```

El cliente se conecta al servidor y permite enviar comandos desde la terminal.

## 4. Comandos del chat

| Comando | Descripción |
|---|---|
| `NICK <apodo>` | Registrar un apodo. |
| `JOIN <#canal>` | Entrar a un canal o crearlo. |
| `PART <#canal>` | Salir de un canal. |
| `MSG <#canal> <texto>` | Enviar un mensaje al canal. |
| `PRIV <apodo> <texto>` | Enviar un mensaje privado. |
| `CHANS` | Mostrar los canales disponibles. |
| `WHO <#canal>` | Mostrar los usuarios de un canal. |
| `STAT` | Consultar la cantidad y el tamaño de los mensajes del buzón. |
| `LIST` | Listar los mensajes del buzón. |
| `RETR <n>` | Recuperar un mensaje del buzón. |
| `DELE <n>` | Marcar un mensaje para eliminarlo. |
| `QUIT` | Desconectarse y aplicar los borrados pendientes. |

### Ejemplo de uso

```text
NICK moises
JOIN #voley
MSG #voley Hola a todos
WHO #voley
CHANS
QUIT
```

## 5. Puerta HTTP

El servidor dispone de una interfaz HTTP/1.1 de solo lectura que permite consultar información del chat desde un navegador o mediante `curl`.

### Consultar usuarios conectados

```bash
curl -v http://localhost:8080/usuarios
```

### Consultar canales disponibles

```bash
curl http://localhost:8080/canales
```

### Consultar el historial de un canal

```bash
curl http://localhost:8080/historial/voley
```

El historial contiene los últimos 50 mensajes registrados en el canal.

## 6. Estructura del proyecto

```text
proyecto1/
├── Makefile
├── README.md
├── PROTOCOLO.md
├── captura.pcapng
└── src/
    ├── servidor.cpp
    ├── cliente.cpp
    ├── comun/
    │   ├── protocolo.h
    │   └── protocolo.cpp
    └── servidor/
        ├── estado.h
        ├── buzon.h
        ├── buzon.cpp
        ├── http.h
        └── http.cpp
```

El proyecto también genera los ejecutables `servidor` y `cliente`, así como archivos objeto durante la compilación.

## 7. Documentación y pruebas

- **PROTOCOLO.md:** especificación de los comandos, respuestas, códigos de error y eventos del protocolo.
- **captura.pcapng:** captura del tráfico de red realizada con Wireshark.
- **Informe del proyecto:** descripción del modelo de concurrencia, delimitación de mensajes y limitaciones conocidas.

El protocolo utiliza texto UTF-8 y mensajes terminados en CRLF (`\r\n`). El servidor implementa concurrencia mediante hilos y mecanismos de exclusión mutua para proteger el estado compartido.

## 8. Tecnologías utilizadas

- C++17.
- API de sockets POSIX.
- TCP.
- `std::thread` y `std::mutex`.
- HTTP/1.1.
- Formato RFC 5322 para los mensajes almacenados.
- Wireshark para el análisis del tráfico de red.
