
#ifndef ESTADO_H
#define ESTADO_H

#include <string>
#include <map>
#include <mutex>
#include <set>
#include <vector>

// Estructura para guardar los datos de un usuario conectado
struct Usuario {
    std::string apodo;
    int socket_fd;
    bool registrado;
    std::set<int> mensajes_a_borrar;
};

// Estructura para guardar los datos de un canal
struct Canal {
    std::set<int> miembros;
    std::vector<std::string> historial;
};

// Clase que maneja el estado global del servidor
class EstadoServidor {
public:
    std::map<int, Usuario> usuarios_conectados;
    std::map<std::string, Canal> canales;

    std::mutex candado;

    // Verifica si un apodo ya está en uso
    bool apodo_en_uso(const std::string& apodo) {
        for (const auto& par : usuarios_conectados) {
            if (par.second.registrado && par.second.apodo == apodo) {
                return true;
            }
        }
        return false;
    }

    // Busca el apodo a partir del socket
    std::string obtener_apodo(int fd) {
        if (usuarios_conectados.count(fd))
            return usuarios_conectados[fd].apodo;
        return "Desconocido";
    }

    // Busca el socket de un usuario conectado
    int obtener_socket(const std::string& apodo) {
        for (const auto& par : usuarios_conectados) {
            if (par.second.registrado && par.second.apodo == apodo) {
                return par.second.socket_fd;
            }
        }
        return -1;
    }
};

#endif
