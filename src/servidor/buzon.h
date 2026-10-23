#ifndef BUZON_H
#define BUZON_H

#include <string>
#include <vector>
#include <filesystem> // Necesario para las rutas

// Guarda un mensaje RFC 5322; devuelve true si pudo escribirlo completo.
bool guardar_en_buzon(const std::string& dir_buzones, const std::string& destinatario, 
                      const std::string& remitente, const std::string& texto);

// Devuelve la lista de rutas de los mensajes de un usuario, ordenados por nombre
std::vector<std::filesystem::path> obtener_mensajes(const std::string& dir_buzones, const std::string& apodo);

// Borra un archivo específico del buzón
void borrar_mensaje(const std::filesystem::path& ruta);

#endif
