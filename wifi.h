#pragma once

#include "ollama_app_i.h"

void wifi_init();
void wifi_deinit();
void wifi_scan(OllamaAppState* state);
void wifi_connect(OllamaAppState* state);