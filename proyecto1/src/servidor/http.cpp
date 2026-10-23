#include "servidor/http.h"

#include <atomic>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <unistd.h>

namespace {

// Limite razonable para las cabeceras HTTP. No afecta el limite de 512 bytes
// de los comandos del chat (son dos protocolos y puertos distintos).
constexpr size_t MAX_CABECERAS_HTTP = 16 * 1024;
constexpr size_t TAM_BUFFER_HTTP = 2048;
constexpr unsigned int MAX_CLIENTES_HTTP = 256;

std::atomic<unsigned int> clientes_http_activos{0};

std::string construir_respuesta_http(int codigo, const std::string& cuerpo) {
    std::string motivo;
    switch (codigo) {
        case 200: motivo = "OK"; break;
        case 400: motivo = "Bad Request"; break;
        case 404: motivo = "Not Found"; break;
        case 405: motivo = "Method Not Allowed"; break;
        default: motivo = "Bad Request"; codigo = 400;
    }

    std::ostringstream respuesta;
    respuesta << "HTTP/1.1 " << codigo << " " << motivo << "\r\n"
              << "Content-Type: text/plain; charset=utf-8\r\n"
              << "Content-Length: " << cuerpo.size() << "\r\n"
              << "Connection: close\r\n";
    if (codigo == 405) respuesta << "Allow: GET\r\n";
    respuesta << "\r\n" << cuerpo;
    return respuesta.str();
}

// Es necesario repetir send() porque una llamada puede escribir solo
// una parte de la respuesta. MSG_NOSIGNAL evita SIGPIPE al desconectarse.
bool enviar_completo(int fd, const std::string& datos) {
    size_t total = 0;
    while (total < datos.size()) {
        ssize_t n = send(fd, datos.data() + total, datos.size() - total, MSG_NOSIGNAL);
        if (n > 0) {
            total += static_cast<size_t>(n);
        } else if (n < 0 && errno == EINTR) {
            continue;
        } else {
            return false;
        }
    }
    return true;
}

bool canal_http_valido(const std::string& nombre) {
    if (nombre.empty() || nombre.size() > 32) return false;
    for (char c : nombre) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
              c == '_' || c == '-')) {
            return false;
        }
    }
    return true;
}

// Devuelve 400 cuando no llega una cabecera HTTP completa y valida.
// La delimitacion se realiza buscando CRLFCRLF entre varias llamadas recv.
std::string procesar_peticion(const std::string& peticion, EstadoServidor& estado) {
    size_t fin_linea = peticion.find("\r\n");
    if (fin_linea == std::string::npos) {
        return construir_respuesta_http(400, "400 Peticion mal formada\n");
    }

    std::string primera_linea = peticion.substr(0, fin_linea);
    std::istringstream entrada(primera_linea);
    std::string metodo, ruta, version, extra;
    if (!(entrada >> metodo >> ruta >> version) || entrada >> extra ||
        version != "HTTP/1.1" || ruta.empty() || ruta.front() != '/') {
        return construir_respuesta_http(400, "400 Peticion mal formada\n");
    }

    if (metodo != "GET") {
        return construir_respuesta_http(405, "405 Metodo no permitido\n");
    }

    if (ruta == "/usuarios") {
        std::string cuerpo;
        {
            std::lock_guard<std::mutex> lock(estado.candado);
            for (const auto& par : estado.usuarios_conectados) {
                if (par.second.registrado) cuerpo += par.second.apodo + "\n";
            }
        }
        return construir_respuesta_http(200, cuerpo);
    }

    if (ruta == "/canales") {
        std::string cuerpo;
        {
            std::lock_guard<std::mutex> lock(estado.candado);
            for (const auto& par : estado.canales) {
                cuerpo += par.first + " " + std::to_string(par.second.miembros.size()) + "\n";
            }
        }
        return construir_respuesta_http(200, cuerpo);
    }

    const std::string prefijo = "/historial/";
    if (ruta.compare(0, prefijo.size(), prefijo) == 0) {
        const std::string nombre_canal = ruta.substr(prefijo.size());
        if (!canal_http_valido(nombre_canal)) {
            return construir_respuesta_http(404, "404 Canal no encontrado\n");
        }
        const std::string canal = "#" + nombre_canal;
        std::string cuerpo;
        bool existe = false;
        {
            std::lock_guard<std::mutex> lock(estado.candado);
            auto it = estado.canales.find(canal);
            if (it != estado.canales.end()) {
                existe = true;
                for (const auto& linea : it->second.historial) {
                    cuerpo += linea + "\n";
                }
            }
        }
        if (existe) return construir_respuesta_http(200, cuerpo);
        return construir_respuesta_http(404, "404 Canal no encontrado\n");
    }

    return construir_respuesta_http(404, "404 Ruta no encontrada\n");
}

void atender_http(int client_fd, EstadoServidor& estado) {
    // Un cliente HTTP que no complete su peticion no debe bloquear a otros.
    timeval timeout{};
    timeout.tv_sec = 5;
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    std::string peticion;
    char buffer[TAM_BUFFER_HTTP];
    bool completa = false;
    bool demasiada_larga = false;

    while (true) {
        ssize_t n = recv(client_fd, buffer, sizeof(buffer), 0);
        if (n > 0) {
            peticion.append(buffer, static_cast<size_t>(n));
            size_t fin = peticion.find("\r\n\r\n");
            if (fin != std::string::npos) {
                if (fin + 4 > MAX_CABECERAS_HTTP) demasiada_larga = true;
                else completa = true;
                break;
            }
            if (peticion.size() > MAX_CABECERAS_HTTP) {
                demasiada_larga = true;
                break;
            }
        } else if (n < 0 && errno == EINTR) {
            continue;
        } else {
            break; // cierre del cliente, tiempo agotado o error de recepcion
        }
    }

    std::string respuesta;
    if (!completa || demasiada_larga) {
        respuesta = construir_respuesta_http(400, "400 Peticion mal formada\n");
    } else {
        respuesta = procesar_peticion(peticion, estado);
    }

    enviar_completo(client_fd, respuesta);
    close(client_fd);
    --clientes_http_activos;
}

} // namespace

void iniciar_http(int puerto, EstadoServidor& estado_global) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("[HTTP] socket");
        return;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in direccion{};
    direccion.sin_family = AF_INET;
    direccion.sin_addr.s_addr = INADDR_ANY;
    direccion.sin_port = htons(static_cast<uint16_t>(puerto));

    if (bind(server_fd, reinterpret_cast<sockaddr*>(&direccion), sizeof(direccion)) < 0) {
        perror("[HTTP] bind");
        close(server_fd);
        return;
    }
    if (listen(server_fd, SOMAXCONN) < 0) {
        perror("[HTTP] listen");
        close(server_fd);
        return;
    }

    std::cout << "[HTTP] Escuchando en el puerto " << puerto << "...\n";

    while (true) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            perror("[HTTP] accept");
            continue;
        }

        if (clientes_http_activos.load() >= MAX_CLIENTES_HTTP) {
            close(client_fd);
            continue;
        }
        ++clientes_http_activos;
        try {
            std::thread(atender_http, client_fd, std::ref(estado_global)).detach();
        } catch (const std::system_error&) {
            --clientes_http_activos;
            close(client_fd);
        }
    }
    close(server_fd);
}
