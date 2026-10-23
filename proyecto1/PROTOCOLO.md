
# Especificación del Protocolo de Chat (Estilo RFC)

## 1. Reglas Generales

- **Transporte:** TCP.
- **Codificación:** UTF-8.
- **Terminación de línea:** Todas las líneas (tanto del cliente como del servidor) terminan en `\r\n` (CRLF).
- **Límite de línea:** Una línea enviada por el cliente mide como máximo 512 bytes, incluido el CRLF. Si se supera, el servidor responde `502` y descarta la entrada hasta el siguiente CRLF. Las líneas enviadas por el servidor pueden ser más largas.
- **Comandos:** No distinguen mayúsculas de minúsculas (`NICK` es igual a `nick`). Los argumentos se separan por un espacio. El último argumento `<texto>` puede contener espacios.
- **Apodos:** De 1 a 16 caracteres en `[A-Za-z0-9_-]`.
- **Canales:** Comienzan con `#` seguido de 1 a 32 caracteres en `[a-z0-9_-]`.
- **Formato de respuesta:** `ddd texto`, donde `ddd` es un código de tres dígitos.
- **Respuestas multilínea:** Siguen la convención de SMTP. Todas las líneas intermedias usan `ddd-texto`, y la última línea usa `ddd texto`.
- **Saludo inicial:** Al conectarse, el servidor envía `200 <nombre> listo`, donde `<nombre>` identifica al servidor.
- **Restricción de registro:** Antes de un `NICK` aceptado, solo se admiten los comandos `NICK` y `QUIT`. Cualquier otro comando recibe `530`.

## 2. Gramática de los Comandos

| Comando | Sintaxis | Descripción | Respuesta |
|---|---|---|---|
| **NICK** | `NICK <apodo>` | Registra el apodo. | `201 Bienvenido, <apodo>` |
| **JOIN** | `JOIN <#canal>` | Entra al canal (lo crea si no existe). | `210 Unida a <#canal>` |
| **PART** | `PART <#canal>` | Sale del canal. | `211 Saliste de <#canal>` |
| **MSG** | `MSG <#canal> <texto>` | Envía un mensaje al canal. | `250 OK` |
| **PRIV** | `PRIV <apodo> <texto>` | Envía un mensaje privado. | `250` (conectado) / `251` (guardado) |
| **CHANS** | `CHANS` | Lista canales y cantidad de miembros. | `212` (multilínea) |
| **WHO** | `WHO <#canal>` | Lista miembros de un canal. | `213` (multilínea) |
| **STAT** | `STAT` | Cantidad de mensajes y bytes en buzón. | `220 <n> <bytes>` |
| **LIST** | `LIST` | Lista mensajes del buzón con su tamaño. | `221` (multilínea) |
| **RETR** | `RETR <n>` | Devuelve el mensaje `n` (RFC 5322). | `222` (multilínea) |
| **DELE** | `DELE <n>` | Marca el mensaje `n` para borrar. | `223` |
| **QUIT** | `QUIT` | Aplica borrados pendientes y cierra. | `299 Adiós` |

## 3. Tabla de Códigos de Respuesta

### 3.1. Éxito (2xx)

- `200`: Saludo del servidor al conectar.
- `201`: `NICK` aceptado.
- `210`: `JOIN` aceptado.
- `211`: `PART` aceptado.
- `212`: Respuesta multilínea de `CHANS`. Se envía una línea `212-<#canal> <n>` por cada canal y una última línea `212 <total> canales`, donde `n` es la cantidad de miembros y `total` es la cantidad de canales.
- `213`: Respuesta multilínea de `WHO`. Se envía una línea `213-<apodo>` por cada miembro del canal y una última línea `213 <n> miembros`, donde `n` es la cantidad total de miembros.
- `220`: Respuesta de `STAT`.
- `221`: Respuesta multilínea de `LIST`. Se envía una línea `221-<número> <bytes>` por cada mensaje guardado y una última línea `221 <n> mensajes`, donde `n` es la cantidad total de mensajes.
- `222`: Respuesta multilínea de `RETR`. El servidor devuelve el mensaje completo en formato RFC 5322. Cada línea del mensaje comienza con `222-` y la última respuesta es `222 fin del mensaje`.
- `223`: `DELE` aceptado.
- `250`: Mensaje entregado.
- `251`: Mensaje guardado en buzón (usuario desconectado).
- `299`: Respuesta a `QUIT`.

### 3.2. Eventos Asíncronos (3xx)

- `300 <#canal> <apodo>: <texto>`: Mensaje publicado en un canal.
- `301 <apodo> <texto>`: Mensaje privado recibido en vivo.
- `302 <#canal> <apodo> <evento>`: Aviso de entrada, salida o desconexión.
- `303 <n>`: Aviso de que el usuario tiene `n` mensajes en su buzón.

### 3.3. Errores Temporales (4xx)

- `421`: Servidor saturado; la conexión se cerrará.
- `433`: Apodo en uso por otro usuario conectado.
- `442`: No eres miembro de ese canal.

### 3.4. Errores Permanentes (5xx)

- `500`: Comando desconocido.
- `501`: Sintaxis inválida (argumentos faltantes o mal formados).
- `502`: Línea demasiado larga (se descarta hasta el siguiente CRLF).
- `530`: Primero debes registrarte con `NICK`.
- `550`: Usuario desconocido.
- `551`: Canal inexistente.
- `552`: No existe ese número de mensaje en el buzón.

## 4. Extensiones Propias y Casos Borde

- **Formato de apodos:** En los eventos `300`, `301` y `302`, el servidor envía los apodos sin corchetes. En el evento `300`, se agregan dos puntos después del apodo, como indica el protocolo.
- **Historial de canal:** El servidor mantiene en memoria los últimos 50 mensajes de cada canal. Se puede consultar mediante la puerta HTTP (`GET /historial/<canal>`), pero no existe un comando de texto para ello en el protocolo base.
- **Persistencia del buzón:** Los mensajes privados enviados a usuarios conocidos que están desconectados se guardan en disco con formato RFC 5322. Cada mensaje incluye las cabeceras `From`, `To`, `Date`, `Subject` y `Message-ID`, una línea vacía y el cuerpo del mensaje. Las líneas terminan en CRLF.
- **Extensión de los archivos:** Se utiliza la extensión `.eml` para identificar los mensajes almacenados. Esta es una decisión de implementación del equipo.
- **Eliminación de mensajes:** El comando `DELE` solamente marca los mensajes para borrarlos. La eliminación se hace efectiva al recibir `QUIT`. Si la conexión se corta inesperadamente, los mensajes no se borran.
- **Numeración del buzón:** Los números asignados a los mensajes permanecen estables durante la sesión del usuario.
- **Persistencia entre reinicios:** Los mensajes almacenados deben mantenerse disponibles aunque el servidor se cierre y vuelva a iniciarse.
- **`PRIV` a uno mismo:** Si un usuario envía un mensaje privado a su propio apodo, el servidor lo procesa como un mensaje privado normal y lo entrega al usuario conectado, sin aplicar una regla especial de rechazo.

## 5. Puerta HTTP

El servidor tiene un segundo puerto para consultar información del chat mediante HTTP/1.1. Solo permite peticiones GET.

### Rutas disponibles

- `GET /usuarios`: Muestra los usuarios conectados, uno por línea.
- `GET /canales`: Muestra los canales disponibles y su cantidad de miembros.
- `GET /historial/<canal>`: Muestra los últimos 50 mensajes de un canal, con hora, apodo y texto. En la ruta no se incluye el `#`.

### Códigos de respuesta

- `200`: Petición realizada correctamente.
- `400`: Petición mal formada.
- `404`: Ruta o canal inexistente.
- `405`: Método no permitido.

### Cabeceras HTTP

Todas las respuestas incluyen:

- `Content-Type: text/plain; charset=utf-8`
- `Content-Length`: Tamaño del cuerpo en bytes.
- `Connection: close`

Cuando el método no es GET, también se incluye `Allow: GET`.

El servidor lee la petición hasta encontrar `\r\n\r\n` y cierra la conexión después de responder.

## 6. Ejemplo de Sesión

Este ejemplo muestra una conexión al servidor y el uso de algunos comandos.

Las líneas `C:` son enviadas por el cliente y las líneas `S:` son respuestas del servidor.

```text
S: 200 servidor-prueba listo
C: NICK ana
S: 201 Bienvenido, ana
C: JOIN #general
S: 210 Unida a #general
S: 302 #general ana entró
C: CHANS
S: 212-#general 1
S: 212 1 canales
C: WHO #general
S: 213-ana
S: 213 1 miembros
C: MSG #general Hola a todos
S: 250 OK
C: QUIT
S: 299 Adiós
```

Cuando un usuario envía un mensaje a un canal, los demás miembros reciben un evento `300`. Por ejemplo:

```text
300 #general ana: Hola a todos
```

El usuario que envía el mensaje recibe `250 OK`, pero no recibe su propio evento `300`.

### Ejemplo de Mensaje Privado

Si el usuario `ana` envía un mensaje privado a `pedro`, que está conectado:

```text
C: PRIV pedro Hola Pedro
S: 250 Mensaje entregado
```

El usuario `pedro` recibe:

```text
301 ana Hola Pedro
```

Si `pedro` está desconectado, pero ya se había registrado anteriormente, el mensaje se guarda en su buzón y el emisor recibe una respuesta `251`.

### Ejemplo de Consulta del Buzón

```text
C: STAT
S: 220 1 150
C: LIST
S: 221-1 150
S: 221 1 mensajes
C: RETR 1
S: 222-From: ana@chat.ci4835
S: 222-To: pedro@chat.ci4835
S: 222-Date: Thu, 15 Oct 2026 10:15:02 -0400
S: 222-Subject: Mensaje privado
S: 222-Message-ID: <12345@chat.ci4835>
S: 222-
S: 222-Hola Pedro
S: 222 fin del mensaje
C: DELE 1
S: 223 Mensaje 1 marcado para borrar
C: QUIT
S: 299 Adiós
```

Este ejemplo utiliza tamaños y datos ilustrativos. Los valores reales dependen de cada mensaje guardado.

Todas las líneas del protocolo terminan en `\r\n`, aunque no se muestre en los ejemplos.
