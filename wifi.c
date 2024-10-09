#include "ollama_app_i.h"
#include <furi.h>
#include <furi_hal.h>
#include "helpers/uart_helper.h"
#include <storage/storage.h>
#include <stream/stream.h>
#include <stream/buffered_file_stream.h>

static UartHelper* uart_helper = NULL;

void wifi_init() {
    FURI_LOG_I("WiFi", "Initializing WiFi module");
    if (uart_helper == NULL) {
        uart_helper = uart_helper_alloc();
        if (uart_helper == NULL) {
            FURI_LOG_E("WiFi", "Failed to allocate UART helper");
        } else {
            FURI_LOG_I("WiFi", "UART helper allocated successfully");
        }
    } else {
        FURI_LOG_W("WiFi", "UART helper already initialized");
    }
}

void wifi_deinit() {
    FURI_LOG_I("WiFi", "Deinitializing WiFi module");
    if (uart_helper != NULL) {
        uart_helper_free(uart_helper);
        uart_helper = NULL;
        FURI_LOG_I("WiFi", "UART helper freed");
    } else {
        FURI_LOG_W("WiFi", "UART helper was already NULL");
    }
}

void wifi_connect_known(OllamaAppState* state) {
    FURI_LOG_I("WiFi", "Attempting to connect to known APs");
    
    if (uart_helper == NULL) {
        FURI_LOG_E("WiFi", "UART helper not initialized");
        state->current_state = AppStateMainMenu;
        state->ui_update_needed = true;
        return;
    }
    
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if (storage == NULL) {
        FURI_LOG_E("WiFi", "Failed to open storage");
        state->current_state = AppStateMainMenu;
        state->ui_update_needed = true;
        return;
    }

    Stream* file_stream = buffered_file_stream_alloc(storage);
    if (file_stream == NULL) {
        FURI_LOG_E("WiFi", "Failed to allocate file stream");
        furi_record_close(RECORD_STORAGE);
        state->current_state = AppStateMainMenu;
        state->ui_update_needed = true;
        return;
    }
    
    if (!storage_file_exists(storage, WIFI_CONFIG_PATH)) {
        FURI_LOG_E("WiFi", "SavedAPs.txt does not exist");
        state->current_state = AppStateMainMenu;
        stream_free(file_stream);
        furi_record_close(RECORD_STORAGE);
        state->ui_update_needed = true;
        return;
    }
    
    if (buffered_file_stream_open(file_stream, WIFI_CONFIG_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FuriString* line = furi_string_alloc();
        bool ap_found = false;
        
        while (stream_read_line(file_stream, line)) {
            const char* full_line = furi_string_get_cstr(line);
            char ssid[MAX_SSID_LENGTH];
            char password[MAX_PASSWORD_LENGTH];
            
            if (sscanf(full_line, "%[^//]//%s", ssid, password) == 2) {
                ap_found = true;
                FURI_LOG_I("WiFi", "Attempting to connect to %s", ssid);
                
                char connect_cmd[MAX_SSID_LENGTH + MAX_PASSWORD_LENGTH + 10];
                snprintf(connect_cmd, sizeof(connect_cmd), "CONNECT %s %s\r\n", ssid, password);
                uart_helper_send(uart_helper, connect_cmd, strlen(connect_cmd));
                
                // Wait for a response (you might need to implement a proper response handling mechanism)
                furi_delay_ms(5000);
                
                if (state->wifi_connected) {
                    FURI_LOG_I("WiFi", "Successfully connected to %s", ssid);
                    break;
                }
            }
            
            furi_string_reset(line);
        }
        
        furi_string_free(line);
        
        if (!ap_found) {
            FURI_LOG_E("WiFi", "No valid APs found in SavedAPs.txt");
            state->current_state = AppStateMainMenu;
        }
    } else {
        FURI_LOG_E("WiFi", "Failed to open SavedAPs.txt");
        state->current_state = AppStateMainMenu;
    }
    
    buffered_file_stream_close(file_stream);
    stream_free(file_stream);
    furi_record_close(RECORD_STORAGE);
    
    if (!state->wifi_connected) {
        FURI_LOG_I("WiFi", "Failed to connect to any known APs");
        state->current_state = AppStateMainMenu;
    }
    
    state->ui_update_needed = true;
}

static void process_line(FuriString* line, void* context) {
    OllamaAppState* state = (OllamaAppState*)context;
    const char* line_str = furi_string_get_cstr(line);

    FURI_LOG_I("WiFi", "Processing line: %s", line_str);

    if(strcmp(line_str, "SCAN_COMPLETE") == 0) {
        FURI_LOG_I("WiFi", "Scan complete, found %d networks", state->network_count);
        if(state->network_count > 0) {
            state->current_state = AppStateWifiSelect;
            state->selected_network = 0;
            FURI_LOG_I("WiFi", "Transitioning to AppStateWifiSelect");
        } else {
            state->current_state = AppStateMainMenu;
            FURI_LOG_I("WiFi", "No networks found, returning to AppStateMainMenu");
        }
        state->ui_update_needed = true;
        FURI_LOG_I("WiFi", "UI update flagged, new state: %d", state->current_state);
    } else if(strncmp(line_str, "NETWORK:", 8) == 0) {
        char* network_info = (char*)line_str + 8;
        char* rssi_str = strrchr(network_info, ',');
        if(rssi_str && state->network_count < MAX_NETWORKS) {
            *rssi_str = '\0';
            rssi_str++;
            strncpy(state->networks[state->network_count].ssid, network_info, MAX_SSID_LENGTH - 1);
            state->networks[state->network_count].ssid[MAX_SSID_LENGTH - 1] = '\0';
            state->networks[state->network_count].rssi = atoi(rssi_str);
            state->network_count++;
            FURI_LOG_I("WiFi", "Added network: %s (%ld dBm)", 
                       state->networks[state->network_count-1].ssid, 
                       (long)state->networks[state->network_count-1].rssi);
        }
    }

    FURI_LOG_I("WiFi", "Current state after processing: %d", state->current_state);
    FURI_LOG_I("WiFi", "UI update needed: %s", state->ui_update_needed ? "Yes" : "No");
}

void wifi_scan(OllamaAppState* state) {
    FURI_LOG_I("WiFi", "Starting WiFi scan");
    state->network_count = 0;
    state->current_state = AppStateWifiScan;
    state->selected_network = 0;
    state->ui_update_needed = true;

    uart_helper_set_callback(uart_helper, process_line, state);
    uart_helper_send(uart_helper, "SCAN\r\n", 6);
}

void wifi_connect(OllamaAppState* state) {
    state->current_state = AppStateWifiConnect;

    char connect_cmd[MAX_SSID_LENGTH + MAX_PASSWORD_LENGTH + 10];
    snprintf(connect_cmd, sizeof(connect_cmd), "CONNECT %s %s\r\n", state->wifi_ssid, state->wifi_password);
    uart_helper_send(uart_helper, connect_cmd, strlen(connect_cmd));
    FURI_LOG_I("WiFi", "Attempting to connect to WiFi: %s", state->wifi_ssid);
}