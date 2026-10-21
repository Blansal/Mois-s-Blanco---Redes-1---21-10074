#include <iostream>
#include <string>
#include <cstring>
#include <filesystem> // Para crear la carpeta de buzones en el caso que no exista
#include <csignal>    // Para manejar Ctrl + C

// Librerias de red permitidas 
#include <sys/socket.h> // socket(), bind(), listen(), accept()
#include <netinet/in.h> // sockaddr_in, INADDR_ANY, htons()
#include <arpa/inet.h>  // inet_ntoa(), inet_addr()
#include <unistd.h>     // close(), read(), write()
#include <cstring>      // memset()

// Librero del protocolo
#include "comun/protocolo.h"

// Agregamos el librero para procesar los comandos
#include "servidor/estado.h"
#include <sstream> // Para separar palabras por espacios

namespace fs = std::filesystem; // abreviatura para no estar escribiendo stdd...

// VARIABLES GLOBALES //
int puerto_chat = 0;
int puerto_http = 0;
std::string dir_buzones = "";
EstadoServidor estado_global;

// FUNCIONES //
void mostrar_uso(const char* programa){
    std::cerr<< "Uso: " << programa << " --chat <puerto> --http <puerto> --buzones <carpeta>\n"; 
}

void manejar_signal(int sig){
    std::cout << "\nCerrando servidor (señal " << sig << ")...\n"; 
    exit(0); // Esta funcion es para que el servidor corra hasta recibir SIGINT (ctrl + c)
}

int iniciar_servidor(int puerto){
    // Creamos el socket: AF_INET = IPv4, SOCK_STREAM = TCP
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0) {
        perror("[Error] socket()"); exit(1);
    }

    // Permitimos que se reutilice el puerto para evitar errores
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("[Error] setsockopt()"); exit(1);
    }
    
    // Configuramos la direcccion 0.0.0.0 y el puerto
    struct sockaddr_in address; 
    memset(&address, 0, sizeof(address)); // Limpia la memoria
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // es el puerto 0.0.0.0
    address.sin_port = htons(puerto); // Convierte el puerto a formato de red

    // Enlazamos el socket al puerto bind()
    if(bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("[Error] bind()"); exit(1);
    } 

    // Lo colocamos a escuchar
    if(listen(server_fd, 10) < 0) {
        perror("[Error] listen()"); exit(1);
    }
    std::cout << "[Servidor] Escuchando en el puerto " << puerto << "...\n";
    return server_fd; // Retornamos el descriptor del socket
}    

// FUNCION MAIN
int main(int argc, char* argv[]){
    signal(SIGINT, manejar_signal); // Se registra la funcion para que se ejecute cuando el usuario precione ctrl + c o se envie el SIGTERM
    signal(SIGTERM, manejar_signal);

    // Iteramos sobre los argumentos pasados del ejecutable, empezando en i porque 0 es el nombre del programa 
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--chat") {
            if (i + 1 < argc) {
                puerto_chat = std::stoi(argv[i + 1]);
                i++; // Avanzamos el índice para saltar el valor
            }
        } 
        else if (arg == "--http") {
            if (i + 1 < argc) {
                puerto_http = std::stoi(argv[i + 1]);
                i++;
            }
        } 
        else if (arg == "--buzones") {
            if (i + 1 < argc) {
                dir_buzones = argv[i + 1];
                i++;
            }
        } 
        else {
            std::cerr << "Argumento inválido: " << arg << "\n";
            mostrar_uso(argv[0]);
            return 1;
        }
        
    }
        
    // VALIDAMOS LOS ARGUMENTOS: si falta alguno el servidor no va a iniciar
    if (puerto_chat == 0 || puerto_http == 0 || dir_buzones.empty()) {
        std::cerr << "Error: Faltan argumentos obligatorios.\n";
        mostrar_uso(argv[0]);
        return 1;
    }

    // Creamos los directorios para el buzon 
    try {
        if (!fs::exists(dir_buzones)) {
            // create_directories crea la carpeta y cualquier subcarpeta necesaria
            fs::create_directories(dir_buzones);
            std::cout << "Carpeta de buzones creada en: " << dir_buzones << "\n";
        }
    } 
    catch (const fs::filesystem_error& e) {
        // Si hay un error de permisos o ruta inválida, atrapamos la excepción.
        std::cerr << "Error al crear la carpeta de buzones: " << e.what() << "\n";
        return 1;
    }

    // Imprimimos la configuracion inicial
    std::cout << "=== Servidor iniciando ===\n";
    std::cout << "Puerto Chat: " << puerto_chat << "\n";
    std::cout << "Puerto HTTP: " << puerto_http << "\n";
    std::cout << "Buzones en:  " << dir_buzones << "\n";
    std::cout << "==========================\n";

    // BUCLE PRINCIPAL
    int server_fd = iniciar_servidor(puerto_chat);

    std::cout << "[Servidor] Esperando conexiones...\n";
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    while(true){
        // accept() se bloquea hasta que llega un cliente
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) continue;
    
        // Mostramos la ip que tiene el cliente´
        char ip_cliente[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), ip_cliente, INET_ADDRSTRLEN);
        std::cout << "[Servidor] ¡Cliente conectado desde " << ip_cliente << "!\n";
        
        // Enviamos el saludo general
        std::string saludo = "200 servidor-prueba listo\r\n";
        send(client_fd, saludo.c_str(), saludo.length(), 0);
        
        // Al conectarse, se registra en el estado de "no registrado"
        estado_global.usuarios_conectados[client_fd] = {"", client_fd, false};
        
        // Aplicamos la delimitacion 
        std::string buffer_cliente;
        char buffer_recv[1024];

        
        while(true){
            // Limpiamos primero el buffer de recepcion
            memset(buffer_recv, 0, sizeof(buffer_recv));

            // Recibimos hasta 1024 bytes
            int bytes_leidos = recv(client_fd, buffer_recv, sizeof(buffer_recv), 0);
            
            // Si los bytes leidos son 0, el cliente cerro la conexion
            if (bytes_leidos <= 0) {
                std::cout << "[Servidor] Cliente desconectado.\n";
                estado_global.usuarios_conectados.erase(client_fd);
                break;
            }

            // Agregamos los bytes crudos al búfer acumulador
            buffer_cliente.append(buffer_recv, bytes_leidos);

            // Magia: Extraemos las líneas completas
            std::vector<std::string> lineas = extraer_lineas(buffer_cliente);
            bool cerrar_conexion = false;

            // Procesamos cada línea completa
            for (const std::string& linea : lineas) {
                //Validamos que la linea no este vacia ni que exceda el tamañó
                if(linea.empty()) continue;

                //Separar el comando de los arg
                std::istringstream iss(linea);
                std::string comando, arg1;
                iss >> comando >> arg1; // iss >> extrae palabras separadas por espacio
            
                // Convertimos el comando a mayuscula
                for (char& c : comando) c = toupper(c);
                Usuario& usuario = estado_global.usuarios_conectados[client_fd];
            
                //Procesamos cada comando
                if (comando == "QUIT"){
                    std::string resp = "299 Adiós\r\n";
                    send(client_fd, resp.c_str(), resp.length(), 0);
                    cerrar_conexion = true;
                    break; // Salir del for de líneas
                }

                else if (comando == "NICK"){
                    if (arg1.empty() || arg1.length() > 16) {
                        std::string resp = "501 Apodo inválido (1-16 caracteres)\r\n";
                        send(client_fd, resp.c_str(), resp.length(), 0);
                    } else if (estado_global.apodo_en_uso(arg1)) {
                        std::string resp = "433 Apodo en uso\r\n";
                        send(client_fd, resp.c_str(), resp.length(), 0);
                    } else {
                        usuario.apodo = arg1;
                        usuario.registrado = true;
                        std::string resp = "201 Bienvenido, " + arg1 + "\r\n";
                        send(client_fd, resp.c_str(), resp.length(), 0);
                        std::cout << "[Servidor] Cliente " << client_fd << " registrado como " << arg1 << "\n";
                        // Nota: Aquí iría el 303 si tuviera mensajes, lo haremos en el buzón.
                    }
                    }
                else {
                    // Si no es QUIT ni NICK, verificar si está registrado
                    if (!usuario.registrado) {
                        std::string resp = "530 Primero debes registrarte con NICK\r\n";
                        send(client_fd, resp.c_str(), resp.length(), 0);
                    } else {
                        // Si ya está registrado pero es un comando que no existe aún
                        std::string resp = "500 Comando desconocido o no implementado\r\n";
                        send(client_fd, resp.c_str(), resp.length(), 0);
                    }
                }
            }

            if (cerrar_conexion) {
                estado_global.usuarios_conectados.erase(client_fd);
                break; // Sale del while de recv
            }
        }
        close(client_fd);
    }
}
    
