#pragma once

#include "ollama_app_i.h"

bool read_url_from_file(OllamaAppState* state);
bool read_wifi_config(OllamaAppState* state);
void save_ap(OllamaAppState* state);