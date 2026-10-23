#include <iostream>
#include <charconv> // Para validar números sin lanzar excepciones
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

// Librerias para los hilos y el mutex
#include <thread>      
#include <mutex> 

// Libreria para el mapa de canales
#include <set>

// Libreria para conectar con el guardado
#include "servidor/buzon.h"
#include <fstream> // Para leer archivos

//Librerias para usar las fechas
#include <chrono>
#include <ctime>

#include "servidor/http.h"

namespace fs = std::filesystem; // abreviatura para no estar escribiendo stdd...

// VARIABLES GLOBALES //
int puerto_chat = 0;
int puerto_http = 0;
std::string dir_buzones = "";
EstadoServidor estado_global;

// FUNCIONES //
// Validaciones de nombres según el protocolo.
bool apodo_valido(const std::string& apodo) {
    if (apodo.empty() || apodo.size() > 16) return false;

    for (char c : apodo) {
        bool letra = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
        bool numero = (c >= '0' && c <= '9');
        if (!letra && !numero && c != '_' && c != '-') return false;
    }
    return true;
}

bool canal_valido(const std::string& canal) {
    if (canal.size() < 2 || canal.size() > 33 || canal[0] != '#') return false;

    for (size_t i = 1; i < canal.size(); ++i) {
        char c = canal[i];
        bool letra = (c >= 'a' && c <= 'z');
        bool numero = (c >= '0' && c <= '9');
        if (!letra && !numero && c != '_' && c != '-') return false;
    }
    return true;
}

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

void atender_cliente(int client_fd) {
    //Enviamos el saludo inicial
    std::string saludo = "200 servidor-prueba listo\r\n";
    send(client_fd, saludo.c_str(), saludo.length(), 0);

    //Registrar al cliente en el estado (protegido por el candado)
    {
        std::lock_guard<std::mutex> lock(estado_global.candado);
        estado_global.usuarios_conectados[client_fd] = {"", client_fd, false, {}};
    }

    std::string buffer_cliente; // Acumula solo la línea actual
    bool descartando_linea = false; // Si supera 512 bytes, ignorar hasta CRLF
    bool ultimo_era_cr = false; // Permite detectar CRLF aunque llegue dividido
    char buffer_recv[1024]; // Buffer temporal


    // BUCLE PRINCIPAL
    while (true) {
        //Primero limpiamos el buffer y luego leemos los bytes que llegan del cliente
        memset(buffer_recv, 0, sizeof(buffer_recv));
        int bytes_leidos = recv(client_fd, buffer_recv, sizeof(buffer_recv), 0);
        
        //si recibimos un cero o menos., cerramos la conexion
        if (bytes_leidos <= 0) {
            std::cout << "[Servidor] Cliente desconectado abruptamente.\n";
            std::lock_guard<std::mutex> lock(estado_global.candado);
            estado_global.usuarios_conectados.erase(client_fd);
            break; //Rompe el while y la funcion termina
        }

        // TCP puede entregar varios comandos juntos o uno dividido en partes.
        // Cada línea del CLIENTE puede medir como máximo 512 bytes, incluido CRLF.
        std::vector<std::string> lineas;
        for (int i = 0; i < bytes_leidos; ++i) {
            char c = buffer_recv[i];

            if (descartando_linea) {
                // Ya respondimos 502: descartar solo hasta el siguiente CRLF.
                if (ultimo_era_cr && c == '\n') {
                    descartando_linea = false;
                    ultimo_era_cr = false;
                } else {
                    ultimo_era_cr = (c == '\r');
                }
                continue;
            }

            buffer_cliente.push_back(c);
            size_t tam = buffer_cliente.size();

            if (tam >= 2 && buffer_cliente[tam - 2] == '\r' && c == '\n') {
                if (tam <= 512) {
                    lineas.push_back(buffer_cliente.substr(0, tam - 2));
                } else {
                    std::string resp = "502 Línea demasiado larga\r\n";
                    send(client_fd, resp.c_str(), resp.size(), 0);
                }
                buffer_cliente.clear();
            } else if (tam > 512) {
                // Todavía no llegó CRLF y la línea ya excede el límite.
                std::string resp = "502 Línea demasiado larga\r\n";
                send(client_fd, resp.c_str(), resp.size(), 0);
                buffer_cliente.clear();
                descartando_linea = true;
                ultimo_era_cr = (c == '\r');
            }
        }

        bool cerrar_conexion = false; //Bandera si el cliente hace QUIT

        // COMANDOS
        for (const std::string& linea : lineas) {
            if (linea.empty()) continue;

            std::istringstream iss(linea);
            std::string comando, arg1;
            iss >> comando >> arg1; 

            for (char& c : comando) c = toupper(c);

            //Protegemos el acceso al usuario
            std::lock_guard<std::mutex> lock(estado_global.candado);
            Usuario& usuario = estado_global.usuarios_conectados[client_fd];

            //Comando QUIT
            if (comando == "QUIT") {
                // borrados pendientes (DELE)
                auto mensajes = obtener_mensajes(dir_buzones, usuario.apodo);
                for (int idx : usuario.mensajes_a_borrar) {
                    // idx es 1-based (el primero es 1)
                    if (idx > 0 && idx <= (int)mensajes.size()) {
                        borrar_mensaje(mensajes[idx - 1]);
                    }
                }
                usuario.mensajes_a_borrar.clear();

                std::string resp = "299 Adiós\r\n";
                send(client_fd, resp.c_str(), resp.length(), 0);
                cerrar_conexion = true;
                break; 
            }

            // Antes de registrarse, solo se permiten NICK y QUIT
            else if (!usuario.registrado && comando != "NICK") {
                std::string resp = "530 Primero debes registrarte con NICK\r\n";
                send(client_fd, resp.c_str(), resp.length(), 0);
            }

            else if (comando == "NICK") {
                std::string argumento_extra;
                if (!apodo_valido(arg1) || (iss >> argumento_extra)) {
                    std::string resp = "501 Apodo inválido\r\n";
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
                
                    // Avisamos si hay mensajes en el buzon
                    auto mensajes = obtener_mensajes(dir_buzones, arg1);
                    if (!mensajes.empty()) {
                        std::string resp303 = "303 " + std::to_string(mensajes.size()) + "\r\n";
                        send(client_fd, resp303.c_str(), resp303.length(), 0);
                    }

                    // Crear la carpeta para los futuros mensajes
                    fs::path carpeta_usuario = fs::path(dir_buzones) / arg1;
                    if (!fs::exists(carpeta_usuario)) {
                        fs::create_directories(carpeta_usuario);
                    }
                }
            }

            // COMANDO JOIN
            else if (comando == "JOIN") {
                std::string argumento_extra;
                if (!canal_valido(arg1) || (iss >> argumento_extra)) {
                    std::string resp = "501 Canal inválido\r\n";
                    send(client_fd, resp.c_str(), resp.length(), 0);
                } else {
                    // Agregamos al usuario al canal
                    estado_global.canales[arg1].miembros.insert(client_fd);
                    
                    // El que se une recibe la confirmación 210
                    std::string resp = "210 Unida a " + arg1 + "\r\n";
                    send(client_fd, resp.c_str(), resp.length(), 0);

                    // Avisamos a TODOS los del canal (incluyendo al que entró) con el evento 302
                    std::string aviso = "302 " + arg1 + " " + usuario.apodo + " entró\r\n";
                    for (int fd : estado_global.canales[arg1].miembros) {
                        send(fd, aviso.c_str(), aviso.length(), 0);
                    }
                }
            }

            // COMANDO PART
            else if (comando == "PART") {
                if (estado_global.canales.count(arg1) && estado_global.canales[arg1].miembros.count(client_fd)) {
                    estado_global.canales[arg1].miembros.erase(client_fd);
                    
                    std::string resp = "211 Saliste de " + arg1 + "\r\n";
                    send(client_fd, resp.c_str(), resp.length(), 0);

                    std::string aviso = "302 " + arg1 + " " + usuario.apodo + " salió\r\n";
                    for (int fd : estado_global.canales[arg1].miembros) {
                        send(fd, aviso.c_str(), aviso.length(), 0);
                    }
                } else {
                    std::string resp = "442 No eres miembro de ese canal\r\n";
                    send(client_fd, resp.c_str(), resp.length(), 0);
                }
            }

            // COMANDO MSG
            else if (comando == "MSG") {
                // Separamos el canal del resto del texto (que puede tener espacios)
                size_t espacio = linea.find(' ', 4); // 4 porque "MSG " mide 4 letras
                if (espacio != std::string::npos) {
                    std::string canal = linea.substr(4, espacio - 4);
                    std::string texto = linea.substr(espacio + 1);

                    if (estado_global.canales.count(canal) && estado_global.canales[canal].miembros.count(client_fd)) {
                        //Guardar en el historial con hora
                        auto ahora = std::chrono::system_clock::now();
                        std::time_t t = std::chrono::system_clock::to_time_t(ahora);
                        std::tm tm_buf;
                        localtime_r(&t, &tm_buf);
                        char hora_str[10];
                        std::strftime(hora_str, sizeof(hora_str), "%H:%M:%S", &tm_buf);
                        
                        std::string linea_historial = std::string(hora_str) + " [" + usuario.apodo + "] " + texto;
                        estado_global.canales[canal].historial.push_back(linea_historial);
                        
                        // Mantener solo los últimos 50
                        if (estado_global.canales[canal].historial.size() > 50) {
                            estado_global.canales[canal].historial.erase(estado_global.canales[canal].historial.begin());
                        }
                        
                        // Confirmación al emisor
                        std::string resp = "250 OK\r\n";
                        send(client_fd, resp.c_str(), resp.length(), 0);

                        // Enviar a los demás (el emisor no recibe el evento 300)
                        std::string mensaje_evt = "300 " + canal + " " + usuario.apodo + ": " + texto + "\r\n";
                        for (int fd : estado_global.canales[canal].miembros) {
                            if (fd != client_fd) { // El emisor no recibe su propio mensaje
                                send(fd, mensaje_evt.c_str(), mensaje_evt.length(), 0);
                            }
                        }
                    } else {
                        std::string resp = "442 No eres miembro de ese canal\r\n";
                        send(client_fd, resp.c_str(), resp.length(), 0);
                    }
                }
            }

            // COMANDO WHO
            else if (comando == "WHO") {
                if (estado_global.canales.count(arg1)) {
                    auto& miembros = estado_global.canales[arg1].miembros;
                    for (int fd : miembros) {
                        std::string resp = "213-" + estado_global.obtener_apodo(fd) + "\r\n";
                        send(client_fd, resp.c_str(), resp.length(), 0);
                    }
                    // Línea final
                    std::string final_resp = "213 " + std::to_string(miembros.size()) + " miembros\r\n";
                    send(client_fd, final_resp.c_str(), final_resp.length(), 0);
                } else {
                    std::string resp = "551 Canal inexistente\r\n";
                    send(client_fd, resp.c_str(), resp.length(), 0);
                }
            }

            // COMANDO CHANS
            else if (comando == "CHANS") {
                for (const auto& par : estado_global.canales) {
                    std::string resp = "212-" + par.first + " " + std::to_string(par.second.miembros.size()) + "\r\n";
                    send(client_fd, resp.c_str(), resp.length(), 0);
                }
                std::string final_resp = "212 " + std::to_string(estado_global.canales.size()) + " canales\r\n";
                send(client_fd, final_resp.c_str(), final_resp.length(), 0);
            }

            // COMANDO PRIV
            else if (comando == "PRIV") {
                // Separamos el destinatario (arg1) del texto (que puede tener espacios)
                size_t espacio = linea.find(' ', 5); // 5 porque "PRIV " mide 5 letras
                if (espacio != std::string::npos && !arg1.empty()) {
                    std::string destinatario = arg1;
                    std::string texto = linea.substr(espacio + 1);

                    int fd_destino = estado_global.obtener_socket(destinatario);

                    if (fd_destino != -1) {
                        // El destinatario ESTÁ conectado -> Enviar evento 301
                        std::string evt = "301 " + usuario.apodo + " " + texto + "\r\n";
                        send(fd_destino, evt.c_str(), evt.length(), 0);
                        
                        std::string resp = "250 Mensaje entregado\r\n";
                        send(client_fd, resp.c_str(), resp.length(), 0);
                    } else {
                        // El destinatario NO está conectado -> Verificar si existe en disco
                        fs::path carpeta = fs::path(dir_buzones) / destinatario;
                        if (fs::exists(carpeta)) {
                            // Existe -> Guardar en buzón (RFC 5322)
                            guardar_en_buzon(dir_buzones, destinatario, usuario.apodo, texto);
                            std::string resp = "251 Usuario desconectado; mensaje guardado en su buzón\r\n";
                            send(client_fd, resp.c_str(), resp.length(), 0);
                        } else {
                            // No existe -> Error 550
                            std::string resp = "550 Usuario desconocido\r\n";
                            send(client_fd, resp.c_str(), resp.length(), 0);
                        }
                    }
                } else {
                    std::string resp = "501 Sintaxis inválida\r\n";
                    send(client_fd, resp.c_str(), resp.length(), 0);
                }
            }

            // COMANDO DEL BUZON ---------------------------------------- //
            // COMANDO STAT
            else if (comando == "STAT") {
                auto mensajes = obtener_mensajes(dir_buzones, usuario.apodo);
                uintmax_t total_bytes = 0;
                for (const auto& m : mensajes) total_bytes += fs::file_size(m);
                std::string resp = "220 " + std::to_string(mensajes.size()) + " " + std::to_string(total_bytes) + "\r\n";
                send(client_fd, resp.c_str(), resp.length(), 0);
            }

            // COMANDO LIST
            else if (comando == "LIST") {
                auto mensajes = obtener_mensajes(dir_buzones, usuario.apodo);
                for (size_t i = 0; i < mensajes.size(); ++i) {
                    std::string resp = "221-" + std::to_string(i + 1) + " " + std::to_string(fs::file_size(mensajes[i])) + "\r\n";
                    send(client_fd, resp.c_str(), resp.length(), 0);
                }
                std::string final_resp = "221 " + std::to_string(mensajes.size()) + " mensajes\r\n";
                send(client_fd, final_resp.c_str(), final_resp.length(), 0);
            }

            // COMANDO RETR
            else if (comando == "RETR") {
                // El argumento debe ser un número entero sin otros argumentos.
                std::string argumento_extra;
                unsigned long long n = 0;
                bool numero_valido = !arg1.empty();
                for (char c : arg1) {
                    if (c < '0' || c > '9') {
                        numero_valido = false;
                        break;
                    }
                }
                if (numero_valido) {
                    auto resultado = std::from_chars(arg1.data(), arg1.data() + arg1.size(), n);
                    numero_valido = (resultado.ec == std::errc() &&
                                     resultado.ptr == arg1.data() + arg1.size());
                }

                if (!numero_valido || (iss >> argumento_extra)) {
                    std::string resp = "501 Número inválido\r\n";
                    send(client_fd, resp.c_str(), resp.length(), 0);
                } else {
                    auto mensajes = obtener_mensajes(dir_buzones, usuario.apodo);
                    if (n == 0 || n > mensajes.size()) {
                        std::string resp = "552 No existe ese número de mensaje\r\n";
                        send(client_fd, resp.c_str(), resp.length(), 0);
                    } else {
                        // Leer los bytes del archivo tal como se guardaron.
                        std::ifstream archivo(mensajes[static_cast<size_t>(n - 1)], std::ios::binary);
                        if (!archivo.is_open()) {
                            std::string resp = "552 No existe ese número de mensaje\r\n";
                            send(client_fd, resp.c_str(), resp.length(), 0);
                        } else {
                            std::string linea_archivo;
                            while (std::getline(archivo, linea_archivo)) {
                                // getline elimina LF, pero deja CR si la línea usa CRLF.
                                if (!linea_archivo.empty() && linea_archivo.back() == '\r') {
                                    linea_archivo.pop_back();
                                }
                                std::string resp = "222-" + linea_archivo + "\r\n";
                                send(client_fd, resp.c_str(), resp.length(), 0);
                            }
                            std::string fin = "222 fin del mensaje\r\n";
                            send(client_fd, fin.c_str(), fin.length(), 0);
                        }
                    }
                }
            }

            // COMANDO DELE
            else if (comando == "DELE") {
                try {
                    int n = std::stoi(arg1);
                    auto mensajes = obtener_mensajes(dir_buzones, usuario.apodo);
                    if (n > 0 && n <= (int)mensajes.size()) {
                        usuario.mensajes_a_borrar.insert(n); // Solo lo marcamos
                        std::string resp = "223 Mensaje " + std::to_string(n) + " marcado para borrar\r\n";
                        send(client_fd, resp.c_str(), resp.length(), 0);
                    } else {
                        std::string resp = "552 No existe ese número de mensaje\r\n";
                        send(client_fd, resp.c_str(), resp.length(), 0);
                    }
                } catch (...) {
                    std::string resp = "501 Número inválido\r\n";
                    send(client_fd, resp.c_str(), resp.length(), 0);
                }
            }

            else {
                if (!usuario.registrado) {
                    std::string resp = "530 Primero debes registrarte con NICK\r\n";
                    send(client_fd, resp.c_str(), resp.length(), 0);
                } else {
                    std::string resp = "500 Comando desconocido o no implementado\r\n";
                    send(client_fd, resp.c_str(), resp.length(), 0);
                }
            }
        }

        if (cerrar_conexion) {
            std::lock_guard<std::mutex> lock(estado_global.candado);
            estado_global.usuarios_conectados.erase(client_fd);
            break; 
        }
    }
    close(client_fd);
    std::cout << "[Servidor] Hilo del cliente " << client_fd << " finalizado.\n";
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

    // Iniciamos el hilo del servidor http
    std::thread hilo_http(iniciar_http, puerto_http, std::ref(estado_global));
    hilo_http.detach();

    std::cout << "[Servidor] Esperando conexiones...\n";
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    while(true){
                int client_fd = accept(server_fd, (sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) continue;

        char ip_cliente[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), ip_cliente, INET_ADDRSTRLEN);
        std::cout << "[Servidor] ¡Cliente conectado desde " << ip_cliente << "!\n";

        // Lanzamos el hilo y lo separamos para que el bucle siga aceptando
        std::thread hilo_cliente(atender_cliente, client_fd);
        hilo_cliente.detach(); 
    }

    close(server_fd);
    return 0;
}
    
