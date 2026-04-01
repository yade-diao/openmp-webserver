#pragma once

#include <string>

unsigned long long backend_pthread_checksum(const std::string& data, int rounds, int workers);
