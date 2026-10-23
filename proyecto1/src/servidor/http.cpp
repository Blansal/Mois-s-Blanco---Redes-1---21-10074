#include "servidor/http.h"
#include <iostream>
#include <string>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

std::string construir_respuesta_http(int codigo, const std::string& cuerpo) {
    std::string status_text;
    if (codigo == 200) status_text = "OK";
    else if (codigo == 400) status_text = "Bad Request";
    else if (codigo == 404) status_text = "Not Found";
    else if (codigo == 405) status_text = "Method Not Allowed";

    std::ostringstream oss;
    // Línea de estado
    oss << "HTTP/1.1 " << codigo << " " << status_text << "\r\n";
    // Cabeceras obligatorias
    oss << "Content-Type: text/plain; charset=utf-8\r\n";
    oss << "Content-Length: " << cuerpo.length() << "\r\n";
    oss << "Connection: close\r\n";
    if (codigo == 405) oss << "Allow: GET\r\n";
    // Separación cabecera/cuerpo
    oss << "\r\n";
    // Cuerpo
    oss << cuerpo;
    return oss.str();
}

// --- INICIAR SERVIDOR HTTP ---
void iniciar_http(int puerto, EstadoServidor& estado_global) {
    // Configuración del socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("[HTTP] Error socket"); return; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(puerto);

    if (bind(server_fd, (sockaddr *)&address, sizeof(address)) < 0) {
        perror("[HTTP] Error bind"); return;
    }
    if (listen(server_fd, 10) < 0) {
        perror("[HTTP] Error listen"); return;
    }

    std::cout << "[HTTP] Escuchando en el puerto " << puerto << "...\n";

    // Bucle principal de aceptación
    while (true) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd < 0) continue;

        // Lectura de la petición (delimitación \r\n\r\n)
        std::string peticion, buffer_recv(1024, '\0');
        while (peticion.find("\r\n\r\n") == std::string::npos) {
            memset(&buffer_recv[0], 0, buffer_recv.size());
            int bytes = recv(client_fd, &buffer_recv[0], buffer_recv.size() - 1, 0);
            if (bytes <= 0) break;
            peticion.append(&buffer_recv[0], bytes);
        }

        // Parseo de la línea de petición
        std::istringstream iss(peticion);
        std::string metodo, ruta, version;
        iss >> metodo >> ruta >> version;

        std::string respuesta;

        // Validación y enrutamiento
        if (metodo.empty() || ruta.empty() || version.empty()) {
            respuesta = construir_respuesta_http(400, "400 Bad Request\n");
        }
        else if (metodo != "GET") {
            respuesta = construir_respuesta_http(405, "405 Method Not Allowed\n");
        }
        else if (ruta == "/usuarios") {
            std::string cuerpo;
            // Bloqueo del estado para lectura segura
            std::lock_guard<std::mutex> lock(estado_global.candado);
            for (const auto& par : estado_global.usuarios_conectados) {
                if (par.second.registrado) cuerpo += par.second.apodo + "\n";
            }
            respuesta = construir_respuesta_http(200, cuerpo);
        }
        else if (ruta == "/canales") {
            std::string cuerpo;
            std::lock_guard<std::mutex> lock(estado_global.candado);
            for (const auto& par : estado_global.canales) {
                cuerpo += par.first + " " + std::to_string(par.second.miembros.size()) + "\n";
            }
            respuesta = construir_respuesta_http(200, cuerpo);
        }
        else if (ruta.find("/historial/") == 0) {
            std::string canal = ruta.substr(11);
            if (canal[0] != '#') canal = "#" + canal;
            
            std::string cuerpo;
            bool encontrado = false;
            
            {
                std::lock_guard<std::mutex> lock(estado_global.candado);
                if (estado_global.canales.count(canal)) {
                    encontrado = true;
                    for (const std::string& linea : estado_global.canales[canal].historial) {
                        cuerpo += linea + "\n";
                    }
                }
            }
            
            if (encontrado) {
                respuesta = construir_respuesta_http(200, cuerpo);
            } else {
                respuesta = construir_respuesta_http(404, "404 Canal no encontrado\n");
            }
        }
        else {
            respuesta = construir_respuesta_http(404, "404 Ruta no encontrada\n");
        }

        // Envío y cierre de conexión
        send(client_fd, respuesta.c_str(), respuesta.length(), 0);
        close(client_fd);
    }
    close(server_fd);
}
