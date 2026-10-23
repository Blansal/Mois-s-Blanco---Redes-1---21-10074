#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>
#include <csignal>

#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

// Cliente de terminal: lee el teclado y el socket sin bloquear uno por el otro.
// El protocolo utiliza UTF-8 y líneas terminadas en CRLF.

void mostrar_uso(const char* programa) {
    std::cerr << "Uso: " << programa << " <host> <puerto>\n";
}

// send() puede escribir menos bytes que los pedidos: enviamos la línea completa.
bool enviar_todo(int fd, const std::string& datos) {
    size_t enviados = 0;
    while (enviados < datos.size()) {
        ssize_t n = send(fd, datos.data() + enviados,
                         datos.size() - enviados, MSG_NOSIGNAL);
        if (n > 0) {
            enviados += static_cast<size_t>(n);
        } else if (n < 0 && errno == EINTR) {
            continue;
        } else {
            return false;
        }
    }
    return true;
}

// Convierte un comando escrito en el teclado en una línea del protocolo.
// Devuelve false cuando no hay que enviar nada (por ejemplo, línea excesiva).
bool enviar_comando(int fd, std::string linea, bool& salir) {
    // Si la entrada ya incluía CRLF, quitamos el CR antes de normalizarla.
    if (!linea.empty() && linea.back() == '\r') linea.pop_back();

    if (linea == "salir") linea = "QUIT";

    // 512 bytes, incluidos los dos del CRLF.
    if (linea.size() + 2 > 512) {
        std::cerr << "[Cliente] Línea demasiado larga (máximo 512 bytes incluyendo CRLF).\n";
        return true; // No cerrar la conexión; se puede intentar de nuevo.
    }

    std::string comando = linea;
    for (char& c : comando) {
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    }

    if (!enviar_todo(fd, linea + "\r\n")) return false;
    if (comando == "QUIT") salir = true;
    return true;
}

// Muestra únicamente líneas completas, incluso si recv() las entrega partidas
// o varias juntas. No aplica el límite de 512 al tráfico del servidor.
void mostrar_respuestas(std::string& buffer) {
    size_t fin;
    while ((fin = buffer.find("\r\n")) != std::string::npos) {
        std::cout << "[SERVIDOR]: " << buffer.substr(0, fin) << '\n';
        buffer.erase(0, fin + 2);
    }
    std::cout.flush();
}

int conectar(const std::string& host, const std::string& puerto) {
    addrinfo pistas{};
    pistas.ai_family = AF_UNSPEC; // IPv4 o IPv6
    pistas.ai_socktype = SOCK_STREAM;

    addrinfo* direcciones = nullptr;
    int error = getaddrinfo(host.c_str(), puerto.c_str(), &pistas, &direcciones);
    if (error != 0) {
        std::cerr << "[Error] No se pudo resolver el host: " << gai_strerror(error) << '\n';
        return -1;
    }

    int fd = -1;
    for (addrinfo* p = direcciones; p != nullptr; p = p->ai_next) {
        int candidato = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (candidato < 0) continue;
        if (connect(candidato, p->ai_addr, p->ai_addrlen) == 0) {
            fd = candidato;
            break;
        }
        close(candidato);
    }
    freeaddrinfo(direcciones);
    return fd;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        mostrar_uso(argv[0]);
        return 1;
    }

    // La cadena del puerto debe contener solo dígitos y estar entre 1 y 65535.
    std::string puerto = argv[2];
    if (puerto.empty() || puerto.size() > 5 ||
        puerto.find_first_not_of("0123456789") != std::string::npos) {
        std::cerr << "[Error] Puerto inválido.\n";
        return 1;
    }
    int numero_puerto = std::stoi(puerto);
    if (numero_puerto < 1 || numero_puerto > 65535) {
        std::cerr << "[Error] El puerto debe estar entre 1 y 65535.\n";
        return 1;
    }

    // Linux evita terminar el proceso por SIGPIPE al escribir en un socket cerrado.
    std::signal(SIGPIPE, SIG_IGN);
    std::cout << "[Cliente] Conectando a " << argv[1] << ':' << puerto << "...\n";

    int fd = conectar(argv[1], puerto);
    if (fd < 0) {
        std::cerr << "[Error] No se pudo conectar al servidor.\n";
        return 1;
    }
    std::cout << "[Cliente] Conectado. Escribe comandos o 'salir' para terminar.\n";

    std::string entrada;
    std::string respuestas;
    bool esperando_cierre = false;
    bool descartando_entrada = false;
    bool stdin_activo = true;
    bool hubo_error = false;

    while (true) {
        pollfd fds[2]{};
        fds[0].fd = fd;
        fds[0].events = POLLIN;
        fds[1].fd = (stdin_activo && !esperando_cierre) ? STDIN_FILENO : -1;
        fds[1].events = POLLIN;

        int n = poll(fds, 2, -1);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("[Error] poll()");
            hubo_error = true;
            break;
        }

        // Atendemos primero los datos del servidor, incluso si el usuario
        // está escribiendo un comando por el teclado.
        if (fds[0].revents & (POLLIN | POLLHUP | POLLERR)) {
            char trozo[4096];
            ssize_t leidos = recv(fd, trozo, sizeof(trozo), 0);
            if (leidos > 0) {
                respuestas.append(trozo, static_cast<size_t>(leidos));
                mostrar_respuestas(respuestas);
            } else {
                if (leidos < 0 && errno == EINTR) continue;
                if (leidos < 0) {
                    perror("[Error] recv()");
                    hubo_error = true;
                } else {
                    std::cout << "[Cliente] Servidor desconectado.\n";
                }
                break;
            }
        }

        if (!esperando_cierre && stdin_activo &&
            (fds[1].revents & (POLLIN | POLLHUP | POLLERR))) {
            char trozo[4096];
            ssize_t leidos = read(STDIN_FILENO, trozo, sizeof(trozo));
            if (leidos < 0) {
                if (errno == EINTR) continue;
                perror("[Error] read() del teclado");
                hubo_error = true;
                break;
            }
            if (leidos == 0) {
                // El teclado cerró (por ejemplo, al finalizar una tubería).
                stdin_activo = false;
                if (!entrada.empty() && !descartando_entrada) {
                    if (!enviar_comando(fd, entrada, esperando_cierre)) {
                        hubo_error = true;
                        break;
                    }
                    entrada.clear();
                }
                if (!esperando_cierre) {
                    if (!enviar_todo(fd, "QUIT\r\n")) {
                        hubo_error = true;
                        break;
                    }
                    esperando_cierre = true;
                }
            } else {
                for (ssize_t i = 0; i < leidos; ++i) {
                    char c = trozo[i];
                    if (c == '\n') {
                        if (!descartando_entrada &&
                            !enviar_comando(fd, entrada, esperando_cierre)) {
                            std::cerr << "[Error] No se pudo enviar el comando.\n";
                            hubo_error = true;
                            break;
                        }
                        entrada.clear();
                        descartando_entrada = false;
                        if (esperando_cierre) break;
                    } else if (!descartando_entrada) {
                        entrada.push_back(c);
                        // Un CR final puede formar parte de CRLF en stdin.
                        if (entrada.size() > 511) {
                            std::cerr << "[Cliente] Línea demasiado larga; se descarta.\n";
                            entrada.clear();
                            descartando_entrada = true;
                        }
                    }
                }
                if (hubo_error) break;
            }
        }
    }

    close(fd);
    return hubo_error ? 1 : 0;
}
