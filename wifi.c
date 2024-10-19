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

static void process_lines(FuriString* line, void* context) {
    OllamaAppState* state = (OllamaAppState*)context;
    const char* line_str = furi_string_get_cstr(line);

    FURI_LOG_I("WiFi", "ESP32: %s", line_str);

    if (strncmp(line_str, "STATUS:", 7) == 0) {
        char status[20];
        char details[MAX_STATUS_LENGTH];
        sscanf(line_str, "STATUS:%19s %63s", status, details);

        if (strcmp(status, "SCANNING") == 0) {
            snprintf(state->status_message, MAX_STATUS_LENGTH, "Scanning networks...");
        } else if (strcmp(status, "FOUND") == 0) {
            snprintf(state->status_message, MAX_STATUS_LENGTH, "Found: %.56s", details);
        } else if (strcmp(status, "CONNECTING") == 0) {
            snprintf(state->status_message, MAX_STATUS_LENGTH, "Connecting: %.51s", details);
        } else if (strcmp(status, "CONNECTED") == 0) {
            state->wifi_connected = true;
            snprintf(state->status_message, MAX_STATUS_LENGTH, "Connected: %.52s", details);
        } else if (strcmp(status, "IP") == 0) {
            snprintf(state->status_message, MAX_STATUS_LENGTH, "IP: %.59s", details);
        } else if (strcmp(status, "FAILED") == 0) {
            snprintf(state->status_message, MAX_STATUS_LENGTH, "Failed: %.55s", details);
        } else if (strcmp(status, "NO_MATCH") == 0) {
            strncpy(state->status_message, "No known networks found", MAX_STATUS_LENGTH - 1);
        } else if (strcmp(status, "ERROR") == 0) {
            snprintf(state->status_message, MAX_STATUS_LENGTH, "Error: %.56s", details);
        }

        state->status_message[MAX_STATUS_LENGTH - 1] = '\0';
        state->ui_update_needed = true;
    } else if (strncmp(line_str, "NETWORK:", 8) == 0) {
        if (state->network_count < MAX_NETWORKS) {
            char* network_info = (char*)line_str + 8;
            char* rssi_str = strrchr(network_info, ',');
            if (rssi_str) {
                *rssi_str = '\0';
                rssi_str++;
                strncpy(state->networks[state->network_count].ssid, network_info, MAX_SSID_LENGTH - 1);
                state->networks[state->network_count].ssid[MAX_SSID_LENGTH - 1] = '\0';  // Corrected this line
                state->networks[state->network_count].rssi = atoi(rssi_str);
                state->network_count++;
                FURI_LOG_I("WiFi", "Added network: %s (%ld dBm)", 
                           state->networks[state->network_count-1].ssid, 
                           (long)state->networks[state->network_count-1].rssi);
            }
        }
    } else if (strcmp(line_str, "SCAN_COMPLETE") == 0) {
        strncpy(state->status_message, "Scan complete", MAX_STATUS_LENGTH - 1);
        state->status_message[MAX_STATUS_LENGTH - 1] = '\0';
        state->ui_update_needed = true;
    }
}

void wifi_scan(OllamaAppState* state) {
    FURI_LOG_I("WiFi", "Starting WiFi scan");
    state->network_count = 0;
    state->current_state = AppStateWifiScan;
    state->selected_network = 0;
    state->ui_update_needed = true;

    uart_helper_send(uart_helper, "SCAN\r\n", 6);
    furi_delay_ms(COMMAND_DELAY_MS);  // Give ESP32 time to process the command

    // Set up UART helper to process incoming lines
    uart_helper_set_callback(uart_helper, process_lines, state);

    uint32_t start_time = furi_get_tick();
    uint32_t timeout = furi_ms_to_ticks(30000); // 30 second timeout

    while (furi_get_tick() - start_time < timeout) {
        furi_delay_ms(100);  // Small delay to prevent busy waiting
        
        if (strcmp(state->status_message, "Scan complete") == 0) {
            FURI_LOG_I("WiFi", "Scan complete, found %d networks", state->network_count);
            break;
        }
    }

    // Reset UART helper callback
    uart_helper_set_callback(uart_helper, NULL, NULL);

    if (strcmp(state->status_message, "Scan complete") != 0) {
        FURI_LOG_E("WiFi", "Scan timed out");
        strncpy(state->status_message, "Scan timed out", sizeof(state->status_message) - 1);
        state->status_message[sizeof(state->status_message) - 1] = '\0';
    }

    if (state->network_count > 0) {
        state->current_state = AppStateWifiSelect;
        state->selected_network = 0;
    } else {
        state->current_state = AppStateMainMenu;
    }

    state->ui_update_needed = true;
}

void wifi_connect_known(OllamaAppState* state) {
    FURI_LOG_I("WiFi", "Attempting to connect to known AP or newly added AP");
    
    if (uart_helper == NULL) {
        FURI_LOG_E("WiFi", "UART helper not initialized");
        state->current_state = AppStateMainMenu;
        state->ui_update_needed = true;
        return;
    }
    
    FuriString* ap_data = furi_string_alloc();

    // Check if we have a newly added AP
    if (strlen(state->wifi_ssid) > 0 && strlen(state->wifi_password) > 0) {
        FURI_LOG_I("WiFi", "Using newly added AP: %s", state->wifi_ssid);
        furi_string_printf(ap_data, "%s//%s", state->wifi_ssid, state->wifi_password);
    } else {
        // Read known APs from file
        Storage* storage = furi_record_open(RECORD_STORAGE);
        Stream* file_stream = buffered_file_stream_alloc(storage);
        
        if (storage_file_exists(storage, WIFI_CONFIG_PATH) &&
            buffered_file_stream_open(file_stream, WIFI_CONFIG_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
            
            FuriString* line = furi_string_alloc();
            
            while (stream_read_line(file_stream, line)) {
                furi_string_trim(line);
                if (furi_string_size(line) > 0) {
                    if (furi_string_size(ap_data) > 0) {
                        furi_string_cat_str(ap_data, ",");
                    }
                    furi_string_cat(ap_data, line);
                }
            }
            
            furi_string_free(line);
            
            buffered_file_stream_close(file_stream);
        } else {
            FURI_LOG_E("WiFi", "Failed to open SavedAPs.txt");
        }
        
        stream_free(file_stream);
        furi_record_close(RECORD_STORAGE);
    }
    
    if (furi_string_size(ap_data) > 0) {
        FURI_LOG_I("WiFi", "Sending AP to ESP32: %s", furi_string_get_cstr(ap_data));
        uart_helper_send(uart_helper, furi_string_get_cstr(ap_data), furi_string_size(ap_data));
        uart_helper_send(uart_helper, "\r\n", 2);
    } else {
        FURI_LOG_W("WiFi", "No AP to connect to");
        strncpy(state->status_message, "No AP to connect to", sizeof(state->status_message) - 1);
        state->status_message[sizeof(state->status_message) - 1] = '\0';
        state->current_state = AppStateMainMenu;
        state->ui_update_needed = true;
        furi_string_free(ap_data);
        return;
    }
    
    furi_string_free(ap_data);
    
    // Set up UART helper to process incoming lines
    uart_helper_set_callback(uart_helper, process_lines, state);

    // Wait for connection process to complete
    uint32_t start_time = furi_get_tick();
    uint32_t timeout = furi_ms_to_ticks(30000);  // 30 second timeout

    state->wifi_connected = false;
    strncpy(state->status_message, "Connecting...", sizeof(state->status_message) - 1);
    state->status_message[sizeof(state->status_message) - 1] = '\0';
    state->ui_update_needed = true;

    while (furi_get_tick() - start_time < timeout) {
        furi_delay_ms(100);  // Small delay to prevent busy waiting
        
        // Trigger UI update
        state->ui_update_needed = true;
        
        if (state->wifi_connected) {
            FURI_LOG_I("WiFi", "Successfully connected to WiFi");
            break;
        }
    }

    if (!state->wifi_connected) {
        FURI_LOG_W("WiFi", "Failed to connect to AP");
        strncpy(state->status_message, "Connection failed", sizeof(state->status_message) - 1);
        state->status_message[sizeof(state->status_message) - 1] = '\0';
    }
    
    // Reset UART helper callback
    uart_helper_set_callback(uart_helper, NULL, NULL);
    
    // Display the result for a few seconds before returning to the main menu
    state->ui_update_needed = true;
    furi_delay_ms(3000);  // Display the result for 3 seconds
    
    state->current_state = AppStateMainMenu;
    state->ui_update_needed = true;

    // Clear the temporary SSID and password
    memset(state->wifi_ssid, 0, sizeof(state->wifi_ssid));
    memset(state->wifi_password, 0, sizeof(state->wifi_password));
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

void wifi_check_connection(OllamaAppState* state) {
    if (uart_helper == NULL) {
        return;
    }

    uart_helper_send(uart_helper, "STATUS\r\n", 8);
    FuriString* response = furi_string_alloc();
    
    if (uart_helper_read(uart_helper, response, UART_READ_TIMEOUT_MS)) {
        const char* resp_str = furi_string_get_cstr(response);
        if (strstr(resp_str, "WiFi Connected") != NULL) {
            state->wifi_connected = true;
        } else {
            state->wifi_connected = false;
        }
        state->ui_update_needed = true;
    }

    furi_string_free(response);
}