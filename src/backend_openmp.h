#pragma once

#include <string>

unsigned long long backend_openmp_checksum(const std::string& data, int rounds, int workers);
