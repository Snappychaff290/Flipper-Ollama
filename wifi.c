#include "ollama_app_i.h"
#include <furi.h>
#include <furi_hal.h>
#include "helpers/uart_helper.h"
#include <storage/storage.h>
#include <stream/stream.h>
#include <stream/buffered_file_stream.h>

#define UART_READ_TIMEOUT_MS 500
#define WIFI_CONNECT_ATTEMPTS 40
#define WIFI_CONNECT_DELAY_MS 250
#define COMMAND_DELAY_MS 1000

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

void wifi_cleanup() {
    FURI_LOG_I("WiFi", "Cleaning up WiFi module");
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
    
    state->current_state = AppStateWifiConnectKnown;
    state->ui_update_needed = true;
    
    // Send known APs to ESP32
    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* file_stream = buffered_file_stream_alloc(storage);
    
    if (storage_file_exists(storage, WIFI_CONFIG_PATH) &&
        buffered_file_stream_open(file_stream, WIFI_CONFIG_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        
        FuriString* combined_aps = furi_string_alloc();
        FuriString* line = furi_string_alloc();
        
        while (stream_read_line(file_stream, line)) {
            furi_string_trim(line);
            if (furi_string_size(line) > 0) {
                if (furi_string_size(combined_aps) > 0) {
                    furi_string_cat_str(combined_aps, ",");
                }
                furi_string_cat(combined_aps, line);
            }
        }
        
        if (furi_string_size(combined_aps) > 0) {
            FURI_LOG_I("WiFi", "Sending APs to ESP32: %s", furi_string_get_cstr(combined_aps));
            uart_helper_send(uart_helper, furi_string_get_cstr(combined_aps), furi_string_size(combined_aps));
            uart_helper_send(uart_helper, "\r\n", 2);
        }
        
        furi_string_free(line);
        furi_string_free(combined_aps);
        
        buffered_file_stream_close(file_stream);
    } else {
        FURI_LOG_E("WiFi", "Failed to open SavedAPs.txt");
    }
    
    stream_free(file_stream);
    furi_record_close(RECORD_STORAGE);
    
    FuriString* response = furi_string_alloc();
    uint32_t start_time = furi_get_tick();
    uint32_t timeout = furi_ms_to_ticks(30000);  // 30 second timeout
    bool connecting = false;
    
    while (furi_get_tick() - start_time < timeout) {
        if (uart_helper_read(uart_helper, response, UART_READ_TIMEOUT_MS)) {
            const char* resp_str = furi_string_get_cstr(response);
            FURI_LOG_I("WiFi", "Received: %s", resp_str);

            if (strstr(resp_str, "Attempting to auto-connect to known networks...") != NULL) {
                snprintf(state->status_message, sizeof(state->status_message), "Searching for known networks...");
                state->ui_update_needed = true;
            } else if (strstr(resp_str, "Found matching SSID:") != NULL) {
                const char* ssid_start = strstr(resp_str, ": ") + 2;
                strncpy(state->wifi_ssid, ssid_start, MAX_SSID_LENGTH - 1);
                state->wifi_ssid[MAX_SSID_LENGTH - 1] = '\0';
                snprintf(state->status_message, sizeof(state->status_message), "Found network: %s", state->wifi_ssid);
                state->ui_update_needed = true;
            } else if (strstr(resp_str, "Connecting to WiFi") != NULL) {
                connecting = true;
                snprintf(state->status_message, sizeof(state->status_message), "Connecting to %s...", state->wifi_ssid);
                state->ui_update_needed = true;
            } else if (strstr(resp_str, "WiFi connected") != NULL) {
                state->wifi_connected = true;
                snprintf(state->status_message, sizeof(state->status_message), "Connected to %s", state->wifi_ssid);
                state->ui_update_needed = true;
                break;
            } else if (strstr(resp_str, "Failed to connect to") != NULL) {
                snprintf(state->status_message, sizeof(state->status_message), "Failed to connect to %s", state->wifi_ssid);
                state->ui_update_needed = true;
                connecting = false;
            } else if (strstr(resp_str, "No matching networks found") != NULL) {
                strncpy(state->status_message, "No known networks found", sizeof(state->status_message));
                state->ui_update_needed = true;
                break;
            } else if (strstr(resp_str, "WiFi not connected") != NULL && !connecting) {
                strncpy(state->status_message, "Not connected to WiFi", sizeof(state->status_message));
                state->ui_update_needed = true;
                break;
            }
            
            furi_string_reset(response);
        }
        
        furi_delay_ms(100);
    }
    
    if (!state->wifi_connected) {
        FURI_LOG_W("WiFi", "Failed to connect to any known AP");
        if (furi_get_tick() - start_time >= timeout) {
            strncpy(state->status_message, "Connection attempt timed out", sizeof(state->status_message));
        }
    }
    
    FURI_LOG_I("WiFi", "Final status: %s", state->status_message);
    
    furi_string_free(response);
    
    // Display the result for a few seconds before returning to the main menu
    state->ui_update_needed = true;
    furi_delay_ms(3000);  // Display the result for 3 seconds
    
    state->current_state = AppStateMainMenu;
    state->ui_update_needed = true;
}

void wifi_scan(OllamaAppState* state) {
    FURI_LOG_I("WiFi", "Starting WiFi scan");
    state->network_count = 0;
    state->current_state = AppStateWifiScan;
    state->selected_network = 0;
    state->ui_update_needed = true;

    uart_helper_send(uart_helper, "SCAN\r\n", 6);
    furi_delay_ms(COMMAND_DELAY_MS);  // Give ESP32 time to process the command

    FuriString* response = furi_string_alloc();
    bool scan_complete = false;
    uint32_t start_time = furi_get_tick();
    uint32_t timeout = furi_ms_to_ticks(30000); // 30 second timeout

    while (!scan_complete && (furi_get_tick() - start_time < timeout)) {
        if (uart_helper_read(uart_helper, response, UART_READ_TIMEOUT_MS)) {
            const char* resp_str = furi_string_get_cstr(response);
            FURI_LOG_D("WiFi", "Received: %s", resp_str);

            if (strncmp(resp_str, "NETWORK:", 8) == 0) {
                char* network_info = (char*)resp_str + 8;
                char* rssi_str = strrchr(network_info, ',');
                if (rssi_str && state->network_count < MAX_NETWORKS) {
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
            } else if (strcmp(resp_str, "SCAN_COMPLETE") == 0) {
                scan_complete = true;
            } else if (strstr(resp_str, "Scan complete. Networks found:") != NULL) {
                // Extract number of networks found
                int networks_found = 0;
                sscanf(resp_str, "DEBUG: Scan complete. Networks found: %d", &networks_found);
                FURI_LOG_I("WiFi", "ESP32 reported %d networks found", networks_found);
            }
        }
        furi_delay_ms(100);
    }

    furi_string_free(response);

    if (scan_complete) {
        FURI_LOG_I("WiFi", "Scan complete, found %d networks", state->network_count);
        if (state->network_count > 0) {
            state->current_state = AppStateWifiSelect;
            state->selected_network = 0;
        } else {
            state->current_state = AppStateMainMenu;
        }
    } else {
        FURI_LOG_E("WiFi", "Scan timed out");
        state->current_state = AppStateMainMenu;
    }

    state->ui_update_needed = true;
}

void wifi_connect(OllamaAppState* state) {
    state->current_state = AppStateWifiConnect;

    char connect_cmd[MAX_SSID_LENGTH + MAX_PASSWORD_LENGTH + 10];
    snprintf(connect_cmd, sizeof(connect_cmd), "CONNECT %s %s\r\n", state->wifi_ssid, state->wifi_password);
    uart_helper_send(uart_helper, connect_cmd, strlen(connect_cmd));
    FURI_LOG_I("WiFi", "Attempting to connect to WiFi: %s", state->wifi_ssid);

    FuriString* response = furi_string_alloc();
    bool connected = false;
    for (int i = 0; i < WIFI_CONNECT_ATTEMPTS; i++) {
        if (uart_helper_read(uart_helper, response, UART_READ_TIMEOUT_MS)) {
            const char* resp_str = furi_string_get_cstr(response);
            FURI_LOG_D("WiFi", "Received: %s", resp_str);
            if (strstr(resp_str, "Connected successfully to") != NULL) {
                connected = true;
                break;
            } else if (strstr(resp_str, "Failed to connect to") != NULL) {
                break;
            }
        }
        furi_delay_ms(WIFI_CONNECT_DELAY_MS);
    }

    if (connected) {
        FURI_LOG_I("WiFi", "Successfully connected to %s", state->wifi_ssid);
        state->wifi_connected = true;
    } else {
        FURI_LOG_W("WiFi", "Failed to connect to %s", state->wifi_ssid);
        state->wifi_connected = false;
    }

    furi_string_free(response);

    state->current_state = AppStateMainMenu;
    state->ui_update_needed = true;
}