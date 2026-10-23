#include "comun/protocolo.h"

std::vector<std::string> extraer_lineas(std::string& buffer) {
    std::vector<std::string> lineas;
    size_t pos = 0;

    // Buscamos el CRLF (\r\n) en el búfer
    while ((pos = buffer.find("\r\n")) != std::string::npos) {
        // Extraemos la línea (sin el CRLF)
        std::string linea = buffer.substr(0, pos);
        
        // Si la línea supera los 512 bytes, la ignoramos (el PDF pide un 502)
        // (El manejo del 502 lo haremos en el servidor, aquí solo la extraemos)
        lineas.push_back(linea);
        
        // Borramos la línea procesada y el CRLF del búfer
        buffer.erase(0, pos + 2);
    }
    return lineas;
}