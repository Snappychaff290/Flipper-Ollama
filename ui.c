#include "ui.h"
#include <gui/canvas.h>
#include <furi.h>

#define KEYBOARD_WIDTH 14
#define KEYBOARD_HEIGHT 3

static void draw_main_menu(Canvas* canvas, OllamaAppState* state) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Ollama AI");
    
    // Draw WiFi connection icon
    if (state->wifi_connected) {
        canvas_draw_str(canvas, 100, 10, "CON");
    }
    
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 26, state->menu_index == 0 ? "> Scan WiFi" : "  Scan WiFi");
    canvas_draw_str(canvas, 2, 38, state->menu_index == 1 ? "> Connect Known AP" : "  Connect Known AP");
    canvas_draw_str(canvas, 2, 50, state->menu_index == 2 ? "> Show URL" : "  Show URL");
    canvas_draw_str(canvas, 2, 62, state->menu_index == 3 ? "> Start Chat" : "  Start Chat");
}

static void draw_show_url(Canvas* canvas, OllamaAppState* state) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Server URL");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, state->server_url);
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

static void draw_chat(Canvas* canvas, OllamaAppState* state) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Chat");
    
    canvas_set_font(canvas, FontSecondary);
    int y = 22;
    for(int i = 0; i < state->chat_message_count; i++) {
        canvas_draw_str(canvas, 2, y, state->chat_messages[i].is_user ? "You: " : "AI: ");
        y += 10;
        canvas_draw_str(canvas, 2, y, state->chat_messages[i].content);
        y += 12;
    }

    canvas_draw_str(canvas, 2, 62, "Press OK to chat");
}

static void draw_wifi_connect(Canvas* canvas, OllamaAppState* state) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "WiFi Connection");
    
    canvas_set_font(canvas, FontSecondary);
    
    // Split the status message into multiple lines if necessary
    char* status = state->status_message;
    int y = 30;
    char line[25];
    int i = 0;
    while (*status) {
        if (i == 24 || *status == '\n') {
            line[i] = '\0';
            canvas_draw_str(canvas, 2, y, line);
            y += 12;
            i = 0;
            if (*status == '\n') status++;
        } else {
            line[i++] = *status++;
        }
    }
    if (i > 0) {
        line[i] = '\0';
        canvas_draw_str(canvas, 2, y, line);
    }
    
    if (state->wifi_connected) {
        canvas_draw_str(canvas, 2, 62, "Connected to WiFi");
    } else {
        // Show a simple animation to indicate ongoing process
        static int anim_frame = 0;
        char anim_chars[] = {'|', '/', '-', '\\'};
        char anim_str[2] = {anim_chars[anim_frame], '\0'};
        canvas_draw_str(canvas, 2, 62, "Connecting ");
        canvas_draw_str(canvas, 62, 62, anim_str);
        anim_frame = (anim_frame + 1) % 4;
    }
}

void ollama_app_draw_callback(Canvas* canvas, void* ctx) {
    OllamaAppState* state = ctx;
    canvas_clear(canvas);

    switch(state->current_state) {
        case AppStateMainMenu:
            draw_main_menu(canvas, state);
            break;
        case AppStateShowURL:
            draw_show_url(canvas, state);
            break;
        case AppStateWifiScan:
        case AppStateWifiSelect:
            draw_wifi_scan(canvas, state);
            break;
        case AppStateWifiConnect:
        case AppStateWifiConnectKnown:
        case AppStateWifiSaveAndConnect:
            draw_wifi_connect(canvas, state);
            break;
        case AppStateChat:
            draw_chat(canvas, state);
            break;
        case AppStateWifiPassword:
            // TextInput view is handled separately
            break;
    }
}