#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#include <string>
#include <vector>
#include <cstddef>

// Extrae todas las líneas completas (que terminan en \r\n) de un búfer.
// Retorna un vector con las líneas SIN el \r\n.
// El parámetro 'buffer' se modifica, eliminando los bytes ya procesados.
std::vector<std::string> extraer_lineas(std::string& buffer);

#endif