// wifi.c
#include "ollama_app_i.h"
#include <furi.h>
#include <furi_hal.h>
#include "helpers/uart_helper.h"

static UartHelper* uart_helper;

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

void wifi_init() {
    FURI_LOG_I("OllamaApp", "Initializing WiFi");
    uart_helper = uart_helper_alloc();
    uart_helper_set_callback(uart_helper, process_line, NULL);
    FURI_LOG_I("WiFi", "WiFi module initialized");
}

void wifi_deinit() {
    uart_helper_free(uart_helper);
    FURI_LOG_I("WiFi", "WiFi module deinitialized");
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