
#include "servidor/buzon.h"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <random>
#include <algorithm>

namespace fs = std::filesystem;

// Genera un Message-ID único (simulado)
std::string generar_message_id() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(10000, 99999);
    return "<" + std::to_string(dis(gen)) + "@chat.ci4835>";
}

// Obtiene la fecha actual en formato RFC 5322
std::string obtener_fecha_rfc5322() {
    auto ahora = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(ahora);
    std::tm tm_buf;
    localtime_r(&t, &tm_buf);

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%a, %d %b %Y %H:%M:%S -0400");
    return oss.str();
}

void guardar_en_buzon(const std::string& dir_buzones,
                      const std::string& destinatario,
                      const std::string& remitente,
                      const std::string& texto) {
    // Crear la carpeta del usuario si no existe
    fs::path carpeta_usuario = fs::path(dir_buzones) / destinatario;
    if (!fs::exists(carpeta_usuario)) {
        fs::create_directories(carpeta_usuario);
    }

    // Generar el nombre del archivo usando timestamp
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    fs::path ruta_archivo = carpeta_usuario /
        ("msg_" + std::to_string(ms) + ".eml");

    // Guardar el mensaje en formato RFC 5322
    std::ofstream archivo(ruta_archivo);
    if (archivo.is_open()) {
        archivo << "From: " << remitente << "@chat.ci4835\r\n";
        archivo << "To: " << destinatario << "@chat.ci4835\r\n";
        archivo << "Date: " << obtener_fecha_rfc5322() << "\r\n";
        archivo << "Subject: Mensaje privado\r\n";
        archivo << "Message-ID: " << generar_message_id() << "\r\n";
        archivo << "\r\n";
        archivo << texto << "\r\n";
        archivo.close();
    }
}

std::vector<fs::path> obtener_mensajes(
    const std::string& dir_buzones,
    const std::string& apodo) {

    std::vector<fs::path> mensajes;
    fs::path carpeta = fs::path(dir_buzones) / apodo;

    if (fs::exists(carpeta)) {
        for (const auto& entry : fs::directory_iterator(carpeta)) {
            if (entry.path().extension() == ".eml") {
                mensajes.push_back(entry.path());
            }
        }

        std::sort(mensajes.begin(), mensajes.end());
    }

    return mensajes;
}

void borrar_mensaje(const fs::path& ruta) {
    if (fs::exists(ruta)) {
        fs::remove(ruta);
    }
}
