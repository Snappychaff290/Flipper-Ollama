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
