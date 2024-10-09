#include "ui.h"
#include <gui/canvas.h>
#include <furi.h>

static void draw_main_menu(Canvas* canvas, OllamaAppState* state) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Ollama AI");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 26, state->menu_index == 0 ? "> Scan WiFi" : "  Scan WiFi");
    canvas_draw_str(canvas, 2, 38, state->menu_index == 1 ? "> Show URL" : "  Show URL");
    canvas_draw_str(canvas, 2, 50, state->menu_index == 2 ? "> Start Chat" : "  Start Chat");
}

static void draw_show_url(Canvas* canvas, OllamaAppState* state) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Server URL");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, state->server_url);
}

static void draw_chat(Canvas* canvas, OllamaAppState* state) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Chat");
    canvas_set_font(canvas, FontSecondary);
    
    // Draw chat messages
    int y = 20;
    for (int i = 0; i < state->chat_message_count; i++) {
        canvas_draw_str(canvas, 2, y, state->chat_messages[i].is_user ? "You: " : "AI: ");
        y += 10;
        canvas_draw_str(canvas, 2, y, state->chat_messages[i].content);
        y += 10;
    }
    
    // Draw input field
    canvas_draw_line(canvas, 0, 50, 128, 50);
    canvas_draw_str(canvas, 2, 62, state->current_message);
    if (strlen(state->current_message) < MAX_MESSAGE_LENGTH - 1) {
        canvas_draw_str(canvas, 2 + canvas_string_width(canvas, state->current_message), 62, "_");
    }
}

static void draw_wifi_scan(Canvas* canvas, OllamaAppState* state) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "WiFi Scan");
    canvas_set_font(canvas, FontSecondary);

    if(state->current_state == AppStateWifiScan) {
        canvas_draw_str(canvas, 2, 26, "Scanning networks...");
    } else if(state->current_state == AppStateWifiSelect) {
        if(state->network_count == 0) {
            canvas_draw_str(canvas, 2, 26, "No networks found");
        } else {
            canvas_draw_str(canvas, 2, 26, "Select a network:");
            int start_index = (state->selected_network / 3) * 3;
            for(int i = start_index; i < start_index + 3 && i < state->network_count; i++) {
                char network_info[32];
                snprintf(network_info, sizeof(network_info), "%s (%ld dBm)", 
                         state->networks[i].ssid, (long)state->networks[i].rssi);
                canvas_draw_str(canvas, 2, 38 + (i - start_index) * 10, 
                                i == state->selected_network ? "> " : "  ");
                canvas_draw_str(canvas, 14, 38 + (i - start_index) * 10, network_info);
            }
        }
    }
}

static void draw_keyboard(Canvas* canvas, OllamaAppState* state) {
    const char* keyboard = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*()";
    int key_width = 8;
    int key_height = 10;
    int keys_per_row = 10;

    canvas_set_font(canvas, FontSecondary);
    
    // Draw the entered password
    canvas_draw_str(canvas, 2, 10, "Enter Password:");
    canvas_draw_str(canvas, 2, 22, state->wifi_password);
    canvas_draw_str(canvas, 2 + canvas_string_width(canvas, state->wifi_password), 22, "_");

    // Draw the keyboard
    for(size_t i = 0; i < strlen(keyboard); i++) {
        int row = i / keys_per_row;
        int col = i % keys_per_row;
        int x = col * key_width + 2;
        int y = row * key_height + 34;

        if(i == state->keyboard_index) {
            canvas_draw_frame(canvas, x, y, key_width, key_height);
        }
        canvas_draw_glyph(canvas, x + 2, y + 8, keyboard[i]);
    }
}

static void draw_wifi_connect(Canvas* canvas, OllamaAppState* state) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "WiFi Connection");
    canvas_set_font(canvas, FontSecondary);
    if (state->wifi_connected) {
        canvas_draw_str(canvas, 2, 26, "Connected to:");
        canvas_draw_str(canvas, 2, 38, state->wifi_ssid);
    } else {
        canvas_draw_str(canvas, 2, 26, "Connecting to:");
        canvas_draw_str(canvas, 2, 38, state->wifi_ssid);
        canvas_draw_str(canvas, 2, 50, "Please wait...");
    }
}

void ollama_app_draw_callback(Canvas* canvas, void* ctx) {
    FURI_LOG_D("OllamaApp", "Draw callback called with context: %p", ctx);
    canvas_clear(canvas);

    if (ctx == NULL) {
        FURI_LOG_E("OllamaApp", "State is NULL in draw callback");
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 32, "Error: NULL state");
        return;
    }

    OllamaAppState* state = (OllamaAppState*)ctx;
    FURI_LOG_D("OllamaApp", "Current state: %d", state->current_state);

    switch(state->current_state) {
        case AppStateMainMenu:
            draw_main_menu(canvas, state);
            break;
        case AppStateShowURL:
            draw_show_url(canvas, state);
            break;
        case AppStateChat:
            draw_chat(canvas, state);
            break;
        case AppStateWifiScan:
        case AppStateWifiSelect:
            draw_wifi_scan(canvas, state);
            break;
        case AppStateWifiConnect:
            draw_wifi_connect(canvas, state);
            break;
        case AppStateWifiPassword:
            draw_keyboard(canvas, state);
            break;
        default:
            FURI_LOG_E("OllamaApp", "Unknown state: %d", state->current_state);
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 2, 32, "Unknown state");
            break;
    }
}