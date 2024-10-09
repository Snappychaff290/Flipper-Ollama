#include <furi.h>
#include <furi_hal.h>
#include "ollama_app_i.h"
#include "ui.h"
#include "wifi.h"
#include "chat.h"
#include "file_ops.h"
#include "helpers/uart_helper.h"
#include "helpers/ring_buffer.h"
#include <gui/view_dispatcher.h>
#include <gui/modules/text_input.h>
#include <gui/modules/text_input.h>

static void ollama_app_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    OllamaAppState* state = ctx;
    OllamaAppEvent event = {.type = EventTypeKey, .input = *input_event};
    furi_message_queue_put(state->event_queue, &event, FuriWaitForever);
}

static bool main_view_input_callback(InputEvent* event, void* context) {
    FURI_LOG_D("OllamaApp", "Main view input callback called");
    OllamaAppState* state = context;
    if (state == NULL) {
        FURI_LOG_E("OllamaApp", "State is NULL in main view input callback");
        return false;
    }
    ollama_app_input_callback(event, state);
    return true;
}

bool custom_event_callback(void* context, uint32_t event) {
    FURI_LOG_D("OllamaApp", "Custom event: %lu", event);
    UNUSED(context);
    bool consumed = false;

    switch(event) {
        // Handle your custom events here
        default:
            break;
    }

    return consumed;
}

bool navigation_event_callback(void* context) {
    FURI_LOG_D("OllamaApp", "Navigation event");
    OllamaAppState* state = context;
    bool consumed = false;

    switch(state->current_state) {
        case AppStateMainMenu:
            // Exit the application from the main menu
            state->should_exit = true;
            consumed = true;
            break;
        default:
            // For all other states, go back to main menu
            state->current_state = AppStateMainMenu;
            view_dispatcher_switch_to_view(state->view_dispatcher, MainViewID);
            consumed = true;
            break;
    }

    return consumed;
}

static void input_callback(void* context) {
    OllamaAppState* state = context;
    // Handle the input based on the current state
    if (state->current_state == AppStateWifiPassword) {
        strncpy(state->wifi_password, state->input_buffer, MAX_PASSWORD_LENGTH - 1);
        state->wifi_password[MAX_PASSWORD_LENGTH - 1] = '\0';
        wifi_connect(state);
        state->current_state = AppStateWifiConnect;
    } else if (state->current_state == AppStateChat) {
        add_chat_message(state, state->input_buffer, true);
        // TODO: Implement API call to get AI response
        add_chat_message(state, "I am a simulated response.", false);
    }
    state->input_buffer[0] = '\0';
    state->ui_update_needed = true;
}

static bool input_validator(const char* text, FuriString* error, void* context) {
    OllamaAppState* state = context;
    bool valid = true;
    
    if (state->current_state == AppStateWifiPassword) {
        if (strlen(text) < 8) {
            furi_string_set(error, "Password must be\nat least 8 characters");
            valid = false;
        }
    } else if (state->current_state == AppStateChat) {
        if (strlen(text) == 0) {
            furi_string_set(error, "Message cannot\nbe empty");
            valid = false;
        }
    }
    
    return valid;
}

void setup_text_input(OllamaAppState* state) {
    FURI_LOG_D("OllamaApp", "Setting up text input");
    if (state->text_input == NULL) {
        FURI_LOG_E("OllamaApp", "Text input is NULL");
        return;
    }
    text_input_reset(state->text_input);
    
    if (state->current_state == AppStateWifiPassword) {
        text_input_set_header_text(state->text_input, "Enter WiFi Password");
    } else if (state->current_state == AppStateChat) {
        text_input_set_header_text(state->text_input, "Enter your message");
    }
    
    text_input_set_result_callback(
        state->text_input,
        input_callback,
        state,
        state->input_buffer,
        sizeof(state->input_buffer),
        true
    );
    
    text_input_set_validator(state->text_input, input_validator, state);
    FURI_LOG_D("OllamaApp", "Text input setup complete");
}

void ollama_app_state_init(OllamaAppState* state) {
    FURI_LOG_D("OllamaApp", "Initializing state");
    state->should_exit = false;
    memset(state, 0, sizeof(OllamaAppState));
    state->current_state = AppStateMainMenu;
    state->menu_index = 0;
    state->wifi_connected = false;
    strncpy(state->user_name, "User", MAX_SSID_LENGTH - 1);
    state->user_name[MAX_SSID_LENGTH - 1] = '\0';
    state->event_queue = furi_message_queue_alloc(8, sizeof(OllamaAppEvent));
    if (state->event_queue == NULL) {
        FURI_LOG_E("OllamaApp", "Failed to allocate event queue");
    }
    state->text_input = text_input_alloc();
    if (state->text_input == NULL) {
        FURI_LOG_E("OllamaApp", "Failed to allocate text input");
    }
    FURI_LOG_D("OllamaApp", "State initialized");
}

void ollama_app_state_free(OllamaAppState* state) {
    furi_message_queue_free(state->event_queue);
    text_input_free(state->text_input);
}

bool ollama_app_handle_key_event(OllamaAppState* state, InputEvent* event) {
    bool running = true;
    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        switch(state->current_state) {
            case AppStateMainMenu:
                if(event->key == InputKeyUp) {
                    state->menu_index = (state->menu_index - 1 + 3) % 3;
                    state->ui_update_needed = true;
                } else if(event->key == InputKeyDown) {
                    state->menu_index = (state->menu_index + 1) % 3;
                    state->ui_update_needed = true;
                } else if(event->key == InputKeyOk) {
                    if(state->menu_index == 0) {
                        state->current_state = AppStateWifiScan;
                        wifi_scan(state);
                    } else if(state->menu_index == 1) {
                        if(read_url_from_file(state)) {
                            state->current_state = AppStateShowURL;
                        }
                    } else if(state->menu_index == 2) {
                        state->current_state = AppStateChat;
                        setup_text_input(state);  // Set up text input for chat
                        view_dispatcher_switch_to_view(state->view_dispatcher, TextInputViewID);
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
                        setup_text_input(state);  // Set up text input for WiFi password
                        view_dispatcher_switch_to_view(state->view_dispatcher, TextInputViewID);
                        state->ui_update_needed = true;
                    }
                }
                break;
            case AppStateWifiPassword:
            case AppStateChat:
                if (event->key == InputKeyBack) {
                    state->current_state = AppStateMainMenu;
                    view_dispatcher_switch_to_view(state->view_dispatcher, MainViewID);
                } else {
                    view_dispatcher_send_custom_event(state->view_dispatcher, event->key);
                }
                state->ui_update_needed = true;
                break;
            case AppStateShowURL:
            case AppStateWifiConnect:
            case AppStateWifiScan:
                if(event->key == InputKeyBack) {
                    state->current_state = AppStateMainMenu;
                    state->ui_update_needed = true;
                }
                break;
        }
    } else if(event->type == InputTypeLong && event->key == InputKeyOk && state->current_state == AppStateWifiPassword) {
        wifi_connect(state);
        state->current_state = AppStateWifiConnect;
        state->ui_update_needed = true;
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
                state->current_state = AppStateMainMenu;
                view_dispatcher_switch_to_view(state->view_dispatcher, MainViewID);
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
    FURI_LOG_I("OllamaApp", "Starting application");
    
    OllamaAppState* state = malloc(sizeof(OllamaAppState));
    if (state == NULL) {
        FURI_LOG_E("OllamaApp", "Failed to allocate state");
        return 255;
    }
    
    FURI_LOG_D("OllamaApp", "Initializing state");
    ollama_app_state_init(state);
    
    // Open GUI record
    state->gui = furi_record_open(RECORD_GUI);
    if (state->gui == NULL) {
        FURI_LOG_E("OllamaApp", "Failed to open GUI");
        ollama_app_state_free(state);
        free(state);
        return 255;
    }

    FURI_LOG_D("OllamaApp", "Allocating view dispatcher");
    state->view_dispatcher = view_dispatcher_alloc();
    if (state->view_dispatcher == NULL) {
        FURI_LOG_E("OllamaApp", "Failed to allocate view dispatcher");
        furi_record_close(RECORD_GUI);
        ollama_app_state_free(state);
        free(state);
        return 255;
    }

    view_dispatcher_enable_queue(state->view_dispatcher);
    view_dispatcher_set_event_callback_context(state->view_dispatcher, state);
    view_dispatcher_set_custom_event_callback(state->view_dispatcher, custom_event_callback);
    view_dispatcher_set_navigation_event_callback(state->view_dispatcher, navigation_event_callback);

    // Initialize WiFi module
    wifi_init();

    // Add main view to view dispatcher
    View* main_view = view_alloc();
    if (main_view == NULL) {
        FURI_LOG_E("OllamaApp", "Failed to allocate main view");
        view_dispatcher_free(state->view_dispatcher);
        furi_record_close(RECORD_GUI);
        ollama_app_state_free(state);
        free(state);
        return 255;
    }
    view_set_context(main_view, state);
    view_set_draw_callback(main_view, ollama_app_draw_callback);
    view_set_input_callback(main_view, main_view_input_callback);
    view_dispatcher_add_view(state->view_dispatcher, MainViewID, main_view);
    FURI_LOG_D("OllamaApp", "Added main view to dispatcher");

    // Add text input view to view dispatcher
    if (state->text_input != NULL) {
        view_dispatcher_add_view(state->view_dispatcher, TextInputViewID, text_input_get_view(state->text_input));
    } else {
        FURI_LOG_E("OllamaApp", "Text input is NULL");
    }

    view_dispatcher_attach_to_gui(state->view_dispatcher, state->gui, ViewDispatcherTypeFullscreen);
    FURI_LOG_I("OllamaApp", "Switching to main view");
    view_dispatcher_switch_to_view(state->view_dispatcher, MainViewID);

    FURI_LOG_I("OllamaApp", "Entering main loop");
    view_dispatcher_run(state->view_dispatcher);

    FURI_LOG_I("OllamaApp", "Cleaning up");
    view_dispatcher_remove_view(state->view_dispatcher, MainViewID);
    if (state->text_input != NULL) {
        view_dispatcher_remove_view(state->view_dispatcher, TextInputViewID);
    }
    view_free(main_view);

    view_dispatcher_free(state->view_dispatcher);
    furi_record_close(RECORD_GUI);
    ollama_app_state_free(state);
    free(state);

    // Deinitialize WiFi module
    wifi_deinit();

    return 0;
}