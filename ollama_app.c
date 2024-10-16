#include <furi.h>
#include <furi_hal.h>
#include "ollama_app_i.h"
#include "ui.h"
#include "wifi.h"
#include "chat.h"
#include "file_ops.h"
#include "helpers/uart_helper.h"
#include "helpers/ring_buffer.h"

static void ollama_app_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    OllamaAppState* state = ctx;
    OllamaAppEvent event = {.type = EventTypeKey, .input = *input_event};
    furi_message_queue_put(state->event_queue, &event, FuriWaitForever);
}

void ollama_app_state_init(OllamaAppState* state) {
    memset(state, 0, sizeof(OllamaAppState));
    state->current_state = AppStateMainMenu;
    state->menu_index = 0;
    state->wifi_connected = false;
    strncpy(state->user_name, "User", MAX_SSID_LENGTH - 1);
    state->user_name[MAX_SSID_LENGTH - 1] = '\0';
    state->event_queue = furi_message_queue_alloc(8, sizeof(OllamaAppEvent));
    state->ui_update_needed = false;
    state->keyboard_mode = KeyboardModeLower;
    state->keyboard_cursor_x = 0;
    state->keyboard_cursor_y = 0;
    state->caps_lock = false;
    state->special_chars_mode = false;
}

void ollama_app_state_free(OllamaAppState* state) {
    furi_message_queue_free(state->event_queue);
    view_port_enabled_set(state->view_port, false);
    gui_remove_view_port(state->gui, state->view_port);
    view_port_free(state->view_port);
    furi_record_close(RECORD_GUI);
}

bool ollama_app_handle_key_event(OllamaAppState* state, InputEvent* event) {
    bool running = true;
    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        switch(state->current_state) {
            case AppStateMainMenu:
                if(event->key == InputKeyUp) {
                    state->menu_index = (state->menu_index - 1 + 4) % 4;
                    state->ui_update_needed = true;
                } else if(event->key == InputKeyDown) {
                    state->menu_index = (state->menu_index + 1) % 4;
                    state->ui_update_needed = true;
                } else if(event->key == InputKeyOk) {
                    if(state->menu_index == 0) {
                        state->current_state = AppStateWifiScan;
                        wifi_scan(state);
                    } else if(state->menu_index == 1) {
                        state->current_state = AppStateWifiConnectKnown;
                        wifi_connect_known(state);
                    } else if(state->menu_index == 2) {
                        if(read_url_from_file(state)) {
                            state->current_state = AppStateShowURL;
                        }
                    } else if(state->menu_index == 3) {
                        state->current_state = AppStateChat;
                        state->chat_message_count = 0;
                        state->current_message[0] = '\0';
                        state->cursor_position = 0;
                    }
                    state->ui_update_needed = true;
                }
                break;
            case AppStateWifiSelect:
                if(event->key == InputKeyUp) {
                    if(state->selected_network > 0) {
                        state->selected_network--;
                        state->ui_update_needed = true;
                    }
                } else if(event->key == InputKeyDown) {
                    if(state->selected_network < state->network_count - 1) {
                        state->selected_network++;
                        state->ui_update_needed = true;
                    }
                } else if(event->key == InputKeyOk) {
                    if(state->network_count > 0) {
                        strncpy(state->wifi_ssid, state->networks[state->selected_network].ssid, MAX_SSID_LENGTH - 1);
                        state->wifi_ssid[MAX_SSID_LENGTH - 1] = '\0';
                        state->current_state = AppStateWifiPassword;
                        state->keyboard_index = 0;
                        memset(state->wifi_password, 0, sizeof(state->wifi_password));
                        state->ui_update_needed = true;
                    }
                }
                break;
            case AppStateChat:
            case AppStateWifiPassword:
                process_keyboard_input(state, event);
                break;
            case AppStateShowURL:
            case AppStateWifiConnect:
            case AppStateWifiScan:
            case AppStateWifiConnectKnown:
            case AppStateOllamaResponse:  // Add this case
                if(event->key == InputKeyBack) {
                    if(state->current_state == AppStateOllamaResponse) {
                        state->current_state = AppStateChat;
                    } else {
                        state->current_state = AppStateMainMenu;
                    }
                    state->ui_update_needed = true;
                }
                break;
        }
    } else if(event->type == InputTypeLong && event->key == InputKeyOk) {
        switch(state->current_state) {
            case AppStateWifiPassword:
                wifi_connect(state);
                state->current_state = AppStateWifiConnect;
                state->ui_update_needed = true;
                break;
            case AppStateWifiConnectKnown:
                // No specific action needed for long press in this state
                break;
            default:
                break;
        }
    } else if(event->type == InputTypeShort && event->key == InputKeyBack) {
        // Global back button handling
        switch(state->current_state) {
            case AppStateMainMenu:
                running = false;
                break;
            case AppStateShowURL:
            case AppStateChat:
            case AppStateWifiConnect:
            case AppStateWifiScan:
            case AppStateWifiSelect:
            case AppStateWifiPassword:
            case AppStateWifiConnectKnown:
            case AppStateOllamaResponse:  // Add this case
                state->current_state = AppStateMainMenu;
                state->ui_update_needed = true;
                break;
        }
    }
    return running;
}

void ollama_app_handle_tick_event(OllamaAppState* state) {
    if(state->current_state == AppStateWifiConnect && !state->wifi_connected) {
        wifi_connect(state);
        if(state->wifi_connected) {
            save_ap(state);
            state->current_state = AppStateMainMenu;
        }
    }
}

int32_t ollama_app(void* p) {
    UNUSED(p);
    OllamaAppState* state = malloc(sizeof(OllamaAppState));
    ollama_app_state_init(state);

    // Initialize WiFi module
    wifi_init();

    // Configure view port
    state->view_port = view_port_alloc();
    view_port_draw_callback_set(state->view_port, ollama_app_draw_callback, state);
    view_port_input_callback_set(state->view_port, ollama_app_input_callback, state);

    // Register view port in GUI
    state->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(state->gui, state->view_port, GuiLayerFullscreen);

    // Main loop
    OllamaAppEvent event;
    bool running = true;
    while(running) {
        FuriStatus status = furi_message_queue_get(state->event_queue, &event, 100);
        if(status == FuriStatusOk) {
            switch(event.type) {
                case EventTypeKey:
                    running = ollama_app_handle_key_event(state, &event.input);
                    break;
                case EventTypeTick:
                    ollama_app_handle_tick_event(state);
                    break;
                default:
                    break;
            }
        }

        // Check if UI update is needed
        if(state->ui_update_needed) {
            view_port_update(state->view_port);
            state->ui_update_needed = false;
        }
    }

    // Cleanup
    view_port_enabled_set(state->view_port, false);
    gui_remove_view_port(state->gui, state->view_port);
    view_port_free(state->view_port);
    furi_record_close(RECORD_GUI);
    ollama_app_state_free(state);
    free(state);

    // Clean up WiFi module
    wifi_cleanup();

    return 0;
}