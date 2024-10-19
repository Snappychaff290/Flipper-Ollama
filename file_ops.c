#include "file_ops.h"
#include "ollama_app_i.h"
#include <storage/storage.h>
#include <furi.h>

bool read_url_from_file(OllamaAppState* state) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, URL_FILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint16_t bytes_read = storage_file_read(file, state->server_url, MAX_URL_LENGTH - 1);
        if(bytes_read > 0) {
            state->server_url[bytes_read] = '\0';
            success = true;
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return success;
}

bool read_wifi_config(OllamaAppState* state) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, WIFI_CONFIG_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buffer[MAX_SSID_LENGTH + MAX_PASSWORD_LENGTH + 2];
        uint16_t bytes_read = storage_file_read(file, buffer, sizeof(buffer) - 1);
        if(bytes_read > 0) {
            buffer[bytes_read] = '\0';
            char* password_start = strchr(buffer, '/');
            if(password_start && *(password_start + 1) == '/') {
                *password_start = '\0';
                password_start += 2;
                strncpy(state->wifi_ssid, buffer, MAX_SSID_LENGTH - 1);
                strncpy(state->wifi_password, password_start, MAX_PASSWORD_LENGTH - 1);
                state->wifi_ssid[MAX_SSID_LENGTH - 1] = '\0';
                state->wifi_password[MAX_PASSWORD_LENGTH - 1] = '\0';
                success = true;
            }
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return success;
}

void save_ap(OllamaAppState* state) {
    FURI_LOG_I("FileOps", "Saving AP: %s", state->wifi_ssid);
    
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, WIFI_CONFIG_PATH, FSAM_WRITE, FSOM_OPEN_ALWAYS)) {
        char buffer[MAX_SSID_LENGTH + MAX_PASSWORD_LENGTH + 3];
        int len = snprintf(buffer, sizeof(buffer), "%s//%s\n", state->wifi_ssid, state->wifi_password);
        if(len > 0) {
            storage_file_write(file, buffer, len);
            FURI_LOG_I("FileOps", "AP saved successfully");
        } else {
            FURI_LOG_E("FileOps", "Failed to format AP data");
        }
    } else {
        FURI_LOG_E("FileOps", "Failed to open file for writing");
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}