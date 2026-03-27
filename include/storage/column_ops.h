#pragma once

#include "page.h"
#include <cstdint>

bool insert_fixed(Page* page, const void* data, uint16_t size);
bool insert_variable(Page* page, const uint8_t* data, uint16_t size);