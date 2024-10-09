#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <gui/modules/text_input.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/text_input.h>

#define MAX_URL_LENGTH 256
#define MAX_MESSAGE_LENGTH 128
#define MAX_CHAT_MESSAGES 5
#define MAX_SSID_LENGTH 32
#define MAX_PASSWORD_LENGTH 64
#define MAX_NETWORKS 10

#define URL_FILE_PATH EXT_PATH("ollama/server_url.txt")
#define WIFI_CONFIG_PATH EXT_PATH("ollama/SavedAPs.txt")
#define MAX_INPUT_LENGTH 128  // or whatever length you prefer

typedef enum {
    AppStateMainMenu,
    AppStateShowURL,
    AppStateChat,
    AppStateWifiConnect,
    AppStateWifiScan,
    AppStateWifiSelect,
    AppStateWifiPassword,
} AppState;

typedef struct {
    char content[MAX_MESSAGE_LENGTH];
    bool is_user;
} ChatMessage;

typedef enum {
    MainViewID,
    TextInputViewID,
    // ... other view IDs ...
} ViewID;

typedef struct {
    char ssid[MAX_SSID_LENGTH];
    int32_t rssi;
} WiFiNetwork;

typedef struct {
    FuriMessageQueue* event_queue;
    ViewPort* view_port;
    Gui* gui;
    AppState current_state;
    int8_t menu_index;
    char server_url[MAX_URL_LENGTH];
    ChatMessage chat_messages[MAX_CHAT_MESSAGES];
    uint8_t chat_message_count;
    char current_message[MAX_MESSAGE_LENGTH];
    uint8_t cursor_position;
    char wifi_ssid[MAX_SSID_LENGTH];
    char wifi_password[MAX_PASSWORD_LENGTH];
    bool wifi_connected;
    char user_name[MAX_SSID_LENGTH];
    WiFiNetwork networks[MAX_NETWORKS];
    uint8_t network_count;
    uint8_t selected_network;
    uint8_t keyboard_index;
    bool ui_update_needed;
    TextInput* text_input;
    char input_buffer[MAX_INPUT_LENGTH];  // Define MAX_INPUT_LENGTH as needed
    ViewDispatcher* view_dispatcher;
    bool should_exit;
} OllamaAppState;

typedef enum {
    EventTypeTick,
    EventTypeKey,
    EventTypeUpdateUI,  // Add this line
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} OllamaAppEvent;

void ollama_app_state_init(OllamaAppState* state);
void ollama_app_state_free(OllamaAppState* state);
bool ollama_app_handle_key_event(OllamaAppState* state, InputEvent* event);
void ollama_app_handle_tick_event(OllamaAppState* state);