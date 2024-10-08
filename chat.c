#include "ollama_app_i.h"

void add_chat_message(OllamaAppState* state, const char* message, bool is_user) {
    if (state->chat_message_count >= MAX_CHAT_MESSAGES) {
        // Remove the oldest message
        for (int i = 0; i < MAX_CHAT_MESSAGES - 1; i++) {
            memcpy(&state->chat_messages[i], &state->chat_messages[i+1], sizeof(ChatMessage));
        }
        state->chat_message_count--;
    }
    
    strncpy(state->chat_messages[state->chat_message_count].content, message, MAX_MESSAGE_LENGTH - 1);
    state->chat_messages[state->chat_message_count].content[MAX_MESSAGE_LENGTH - 1] = '\0';
    state->chat_messages[state->chat_message_count].is_user = is_user;
    state->chat_message_count++;
}

void process_chat(OllamaAppState* state, InputEvent* event) {
    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        switch(event->key) {
            case InputKeyUp:
            case InputKeyDown:
                // TODO: Implement chat history scrolling
                break;
            case InputKeyRight:
                if (state->cursor_position < strlen(state->current_message)) {
                    state->cursor_position++;
                }
                break;
            case InputKeyLeft:
                if (state->cursor_position > 0) {
                    state->cursor_position--;
                }
                break;
            case InputKeyOk:
                if (strlen(state->current_message) > 0) {
                    add_chat_message(state, state->current_message, true);
                    // TODO: Implement API call to get AI response
                    add_chat_message(state, "I am a simulated response.", false);
                    state->current_message[0] = '\0';
                    state->cursor_position = 0;
                }
                break;
            default:
                if (strlen(state->current_message) < MAX_MESSAGE_LENGTH - 1) {
                    memmove(
                        &state->current_message[state->cursor_position + 1],
                        &state->current_message[state->cursor_position],
                        strlen(&state->current_message[state->cursor_position]) + 1
                    );
                    state->current_message[state->cursor_position] = 'A' + (event->key % 26); // Simple key to char mapping
                    state->cursor_position++;
                }
                break;
        }
    }
}