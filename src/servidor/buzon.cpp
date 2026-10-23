#include "servidor/buzon.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

#include <fcntl.h>    // open(), O_CREAT, O_EXCL
#include <unistd.h>   // write(), close(), getpid()

namespace fs = std::filesystem;

namespace {
// Permite diferenciar mensajes creados en el mismo instante.
std::atomic<unsigned long long> contador_mensajes{0};

std::string fecha_rfc5322() {
    const std::time_t ahora = std::time(nullptr);
    std::tm fecha_utc{};
    if (gmtime_r(&ahora, &fecha_utc) == nullptr) {
        return "Thu, 01 Jan 1970 00:00:00 +0000";
    }

    // Se escribe la hora en UTC, de modo que +0000 siempre sea correcto.
    std::ostringstream salida;
    salida.imbue(std::locale::classic());
    salida << std::put_time(&fecha_utc, "%a, %d %b %Y %H:%M:%S +0000");
    return salida.str();
}

bool escribir_completo(int fd, const std::string& contenido) {
    std::size_t enviados = 0;
    while (enviados < contenido.size()) {
        const ssize_t n = ::write(fd, contenido.data() + enviados,
                                  contenido.size() - enviados);
        if (n > 0) {
            enviados += static_cast<std::size_t>(n);
        } else if (n < 0 && errno == EINTR) {
            continue;
        } else {
            return false;
        }
    }
    return true;
}
} // namespace

bool guardar_en_buzon(const std::string& dir_buzones,
                      const std::string& destinatario,
                      const std::string& remitente,
                      const std::string& texto) {
    const fs::path carpeta = fs::path(dir_buzones) / destinatario;
    std::error_code ec;
    fs::create_directories(carpeta, ec);
    if (ec) {
        std::cerr << "[Buzon] No se pudo crear la carpeta: " << ec.message() << '\n';
        return false;
    }

    // O_EXCL impide sobrescribir otro mensaje si coinciden los nombres.
    fs::path ruta;
    std::string id_unico;
    int fd = -1;
    for (int intento = 0; intento < 100; ++intento) {
        const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        const auto secuencia = contador_mensajes.fetch_add(1);
        std::ostringstream nombre;
        nombre << std::setfill('0') << std::setw(20) << ns
               << '_' << ::getpid() << '_' << secuencia;
        id_unico = nombre.str();
        ruta = carpeta / ("msg_" + id_unico + ".eml");
        fd = ::open(ruta.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (fd >= 0) break;
        if (errno != EEXIST) break;
    }

    if (fd < 0) {
        std::cerr << "[Buzon] No se pudo crear el mensaje: "
                  << std::error_code(errno, std::generic_category()).message() << '\n';
        return false;
    }

    // RFC 5322: cabeceras, linea en blanco y cuerpo, siempre con CRLF.
    std::ostringstream mensaje;
    mensaje << "From: " << remitente << "@chat.ci4835\r\n"
            << "To: " << destinatario << "@chat.ci4835\r\n"
            << "Date: " << fecha_rfc5322() << "\r\n"
            << "Subject: Mensaje privado\r\n"
            << "Message-ID: <" << id_unico << "@chat.ci4835>\r\n"
            << "\r\n"
            << texto << "\r\n";

    const bool escrito = escribir_completo(fd, mensaje.str());
    const bool cerrado = (::close(fd) == 0);
    if (!escrito || !cerrado) {
        std::cerr << "[Buzon] Error al guardar el mensaje.\n";
        fs::remove(ruta, ec); // No dejar mensajes incompletos.
        return false;
    }
    return true;
}

std::vector<fs::path> obtener_mensajes(const std::string& dir_buzones,
                                      const std::string& apodo) {
    std::vector<fs::path> mensajes;
    const fs::path carpeta = fs::path(dir_buzones) / apodo;
    std::error_code ec;
    if (!fs::is_directory(carpeta, ec)) return mensajes;

    for (fs::directory_iterator it(carpeta, ec), fin; !ec && it != fin;
         it.increment(ec)) {
        if (it->is_regular_file(ec) && it->path().extension() == ".eml") {
            mensajes.push_back(it->path());
        }
        if (ec) break;
    }
    std::sort(mensajes.begin(), mensajes.end());
    return mensajes;
}

void borrar_mensaje(const fs::path& ruta) {
    std::error_code ec;
    fs::remove(ruta, ec);
    if (ec) {
        std::cerr << "[Buzon] No se pudo borrar el mensaje: " << ec.message() << '\n';
    }
}
