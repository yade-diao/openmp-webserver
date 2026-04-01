#pragma once

#include <string>

unsigned long long backend_serial_checksum(const std::string& data, int rounds);
