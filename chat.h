#pragma once

#include "ollama_app_i.h"

void add_chat_message(OllamaAppState* state, const char* message, bool is_user);
void process_chat(OllamaAppState* state, InputEvent* event);