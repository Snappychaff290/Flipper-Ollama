#include "ui.h"
#include <gui/canvas.h>
#include <furi.h>

#define KEYBOARD_WIDTH 14
#define KEYBOARD_HEIGHT 3

static void draw_main_menu(Canvas* canvas, OllamaAppState* state) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Ollama AI");
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
const char* keyboard_layout_normal = 
    "qwertyuiop0123"
    "asdfghjkl  456"
    "@zxcvbnm   789";

const char* keyboard_layout_special = 
    "!@#$%^&*()_+-="
    "[]{}\\|;:'  \",."
    " /@?`~<>      ";

const KeyboardKey special_keys[] = {
    {'\b', 9, 1, 2, 1, "<-"},  // Backspace
    {'\n', 9, 2, 2, 1, "->"}, // Save
    {' ', 8, 2, 1, 1, " "},     // Space
    {'\t', 0, 2, 1, 1, "@"},     // Special characters toggle
};
void draw_keyboard(Canvas* canvas, OllamaAppState* state) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Enter your name");

    // Draw input box
    canvas_draw_frame(canvas, 0, 15, 128, 11);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 24, state->current_message);

    const char* current_layout = state->special_chars_mode ? keyboard_layout_special : keyboard_layout_normal;

    canvas_set_font(canvas, FontKeyboard);

    for(int y = 0; y < KEYBOARD_HEIGHT; y++) {
        for(int x = 0; x < KEYBOARD_WIDTH; x++) {
            char c = current_layout[y * KEYBOARD_WIDTH + x];
            if(c != ' ') {
                uint8_t key_x = x * 9;
                uint8_t key_y = y * 11 + 30;
                if(state->keyboard_cursor_x == x && state->keyboard_cursor_y == y) {
                    canvas_draw_box(canvas, key_x, key_y, 8, 10);
                    canvas_set_color(canvas, ColorWhite);
                }
                canvas_draw_glyph(canvas, key_x + 2, key_y + 8, c);
                canvas_set_color(canvas, ColorBlack);
            }
        }
    }

    for(size_t i = 0; i < sizeof(special_keys) / sizeof(special_keys[0]); i++) {
        const KeyboardKey* key = &special_keys[i];
        uint8_t key_x = key->x * 9;
        uint8_t key_y = key->y * 11 + 30;
        uint8_t key_width = key->width * 9 - 1;
        uint8_t key_height = key->height * 11 - 1;

        if(state->keyboard_cursor_x == key->x && state->keyboard_cursor_y == key->y) {
            canvas_draw_box(canvas, key_x, key_y, key_width, key_height);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_draw_frame(canvas, key_x, key_y, key_width, key_height);
        }

        canvas_draw_str_aligned(canvas, key_x + key_width / 2, key_y + key_height / 2, AlignCenter, AlignCenter, key->label);
        canvas_set_color(canvas, ColorBlack);
    }

    // Indicate special characters mode
    if(state->special_chars_mode) {
        canvas_draw_str(canvas, 2, 62, "");
    }
}

void handle_key_press(OllamaAppState* state, bool is_uppercase) {
    char selected_char = 0;
    bool is_special_key = false;

    for(size_t i = 0; i < sizeof(special_keys) / sizeof(special_keys[0]); i++) {
        const KeyboardKey* key = &special_keys[i];
        if(state->keyboard_cursor_x == key->x && state->keyboard_cursor_y == key->y) {
            selected_char = key->character;
            is_special_key = true;
            break;
        }
    }

    if(!is_special_key) {
        const char* current_layout = state->special_chars_mode ? keyboard_layout_special : keyboard_layout_normal;
        selected_char = current_layout[state->keyboard_cursor_y * KEYBOARD_WIDTH + state->keyboard_cursor_x];
        if(is_uppercase && selected_char >= 'a' && selected_char <= 'z') {
            selected_char = selected_char - 'a' + 'A';
        }
    }

    if(selected_char == '\b') {  // Backspace
        if(strlen(state->current_message) > 0) {
            state->current_message[strlen(state->current_message) - 1] = '\0';
        }
    } else if(selected_char == '\n') {  // Save
        // Handle save action here
    } else if(selected_char == '\t') {  // Toggle keyboard mode
        state->special_chars_mode = !state->special_chars_mode;
    } else if(selected_char != '\0') {
        if(strlen(state->current_message) < MAX_MESSAGE_LENGTH - 1) {
            char temp[2] = {selected_char, '\0'};
            strcat(state->current_message, temp);
        }
    }

    state->ui_update_needed = true;
}

void process_keyboard_input(OllamaAppState* state, InputEvent* event) {
    static bool uppercase_typed = false;

    FURI_LOG_I("KEYBOARD", "Event type: %d, Key: %d", event->type, event->key);

    switch(event->type) {
        case InputTypeShort:
            FURI_LOG_I("KEYBOARD", "Short input detected");
            switch(event->key) {
                case InputKeyUp:
                    if(state->keyboard_cursor_y > 0) state->keyboard_cursor_y--;
                    FURI_LOG_I("KEYBOARD", "Cursor moved up to %d", state->keyboard_cursor_y);
                    break;
                case InputKeyDown:
                    if(state->keyboard_cursor_y < KEYBOARD_HEIGHT - 1) state->keyboard_cursor_y++;
                    FURI_LOG_I("KEYBOARD", "Cursor moved down to %d", state->keyboard_cursor_y);
                    break;
                case InputKeyLeft:
                    if(state->keyboard_cursor_x > 0) state->keyboard_cursor_x--;
                    FURI_LOG_I("KEYBOARD", "Cursor moved left to %d", state->keyboard_cursor_x);
                    break;
                case InputKeyRight:
                    if(state->keyboard_cursor_x < KEYBOARD_WIDTH - 1) state->keyboard_cursor_x++;
                    FURI_LOG_I("KEYBOARD", "Cursor moved right to %d", state->keyboard_cursor_x);
                    break;
                case InputKeyOk:
                    FURI_LOG_I("KEYBOARD", "Short press detected, lowercase");
                    handle_key_press(state, false); // Short press (lowercase)
                    break;
                default:
                    break;
            }
            uppercase_typed = false;
            break;

        case InputTypeRepeat:
            if(event->key == InputKeyOk && !uppercase_typed) {
                FURI_LOG_I("KEYBOARD", "Repeat detected, typing uppercase");
                handle_key_press(state, true); // Long press (uppercase)
                uppercase_typed = true;
            }
            break;

        case InputTypeRelease:
            uppercase_typed = false;
            break;

        default:
            break;
    }

    state->ui_update_needed = true;
}

// Update the find_nearest_key function to accept current_layout as a parameter
int find_nearest_key(const char* current_layout, int x, int y) {
    int left = x;
    int right = x;
    while (left > 0 && current_layout[y * KEYBOARD_WIDTH + left] == ' ') {
        left--;
    }
    while (right < KEYBOARD_WIDTH - 1 && current_layout[y * KEYBOARD_WIDTH + right] == ' ') {
        right++;
    }
    if (current_layout[y * KEYBOARD_WIDTH + left] != ' ') {
        return left;
    }
    if (current_layout[y * KEYBOARD_WIDTH + right] != ' ') {
        return right;
    }
    return x;  // If no key found, stay at the current position
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
            draw_wifi_connect(canvas, state);
            break;
        case AppStateChat:
        case AppStateWifiPassword:
            draw_keyboard(canvas, state);
            break;
    }
}