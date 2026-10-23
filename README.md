
# Proyecto 1: Chat Multiusuario estilo IRC con Protocolo de Texto

**Asignatura:** CI4835 — Redes de Computadoras I  
**Universidad:** Universidad Simón Bolívar

**Integrantes:**
- Moisés Blanco — 21-10074
- Sebastián Moglia — 18-10254

Este proyecto implementa un sistema de chat multiusuario concurrente utilizando sockets TCP en C++. Permite la comunicación entre usuarios mediante canales y mensajes privados, incorpora buzones persistentes para usuarios desconectados y ofrece una puerta HTTP para consultar información del chat.

## 1. Dependencias

Para compilar y ejecutar el proyecto se requiere:

- Sistema operativo Linux.
- Compilador `g++` con soporte para C++17.
- Herramienta `make`.
- Bibliotecas estándar de C++ y API de sockets POSIX.

El proyecto no requiere bibliotecas externas para su funcionamiento.

## 2. Compilación

Desde la raíz del repositorio, ejecutar:

```bash
make
```

Esto compila el código fuente y genera los ejecutables `servidor` y `cliente`.

Durante la compilación también se crea la carpeta `obj/`, donde se almacenan los archivos objeto.

Para eliminar los archivos generados durante la compilación, ejecutar:

```bash
make clean
```

Los ejecutables y la carpeta `obj/` están excluidos del repositorio mediante `.gitignore`.

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

El servidor utiliza dos puertos:

- **Puerto 5000:** recibe las conexiones TCP de los clientes del chat.
- **Puerto 8080:** recibe las peticiones HTTP para consultar información del servidor.

La carpeta `buzones/` almacena los mensajes privados enviados a usuarios conocidos que están desconectados. Si la carpeta no existe, el servidor la crea automáticamente.

Los mensajes se guardan en archivos `.eml` con formato RFC 5322 y permanecen disponibles después de reiniciar el servidor.

### Cliente

Formato de ejecución:

```bash
./cliente <host> <puerto>
```

Ejemplo:

```bash
./cliente localhost 5000
```

El cliente establece una conexión TCP con el servidor y permite introducir comandos desde la terminal.

También puede recibir mensajes de otros usuarios mientras se escriben comandos.

Para probar la comunicación entre varios usuarios, se pueden abrir diferentes terminales y ejecutar el cliente en cada una.

## 4. Comandos del chat

| Comando | Descripción |
|---|---|
| `NICK <apodo>` | Registrar un apodo. |
| `JOIN <#canal>` | Entrar a un canal o crearlo si no existe. |
| `PART <#canal>` | Salir de un canal. |
| `MSG <#canal> <texto>` | Enviar un mensaje a los miembros de un canal. |
| `PRIV <apodo> <texto>` | Enviar un mensaje privado a otro usuario. |
| `CHANS` | Consultar los canales disponibles. |
| `WHO <#canal>` | Consultar los miembros de un canal. |
| `STAT` | Consultar la cantidad y el tamaño de los mensajes del buzón. |
| `LIST` | Listar los mensajes almacenados en el buzón. |
| `RETR <n>` | Recuperar el contenido de un mensaje del buzón. |
| `DELE <n>` | Marcar un mensaje para eliminarlo. |
| `QUIT` | Desconectarse y aplicar los borrados pendientes. |

Antes de utilizar los comandos del chat, el usuario debe registrarse mediante `NICK`. Mientras no se haya registrado, solamente se permiten `NICK` y `QUIT`.

### Ejemplo de uso

```text
NICK moises
JOIN #voley
MSG #voley Hola a todos
WHO #voley
CHANS
QUIT
```

Los apodos pueden tener entre 1 y 16 caracteres de `[A-Za-z0-9_-]`.

Los canales deben comenzar con `#` y tener entre 1 y 32 caracteres de `[a-z0-9_-]` después de ese símbolo.

### Mensajes privados y buzones

Cuando se utiliza `PRIV`, el servidor comprueba si el destinatario está conectado.

- Si está conectado, recibe el mensaje directamente.
- Si está desconectado, pero es un usuario conocido, el mensaje se guarda en su buzón.
- Si el destinatario es desconocido, el servidor responde con el error correspondiente.

Los usuarios pueden consultar los mensajes almacenados mediante `STAT` y `LIST`, recuperarlos con `RETR` y marcarlos para eliminar mediante `DELE`.

Los mensajes marcados se eliminan definitivamente al ejecutar `QUIT`. Si la conexión termina inesperadamente, los borrados pendientes no se aplican.

## 5. Puerta HTTP

El servidor dispone de una interfaz HTTP/1.1 de solo lectura que permite consultar información del chat desde un navegador o mediante `curl`.

Solamente se admiten peticiones `GET`.

### Consultar usuarios conectados

```bash
curl -i http://localhost:8080/usuarios
```

Devuelve los apodos de los usuarios conectados.

### Consultar canales disponibles

```bash
curl -i http://localhost:8080/canales
```

Muestra los canales existentes y su cantidad de miembros.

### Consultar el historial de un canal

```bash
curl -i http://localhost:8080/historial/voley
```

Muestra los últimos 50 mensajes registrados en el canal `#voley`, incluyendo la hora, el apodo y el contenido de cada mensaje.

En la dirección HTTP se utiliza el nombre del canal sin el símbolo `#`.

### Respuestas HTTP

El servidor utiliza los siguientes códigos:

- `200 OK`: petición realizada correctamente.
- `400 Bad Request`: petición mal formada.
- `404 Not Found`: ruta o canal inexistente.
- `405 Method Not Allowed`: método HTTP no permitido.

Todas las respuestas incluyen las cabeceras:

- `Content-Type: text/plain; charset=utf-8`
- `Content-Length`
- `Connection: close`

Las respuestas `405` también incluyen `Allow: GET`.

## 6. Estructura del proyecto

Los archivos se encuentran directamente en la raíz del repositorio:

```text
.
├── .gitignore
├── Makefile
├── README.md
├── PROTOCOLO.md
├── informe.pdf
├── captura.pcapng
└── src/
    ├── servidor.cpp
    ├── cliente.cpp
    ├── cliente/
    │   ├── ui.cpp
    │   └── ui.h
    ├── comun/
    │   ├── protocolo.h
    │   └── protocolo.cpp
    └── servidor/
        ├── estado.h
        ├── estado.cpp
        ├── conexion.h
        ├── conexion.cpp
        ├── buzon.h
        ├── buzon.cpp
        ├── http.h
        └── http.cpp
```

Al compilar, se generan los ejecutables `servidor` y `cliente`, además de la carpeta `obj/`.

Durante la ejecución también puede crearse la carpeta `buzones/`. Estos archivos y carpetas generados no se incluyen en el repositorio.

## 7. Documentación y pruebas

El proyecto incluye los siguientes archivos de documentación y entrega:

- **`PROTOCOLO.md`:** especificación de los comandos, respuestas, códigos de error y eventos del protocolo.
- **`informe.pdf`:** informe técnico que explica el modelo de concurrencia, la delimitación de mensajes TCP, las limitaciones conocidas y el uso de herramientas de inteligencia artificial durante el desarrollo.
- **`captura.pcapng`:** captura del tráfico de red realizada con TShark, compatible con Wireshark.

### Pruebas realizadas

Se realizaron pruebas utilizando varios clientes conectados al servidor y herramientas como Python, `curl` y TShark.

Entre las principales pruebas se encuentran:

1. **Compilación:** ejecución de `make` y `make clean` desde la raíz del repositorio.
2. **Registro y canales:** funcionamiento de `NICK`, `JOIN`, `WHO` y `CHANS`.
3. **Mensajes:** envío de mensajes mediante `MSG` y `PRIV` entre usuarios conectados.
4. **Buzones:** almacenamiento de mensajes para usuarios desconectados y recuperación mediante `STAT`, `LIST` y `RETR`.
5. **Borrado diferido:** funcionamiento de `DELE` y `QUIT`, incluyendo la conservación de mensajes ante desconexiones inesperadas.
6. **Puerta HTTP:** consultas a las tres rutas disponibles y verificación de los códigos `200`, `400`, `404` y `405`.
7. **Fragmentación TCP:** envío de comandos divididos en varias partes y de varios comandos en una misma transmisión.
8. **Límite de longitud:** aceptación de líneas de 512 bytes y rechazo de líneas de 513 bytes.
9. **Líneas excesivamente largas:** envío de una línea de 10 MB y comprobación de que el servidor continúa procesando comandos.
10. **Concurrencia:** prueba de 200 conexiones simultáneas.
11. **Clientes lentos:** envío de 3000 mensajes mientras un cliente dejaba de leer, comprobando que otros usuarios podían continuar utilizando el servidor.
12. **Captura de tráfico:** registro de comandos y respuestas del chat, recuperación del buzón y una petición HTTP.

Las pruebas realizadas permiten comprobar el funcionamiento de las principales características del proyecto, aunque no garantizan la ausencia de errores en todos los casos posibles.

## 8. Tecnologías utilizadas

- C++17.
- API de sockets POSIX.
- Protocolo TCP.
- `std::thread` y `std::mutex`.
- `poll()` para atender las entradas del cliente.
- HTTP/1.1.
- Formato RFC 5322 para los mensajes almacenados.
- Git y GitHub para el control de versiones.
- TShark y Wireshark para el análisis del tráfico de red.

## 9. Funcionamiento general del protocolo

El protocolo del chat utiliza texto codificado en UTF-8 y líneas terminadas en CRLF (`\r\n`).

El servidor identifica los comandos completos mediante esta terminación y mantiene los datos parciales en un búfer cuando recibe información fragmentada.

Cada línea enviada por el cliente tiene un límite máximo de 512 bytes, incluyendo el CRLF. Si se supera este límite, el servidor responde con el código `502` y descarta la línea correspondiente.

El servidor utiliza hilos para atender múltiples conexiones y mecanismos de exclusión mutua para proteger el acceso al estado compartido.

El historial de cada canal se mantiene en memoria y conserva los últimos 50 mensajes, que pueden consultarse mediante HTTP.

Para más información sobre los comandos, eventos y códigos de respuesta, consultar `PROTOCOLO.md`.
