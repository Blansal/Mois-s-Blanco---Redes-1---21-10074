# Especificación del Protocolo de Chat (Estilo RFC)


## 1. Reglas Generales
- **Transporte:** TCP.
- **Codificación:** UTF-8.
- **Terminación de línea:** Todas las líneas (tanto del cliente como del servidor) terminan en `\r\n` (CRLF).
- **Límite de línea:** Una línea enviada por el cliente mide como máximo 512 bytes, incluido el CRLF. Si se supera, el servidor responde `502` y descarta la entrada hasta el siguiente CRLF.
- **Comandos:** No distinguen mayúsculas de minúsculas (`NICK` es igual a `nick`). Los argumentos se separan por un espacio. El último argumento `<texto>` puede contener espacios.
- **Apodos:** De 1 a 16 caracteres en `[A-Za-z0-9_-]
- **Canales:** Comienzan con `#` seguido de 1 a 32 caracteres en `[a-z0-9_-]`.
- **Formato de respuesta:** `ddd texto`, donde `ddd` es un código de tres dígitos.
- **Respuestas multilínea:** Siguen la convención de SMTP. Todas las líneas intermedias usan `ddd-texto`, y la última línea usa `ddd texto`.
- **Saludo inicial:** Al conectarse, el servidor envía `200 <nombre> listo`.
- **Restricción de registro:** Antes de un `NICK` aceptado, solo se admiten los comandos `NICK` y `QUIT`. Cualquier otro comando recibe `530`.

## 3. Gramática de los Comandos

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

## 4. Tabla de Códigos de Respuesta

### 4.1. Éxito (2xx)
- `200`: Saludo del servidor al conectar.
- `201`: `NICK` aceptado.
- `210`: `JOIN` aceptado.
- `211`: `PART` aceptado.
- `212`: Respuesta multilínea de `CHANS`.
- `213`: Respuesta multilínea de `WHO`.
- `220`: Respuesta de `STAT`.
- `221`: Respuesta multilínea de `LIST`.
- `222`: Respuesta multilínea de `RETR`.
- `223`: `DELE` aceptado.
- `250`: Mensaje entregado.
- `251`: Mensaje guardado en buzón (usuario desconectado).
- `299`: Respuesta a `QUIT`.

### 4.2. Eventos Asíncronos (3xx)
- `300 <#canal> <apodo> <texto>`: Mensaje publicado en un canal.
- `301 <apodo> <texto>`: Mensaje privado recibido en vivo.
- `302 <#canal> <apodo> <evento>`: Aviso de entrada, salida o desconexión.
- `303 <n>`: Aviso de que el usuario tiene `n` mensajes en su buzón.

### 4.3. Errores Temporales (4xx)
- `421`: Servidor saturado; la conexión se cerrará.
- `433`: Apodo en uso por otro usuario conectado.
- `442`: No eres miembro de ese canal.

### 4.4. Errores Permanentes (5xx)
- `500`: Comando desconocido.
- `501`: Sintaxis inválida (argumentos faltantes o mal formados).
- `502`: Línea demasiado larga (se descarta hasta el siguiente CRLF).
- `530`: Primero debes registrarte con `NICK`.
- `550`: Usuario desconocido.
- `551`: Canal inexistente.
- `552`: No existe ese número de mensaje en el buzón.

## 5. Extensiones Propias y Casos Borde
- **Formato visual de apodos:** En los eventos `300`, `301` y `302`, el servidor envía el apodo del emisor entre corchetes (ej. `[ana]`) para mejorar la legibilidad en el cliente.
- **Historial de canal:** El servidor mantiene en memoria los últimos 50 mensajes de cada canal. Se puede consultar mediante la puerta HTTP (`GET /historial/<canal>`), pero no existe un comando de texto para ello en el protocolo base.
- **Persistencia del buzón:** Los mensajes privados a usuarios desconectados se guardan en disco con formato RFC 5322. El borrado (`DELE`) es diferido: solo se hace efectivo al recibir `QUIT`. Si la conexión se corta abruptamente, no se borra nada.
- **`PRIV` a uno mismo:** Si un usuario se envía un `PRIV` a sí mismo, el servidor lo trata como un mensaje a un usuario desconectado (o conectado) y lo procesa según las reglas. No hay una regla especial de rechazo.
