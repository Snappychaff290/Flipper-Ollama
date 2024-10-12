#pragma once

#include "ollama_app_i.h"

void ollama_app_draw_callback(Canvas* canvas, void* ctx);
void draw_keyboard(Canvas* canvas, OllamaAppState* state);
void process_keyboard_input(OllamaAppState* state, InputEvent* event);
int find_nearest_key(const char* current_layout, int x, int y);
void handle_key_press(OllamaAppState* state);