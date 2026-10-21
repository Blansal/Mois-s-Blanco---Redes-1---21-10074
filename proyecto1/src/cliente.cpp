#include <iostream>
#include <string>
#include <cstdlib> // Para std::stoi

// Librerias para sockets
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>      // Requerido para getaddrinfo() (resolver "localhost")
#include <cstring>

//agregamos el hilo de lectura
#include <thread>

void mostrar_uso(const char* programa) {
    std::cerr << "Uso: " << programa << " <host> <puerto>\n";
}

int main(int argc, char* argv[]) {
    // Validamos los argumentos: El cliente tiene exactamente 2, host y puerto
    if (argc != 3) {
        std::cerr << "Error: Número incorrecto de argumentos.\n";
        mostrar_uso(argv[0]);
        return 1;
    }

    //Leemos los argumentos
    std::string host = argv[1];
    int puerto = 0;

    // Intentamos convertir el argumento del puerto a un número entero.
    // Usamos try-catch porque std::stoi lanza excepción si no es un número válido.
    try {
        puerto = std::stoi(argv[2]);
    } catch (const std::exception& e) {
        std::cerr << "Error: El puerto debe ser un número válido.\n";
        return 1;
    }

    //Mostramos la configuracion inicial
    std::cout << "=== Cliente iniciando ===\n";
    std::cout << "Host:   " << host << "\n";
    std::cout << "Puerto: " << puerto << "\n";
    std::cout << "=========================\n";

    // Creamos el Socket
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(sock_fd < 0) {
        perror("[Error] socket()");
        return 1;
    }
    
    // resolvemos la direccion 
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;       // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP
    
    std::string puerto_str = std::to_string(puerto);
    if (getaddrinfo(host.c_str(), puerto_str.c_str(), &hints, &res) != 0) {
        std::cerr << "[Error] No se pudo resolver el host: " << host << "\n";
        close(sock_fd);
        return 1;
    }
    
    // Conectamos el servidor 
    std::cout << "[Cliente] Conectando a " << host << ":" << puerto << "...\n";
    if (connect(sock_fd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("[Error] connect()");
        freeaddrinfo(res); // Liberar memoria de getaddrinfo
        close(sock_fd);
        return 1;
    }
    freeaddrinfo(res); // Ya no necesitamos la estructura de direcciones

    std::cout << "[Cliente] ¡Conectado exitosamente!\n";
    std::cout << "[Cliente] Escribe un mensaje (o 'salir' para terminar):\n";
    
    // Hilo de escucha del servidor 
    std::thread hilo_lectura([sock_fd]() {
        char buffer[1024];
        while (true) {
            memset(buffer, 0, sizeof(buffer));
            int bytes = recv(sock_fd, buffer, sizeof(buffer) - 1, 0);
            if (bytes <= 0) {
                std::cout << "\n[Cliente] Servidor desconectado.\n";
                exit(0);
            }
            std::cout<< "\r[SERVIDOR]: " << buffer << "\n> " << std::flush;
        }
    });  

    // Hilo para leer el y enviar al servidor
    std::string linea;
    while(std::getline(std::cin, linea)){
        if (linea == "salir") break;
        linea += "\r\n"; // Agregamos el CRLF obligatorio
        send(sock_fd, linea.c_str(), linea.length(), 0);
        std::cout << "> ";
    }

    // AL salir hacemos un cierre de todo 
    hilo_lectura.detach();
    close(sock_fd);
    std::cout << "[Cliente] Desconectado.\n";
    return 0;
}