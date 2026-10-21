#ifndef ESTADO_H
#define ESTADO_H

#include <string>
#include <map>

// Estructura para guardar los datos de un usuario conectado
struct Usuario {
    std::string apodo;
    int socket_fd;
    bool registrado; // false hasta que haga NICK
};

// Clase que maneja el estado global del servidor
class EstadoServidor {
public:
    // Mapa de <socket_fd, Usuario>
    std::map<int, Usuario> usuarios_conectados;

    // Verifica si un apodo ya está en uso
    bool apodo_en_uso(const std::string& apodo) {
        for (const auto& par : usuarios_conectados) {
            if (par.second.registrado && par.second.apodo == apodo) {
                return true;
            }
        }
        return false;
    }
};

#endif