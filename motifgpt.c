#include <Xm/Xm.h>
#include <Xm/MainW.h>
#include <Xm/RowColumn.h>
#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/ToggleB.h> // For tab buttons
#include <Xm/Text.h>
#include <Xm/TextF.h> // For TextField (API Key, Model)
#include <Xm/ScrolledW.h>
#include <Xm/PanedW.h>
#include <Xm/CascadeB.h>
#include <Xm/Separator.h>
#include <Xm/MessageB.h>
#include <Xm/CutPaste.h>
#include <Xm/List.h>     // For model list (better than OptionMenu for many items)
#include <Xm/Frame.h>    // For tab content area
#include <X11/keysym.h>
#include <X11/Xlib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h> // For threading

#include "disasterparty.h"
#include <curl/curl.h>

// --- Configuration ---
#define DEFAULT_PROVIDER DP_PROVIDER_GOOGLE_GEMINI
#define DEFAULT_MODEL_GEMINI "gemini-2.0-flash"
#define DEFAULT_MODEL_OPENAI "gpt-4.1-nano" // Example
#define USER_NICKNAME "User"
#define ASSISTANT_NICKNAME "Assistant"
#define MAX_HISTORY_MESSAGES 100
// --- End Configuration ---

// Globals
Widget app_shell;
Widget conversation_text;
Widget input_text;
Widget send_button;
Widget focused_text_widget = NULL;

// Settings Dialog Globals
Widget settings_shell = NULL;
Widget settings_general_tab_content, settings_gemini_tab_content, settings_openai_tab_content;
Widget settings_current_tab_content = NULL;
Widget provider_gemini_rb, provider_openai_rb;
Widget gemini_api_key_text, gemini_model_text, gemini_model_list;
Widget openai_api_key_text, openai_model_text, openai_model_list;


// Current API Settings (to be loaded/saved, for now, in-memory)
dp_provider_type_t current_api_provider = DEFAULT_PROVIDER;
char current_gemini_api_key[256] = "";
char current_gemini_model[128] = DEFAULT_MODEL_GEMINI;
char current_openai_api_key[256] = "";
char current_openai_model[128] = DEFAULT_MODEL_OPENAI;


dp_context_t *dp_ctx = NULL;
// char *api_key = NULL; // Will use current_gemini_api_key or current_openai_api_key

int pipe_fds[2];
bool assistant_is_replying = false;
char current_assistant_prefix[64];
bool prefix_already_added_for_current_reply = false;

dp_message_t *chat_history = NULL;
int chat_history_count = 0;
int chat_history_capacity = 0;
char *current_assistant_response_buffer = NULL;
size_t current_assistant_response_len = 0;
size_t current_assistant_response_capacity = 0;

// Pipe Message Structure
typedef enum {
    PIPE_MSG_TOKEN,
    PIPE_MSG_STREAM_END,
    PIPE_MSG_ERROR,
    PIPE_MSG_MODEL_LIST_ITEM, // For settings dialog model list
    PIPE_MSG_MODEL_LIST_END,  // For settings dialog model list
    PIPE_MSG_MODEL_LIST_ERROR // For settings dialog model list
} pipe_message_type_t;

typedef struct {
    pipe_message_type_t type;
    char data[512]; // Increased size for model names or longer errors
} pipe_message_t;


// Thread data for LLM request
typedef struct {
    dp_request_config_t config; // A copy of the config for the thread
    // Need to be careful with dp_message_t array if it's dynamically sized
    // For now, assuming chat_history is stable during the thread's operation
    // or a deep copy is made if necessary.
    // For simplicity, the thread will use the global chat_history directly,
    // assuming send_message_callback serializes calls.
} llm_thread_data_t;

// Thread data for Get Models request
typedef struct {
    dp_provider_type_t provider;
    char api_key_for_list[256];
    Widget target_list_widget; // To populate directly or signal update
} get_models_thread_data_t;


// Function Prototypes (main app)
void send_message_callback(Widget w, XtPointer client_data, XtPointer call_data);
int stream_handler(const char* token, void* user_data, bool is_final, const char* error_during_stream);
void handle_pipe_input(XtPointer client_data, int *source, XtInputId *id);
void quit_callback(Widget w, XtPointer client_data, XtPointer call_data);
void show_error_dialog(const char* message);
static void handle_input_key_press(Widget w, XtPointer client_data, XEvent *event, Boolean *continue_to_dispatch);
static void focus_callback(Widget w, XtPointer client_data, XtPointer call_data);
void clear_chat_callback(Widget w, XtPointer client_data, XtPointer call_data);
void add_message_to_history(dp_message_role_t role, const char* text_content);
void free_chat_history();
void remove_oldest_history_messages(int count_to_remove);
void *perform_llm_request_thread(void *arg);
void initialize_dp_context(); // To re-init context after settings change

// Edit menu callbacks
void cut_callback(Widget w, XtPointer client_data, XtPointer call_data);
void copy_callback(Widget w, XtPointer client_data, XtPointer call_data);
void paste_callback(Widget w, XtPointer client_data, XtPointer call_data);
void select_all_callback(Widget w, XtPointer client_data, XtPointer call_data);

// Settings Dialog callbacks and functions
void settings_callback(Widget w, XtPointer client_data, XtPointer call_data);
void settings_tab_change_callback(Widget w, XtPointer client_data, XtPointer call_data);
void settings_apply_callback(Widget w, XtPointer client_data, XtPointer call_data);
void settings_ok_callback(Widget w, XtPointer client_data, XtPointer call_data);
void settings_cancel_callback(Widget w, XtPointer client_data, XtPointer call_data);
void settings_get_models_callback(Widget w, XtPointer client_data, XtPointer call_data);
void *perform_get_models_thread(void *arg);
void settings_use_selected_model_callback(Widget w, XtPointer client_data, XtPointer call_data);
void populate_settings_dialog();
void retrieve_settings_from_dialog();


void append_to_conversation(const char* text) {
    if (!conversation_text || !XtIsManaged(conversation_text)) return;
    XmTextPosition current_pos = XmTextGetLastPosition(conversation_text);
    XmTextInsert(conversation_text, current_pos, (char*)text);
    current_pos = XmTextGetLastPosition(conversation_text);
    XmTextShowPosition(conversation_text, current_pos);
}

void append_to_assistant_buffer(const char* text) {
    if (!text) return;
    size_t text_len = strlen(text);
    if (current_assistant_response_len + text_len + 1 > current_assistant_response_capacity) {
        current_assistant_response_capacity = (current_assistant_response_len + text_len + 1) * 2;
        char *new_buf = realloc(current_assistant_response_buffer, current_assistant_response_capacity);
        if (!new_buf) {
            perror("Failed to realloc assistant response buffer");
            free(current_assistant_response_buffer);
            current_assistant_response_buffer = NULL; current_assistant_response_len = 0; current_assistant_response_capacity = 0;
            return;
        }
        current_assistant_response_buffer = new_buf;
    }
    memcpy(current_assistant_response_buffer + current_assistant_response_len, text, text_len);
    current_assistant_response_len += text_len;
    current_assistant_response_buffer[current_assistant_response_len] = '\0';
}

// Writes a structured message to the pipe
void write_pipe_message(pipe_message_type_t type, const char* data) {
    pipe_message_t msg;
    msg.type = type;
    if (data) {
        strncpy(msg.data, data, sizeof(msg.data) - 1);
        msg.data[sizeof(msg.data) - 1] = '\0';
    } else {
        msg.data[0] = '\0';
    }
    if (write(pipe_fds[1], &msg, sizeof(pipe_message_t)) != sizeof(pipe_message_t)) {
        perror("write_pipe_message: write to pipe failed");
    }
}


int stream_handler(const char* token, void* user_data, bool is_final, const char* error_during_stream) {
    if (error_during_stream) {
        write_pipe_message(PIPE_MSG_ERROR, error_during_stream);
        assistant_is_replying = false; prefix_already_added_for_current_reply = false;
        if (current_assistant_response_buffer) current_assistant_response_buffer[0] = '\0';
        current_assistant_response_len = 0;
        return 1;
    }

    if (!assistant_is_replying) {
        assistant_is_replying = true; prefix_already_added_for_current_reply = false;
        if (current_assistant_response_buffer) current_assistant_response_buffer[0] = '\0';
        current_assistant_response_len = 0;
    }

    if (token) {
        append_to_assistant_buffer(token);
        write_pipe_message(PIPE_MSG_TOKEN, token);
    }

    if (is_final) {
        write_pipe_message(PIPE_MSG_STREAM_END, NULL);
    }
    return 0;
}

void handle_pipe_input(XtPointer client_data, int *source, XtInputId *id) {
    pipe_message_t msg;
    ssize_t nbytes = read(pipe_fds[0], &msg, sizeof(pipe_message_t));

    if (nbytes == sizeof(pipe_message_t)) {
        switch (msg.type) {
            case PIPE_MSG_TOKEN:
                if (assistant_is_replying && !prefix_already_added_for_current_reply) {
                    append_to_conversation(current_assistant_prefix);
                    prefix_already_added_for_current_reply = true;
                }
                append_to_conversation(msg.data);
                break;
            case PIPE_MSG_STREAM_END:
                append_to_conversation("\n");
                if (current_assistant_response_buffer && current_assistant_response_len > 0) {
                    add_message_to_history(DP_ROLE_ASSISTANT, current_assistant_response_buffer);
                }
                if (current_assistant_response_buffer) current_assistant_response_buffer[0] = '\0';
                current_assistant_response_len = 0;
                assistant_is_replying = false; prefix_already_added_for_current_reply = false;
                break;
            case PIPE_MSG_ERROR:
                show_error_dialog(msg.data); append_to_conversation(msg.data); append_to_conversation("\n");
                assistant_is_replying = false; prefix_already_added_for_current_reply = false;
                break;
            case PIPE_MSG_MODEL_LIST_ITEM:
                // This is for the settings dialog model list
                if (settings_shell && XtIsManaged(settings_shell)) {
                    Widget list_to_update = NULL;
                    if(XmToggleButtonGetState(provider_gemini_rb)) list_to_update = gemini_model_list;
                    else if(XmToggleButtonGetState(provider_openai_rb)) list_to_update = openai_model_list;

                    if (list_to_update) {
                        XmString item = XmStringCreateLocalized(msg.data);
                        XmListAddItemUnselected(list_to_update, item, 0);
                        XmStringFree(item);
                    }
                }
                break;
            case PIPE_MSG_MODEL_LIST_END:
                // Signal that model listing is complete (e.g., enable/disable UI elements)
                // For now, just a placeholder.
                printf("Model listing complete.\n");
                break;
            case PIPE_MSG_MODEL_LIST_ERROR:
                 if (settings_shell && XtIsManaged(settings_shell)) {
                    show_error_dialog(msg.data); // Show error in settings dialog or main
                 } else {
                    show_error_dialog(msg.data);
                 }
                break;

        }
    } else if (nbytes == 0) {
        fprintf(stderr, "handle_pipe_input: EOF on pipe.\n"); XtRemoveInput(*id);
    } else if (nbytes != -1) { // Partial read, should not happen with fixed size struct
        fprintf(stderr, "handle_pipe_input: Partial read from pipe (%ld bytes).\n", nbytes);
    } else { // nbytes == -1
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("handle_pipe_input: read from pipe"); XtRemoveInput(*id);
        }
    }
}

void remove_oldest_history_messages(int count_to_remove) {
    if (count_to_remove <= 0 || count_to_remove > chat_history_count) return;
    for (int i = 0; i < count_to_remove; ++i) {
        if (chat_history[i].parts) {
            for (size_t j = 0; j < chat_history[i].num_parts; ++j) {
                free(chat_history[i].parts[j].text);
            }
            free(chat_history[i].parts);
        }
    }
    int remaining_count = chat_history_count - count_to_remove;
    if (remaining_count > 0) {
        memmove(chat_history, &chat_history[count_to_remove], remaining_count * sizeof(dp_message_t));
    }
    chat_history_count = remaining_count;
}

void add_message_to_history(dp_message_role_t role, const char* text_content) {
    if (chat_history_count >= MAX_HISTORY_MESSAGES) {
        int messages_to_remove = (chat_history_count - MAX_HISTORY_MESSAGES) + 2; // Make space for new pair + overflow
        if (messages_to_remove < 2) messages_to_remove = 2; // Always try to remove a pair if at limit
        if (chat_history_count < messages_to_remove) messages_to_remove = chat_history_count;

        printf("Chat history nearly full. Removing %d oldest message(s).\n", messages_to_remove);
        remove_oldest_history_messages(messages_to_remove);
    }

    if (chat_history_count >= chat_history_capacity) {
        chat_history_capacity = (chat_history_capacity == 0) ? 10 : chat_history_capacity * 2;
        if (chat_history_capacity > MAX_HISTORY_MESSAGES) chat_history_capacity = MAX_HISTORY_MESSAGES;
        dp_message_t *new_history = realloc(chat_history, chat_history_capacity * sizeof(dp_message_t));
        if (!new_history) {
            perror("Failed to realloc chat history"); return;
        }
        chat_history = new_history;
    }

    dp_message_t *new_msg = &chat_history[chat_history_count];
    new_msg->role = role; new_msg->num_parts = 0; new_msg->parts = NULL;
    if (!dp_message_add_text_part(new_msg, text_content)) {
        fprintf(stderr, "Failed to add text part to history message.\n");
    } else {
        chat_history_count++;
    }
}

void free_chat_history() {
    if (chat_history) {
        dp_free_messages(chat_history, chat_history_count);
        free(chat_history); chat_history = NULL;
    }
    chat_history_count = 0; chat_history_capacity = 0;
}

void *perform_llm_request_thread(void *arg) {
    llm_thread_data_t *thread_data = (llm_thread_data_t *)arg;
    dp_response_t response_status = {0};

    // Critical: The chat_history pointer in thread_data->config.messages
    // points to the global chat_history. If chat_history can be modified
    // by the main thread (e.g., clear chat) while this thread is running,
    // a deep copy of messages would be needed, or a mutex.
    // For now, assume send_message_callback serializes actual LLM requests.
    printf("Thread: Performing LLM completion with %d messages.\n", (int)thread_data->config.num_messages);

    int ret = dp_perform_streaming_completion(dp_ctx, &thread_data->config, stream_handler, NULL, &response_status);

    if (ret != 0) {
        char error_buf[1024];
        snprintf(error_buf, sizeof(error_buf), "LLM Request Failed (Thread) (HTTP %ld): %s",
                 response_status.http_status_code,
                 response_status.error_message ? response_status.error_message : "Unknown Disaster Party error in thread.");
        write_pipe_message(PIPE_MSG_ERROR, error_buf); // Send error via pipe
    }
    // If ret == 0, stream_handler would have sent PIPE_MSG_STREAM_END

    dp_free_response_content(&response_status);
    // IMPORTANT: If thread_data->config.messages was a deep copy, it should be freed here.
    // Since it's pointing to global chat_history, we don't free it here.
    // The disasterparty library itself does not modify the input messages array.
    free(thread_data); // Free the llm_thread_data_t struct itself
    pthread_detach(pthread_self()); // Allow thread resources to be reclaimed
    return NULL;
}


void send_message_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    char *input_string_raw = XmTextGetString(input_text);
    if (!input_string_raw || strlen(input_string_raw) == 0) {
        XtFree(input_string_raw); return;
    }
    if (input_string_raw[strlen(input_string_raw)-1] == '\n') {
        input_string_raw[strlen(input_string_raw)-1] = '\0';
    }
    if (strlen(input_string_raw) == 0) {
        XtFree(input_string_raw); return;
    }

    char display_user_msg[1024];
    snprintf(display_user_msg, sizeof(display_user_msg), "%s: %s\n", USER_NICKNAME, input_string_raw);
    append_to_conversation(display_user_msg);
    add_message_to_history(DP_ROLE_USER, input_string_raw);
    XmTextSetString(input_text, "");

    if (!dp_ctx) {
        show_error_dialog("LLM context not initialized. Check Settings.");
        XtFree(input_string_raw); return;
    }

    llm_thread_data_t *thread_data = malloc(sizeof(llm_thread_data_t));
    if (!thread_data) {
        perror("Failed to allocate thread data");
        XtFree(input_string_raw); return;
    }

    thread_data->config.model = (current_api_provider == DP_PROVIDER_GOOGLE_GEMINI) ? current_gemini_model : current_openai_model;
    thread_data->config.temperature = 0.7;
    thread_data->config.max_tokens = 1024;
    thread_data->config.stream = true;
    thread_data->config.messages = chat_history; // Pointer to global history
    thread_data->config.num_messages = chat_history_count;

    XtFree(input_string_raw);

    snprintf(current_assistant_prefix, sizeof(current_assistant_prefix), "%s: ", ASSISTANT_NICKNAME);

    pthread_t tid;
    if (pthread_create(&tid, NULL, perform_llm_request_thread, thread_data) != 0) {
        perror("Failed to create LLM request thread");
        free(thread_data);
        show_error_dialog("Failed to start LLM request.");
    }
}

void quit_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    printf("Exiting MotifGPT...\n");
    free_chat_history();
    if (current_assistant_response_buffer) free(current_assistant_response_buffer);
    if (dp_ctx) dp_destroy_context(dp_ctx);
    curl_global_cleanup();
    if (pipe_fds[0] != -1) close(pipe_fds[0]);
    if (pipe_fds[1] != -1) close(pipe_fds[1]);
    XtDestroyApplicationContext(XtWidgetToApplicationContext(app_shell));
    exit(0);
}

void clear_chat_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    XmTextSetString(conversation_text, "");
    free_chat_history();
    append_to_conversation("Chat cleared. Welcome to MotifGPT!\n");
    assistant_is_replying = false; prefix_already_added_for_current_reply = false;
    if (current_assistant_response_buffer) current_assistant_response_buffer[0] = '\0';
    current_assistant_response_len = 0;
}

void show_error_dialog(const char* message) {
    Widget parent = (settings_shell && XtIsManaged(settings_shell)) ? settings_shell : app_shell;
    Widget dialog = XmCreateErrorDialog(parent, "errorDialog", NULL, 0);
    XmString xm_msg = XmStringCreateLocalized((char*)message);
    XtVaSetValues(dialog, XmNmessageString, xm_msg, NULL); XmStringFree(xm_msg);
    XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_CANCEL_BUTTON));
    XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_HELP_BUTTON));
    XtManageChild(dialog);
}

static void handle_input_key_press(Widget w, XtPointer client_data, XEvent *event, Boolean *continue_to_dispatch) {
    if (event->type == KeyPress) {
        XKeyEvent *key_event = (XKeyEvent *)event; KeySym keysym; char buffer[1];
        XLookupString(key_event, buffer, 1, &keysym, NULL);
        if (keysym == XK_Return || keysym == XK_KP_Enter) {
            if (key_event->state & ShiftMask) {
                XmTextInsert(input_text, XmTextGetCursorPosition(input_text), "\n");
                *continue_to_dispatch = False;
            } else {
                send_message_callback(send_button, NULL, NULL);
                *continue_to_dispatch = False;
            }
        }
    }
}

static void focus_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    XmAnyCallbackStruct *focus_data = (XmAnyCallbackStruct *)call_data;
    if (focus_data->reason == XmCR_FOCUS) focused_text_widget = w;
}

void cut_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    if (focused_text_widget && XtIsManaged(focused_text_widget) && XmIsText(focused_text_widget)) {
        XmTextCut(focused_text_widget, XtLastTimestampProcessed(XtDisplay(focused_text_widget)));
    }
}
void copy_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    if (focused_text_widget && XtIsManaged(focused_text_widget) && XmIsText(focused_text_widget)) {
        XmTextCopy(focused_text_widget, XtLastTimestampProcessed(XtDisplay(focused_text_widget)));
    }
}
void paste_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    if (focused_text_widget && XtIsManaged(focused_text_widget) && XmIsText(focused_text_widget)) {
        XmTextPaste(focused_text_widget);
    }
}
void select_all_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    if (focused_text_widget && XtIsManaged(focused_text_widget) && XmIsText(focused_text_widget)) {
        XmTextSetSelection(focused_text_widget, 0, XmTextGetLastPosition(focused_text_widget), CurrentTime);
        XmProcessTraversal(focused_text_widget, XmTRAVERSE_CURRENT);
    }
}

// --- Settings Dialog Functions ---
void settings_tab_change_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    long tab_index = (long)client_data;
    if (settings_current_tab_content) XtUnmanageChild(settings_current_tab_content);

    switch (tab_index) {
        case 0: settings_current_tab_content = settings_general_tab_content; break;
        case 1: settings_current_tab_content = settings_gemini_tab_content; break;
        case 2: settings_current_tab_content = settings_openai_tab_content; break;
        default: return;
    }
    if (settings_current_tab_content) XtManageChild(settings_current_tab_content);
}

void populate_settings_dialog() {
    // General Tab
    XmToggleButtonSetState(provider_gemini_rb, current_api_provider == DP_PROVIDER_GOOGLE_GEMINI, False);
    XmToggleButtonSetState(provider_openai_rb, current_api_provider == DP_PROVIDER_OPENAI_COMPATIBLE, False);

    // Gemini Tab
    XmTextFieldSetString(gemini_api_key_text, current_gemini_api_key);
    XmTextFieldSetString(gemini_model_text, current_gemini_model);
    XmListDeleteAllItems(gemini_model_list); // Clear previous items

    // OpenAI Tab
    XmTextFieldSetString(openai_api_key_text, current_openai_api_key);
    XmTextFieldSetString(openai_model_text, current_openai_model);
    XmListDeleteAllItems(openai_model_list); // Clear previous items
}

void retrieve_settings_from_dialog() {
    // General Tab
    if (XmToggleButtonGetState(provider_gemini_rb)) current_api_provider = DP_PROVIDER_GOOGLE_GEMINI;
    else if (XmToggleButtonGetState(provider_openai_rb)) current_api_provider = DP_PROVIDER_OPENAI_COMPATIBLE;

    // Gemini Tab
    char *temp_gemini_key = XmTextFieldGetString(gemini_api_key_text);
    strncpy(current_gemini_api_key, temp_gemini_key, sizeof(current_gemini_api_key) - 1);
    current_gemini_api_key[sizeof(current_gemini_api_key) - 1] = '\0';
    XtFree(temp_gemini_key);

    char *temp_gemini_model = XmTextFieldGetString(gemini_model_text);
    strncpy(current_gemini_model, temp_gemini_model, sizeof(current_gemini_model) - 1);
    current_gemini_model[sizeof(current_gemini_model) -1] = '\0';
    XtFree(temp_gemini_model);

    // OpenAI Tab
    char *temp_openai_key = XmTextFieldGetString(openai_api_key_text);
    strncpy(current_openai_api_key, temp_openai_key, sizeof(current_openai_api_key) - 1);
    current_openai_api_key[sizeof(current_openai_api_key) - 1] = '\0';
    XtFree(temp_openai_key);

    char *temp_openai_model = XmTextFieldGetString(openai_model_text);
    strncpy(current_openai_model, temp_openai_model, sizeof(current_openai_model) -1);
    current_openai_model[sizeof(current_openai_model) -1] = '\0';
    XtFree(temp_openai_model);
}


void initialize_dp_context() {
    if (dp_ctx) {
        dp_destroy_context(dp_ctx);
        dp_ctx = NULL;
    }
    const char* key_to_use = (current_api_provider == DP_PROVIDER_GOOGLE_GEMINI) ? current_gemini_api_key : current_openai_api_key;
    if (strlen(key_to_use) == 0) {
        show_error_dialog("API Key is not set for the selected provider.");
        return;
    }
    dp_ctx = dp_init_context(current_api_provider, key_to_use, NULL);
    if (!dp_ctx) {
        show_error_dialog("Failed to initialize LLM context with new settings.");
    } else {
        printf("LLM context re-initialized. Provider: %s\n",
               current_api_provider == DP_PROVIDER_GOOGLE_GEMINI ? "Gemini" : "OpenAI");
    }
}


void settings_apply_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    printf("Apply settings callback.\n");
    retrieve_settings_from_dialog();
    initialize_dp_context();
    // Potentially save settings to a file here
}

void settings_ok_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    printf("OK settings callback.\n");
    settings_apply_callback(w, client_data, call_data); // Apply first
    XtUnmanageChild(settings_shell);
    // XtDestroyWidget(settings_shell); // Or destroy if not reusing
    // settings_shell = NULL;
}

void settings_cancel_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    printf("Cancel settings callback.\n");
    XtUnmanageChild(settings_shell);
    // XtDestroyWidget(settings_shell);
    // settings_shell = NULL;
}

void *perform_get_models_thread(void *arg) {
    get_models_thread_data_t *data = (get_models_thread_data_t *)arg;
    dp_context_t *temp_ctx = dp_init_context(data->provider, data->api_key_for_list, NULL);
    if (!temp_ctx) {
        write_pipe_message(PIPE_MSG_MODEL_LIST_ERROR, "Failed to create temp context for Get Models.");
        free(data);
        pthread_detach(pthread_self());
        return NULL;
    }

    dp_model_list_t *model_list_struct = NULL;
    int result = dp_list_models(temp_ctx, &model_list_struct);

    if (result == 0 && model_list_struct) {
        if (model_list_struct->error_message) { // Error from API within a 200 OK
             char err_buf[512];
             snprintf(err_buf, sizeof(err_buf), "API Error (Get Models): %s", model_list_struct->error_message);
             write_pipe_message(PIPE_MSG_MODEL_LIST_ERROR, err_buf);
        } else {
            for (size_t i = 0; i < model_list_struct->count; ++i) {
                write_pipe_message(PIPE_MSG_MODEL_LIST_ITEM, model_list_struct->models[i].model_id);
            }
        }
    } else {
        char err_buf[512];
        if (model_list_struct && model_list_struct->error_message) {
             snprintf(err_buf, sizeof(err_buf), "Error Get Models (HTTP %ld): %s", model_list_struct->http_status_code, model_list_struct->error_message);
        } else if (model_list_struct) {
            snprintf(err_buf, sizeof(err_buf), "Error Get Models (HTTP %ld): Unknown API error.", model_list_struct->http_status_code);
        } else {
            strcpy(err_buf, "Error Get Models: dp_list_models failed critically.");
        }
        write_pipe_message(PIPE_MSG_MODEL_LIST_ERROR, err_buf);
    }

    write_pipe_message(PIPE_MSG_MODEL_LIST_END, NULL);
    dp_free_model_list(model_list_struct);
    dp_destroy_context(temp_ctx);
    free(data);
    pthread_detach(pthread_self());
    return NULL;
}


void settings_get_models_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    dp_provider_type_t provider_for_list;
    char *api_key_for_list_str;
    Widget list_widget_target;

    long tab_type = (long)client_data; // 0 for Gemini, 1 for OpenAI

    if (tab_type == 0) { // Gemini
        provider_for_list = DP_PROVIDER_GOOGLE_GEMINI;
        api_key_for_list_str = XmTextFieldGetString(gemini_api_key_text);
        list_widget_target = gemini_model_list;
        XmListDeleteAllItems(gemini_model_list); // Clear before fetching
    } else { // OpenAI
        provider_for_list = DP_PROVIDER_OPENAI_COMPATIBLE;
        api_key_for_list_str = XmTextFieldGetString(openai_api_key_text);
        list_widget_target = openai_model_list;
        XmListDeleteAllItems(openai_model_list); // Clear before fetching
    }

    if (!api_key_for_list_str || strlen(api_key_for_list_str) == 0) {
        show_error_dialog("API Key is required to fetch models.");
        XtFree(api_key_for_list_str);
        return;
    }

    get_models_thread_data_t *thread_data = malloc(sizeof(get_models_thread_data_t));
    if(!thread_data){
        perror("malloc get_models_thread_data");
        XtFree(api_key_for_list_str);
        return;
    }
    thread_data->provider = provider_for_list;
    strncpy(thread_data->api_key_for_list, api_key_for_list_str, sizeof(thread_data->api_key_for_list)-1);
    thread_data->api_key_for_list[sizeof(thread_data->api_key_for_list)-1] = '\0';
    thread_data->target_list_widget = list_widget_target; // Not used directly by thread, for info
    XtFree(api_key_for_list_str);

    pthread_t tid;
    if (pthread_create(&tid, NULL, perform_get_models_thread, thread_data) != 0) {
        perror("Failed to create Get Models thread");
        free(thread_data);
        show_error_dialog("Failed to start Get Models request.");
    }
    printf("Get Models request initiated for provider %ld.\n", tab_type);
}

void settings_use_selected_model_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    long tab_type = (long)client_data; // 0 for Gemini, 1 for OpenAI
    Widget source_list_widget = (tab_type == 0) ? gemini_model_list : openai_model_list;
    Widget target_text_widget = (tab_type == 0) ? gemini_model_text : openai_model_text;

    XmString *selected_items;
    int item_count;
    XtVaGetValues(source_list_widget, XmNselectedItems, &selected_items, XmNselectedItemCount, &item_count, NULL);

    if (item_count > 0) {
        char *text = NULL;
        XmStringGetLtoR(selected_items[0], XmFONTLIST_DEFAULT_TAG, &text);
        if (text) {
            XmTextFieldSetString(target_text_widget, text);
            XtFree(text);
        }
    }
}


void settings_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    if (settings_shell == NULL) {
        settings_shell = XtVaCreatePopupShell("settingsShell", topLevelShellWidgetClass, app_shell,
                                              XmNtitle, "MotifGPT Settings",
                                              XmNwidth, 500, XmNheight, 450, NULL);
        Widget dialog_form = XtVaCreateManagedWidget("dialogForm", xmFormWidgetClass, settings_shell, NULL);

        // --- Tab Buttons ---
        Widget tab_button_rc = XtVaCreateManagedWidget("tabButtonRc", xmRowColumnWidgetClass, dialog_form,
                                                       XmNorientation, XmHORIZONTAL,
                                                       XmNradioBehavior, True, // Makes ToggleButtons act like radio buttons
                                                       XmNtopAttachment, XmATTACH_FORM, XmNtopOffset, 5,
                                                       XmNleftAttachment, XmATTACH_FORM, XmNleftOffset, 5,
                                                       XmNrightAttachment, XmATTACH_FORM, XmNrightOffset, 5,
                                                       NULL);
        Widget general_tab_btn = XtVaCreateManagedWidget("General", xmToggleButtonWidgetClass, tab_button_rc, XmNindicatorOn, False, NULL);
        Widget gemini_tab_btn = XtVaCreateManagedWidget("Gemini", xmToggleButtonWidgetClass, tab_button_rc, XmNindicatorOn, False, NULL);
        Widget openai_tab_btn = XtVaCreateManagedWidget("OpenAI", xmToggleButtonWidgetClass, tab_button_rc, XmNindicatorOn, False, NULL);

        XtAddCallback(general_tab_btn, XmNvalueChangedCallback, settings_tab_change_callback, (XtPointer)0);
        XtAddCallback(gemini_tab_btn, XmNvalueChangedCallback, settings_tab_change_callback, (XtPointer)1);
        XtAddCallback(openai_tab_btn, XmNvalueChangedCallback, settings_tab_change_callback, (XtPointer)2);

        // --- Tab Content Frame ---
        Widget content_frame = XtVaCreateManagedWidget("contentFrame", xmFrameWidgetClass, dialog_form,
                                                       XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, tab_button_rc, XmNtopOffset, 5,
                                                       XmNleftAttachment, XmATTACH_FORM, XmNleftOffset, 5,
                                                       XmNrightAttachment, XmATTACH_FORM, XmNrightOffset, 5,
                                                       XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, 45, // Make space for OK/Cancel
                                                       XmNshadowType, XmSHADOW_ETCHED_IN,
                                                       NULL);

        // --- General Tab Content ---
        settings_general_tab_content = XtVaCreateWidget("generalTab", xmFormWidgetClass, content_frame, XmNmarginWidth, 10, XmNmarginHeight, 10, NULL);
        Widget provider_label = XtVaCreateManagedWidget("Provider:", xmLabelWidgetClass, settings_general_tab_content,
                                                        XmNtopAttachment, XmATTACH_FORM, XmNleftAttachment, XmATTACH_FORM, NULL);
        Widget provider_radio_box = XmCreateRadioBox(settings_general_tab_content, "providerRadioBox", NULL, 0);
        XtVaSetValues(provider_radio_box, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, provider_label, XmNleftAttachment, XmATTACH_FORM, NULL);
        provider_gemini_rb = XtVaCreateManagedWidget("Gemini", xmToggleButtonWidgetClass, provider_radio_box, NULL);
        provider_openai_rb = XtVaCreateManagedWidget("OpenAI-compatible", xmToggleButtonWidgetClass, provider_radio_box, NULL);
        XtManageChild(provider_radio_box);


        // --- Gemini Tab Content ---
        settings_gemini_tab_content = XtVaCreateWidget("geminiTab", xmFormWidgetClass, content_frame, XmNmarginWidth, 10, XmNmarginHeight, 10, NULL);
        Widget gemini_api_label = XtVaCreateManagedWidget("API Key:", xmLabelWidgetClass, settings_gemini_tab_content, XmNtopAttachment, XmATTACH_FORM, XmNleftAttachment, XmATTACH_FORM, NULL);
        gemini_api_key_text = XtVaCreateManagedWidget("geminiApiKey", xmTextFieldWidgetClass, settings_gemini_tab_content, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, gemini_api_label, XmNleftAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM, NULL);
        Widget gemini_model_label = XtVaCreateManagedWidget("Model ID:", xmLabelWidgetClass, settings_gemini_tab_content, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, gemini_api_key_text, XmNleftAttachment, XmATTACH_FORM, NULL);
        gemini_model_text = XtVaCreateManagedWidget("geminiModel", xmTextFieldWidgetClass, settings_gemini_tab_content, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, gemini_model_label, XmNleftAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM, NULL);
        Widget gemini_get_models_btn = XtVaCreateManagedWidget("Get Models", xmPushButtonWidgetClass, settings_gemini_tab_content, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, gemini_model_text, XmNleftAttachment, XmATTACH_FORM, NULL);
        XtAddCallback(gemini_get_models_btn, XmNactivateCallback, settings_get_models_callback, (XtPointer)0); // 0 for Gemini
        gemini_model_list = XmCreateScrolledList(settings_gemini_tab_content, "geminiModelList", NULL, 0);
        XtVaSetValues(XtParent(gemini_model_list), XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, gemini_get_models_btn, XmNleftAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, 35,NULL); // Space for "Use" button
        XtManageChild(gemini_model_list);
        Widget gemini_use_model_btn = XtVaCreateManagedWidget("Use Selected", xmPushButtonWidgetClass, settings_gemini_tab_content, XmNbottomAttachment, XmATTACH_FORM, XmNleftAttachment, XmATTACH_FORM, NULL);
        XtAddCallback(gemini_use_model_btn, XmNactivateCallback, settings_use_selected_model_callback, (XtPointer)0);


        // --- OpenAI Tab Content (similar to Gemini) ---
        settings_openai_tab_content = XtVaCreateWidget("openaiTab", xmFormWidgetClass, content_frame, XmNmarginWidth, 10, XmNmarginHeight, 10, NULL);
        Widget openai_api_label = XtVaCreateManagedWidget("API Key:", xmLabelWidgetClass, settings_openai_tab_content, XmNtopAttachment, XmATTACH_FORM, XmNleftAttachment, XmATTACH_FORM, NULL);
        openai_api_key_text = XtVaCreateManagedWidget("openaiApiKey", xmTextFieldWidgetClass, settings_openai_tab_content, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, openai_api_label, XmNleftAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM, NULL);
        Widget openai_model_label = XtVaCreateManagedWidget("Model ID:", xmLabelWidgetClass, settings_openai_tab_content, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, openai_api_key_text, XmNleftAttachment, XmATTACH_FORM, NULL);
        openai_model_text = XtVaCreateManagedWidget("openaiModel", xmTextFieldWidgetClass, settings_openai_tab_content, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, openai_model_label, XmNleftAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM, NULL);
        Widget openai_get_models_btn = XtVaCreateManagedWidget("Get Models", xmPushButtonWidgetClass, settings_openai_tab_content, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, openai_model_text, XmNleftAttachment, XmATTACH_FORM, NULL);
        XtAddCallback(openai_get_models_btn, XmNactivateCallback, settings_get_models_callback, (XtPointer)1); // 1 for OpenAI
        openai_model_list = XmCreateScrolledList(settings_openai_tab_content, "openaiModelList", NULL, 0);
        XtVaSetValues(XtParent(openai_model_list), XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, openai_get_models_btn, XmNleftAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, 35, NULL);
        XtManageChild(openai_model_list);
        Widget openai_use_model_btn = XtVaCreateManagedWidget("Use Selected", xmPushButtonWidgetClass, settings_openai_tab_content, XmNbottomAttachment, XmATTACH_FORM, XmNleftAttachment, XmATTACH_FORM, NULL);
        XtAddCallback(openai_use_model_btn, XmNactivateCallback, settings_use_selected_model_callback, (XtPointer)1);


        // --- Dialog Buttons (OK, Apply, Cancel) ---
        Widget button_rc = XtVaCreateManagedWidget("dialogButtonRc", xmRowColumnWidgetClass, dialog_form,
                                                   XmNorientation, XmHORIZONTAL, XmNpacking, XmPACK_COLUMN,
                                                   XmNnumColumns, 3, // For OK, Apply, Cancel
                                                   XmNentryAlignment, XmALIGNMENT_CENTER,
                                                   XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, 5,
                                                   XmNleftAttachment, XmATTACH_FORM, XmNleftOffset, 5,
                                                   XmNrightAttachment, XmATTACH_FORM, XmNrightOffset, 5,
                                                   NULL);
        Widget ok_button = XtVaCreateManagedWidget("OK", xmPushButtonWidgetClass, button_rc, NULL);
        Widget apply_button = XtVaCreateManagedWidget("Apply", xmPushButtonWidgetClass, button_rc, NULL);
        Widget cancel_button = XtVaCreateManagedWidget("Cancel", xmPushButtonWidgetClass, button_rc, NULL);

        XtAddCallback(ok_button, XmNactivateCallback, settings_ok_callback, NULL);
        XtAddCallback(apply_button, XmNactivateCallback, settings_apply_callback, NULL);
        XtAddCallback(cancel_button, XmNactivateCallback, settings_cancel_callback, NULL);

        // Set initial tab
        XmToggleButtonSetState(general_tab_btn, True, True); // This will trigger its callback
    }
    populate_settings_dialog(); // Populate with current settings
    XtManageChild(settings_shell);
    XtPopup(settings_shell, XtGrabNone);
}


// --- Main Application Setup ---
int main(int argc, char **argv) {
    XtAppContext app_context;
    Widget main_window, menu_bar, main_form;
    Widget chat_area_paned, input_form, bottom_buttons_form;
    Widget file_menu, file_cascade, quit_button_widget, clear_chat_button, settings_button, file_sep_exit;
    Widget edit_menu, edit_cascade;
    Widget cut_button, copy_button, paste_button, select_all_button, edit_sep;
    XmString acc_text_ctrl_q, acc_text_ctrl_o, acc_text_ctrl_x, acc_text_ctrl_c, acc_text_ctrl_v, acc_text_ctrl_a;

    current_assistant_response_capacity = 1024;
    current_assistant_response_buffer = malloc(current_assistant_response_capacity);
    if (!current_assistant_response_buffer) {
        perror("Failed to allocate initial assistant response buffer"); return 1;
    }
    current_assistant_response_buffer[0] = '\0';

    if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK) {
        fprintf(stderr, "Fatal: Failed to initialize libcurl.\n"); return 1;
    }

    // Initialize API key from environment or use default empty
    // User will set it in settings dialog.
    const char* gemini_env_key = getenv("GEMINI_API_KEY");
    if (gemini_env_key) strncpy(current_gemini_api_key, gemini_env_key, sizeof(current_gemini_api_key)-1);
    const char* openai_env_key = getenv("OPENAI_API_KEY");
    if (openai_env_key) strncpy(current_openai_api_key, openai_env_key, sizeof(current_openai_api_key)-1);

    initialize_dp_context(); // Initial context based on defaults/env


    if (pipe(pipe_fds) == -1) {
        perror("Fatal: pipe creation failed");
        if(dp_ctx) dp_destroy_context(dp_ctx); curl_global_cleanup(); return 1;
    }
    if (fcntl(pipe_fds[0], F_SETFL, O_NONBLOCK) == -1) {
        perror("Fatal: fcntl O_NONBLOCK failed");
        close(pipe_fds[0]); close(pipe_fds[1]);
        if(dp_ctx) dp_destroy_context(dp_ctx); curl_global_cleanup(); return 1;
    }

    app_shell = XtAppInitialize(&app_context, "MotifGPT", NULL, 0, &argc, argv, NULL, NULL, 0);
    XtAddCallback(app_shell, XmNdestroyCallback, quit_callback, NULL);
    main_window = XmCreateMainWindow(app_shell, "mainWindow", NULL, 0);
    XtManageChild(main_window);

    menu_bar = XmCreateMenuBar(main_window, "menuBar", NULL, 0);
    XtManageChild(menu_bar);

    file_menu = XmCreatePulldownMenu(menu_bar, "fileMenu", NULL, 0);
    file_cascade = XtVaCreateManagedWidget("File", xmCascadeButtonWidgetClass, menu_bar,
                                           XmNsubMenuId, file_menu, XmNmnemonic, XK_F, NULL);
    acc_text_ctrl_o = XmStringCreateLocalized("Ctrl+O");
    settings_button = XtVaCreateManagedWidget("Settings...", xmPushButtonWidgetClass, file_menu,
                                           XmNmnemonic, XK_S, XmNaccelerator, "Ctrl<Key>o",
                                           XmNacceleratorText, acc_text_ctrl_o, NULL);
    XmStringFree(acc_text_ctrl_o);
    XtAddCallback(settings_button, XmNactivateCallback, settings_callback, NULL);

    clear_chat_button = XtVaCreateManagedWidget("Clear Chat", xmPushButtonWidgetClass, file_menu,
                                           XmNmnemonic, XK_C, NULL);
    XtAddCallback(clear_chat_button, XmNactivateCallback, clear_chat_callback, NULL);
    file_sep_exit = XtVaCreateManagedWidget("fileSeparatorExit", xmSeparatorWidgetClass, file_menu, NULL);
    acc_text_ctrl_q = XmStringCreateLocalized("Ctrl+Q");
    quit_button_widget = XtVaCreateManagedWidget("Exit", xmPushButtonWidgetClass, file_menu,
                                           XmNmnemonic, XK_x, XmNaccelerator, "Ctrl<Key>q",
                                           XmNacceleratorText, acc_text_ctrl_q, NULL);
    XmStringFree(acc_text_ctrl_q);
    XtAddCallback(quit_button_widget, XmNactivateCallback, quit_callback, NULL);

    edit_menu = XmCreatePulldownMenu(menu_bar, "editMenu", NULL, 0);
    edit_cascade = XtVaCreateManagedWidget("Edit", xmCascadeButtonWidgetClass, menu_bar,
                                           XmNsubMenuId, edit_menu, XmNmnemonic, XK_E, NULL);
    acc_text_ctrl_x = XmStringCreateLocalized("Ctrl+X");
    cut_button = XtVaCreateManagedWidget("Cut", xmPushButtonWidgetClass, edit_menu,
                                         XmNmnemonic, XK_t, XmNaccelerator, "Ctrl<Key>x",
                                         XmNacceleratorText, acc_text_ctrl_x, NULL);
    XmStringFree(acc_text_ctrl_x);
    XtAddCallback(cut_button, XmNactivateCallback, cut_callback, NULL);
    acc_text_ctrl_c = XmStringCreateLocalized("Ctrl+C");
    copy_button = XtVaCreateManagedWidget("Copy", xmPushButtonWidgetClass, edit_menu,
                                          XmNmnemonic, XK_C, XmNaccelerator, "Ctrl<Key>c",
                                          XmNacceleratorText, acc_text_ctrl_c, NULL);
    XmStringFree(acc_text_ctrl_c);
    XtAddCallback(copy_button, XmNactivateCallback, copy_callback, NULL);
    acc_text_ctrl_v = XmStringCreateLocalized("Ctrl+V");
    paste_button = XtVaCreateManagedWidget("Paste", xmPushButtonWidgetClass, edit_menu,
                                           XmNmnemonic, XK_P, XmNaccelerator, "Ctrl<Key>v",
                                           XmNacceleratorText, acc_text_ctrl_v, NULL);
    XmStringFree(acc_text_ctrl_v);
    XtAddCallback(paste_button, XmNactivateCallback, paste_callback, NULL);
    edit_sep = XtVaCreateManagedWidget("editSeparator", xmSeparatorWidgetClass, edit_menu, NULL);
    acc_text_ctrl_a = XmStringCreateLocalized("Ctrl+A");
    select_all_button = XtVaCreateManagedWidget("Select All", xmPushButtonWidgetClass, edit_menu,
                                                XmNmnemonic, XK_A, XmNaccelerator, "Ctrl<Key>a",
                                                XmNacceleratorText, acc_text_ctrl_a, NULL);
    XmStringFree(acc_text_ctrl_a);
    XtAddCallback(select_all_button, XmNactivateCallback, select_all_callback, NULL);

    main_form = XtVaCreateWidget("mainForm", xmFormWidgetClass, main_window, XmNwidth, 600, XmNheight, 450, NULL);
    XtManageChild(main_form);

    chat_area_paned = XtVaCreateManagedWidget("chatAreaPaned", xmPanedWindowWidgetClass, main_form,
                                              XmNtopAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM,
                                              XmNleftAttachment, XmATTACH_FORM,
                                              XmNrightAttachment, XmATTACH_FORM, XmNsashWidth, 1, XmNsashHeight, 1, NULL);
    Widget scrolled_conv_win = XmCreateScrolledWindow(chat_area_paned, "scrolledConvWin", NULL, 0);
    XtVaSetValues(scrolled_conv_win, XmNpaneMinimum, 100, XmNpaneMaximum, 1000, NULL);
    conversation_text = XmCreateText(scrolled_conv_win, "conversationText", NULL, 0);
    XtVaSetValues(conversation_text, XmNeditMode, XmMULTI_LINE_EDIT, XmNeditable, False,
                  XmNcursorPositionVisible, False, XmNwordWrap, True, XmNscrollHorizontal, False, XmNrows, 15,
                  XmNbackground, WhitePixelOfScreen(XtScreen(conversation_text)), NULL);
    XtManageChild(conversation_text);
    XtManageChild(scrolled_conv_win);
    XmScrolledWindowSetAreas(scrolled_conv_win, NULL, NULL, conversation_text);
    XtAddCallback(conversation_text, XmNfocusCallback, focus_callback, NULL);

    input_form = XtVaCreateWidget("inputForm", xmFormWidgetClass, chat_area_paned,
                                   XmNpaneMinimum, 100, XmNpaneMaximum, 200, XmNfractionBase, 10, NULL);
    XtManageChild(input_form);
    bottom_buttons_form = XtVaCreateManagedWidget("bottomButtonsForm", xmFormWidgetClass, input_form,
                                                 XmNbottomAttachment, XmATTACH_FORM, XmNleftAttachment, XmATTACH_FORM,
                                                 XmNrightAttachment, XmATTACH_FORM, XmNheight, 30,
                                                 NULL);
    send_button = XtVaCreateManagedWidget("Send", xmPushButtonWidgetClass, bottom_buttons_form,
                                          XmNrightAttachment, XmATTACH_FORM, XmNleftAttachment, XmATTACH_FORM,
                                          XmNbottomAttachment, XmATTACH_FORM, XmNtopAttachment, XmATTACH_FORM,
                                          XmNdefaultButtonShadowThickness, 1, NULL);
    XtAddCallback(send_button, XmNactivateCallback, send_message_callback, NULL);
    XtVaSetValues(input_form, XmNdefaultButton, send_button, NULL);

    Widget scrolled_input_win = XmCreateScrolledWindow(input_form, "scrolledInputWin", NULL, 0);
    XtVaSetValues(scrolled_input_win, XmNtopAttachment, XmATTACH_FORM,
                  XmNbottomAttachment, XmATTACH_WIDGET, XmNbottomWidget, bottom_buttons_form,
                  XmNleftAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM, NULL);
    input_text = XmCreateText(scrolled_input_win, "inputText", NULL, 0);
    XtVaSetValues(input_text, XmNeditMode, XmMULTI_LINE_EDIT, XmNrows, 3, XmNwordWrap, True,
                  XmNbackground, WhitePixelOfScreen(XtScreen(input_text)), NULL);
    XtManageChild(input_text);
    XtManageChild(scrolled_input_win);
    XmScrolledWindowSetAreas(scrolled_input_win, NULL, NULL, input_text);
    XtAddEventHandler(input_text, KeyPressMask, False, handle_input_key_press, NULL);
    XtAddCallback(input_text, XmNfocusCallback, focus_callback, NULL);

    XmMainWindowSetAreas(main_window, menu_bar, NULL, NULL, NULL, main_form);
    XtAppAddInput(app_context, pipe_fds[0], (XtPointer)XtInputReadMask, handle_pipe_input, NULL);
    XtRealizeWidget(app_shell);
    focused_text_widget = input_text;
    append_to_conversation("Welcome to MotifGPT! Type message, Shift+Enter for newline, Enter to send.\n");
    XtAppMainLoop(app_context);

    if (current_assistant_response_buffer) free(current_assistant_response_buffer);
    free_chat_history();
    if (dp_ctx) dp_destroy_context(dp_ctx);
    curl_global_cleanup();
    if (pipe_fds[0] != -1) close(pipe_fds[0]);
    if (pipe_fds[1] != -1) close(pipe_fds[1]);
    return 0;
}

