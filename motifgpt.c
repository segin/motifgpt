/*
 * MotifGPT: A Motif-based LLM Chat Client
 * Utilizes libdisasterparty for LLM communication.
 */
#include <Xm/Xm.h>
#include <Xm/MainW.h>
#include <Xm/RowColumn.h>
#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/ToggleB.h>
#include <Xm/Text.h>
#include <Xm/TextF.h>
#include <Xm/ScrolledW.h>
#include <Xm/PanedW.h>
#include <Xm/CascadeB.h>
#include <Xm/Separator.h>
#include <Xm/MessageB.h>
#include <Xm/CutPaste.h>
#include <Xm/List.h>
#include <Xm/Frame.h>
#include <Xm/FileSB.h>

#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libgen.h>
#include <wordexp.h>
#include <limits.h>
#include <ctype.h>
#include <locale.h>

#include "disasterparty.h"
#include <curl/curl.h>

// --- Configuration ---
#define DEFAULT_PROVIDER DP_PROVIDER_GOOGLE_GEMINI
#define DEFAULT_GEMINI_MODEL "gemini-2.0-flash"
#define DEFAULT_OPENAI_MODEL "gpt-4.1-nano"
#define DEFAULT_OPENAI_BASE_URL "https://api.openai.com/v1"
#define DEFAULT_GEMINI_KEY_PLACEHOLDER "AIkeygoesherexxx..."
#define DEFAULT_OPENAI_KEY_PLACEHOLDER "sk-yourkeygoesherexxxx..."
#define USER_NICKNAME "User"
#define ASSISTANT_NICKNAME "Assistant"
#define DEFAULT_MAX_HISTORY_MESSAGES 100
#define INTERNAL_MAX_HISTORY_CAPACITY 10000
#define CONFIG_DIR_MODE 0755
#define CONFIG_FILE_NAME "settings.conf"
#define CACHE_DIR_NAME "cache"
// --- End Configuration ---

// Globals
Widget app_shell;
Widget conversation_text;
Widget input_text;
Widget send_button;
Widget attach_image_button;
Widget focused_text_widget = NULL;
Widget popup_menu = NULL;
Widget popup_cut_item, popup_paste_item, popup_copy_item, popup_select_all_item;

Widget settings_shell = NULL;
Widget settings_general_tab_content, settings_gemini_tab_content, settings_openai_tab_content;
Widget settings_current_tab_content = NULL;
Widget provider_gemini_rb, provider_openai_rb;
Widget gemini_api_key_text, gemini_model_text, gemini_model_list;
Widget openai_api_key_text, openai_model_text, openai_base_url_text, openai_model_list;
Widget history_length_text;
Widget disable_history_limit_toggle;
Widget enter_sends_message_toggle;

dp_provider_type_t current_api_provider = DEFAULT_PROVIDER;
char current_gemini_api_key[256] = "";
char current_gemini_model[128] = DEFAULT_GEMINI_MODEL;
char current_openai_api_key[256] = "";
char current_openai_model[128] = DEFAULT_OPENAI_MODEL;
char current_openai_base_url[256] = "";
int current_max_history_messages = DEFAULT_MAX_HISTORY_MESSAGES;
Boolean history_limits_disabled = False;
Boolean enter_key_sends_message = True;

char attached_image_path[PATH_MAX] = "";
char attached_image_mime_type[64] = "";
char *attached_image_base64_data = NULL;

dp_context_t *dp_ctx = NULL;
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

Pixel normal_fg_color, grey_fg_color;

typedef enum {
    PIPE_MSG_TOKEN, PIPE_MSG_STREAM_END, PIPE_MSG_ERROR,
    PIPE_MSG_MODEL_LIST_ITEM, PIPE_MSG_MODEL_LIST_END, PIPE_MSG_MODEL_LIST_ERROR
} pipe_message_type_t;
typedef struct { pipe_message_type_t type; char data[512]; } pipe_message_t;
typedef struct { dp_request_config_t config; } llm_thread_data_t;
typedef struct { dp_provider_type_t provider; char api_key_for_list[256]; char base_url_for_list[256]; } get_models_thread_data_t;

// Function Prototypes
void send_message_callback(Widget, XtPointer, XtPointer);
int stream_handler(const char*, void*, bool, const char*);
void handle_pipe_input(XtPointer, int*, XtInputId*);
void quit_callback(Widget, XtPointer, XtPointer);
void show_error_dialog(const char*);
static void input_text_key_press_handler(Widget, XtPointer, XEvent*, Boolean*);
static void app_text_key_press_handler(Widget, XtPointer, XEvent*, Boolean*);
static void focus_callback(Widget, XtPointer, XtPointer);
static void settings_text_field_focus_in_cb(Widget, XtPointer, XtPointer);
static void settings_text_field_focus_out_cb(Widget, XtPointer, XtPointer);
void clear_chat_callback(Widget, XtPointer, XtPointer);
void add_message_to_history(dp_message_role_t, const char*, const char*, const char*);
void free_chat_history();
void remove_oldest_history_messages(int);
void *perform_llm_request_thread(void*);
void initialize_dp_context();
char* get_config_path(const char*);
int ensure_config_dir_exists();
void load_settings();
void save_settings();
void attach_image_callback(Widget, XtPointer, XtPointer);
void file_selection_ok_callback(Widget, XtPointer, XtPointer);
unsigned char* read_file_to_buffer(const char*, size_t*);
char* base64_encode(const unsigned char*, size_t);
static void popup_handler(Widget, XtPointer, XEvent*, Boolean*);
Widget create_text_popup_menu(Widget);
static void numeric_verify_cb(Widget, XtPointer, XtPointer);

void cut_callback(Widget, XtPointer, XtPointer); void copy_callback(Widget, XtPointer, XtPointer);
void paste_callback(Widget, XtPointer, XtPointer); void select_all_callback(Widget, XtPointer, XtPointer);
void settings_callback(Widget, XtPointer, XtPointer); void settings_tab_change_callback(Widget, XtPointer, XtPointer);
void settings_apply_callback(Widget, XtPointer, XtPointer); void settings_ok_callback(Widget, XtPointer, XtPointer);
void settings_cancel_callback(Widget, XtPointer, XtPointer); void settings_get_models_callback(Widget, XtPointer, XtPointer);
void *perform_get_models_thread(void*); void settings_use_selected_model_callback(Widget, XtPointer, XtPointer);
void populate_settings_dialog(); void retrieve_settings_from_dialog();
void settings_disable_history_limit_toggle_cb(Widget, XtPointer, XtPointer);
static void openai_base_url_focus_in_cb(Widget, XtPointer, XtPointer);
static void openai_base_url_focus_out_cb(Widget, XtPointer, XtPointer);

// ALL FUNCTION DEFINITIONS START HERE

void append_to_conversation(const char* text) {
    if (!conversation_text || !XtIsManaged(conversation_text)) return;
    XmTextPosition pos = XmTextGetLastPosition(conversation_text);
    XmTextInsert(conversation_text, pos, (char*)text);
    XmTextShowPosition(conversation_text, XmTextGetLastPosition(conversation_text));
}

void append_to_assistant_buffer(const char* text) {
    if (!text) return;
    size_t len = strlen(text);
    if (current_assistant_response_len + len + 1 > current_assistant_response_capacity) {
        current_assistant_response_capacity = (current_assistant_response_len + len + 1) * 2;
        char *new_buf = realloc(current_assistant_response_buffer, current_assistant_response_capacity);
        if (!new_buf) {
            perror("realloc assistant_buffer"); free(current_assistant_response_buffer);
            current_assistant_response_buffer = NULL; current_assistant_response_len = 0; current_assistant_response_capacity = 0;
            return;
        }
        current_assistant_response_buffer = new_buf;
    }
    memcpy(current_assistant_response_buffer + current_assistant_response_len, text, len);
    current_assistant_response_len += len;
    current_assistant_response_buffer[current_assistant_response_len] = '\0';
}

void write_pipe_message(pipe_message_type_t type, const char* data) {
    pipe_message_t msg; msg.type = type;
    if (data) strncpy(msg.data, data, sizeof(msg.data) - 1); else msg.data[0] = '\0';
    msg.data[sizeof(msg.data) - 1] = '\0';
    ssize_t written = write(pipe_fds[1], &msg, sizeof(pipe_message_t));
    if (written != sizeof(pipe_message_t)) {
        if (written == -1) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("write_pipe_message");
            } else {
                fprintf(stderr, "write_pipe_message: Pipe is full or temporarily unavailable (EAGAIN/EWOULDBLOCK).\n");
            }
        } else {
             fprintf(stderr, "write_pipe_message: Partial write to pipe (%ld bytes instead of %zu).\n", written, sizeof(pipe_message_t));
        }
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
        assistant_is_replying = true;
        prefix_already_added_for_current_reply = false;
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
                if (assistant_is_replying && !prefix_already_added_for_current_reply && current_assistant_response_len == 0) {
                    append_to_conversation(current_assistant_prefix);
                }
                append_to_conversation("\n");
                if (current_assistant_response_buffer && current_assistant_response_len > 0) {
                    add_message_to_history(DP_ROLE_ASSISTANT, current_assistant_response_buffer, NULL, NULL);
                } else if (assistant_is_replying && current_assistant_response_len == 0) {
                    add_message_to_history(DP_ROLE_ASSISTANT, "", NULL, NULL);
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
                if (settings_shell && XtIsManaged(settings_shell)) {
                    Widget list_to_update = XmToggleButtonGetState(provider_gemini_rb) ? gemini_model_list : openai_model_list;
                    if (list_to_update) {
                        XmString item = XmStringCreateLocalized(msg.data);
                        XmListAddItemUnselected(list_to_update, item, 0); XmStringFree(item);
                    }
                }
                break;
            case PIPE_MSG_MODEL_LIST_END: printf("Model listing complete.\n"); break;
            case PIPE_MSG_MODEL_LIST_ERROR: show_error_dialog(msg.data); break;
        }
    } else if (nbytes == 0) {
        fprintf(stderr, "handle_pipe_input: EOF on pipe.\n"); XtRemoveInput(*id);
    } else if (nbytes != -1) {
        fprintf(stderr, "handle_pipe_input: Partial read from pipe (%ld bytes).\n", nbytes);
    } else {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("handle_pipe_input: read from pipe"); XtRemoveInput(*id);
        }
    }
}

void remove_oldest_history_messages(int count_to_remove) {
    if (count_to_remove <= 0 || count_to_remove > chat_history_count) return;
    for (int i = 0; i < count_to_remove; ++i) dp_free_messages(&chat_history[i], 1);
    int remaining_count = chat_history_count - count_to_remove;
    if (remaining_count > 0) memmove(chat_history, &chat_history[count_to_remove], remaining_count * sizeof(dp_message_t));
    chat_history_count = remaining_count;
}

void add_message_to_history(dp_message_role_t role, const char* text_content, const char* img_mime_type, const char* img_base64_data) {
    int effective_max_history = history_limits_disabled ? INTERNAL_MAX_HISTORY_CAPACITY : current_max_history_messages;
    if (chat_history_count >= effective_max_history && effective_max_history > 0 && !history_limits_disabled ) {
        int messages_to_remove = (chat_history_count - effective_max_history) + 1;
        if (chat_history_count < messages_to_remove) messages_to_remove = chat_history_count;
        if (messages_to_remove > 0) {
             printf("Chat history limit (%d) reached. Removing %d oldest message(s).\n", effective_max_history, messages_to_remove);
             remove_oldest_history_messages(messages_to_remove);
        }
    }
    if (chat_history_count >= chat_history_capacity) {
        chat_history_capacity = (chat_history_capacity == 0) ? 10 : chat_history_capacity * 2;
        if (chat_history_capacity > INTERNAL_MAX_HISTORY_CAPACITY) chat_history_capacity = INTERNAL_MAX_HISTORY_CAPACITY;
        if (chat_history_count >= chat_history_capacity) {
            fprintf(stderr, "Cannot expand history further due to internal capacity limit.\n"); return;
        }
        dp_message_t *new_history = realloc(chat_history, chat_history_capacity * sizeof(dp_message_t));
        if (!new_history) { perror("realloc chat_history"); return; }
        chat_history = new_history;
    }
    dp_message_t *new_msg = &chat_history[chat_history_count];
    new_msg->role = role; new_msg->num_parts = 0; new_msg->parts = NULL;
    Boolean success = True;
    if ((text_content && strlen(text_content) > 0) || (role == DP_ROLE_ASSISTANT && text_content != NULL) ) {
        if (!dp_message_add_text_part(new_msg, text_content)) {
            fprintf(stderr, "Failed to add text part to history.\n"); success = False;
        }
    }
    if (success && img_base64_data && img_mime_type) {
        if (!dp_message_add_base64_image_part(new_msg, img_mime_type, img_base64_data)) {
            fprintf(stderr, "Failed to add image part to history.\n"); success = False;
        }
    }
    if (success && new_msg->num_parts > 0) {
        chat_history_count++;
    } else if (new_msg->parts) {
        free(new_msg->parts); new_msg->parts = NULL;
    } else if (!text_content && !img_base64_data && role == DP_ROLE_USER) { return; }
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
    printf("Thread: LLM request with %d messages.\n", (int)thread_data->config.num_messages);
    int ret = dp_perform_streaming_completion(dp_ctx, &thread_data->config, stream_handler, NULL, &response_status);
    if (ret != 0) {
        char err_buf[1024];
        snprintf(err_buf, sizeof(err_buf), "LLM Request Failed (Thread) (HTTP %ld): %s",
                 response_status.http_status_code,
                 response_status.error_message ? response_status.error_message : "DP error in thread.");
        write_pipe_message(PIPE_MSG_ERROR, err_buf);
    }
    dp_free_response_content(&response_status);
    free(thread_data);
    pthread_detach(pthread_self());
    return NULL;
}

void send_message_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    char *input_string_raw = XmTextGetString(input_text);
    if ((!input_string_raw || strlen(input_string_raw) == 0) && !attached_image_base64_data) {
        XtFree(input_string_raw); return;
    }
    if (input_string_raw && input_string_raw[strlen(input_string_raw)-1] == '\n') {
        input_string_raw[strlen(input_string_raw)-1] = '\0';
    }
    if (input_string_raw && strlen(input_string_raw) == 0) {
        XtFree(input_string_raw); input_string_raw = NULL;
        if (!attached_image_base64_data) return;
    }

    if (!dp_ctx) {
        show_error_dialog("LLM context not initialized. Please check API Key and Model ID in Settings.");
        if(input_string_raw) XtFree(input_string_raw);
        return;
    }


    char display_msg_text_part[1024] = "";
    if (input_string_raw) {
         snprintf(display_msg_text_part, sizeof(display_msg_text_part), "%s: %s", USER_NICKNAME, input_string_raw);
    } else {
         snprintf(display_msg_text_part, sizeof(display_msg_text_part), "%s: ", USER_NICKNAME);
    }
    char full_display_msg[2048]; strcpy(full_display_msg, display_msg_text_part);
    if (attached_image_base64_data) {
        char image_indicator[FILENAME_MAX + 50];
        char path_copy[PATH_MAX]; strncpy(path_copy, attached_image_path, PATH_MAX); path_copy[PATH_MAX-1] = '\0';
        snprintf(image_indicator, sizeof(image_indicator), " [Image Attached: %s]", basename(path_copy));
        strcat(full_display_msg, image_indicator);
    }
    strcat(full_display_msg, "\n"); append_to_conversation(full_display_msg);
    add_message_to_history(DP_ROLE_USER, input_string_raw ? input_string_raw : "",
                           attached_image_base64_data ? attached_image_mime_type : NULL,
                           attached_image_base64_data);
    XmTextSetString(input_text, "");
    if (input_string_raw) XtFree(input_string_raw);
    if (attached_image_base64_data) {
        free(attached_image_base64_data); attached_image_base64_data = NULL;
        attached_image_path[0] = '\0'; attached_image_mime_type[0] = '\0';
    }

    llm_thread_data_t *thread_data = malloc(sizeof(llm_thread_data_t));
    if (!thread_data) { perror("malloc llm_thread_data"); return; }
    thread_data->config.model = (current_api_provider == DP_PROVIDER_GOOGLE_GEMINI) ? current_gemini_model : current_openai_model;
    thread_data->config.temperature = 0.7; thread_data->config.max_tokens = 2048;
    thread_data->config.stream = true;
    thread_data->config.messages = chat_history;
    thread_data->config.num_messages = chat_history_count;
    snprintf(current_assistant_prefix, sizeof(current_assistant_prefix), "%s: ", ASSISTANT_NICKNAME);
    pthread_t tid;
    if (pthread_create(&tid, NULL, perform_llm_request_thread, thread_data) != 0) {
        perror("pthread_create llm_request"); free(thread_data);
        show_error_dialog("Failed to start LLM request thread.");
    }
}

void quit_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    printf("Exiting MotifGPT...\n"); save_settings(); free_chat_history();
    if (current_assistant_response_buffer) free(current_assistant_response_buffer);
    if (dp_ctx) dp_destroy_context(dp_ctx); curl_global_cleanup();
    if (pipe_fds[0] != -1) close(pipe_fds[0]); if (pipe_fds[1] != -1) close(pipe_fds[1]);
    if (settings_shell) XtDestroyWidget(settings_shell);
    XtDestroyApplicationContext(XtWidgetToApplicationContext(app_shell)); exit(0);
}

void clear_chat_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    XmTextSetString(conversation_text, ""); free_chat_history();
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

// Specific handler for Enter/Shift-Enter/Ctrl-Enter in input_text
static void input_text_key_press_handler(Widget w, XtPointer client_data, XEvent *event, Boolean *continue_to_dispatch) {
    if (event->type == KeyPress) {
        XKeyEvent *key_event = (XKeyEvent *)event; KeySym keysym; char buffer[1];
        XLookupString(key_event, buffer, 1, &keysym, NULL);

        if (keysym == XK_Return || keysym == XK_KP_Enter) {
            if (key_event->state & ShiftMask) { // Shift+Enter always inserts newline
                XmTextInsert(input_text, XmTextGetCursorPosition(input_text), "\n");
                *continue_to_dispatch = False;
            } else if (key_event->state & ControlMask) { // Ctrl+Enter always sends
                 send_message_callback(send_button, NULL, NULL);
                *continue_to_dispatch = False;
            } else { // Enter alone
                if (enter_key_sends_message) { // Check the setting
                    send_message_callback(send_button, NULL, NULL);
                } else {
                    XmTextInsert(input_text, XmTextGetCursorPosition(input_text), "\n");
                }
                *continue_to_dispatch = False;
            }
        } else {
            *continue_to_dispatch = True;
        }
    } else {
        *continue_to_dispatch = True;
    }
}

// General handler for Ctrl+X/C/V/A in any text widget that has focus
static void app_text_key_press_handler(Widget w, XtPointer client_data, XEvent *event, Boolean *continue_to_dispatch) {
    if (event->type == KeyPress) {
        XKeyEvent *key_event = (XKeyEvent *)event;
        KeySym keysym;
        char buffer[10]; XLookupString(key_event, buffer, sizeof(buffer)-1, &keysym, NULL);

        if (key_event->state & ControlMask) {
            if (keysym == XK_Return || keysym == XK_KP_Enter) {
                 *continue_to_dispatch = True;
                 return;
            }
            switch (keysym) {
                case XK_x: case XK_X:
                    if (focused_text_widget && (XmIsText(focused_text_widget) || XmIsTextField(focused_text_widget)) && XmTextGetEditable(focused_text_widget))
                        XmTextCut(focused_text_widget, XtLastTimestampProcessed(XtDisplay(focused_text_widget)));
                    *continue_to_dispatch = False; break;
                case XK_c: case XK_C:
                    if (focused_text_widget && (XmIsText(focused_text_widget) || XmIsTextField(focused_text_widget)))
                        XmTextCopy(focused_text_widget, XtLastTimestampProcessed(XtDisplay(focused_text_widget)));
                    *continue_to_dispatch = False; break;
                case XK_v: case XK_V:
                    if (focused_text_widget && (XmIsText(focused_text_widget) || XmIsTextField(focused_text_widget)) && XmTextGetEditable(focused_text_widget))
                        XmTextPaste(focused_text_widget);
                    *continue_to_dispatch = False; break;
                case XK_a: case XK_A:
                    if (focused_text_widget && (XmIsText(focused_text_widget) || XmIsTextField(focused_text_widget)))
                        XmTextSetSelection(focused_text_widget, 0, XmTextGetLastPosition(focused_text_widget), CurrentTime);
                    *continue_to_dispatch = False; break;
                default: *continue_to_dispatch = True; break;
            }
        } else { *continue_to_dispatch = True; }
    } else { *continue_to_dispatch = True; }
}


static void focus_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    XmAnyCallbackStruct *focus_data = (XmAnyCallbackStruct *)call_data;
    if (focus_data->reason == XmCR_FOCUS) {
        focused_text_widget = w;
    }
}

// Specific focus IN callback for settings text fields with placeholders
static void settings_text_field_focus_in_cb(Widget w, XtPointer client_data, XtPointer call_data) {
    const char* placeholder = (const char*) client_data;
    char *current_text = XmTextFieldGetString(w);
    if (current_text && placeholder && strcmp(current_text, placeholder) == 0) {
        Pixel current_fg;
        XtVaGetValues(w, XmNforeground, &current_fg, NULL);
        if (current_fg == grey_fg_color) {
            XmTextFieldSetString(w, "");
            XtVaSetValues(w, XmNforeground, normal_fg_color, NULL);
        }
    }
    XtFree(current_text);
    focus_callback(w, NULL, call_data); // Call general focus handler too
}

// Specific focus OUT callback for settings text fields with placeholders
static void settings_text_field_focus_out_cb(Widget w, XtPointer client_data, XtPointer call_data) {
    const char* placeholder = (const char*) client_data;
    char *current_text = XmTextFieldGetString(w);
    if (current_text && placeholder && strlen(current_text) == 0) { // Field is empty
        XmTextFieldSetString(w, (char*)placeholder); // Cast placeholder
        XtVaSetValues(w, XmNforeground, grey_fg_color, NULL);
    } else if (current_text && placeholder && strcmp(current_text, placeholder) != 0) {
        // User typed something else, ensure normal color
        XtVaSetValues(w, XmNforeground, normal_fg_color, NULL);
    }
    XtFree(current_text);
}


static void openai_base_url_focus_in_cb(Widget w, XtPointer client_data, XtPointer call_data) {
    settings_text_field_focus_in_cb(w, (XtPointer)DEFAULT_OPENAI_BASE_URL, call_data);
}
static void openai_base_url_focus_out_cb(Widget w, XtPointer client_data, XtPointer call_data) {
    settings_text_field_focus_out_cb(w, (XtPointer)DEFAULT_OPENAI_BASE_URL, call_data);
}


void cut_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    if (focused_text_widget && XtIsManaged(focused_text_widget) && (XmIsText(focused_text_widget) || XmIsTextField(focused_text_widget)) && XmTextGetEditable(focused_text_widget)) {
        XmTextCut(focused_text_widget, XtLastTimestampProcessed(XtDisplay(focused_text_widget)));
    }
}
void copy_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    if (focused_text_widget && XtIsManaged(focused_text_widget) && (XmIsText(focused_text_widget) || XmIsTextField(focused_text_widget))) {
        XmTextCopy(focused_text_widget, XtLastTimestampProcessed(XtDisplay(focused_text_widget)));
    }
}
void paste_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    if (focused_text_widget && XtIsManaged(focused_text_widget) && (XmIsText(focused_text_widget) || XmIsTextField(focused_text_widget)) && XmTextGetEditable(focused_text_widget)) {
        XmTextPaste(focused_text_widget);
    }
}
void select_all_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    if (focused_text_widget && XtIsManaged(focused_text_widget) && (XmIsText(focused_text_widget) || XmIsTextField(focused_text_widget))) {
        XmTextSetSelection(focused_text_widget, 0, XmTextGetLastPosition(focused_text_widget), CurrentTime);
        XmProcessTraversal(focused_text_widget, XmTRAVERSE_CURRENT);
    }
}

static void popup_handler(Widget w, XtPointer client_data, XEvent *event, Boolean *continue_to_dispatch) {
    if (event->type == ButtonPress && event->xbutton.button == 3) {
        focused_text_widget = w;
        if (popup_menu) {
            Boolean editable = False;
            if (XmIsText(focused_text_widget) || XmIsTextField(focused_text_widget)) {
                 editable = XmTextGetEditable(focused_text_widget);
            }
            XtSetSensitive(popup_cut_item, editable);
            XtSetSensitive(popup_paste_item, editable);
            XtSetSensitive(popup_copy_item, True);
            XtSetSensitive(popup_select_all_item, True);
            XmMenuPosition(popup_menu, (XButtonPressedEvent*)event);
            XtManageChild(popup_menu);
        }
        *continue_to_dispatch = False;
    } else {
        *continue_to_dispatch = True;
    }
}

Widget create_text_popup_menu(Widget parent_for_menu_shell) {
    Widget menu;
    XmString label_cs, accel_cs;

    menu = XmCreatePopupMenu(parent_for_menu_shell, "textPopupMenu", NULL, 0);

    label_cs = XmStringCreateLocalized("Cut");
    accel_cs = XmStringCreateLocalized("Ctrl+X");
    popup_cut_item = XtVaCreateManagedWidget("popupCut", xmPushButtonWidgetClass, menu,
                                             XmNlabelString, label_cs,
                                             XmNmnemonic, XK_t,
                                             XmNaccelerator, "Ctrl<Key>x",
                                             XmNacceleratorText, accel_cs,
                                             NULL);
    XtAddCallback(popup_cut_item, XmNactivateCallback, cut_callback, NULL);
    XmStringFree(label_cs); XmStringFree(accel_cs);

    label_cs = XmStringCreateLocalized("Copy");
    accel_cs = XmStringCreateLocalized("Ctrl+C");
    popup_copy_item = XtVaCreateManagedWidget("popupCopy", xmPushButtonWidgetClass, menu,
                                              XmNlabelString, label_cs,
                                              XmNmnemonic, XK_C,
                                              XmNaccelerator, "Ctrl<Key>c",
                                              XmNacceleratorText, accel_cs,
                                              NULL);
    XtAddCallback(popup_copy_item, XmNactivateCallback, copy_callback, NULL);
    XmStringFree(label_cs); XmStringFree(accel_cs);

    label_cs = XmStringCreateLocalized("Paste");
    accel_cs = XmStringCreateLocalized("Ctrl+V");
    popup_paste_item = XtVaCreateManagedWidget("popupPaste", xmPushButtonWidgetClass, menu,
                                               XmNlabelString, label_cs,
                                               XmNmnemonic, XK_P,
                                               XmNaccelerator, "Ctrl<Key>v",
                                               XmNacceleratorText, accel_cs,
                                               NULL);
    XtAddCallback(popup_paste_item, XmNactivateCallback, paste_callback, NULL);
    XmStringFree(label_cs); XmStringFree(accel_cs);

    XtVaCreateManagedWidget("popupSeparator", xmSeparatorWidgetClass, menu, NULL);

    label_cs = XmStringCreateLocalized("Select All");
    accel_cs = XmStringCreateLocalized("Ctrl+A");
    popup_select_all_item = XtVaCreateManagedWidget("popupSelectAll", xmPushButtonWidgetClass, menu,
                                                    XmNlabelString, label_cs,
                                                    XmNmnemonic, XK_A,
                                                    XmNaccelerator, "Ctrl<Key>a",
                                                    XmNacceleratorText, accel_cs,
                                                    NULL);
    XtAddCallback(popup_select_all_item, XmNactivateCallback, select_all_callback, NULL);
    XmStringFree(label_cs); XmStringFree(accel_cs);

    return menu;
}

char* get_config_path(const char* filename) {
    static char path[PATH_MAX]; wordexp_t p; char *env_path;
    if (filename == NULL) filename = "";
    env_path = getenv("XDG_CONFIG_HOME");
    if (env_path && env_path[0]) {
        snprintf(path, sizeof(path), "%s/motifgpt/%s", env_path, filename);
    } else {
        env_path = getenv("HOME");
        if (!env_path || !env_path[0]) {
            fprintf(stderr, "Error: HOME env var not set.\n"); return NULL;
        }
        snprintf(path, sizeof(path), "%s/.config/motifgpt/%s", env_path, filename);
    }
    if (path[0] == '~') {
        if (wordexp(path, &p, 0) == 0) {
            if (p.we_wordc > 0) strncpy(path, p.we_wordv[0], sizeof(path) - 1);
            path[sizeof(path) - 1] = '\0'; wordfree(&p);
        } else { fprintf(stderr, "wordexp failed for path: %s\n", path); }
    }
    return path;
}

int ensure_config_dir_exists() {
    char *base_dir_path_ptr = get_config_path(""); if (!base_dir_path_ptr) return -1;
    char base_dir_path[PATH_MAX]; strncpy(base_dir_path, base_dir_path_ptr, PATH_MAX -1); base_dir_path[PATH_MAX-1] = '\0';
    if (base_dir_path[strlen(base_dir_path)-1] == '/') base_dir_path[strlen(base_dir_path)-1] = '\0';
    struct stat st = {0};
    if (stat(base_dir_path, &st) == -1) {
        if (mkdir(base_dir_path, CONFIG_DIR_MODE) == -1 && errno != EEXIST) {
            char err_msg[PATH_MAX + 100]; snprintf(err_msg, sizeof(err_msg), "mkdir base config dir: %s", base_dir_path);
            perror(err_msg); return -1;
        }
        printf("Created config directory: %s\n", base_dir_path);
    }
    char cache_dir_full_path[PATH_MAX];
    snprintf(cache_dir_full_path, sizeof(cache_dir_full_path), "%s/%s", base_dir_path, CACHE_DIR_NAME);
    if (stat(cache_dir_full_path, &st) == -1) {
        if (mkdir(cache_dir_full_path, CONFIG_DIR_MODE) == -1 && errno != EEXIST) {
            char err_msg[PATH_MAX + 100]; snprintf(err_msg, sizeof(err_msg), "mkdir cache dir: %s", cache_dir_full_path);
            perror(err_msg);
        } else { printf("Created cache directory: %s\n", cache_dir_full_path); }
    }
    return 0;
}

void load_settings() {
    char *settings_file = get_config_path(CONFIG_FILE_NAME);
    if (!settings_file) { fprintf(stderr, "Could not determine settings file path.\n"); return; }
    FILE *fp = fopen(settings_file, "r");
    if (!fp) {
        printf("No settings file (%s). Using defaults/environment variables.\n", settings_file);
        const char* ge = getenv("GEMINI_API_KEY"); if (ge) strncpy(current_gemini_api_key, ge, sizeof(current_gemini_api_key)-1); else current_gemini_api_key[0] = '\0';
        const char* oe = getenv("OPENAI_API_KEY"); if (oe) strncpy(current_openai_api_key, oe, sizeof(current_openai_api_key)-1); else current_openai_api_key[0] = '\0';
        current_max_history_messages = DEFAULT_MAX_HISTORY_MESSAGES;
        history_limits_disabled = False;
        enter_key_sends_message = True;
        return;
    }
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        char *key = strtok(line, "="); char *value = strtok(NULL, "\n");
        if (key && value) {
            if (strcmp(key, "provider") == 0) {
                if (strcmp(value, "gemini") == 0) current_api_provider = DP_PROVIDER_GOOGLE_GEMINI;
                else if (strcmp(value, "openai") == 0) current_api_provider = DP_PROVIDER_OPENAI_COMPATIBLE;
            } else if (strcmp(key, "gemini_api_key") == 0) strncpy(current_gemini_api_key, value, sizeof(current_gemini_api_key)-1);
            else if (strcmp(key, "gemini_model") == 0) strncpy(current_gemini_model, value, sizeof(current_gemini_model)-1);
            else if (strcmp(key, "openai_api_key") == 0) strncpy(current_openai_api_key, value, sizeof(current_openai_api_key)-1);
            else if (strcmp(key, "openai_model") == 0) strncpy(current_openai_model, value, sizeof(current_openai_model)-1);
            else if (strcmp(key, "openai_base_url") == 0) strncpy(current_openai_base_url, value, sizeof(current_openai_base_url)-1);
            else if (strcmp(key, "max_history") == 0) current_max_history_messages = atoi(value);
            else if (strcmp(key, "history_limits_disabled") == 0) history_limits_disabled = (strcmp(value, "true") == 0);
            else if (strcmp(key, "enter_sends_message") == 0) enter_key_sends_message = (strcmp(value, "true") == 0);

        }
    }
    fclose(fp); printf("Settings loaded from %s\n", settings_file);
}

void save_settings() {
    if (ensure_config_dir_exists() != 0) { fprintf(stderr, "Config dir error. Settings not saved.\n"); return; }
    char *settings_file = get_config_path(CONFIG_FILE_NAME);
    if (!settings_file) { fprintf(stderr, "Settings file path error. Not saved.\n"); return; }
    FILE *fp = fopen(settings_file, "w");
    if (!fp) {
        char err_msg[PATH_MAX + 100]; snprintf(err_msg, sizeof(err_msg), "fopen for writing: %s", settings_file);
        perror(err_msg); return;
    }
    fprintf(fp, "provider=%s\n", (current_api_provider == DP_PROVIDER_GOOGLE_GEMINI) ? "gemini" : "openai");
    fprintf(fp, "gemini_api_key=%s\n", current_gemini_api_key);
    fprintf(fp, "gemini_model=%s\n", current_gemini_model);
    fprintf(fp, "openai_api_key=%s\n", current_openai_api_key);
    fprintf(fp, "openai_model=%s\n", current_openai_model);
    fprintf(fp, "openai_base_url=%s\n", current_openai_base_url);
    fprintf(fp, "max_history=%d\n", current_max_history_messages);
    fprintf(fp, "history_limits_disabled=%s\n", history_limits_disabled ? "true" : "false");
    fprintf(fp, "enter_sends_message=%s\n", enter_key_sends_message ? "true" : "false");
    fclose(fp); printf("Settings saved to %s\n", settings_file);
}

void file_selection_ok_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    XmFileSelectionBoxCallbackStruct *cbs = (XmFileSelectionBoxCallbackStruct *)call_data;
    char *filename = NULL; XmStringGetLtoR(cbs->value, XmFONTLIST_DEFAULT_TAG, &filename);
    if (!filename || strlen(filename) == 0) { XtFree(filename); return; }
    strncpy(attached_image_path, filename, PATH_MAX -1); attached_image_path[PATH_MAX-1] = '\0';
    if (strstr(filename, ".png") || strstr(filename, ".PNG")) strcpy(attached_image_mime_type, "image/png");
    else if (strstr(filename, ".jpg") || strstr(filename, ".JPG") || strstr(filename, ".jpeg") || strstr(filename, ".JPEG")) strcpy(attached_image_mime_type, "image/jpeg");
    else if (strstr(filename, ".gif") || strstr(filename, ".GIF")) strcpy(attached_image_mime_type, "image/gif");
    else { show_error_dialog("Unsupported image type (PNG, JPG, GIF)."); XtFree(filename); attached_image_path[0] = '\0'; return; }
    size_t file_size; unsigned char *file_buffer = read_file_to_buffer(filename, &file_size); XtFree(filename);
    if (!file_buffer) { show_error_dialog("Could not read image file."); attached_image_path[0] = '\0'; return; }
    if (attached_image_base64_data) free(attached_image_base64_data);
    attached_image_base64_data = base64_encode(file_buffer, file_size); free(file_buffer);
    if (!attached_image_base64_data) { show_error_dialog("Could not Base64 encode image."); attached_image_path[0] = '\0'; return; }
    char status_msg[PATH_MAX + 50];
    char path_copy[PATH_MAX]; strncpy(path_copy, attached_image_path, PATH_MAX); path_copy[PATH_MAX-1] = '\0';
    snprintf(status_msg, sizeof(status_msg), "[Image ready: %s]", basename(path_copy));
    append_to_conversation(status_msg); append_to_conversation("\n");
    XtUnmanageChild(w);
}

void attach_image_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    static Widget file_selector = NULL;
    if (!file_selector) {
        file_selector = XmCreateFileSelectionDialog(app_shell, "fileSelector", NULL, 0);
        XtAddCallback(file_selector, XmNokCallback, file_selection_ok_callback, NULL);
        XtAddCallback(file_selector, XmNcancelCallback, (XtCallbackProc)XtUnmanageChild, NULL);
        XtUnmanageChild(XmFileSelectionBoxGetChild(file_selector, XmDIALOG_HELP_BUTTON));
    }
    XtManageChild(file_selector);
}

char* base64_encode(const unsigned char *data, size_t input_length) {
    const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t output_length = 4 * ((input_length + 2) / 3);
    char *encoded_data = malloc(output_length + 1);
    if (!encoded_data) { perror("malloc base64"); return NULL; }
    for (size_t i = 0, j = 0; i < input_length;) {
        uint32_t octet_a = i < input_length ? data[i++] : 0;
        uint32_t octet_b = i < input_length ? data[i++] : 0;
        uint32_t octet_c = i < input_length ? data[i++] : 0;
        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;
        encoded_data[j++] = base64_chars[(triple >> 18) & 0x3F];
        encoded_data[j++] = base64_chars[(triple >> 12) & 0x3F];
        encoded_data[j++] = base64_chars[(triple >> 6) & 0x3F];
        encoded_data[j++] = base64_chars[(triple >> 0) & 0x3F];
    }
    for (size_t i = 0; i < (3 - input_length % 3) % 3; i++) encoded_data[output_length - 1 - i] = '=';
    encoded_data[output_length] = '\0';
    return encoded_data;
}

unsigned char* read_file_to_buffer(const char* filename, size_t* file_size) {
    FILE* f = fopen(filename, "rb");
    if (!f) { perror("fopen read_file"); return NULL; }
    fseek(f, 0, SEEK_END); long size = ftell(f);
    if (size < 0 || size > 20 * 1024 * 1024) {
        fclose(f); fprintf(stderr, "File too large (max 20MB) or ftell error.\n"); return NULL;
    }
    *file_size = (size_t)size; fseek(f, 0, SEEK_SET);
    unsigned char* buffer = malloc(*file_size);
    if (!buffer) { fclose(f); perror("malloc read_file"); return NULL; }
    if (fread(buffer, 1, *file_size, f) != *file_size) {
        fclose(f); free(buffer); fprintf(stderr, "fread error.\n"); return NULL;
    }
    fclose(f); return buffer;
}

void initialize_dp_context() {
    if (dp_ctx) { dp_destroy_context(dp_ctx); dp_ctx = NULL; }
    const char* key_to_use = (current_api_provider == DP_PROVIDER_GOOGLE_GEMINI) ? current_gemini_api_key : current_openai_api_key;
    const char* model_to_use = (current_api_provider == DP_PROVIDER_GOOGLE_GEMINI) ? current_gemini_model : current_openai_model;
    const char* base_url_to_use = NULL;

    if (current_api_provider == DP_PROVIDER_OPENAI_COMPATIBLE) {
        if (strlen(current_openai_base_url) > 0 && strcmp(current_openai_base_url, DEFAULT_OPENAI_BASE_URL) != 0) {
             base_url_to_use = current_openai_base_url;
        }
    }


    if (strlen(key_to_use) == 0 ||
        (current_api_provider == DP_PROVIDER_GOOGLE_GEMINI && strcmp(key_to_use, DEFAULT_GEMINI_KEY_PLACEHOLDER)==0 ) ||
        (current_api_provider == DP_PROVIDER_OPENAI_COMPATIBLE && strcmp(key_to_use, DEFAULT_OPENAI_KEY_PLACEHOLDER)==0) ) {
        // Check if the key is literally the placeholder string (which it might be if settings dialog hasn't been touched yet)
        Boolean is_placeholder_key = False;
        if (settings_shell && XtIsRealized(settings_shell)) { // Check widgets only if settings dialog has been created and realized
            Pixel key_fg_color;
            if (current_api_provider == DP_PROVIDER_GOOGLE_GEMINI && gemini_api_key_text) {
                XtVaGetValues(gemini_api_key_text, XmNforeground, &key_fg_color, NULL);
                if (key_fg_color == grey_fg_color) is_placeholder_key = True;
            } else if (current_api_provider == DP_PROVIDER_OPENAI_COMPATIBLE && openai_api_key_text) {
                XtVaGetValues(openai_api_key_text, XmNforeground, &key_fg_color, NULL);
                if (key_fg_color == grey_fg_color) is_placeholder_key = True;
            }
        } else { // Fallback to string comparison if dialog not up (e.g., initial load)
             if ((current_api_provider == DP_PROVIDER_GOOGLE_GEMINI && strcmp(key_to_use, DEFAULT_GEMINI_KEY_PLACEHOLDER)==0) ||
                 (current_api_provider == DP_PROVIDER_OPENAI_COMPATIBLE && strcmp(key_to_use, DEFAULT_OPENAI_KEY_PLACEHOLDER)==0)) {
                 is_placeholder_key = True;
             }
        }
        if (is_placeholder_key || strlen(key_to_use) == 0) {
             fprintf(stderr, "API Key not set or is placeholder. LLM disabled until configured in Settings.\n"); return;
        }
    }

    if (strlen(model_to_use) == 0 ||
        (current_api_provider == DP_PROVIDER_GOOGLE_GEMINI && strcmp(model_to_use, DEFAULT_GEMINI_MODEL)==0 ) ||
        (current_api_provider == DP_PROVIDER_OPENAI_COMPATIBLE && strcmp(model_to_use, DEFAULT_OPENAI_MODEL)==0 ) ) {
        Boolean is_placeholder_model = False;
         if (settings_shell && XtIsRealized(settings_shell)) {
            Pixel model_fg_color;
            if (current_api_provider == DP_PROVIDER_GOOGLE_GEMINI && gemini_model_text) {
                XtVaGetValues(gemini_model_text, XmNforeground, &model_fg_color, NULL);
                if (model_fg_color == grey_fg_color) is_placeholder_model = True;
            } else if (current_api_provider == DP_PROVIDER_OPENAI_COMPATIBLE && openai_model_text) {
                XtVaGetValues(openai_model_text, XmNforeground, &model_fg_color, NULL);
                if (model_fg_color == grey_fg_color) is_placeholder_model = True;
            }
         } else {
             if ((current_api_provider == DP_PROVIDER_GOOGLE_GEMINI && strcmp(model_to_use, DEFAULT_GEMINI_MODEL)==0) ||
                 (current_api_provider == DP_PROVIDER_OPENAI_COMPATIBLE && strcmp(model_to_use, DEFAULT_OPENAI_MODEL)==0)) {
                 is_placeholder_model = True;
             }
         }
         if(is_placeholder_model && (strlen(key_to_use)==0 ||
            (current_api_provider == DP_PROVIDER_GOOGLE_GEMINI && strcmp(key_to_use, DEFAULT_GEMINI_KEY_PLACEHOLDER)==0) ||
            (current_api_provider == DP_PROVIDER_OPENAI_COMPATIBLE && strcmp(key_to_use, DEFAULT_OPENAI_KEY_PLACEHOLDER)==0) )){
             fprintf(stderr, "Model ID not set or is placeholder (and key is missing/placeholder). LLM disabled until configured in Settings.\n"); return;
         }
    }


    dp_ctx = dp_init_context(current_api_provider, key_to_use, base_url_to_use);
    if (!dp_ctx) { fprintf(stderr, "Failed to init LLM context with current settings.\n"); }
    else {
        printf("LLM context initialized. Provider: %s, Model: %s, Base URL: %s\n",
               (current_api_provider == DP_PROVIDER_GOOGLE_GEMINI ? "Gemini" : "OpenAI"),
               model_to_use,
               base_url_to_use ? base_url_to_use : "(default by disasterparty)");
    }
}

void settings_disable_history_limit_toggle_cb(Widget w, XtPointer client_data, XtPointer call_data) {
    Boolean set = XmToggleButtonGetState(w);
    XtSetSensitive(history_length_text, !set);
}

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

static void settings_text_field_focus_in_cb(Widget w, XtPointer client_data, XtPointer call_data) {
    const char* placeholder = (const char*) client_data;
    char *current_text = XmTextFieldGetString(w);
    if (current_text && placeholder && strcmp(current_text, placeholder) == 0) {
        Pixel current_fg;
        XtVaGetValues(w, XmNforeground, &current_fg, NULL);
        if (current_fg == grey_fg_color) {
            XmTextFieldSetString(w, "");
            XtVaSetValues(w, XmNforeground, normal_fg_color, NULL);
        }
    }
    XtFree(current_text);
    focus_callback(w, NULL, call_data);
}

static void settings_text_field_focus_out_cb(Widget w, XtPointer client_data, XtPointer call_data) {
    const char* placeholder = (const char*) client_data;
    char *current_text = XmTextFieldGetString(w);
    if (current_text && placeholder && strlen(current_text) == 0) {
        XmTextFieldSetString(w, (char*)placeholder);
        XtVaSetValues(w, XmNforeground, grey_fg_color, NULL);
    } else if (current_text && placeholder && strcmp(current_text, placeholder) != 0) {
        XtVaSetValues(w, XmNforeground, normal_fg_color, NULL);
    }
    XtFree(current_text);
}

static void openai_base_url_focus_in_cb(Widget w, XtPointer client_data, XtPointer call_data) {
    settings_text_field_focus_in_cb(w, (XtPointer)DEFAULT_OPENAI_BASE_URL, call_data);
}
static void openai_base_url_focus_out_cb(Widget w, XtPointer client_data, XtPointer call_data) {
    settings_text_field_focus_out_cb(w, (XtPointer)DEFAULT_OPENAI_BASE_URL, call_data);
}


void populate_settings_dialog() {
    XmToggleButtonSetState(provider_gemini_rb, current_api_provider == DP_PROVIDER_GOOGLE_GEMINI, False);
    XmToggleButtonSetState(provider_openai_rb, current_api_provider == DP_PROVIDER_OPENAI_COMPATIBLE, False);

    char hist_len_str[10];
    snprintf(hist_len_str, sizeof(hist_len_str), "%d", current_max_history_messages);
    XmTextFieldSetString(history_length_text, hist_len_str);
    XmToggleButtonSetState(disable_history_limit_toggle, history_limits_disabled, False);
    XmToggleButtonSetState(enter_sends_message_toggle, enter_key_sends_message, False);
    XtSetSensitive(history_length_text, !history_limits_disabled);

    if (strlen(current_gemini_api_key) == 0) {
        XmTextFieldSetString(gemini_api_key_text, DEFAULT_GEMINI_KEY_PLACEHOLDER);
        XtVaSetValues(gemini_api_key_text, XmNforeground, grey_fg_color, NULL);
    } else {
        XmTextFieldSetString(gemini_api_key_text, current_gemini_api_key);
        XtVaSetValues(gemini_api_key_text, XmNforeground, normal_fg_color, NULL);
    }
    if (strlen(current_gemini_model) == 0) {
        XmTextFieldSetString(gemini_model_text, DEFAULT_GEMINI_MODEL);
        XtVaSetValues(gemini_model_text, XmNforeground, grey_fg_color, NULL);
    } else {
        XmTextFieldSetString(gemini_model_text, current_gemini_model);
        XtVaSetValues(gemini_model_text, XmNforeground, normal_fg_color, NULL);
    }
    XmListDeleteAllItems(gemini_model_list);

    if (strlen(current_openai_api_key) == 0) {
        XmTextFieldSetString(openai_api_key_text, DEFAULT_OPENAI_KEY_PLACEHOLDER);
        XtVaSetValues(openai_api_key_text, XmNforeground, grey_fg_color, NULL);
    } else {
        XmTextFieldSetString(openai_api_key_text, current_openai_api_key);
        XtVaSetValues(openai_api_key_text, XmNforeground, normal_fg_color, NULL);
    }
    if (strlen(current_openai_model) == 0) {
        XmTextFieldSetString(openai_model_text, DEFAULT_OPENAI_MODEL);
        XtVaSetValues(openai_model_text, XmNforeground, grey_fg_color, NULL);
    } else {
        XmTextFieldSetString(openai_model_text, current_openai_model);
        XtVaSetValues(openai_model_text, XmNforeground, normal_fg_color, NULL);
    }
    if (strlen(current_openai_base_url) == 0) {
        XmTextFieldSetString(openai_base_url_text, DEFAULT_OPENAI_BASE_URL);
        XtVaSetValues(openai_base_url_text, XmNforeground, grey_fg_color, NULL);
    } else {
        XmTextFieldSetString(openai_base_url_text, current_openai_base_url);
        XtVaSetValues(openai_base_url_text, XmNforeground, normal_fg_color, NULL);
    }
    XmListDeleteAllItems(openai_model_list);
}

void retrieve_settings_from_dialog() {
    if (XmToggleButtonGetState(provider_gemini_rb)) current_api_provider = DP_PROVIDER_GOOGLE_GEMINI;
    else if (XmToggleButtonGetState(provider_openai_rb)) current_api_provider = DP_PROVIDER_OPENAI_COMPATIBLE;

    char *hist_len_str = XmTextFieldGetString(history_length_text);
    current_max_history_messages = atoi(hist_len_str);
    if (current_max_history_messages <= 0 && !history_limits_disabled) current_max_history_messages = DEFAULT_MAX_HISTORY_MESSAGES;
    XtFree(hist_len_str);
    history_limits_disabled = XmToggleButtonGetState(disable_history_limit_toggle);
    enter_key_sends_message = XmToggleButtonGetState(enter_sends_message_toggle);

    char *tmp; Pixel fg_color;

    tmp = XmTextFieldGetString(gemini_api_key_text);
    XtVaGetValues(gemini_api_key_text, XmNforeground, &fg_color, NULL);
    if (strcmp(tmp, DEFAULT_GEMINI_KEY_PLACEHOLDER) == 0 && fg_color == grey_fg_color) current_gemini_api_key[0] = '\0';
    else { strncpy(current_gemini_api_key, tmp, sizeof(current_gemini_api_key)-1); current_gemini_api_key[sizeof(current_gemini_api_key)-1]='\0'; }
    XtFree(tmp);
    tmp = XmTextFieldGetString(gemini_model_text);
    XtVaGetValues(gemini_model_text, XmNforeground, &fg_color, NULL);
    if (strcmp(tmp, DEFAULT_GEMINI_MODEL) == 0 && fg_color == grey_fg_color) strcpy(current_gemini_model, DEFAULT_GEMINI_MODEL);
    else { strncpy(current_gemini_model, tmp, sizeof(current_gemini_model)-1); current_gemini_model[sizeof(current_gemini_model)-1]='\0';}
    XtFree(tmp);

    tmp = XmTextFieldGetString(openai_api_key_text);
    XtVaGetValues(openai_api_key_text, XmNforeground, &fg_color, NULL);
    if (strcmp(tmp, DEFAULT_OPENAI_KEY_PLACEHOLDER) == 0 && fg_color == grey_fg_color) current_openai_api_key[0] = '\0';
    else { strncpy(current_openai_api_key, tmp, sizeof(current_openai_api_key)-1); current_openai_api_key[sizeof(current_openai_api_key)-1]='\0'; }
    XtFree(tmp);
    tmp = XmTextFieldGetString(openai_model_text);
    XtVaGetValues(openai_model_text, XmNforeground, &fg_color, NULL);
    if (strcmp(tmp, DEFAULT_OPENAI_MODEL) == 0 && fg_color == grey_fg_color) strcpy(current_openai_model, DEFAULT_OPENAI_MODEL);
    else { strncpy(current_openai_model, tmp, sizeof(current_openai_model)-1); current_openai_model[sizeof(current_openai_model)-1]='\0';}
    XtFree(tmp);
    tmp = XmTextFieldGetString(openai_base_url_text);
    XtVaGetValues(openai_base_url_text, XmNforeground, &fg_color, NULL);
    if (strcmp(tmp, DEFAULT_OPENAI_BASE_URL) == 0 && fg_color == grey_fg_color) current_openai_base_url[0] = '\0';
    else { strncpy(current_openai_base_url, tmp, sizeof(current_openai_base_url)-1); current_openai_base_url[sizeof(current_openai_base_url)-1]='\0'; }
    XtFree(tmp);
}

void settings_apply_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    retrieve_settings_from_dialog(); initialize_dp_context(); save_settings();
}

void settings_ok_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    settings_apply_callback(w, client_data, call_data); XtUnmanageChild(settings_shell);
}

void settings_cancel_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    XtUnmanageChild(settings_shell);
}

void *perform_get_models_thread(void *arg) {
    get_models_thread_data_t *data = (get_models_thread_data_t *)arg;
    dp_context_t *temp_ctx = dp_init_context(data->provider, data->api_key_for_list, (strlen(data->base_url_for_list) > 0 ? data->base_url_for_list : NULL) );
    if (!temp_ctx) {
        write_pipe_message(PIPE_MSG_MODEL_LIST_ERROR, "GetModels: Failed temp context.");
        free(data); pthread_detach(pthread_self()); return NULL;
    }
    dp_model_list_t *model_list_struct = NULL; int result = dp_list_models(temp_ctx, &model_list_struct);
    if (result == 0 && model_list_struct) {
        if (model_list_struct->error_message) {
             char err_buf[512]; snprintf(err_buf, sizeof(err_buf), "API Error (Get Models): %s", model_list_struct->error_message);
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
        } else if (model_list_struct) { snprintf(err_buf, sizeof(err_buf), "Error Get Models (HTTP %ld): Unknown API error.", model_list_struct->http_status_code);
        } else { strcpy(err_buf, "Error Get Models: dp_list_models failed critically."); }
        write_pipe_message(PIPE_MSG_MODEL_LIST_ERROR, err_buf);
    }
    write_pipe_message(PIPE_MSG_MODEL_LIST_END, NULL);
    dp_free_model_list(model_list_struct); dp_destroy_context(temp_ctx);
    free(data); pthread_detach(pthread_self()); return NULL;
}

void settings_get_models_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    dp_provider_type_t provider_for_list; char *api_key_str; char *base_url_str = NULL;
    long tab_type = (long)client_data;

    if (tab_type == 0) {
        provider_for_list = DP_PROVIDER_GOOGLE_GEMINI; api_key_str = XmTextFieldGetString(gemini_api_key_text);
        XmListDeleteAllItems(gemini_model_list);
    } else {
        provider_for_list = DP_PROVIDER_OPENAI_COMPATIBLE; api_key_str = XmTextFieldGetString(openai_api_key_text);
        base_url_str = XmTextFieldGetString(openai_base_url_text);
        XmListDeleteAllItems(openai_model_list);
    }

    Pixel key_fg;
    if (tab_type == 0) XtVaGetValues(gemini_api_key_text, XmNforeground, &key_fg, NULL);
    else XtVaGetValues(openai_api_key_text, XmNforeground, &key_fg, NULL);

    if (!api_key_str || strlen(api_key_str) == 0 || key_fg == grey_fg_color ) {
        show_error_dialog("API Key for the current tab (not placeholder) is required to fetch models.");
        XtFree(api_key_str); if(base_url_str) XtFree(base_url_str); return;
    }
    get_models_thread_data_t *thread_data = malloc(sizeof(get_models_thread_data_t));
    if(!thread_data){ perror("malloc get_models_thread_data"); XtFree(api_key_str); if(base_url_str) XtFree(base_url_str); return; }
    thread_data->provider = provider_for_list;
    strncpy(thread_data->api_key_for_list, api_key_str, sizeof(thread_data->api_key_for_list)-1);
    thread_data->api_key_for_list[sizeof(thread_data->api_key_for_list)-1] = '\0';

    if (base_url_str && strlen(base_url_str) > 0 && strcmp(base_url_str, DEFAULT_OPENAI_BASE_URL) != 0) {
        Pixel current_fg; XtVaGetValues(openai_base_url_text, XmNforeground, &current_fg, NULL);
        if (current_fg != grey_fg_color) {
            strncpy(thread_data->base_url_for_list, base_url_str, sizeof(thread_data->base_url_for_list)-1);
            thread_data->base_url_for_list[sizeof(thread_data->base_url_for_list)-1] = '\0';
        } else {
             thread_data->base_url_for_list[0] = '\0';
        }
    } else {
        thread_data->base_url_for_list[0] = '\0';
    }
    XtFree(api_key_str); if(base_url_str) XtFree(base_url_str);
    pthread_t tid;
    if (pthread_create(&tid, NULL, perform_get_models_thread, thread_data) != 0) {
        perror("pthread_create get_models"); free(thread_data);
        show_error_dialog("Failed to start Get Models thread.");
    }
}

void settings_use_selected_model_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    long tab_type = (long)client_data;
    Widget source_list = (tab_type == 0) ? gemini_model_list : openai_model_list;
    Widget target_text = (tab_type == 0) ? gemini_model_text : openai_model_text;
    XmString *sel_items; int item_count;
    XtVaGetValues(source_list, XmNselectedItems, &sel_items, XmNselectedItemCount, &item_count, NULL);
    if (item_count > 0) {
        char *text = NULL; XmStringGetLtoR(sel_items[0], XmFONTLIST_DEFAULT_TAG, &text);
        if (text) {
             XmTextFieldSetString(target_text, text);
             XtVaSetValues(target_text, XmNforeground, normal_fg_color, NULL);
             XtFree(text);
        }
    }
}

// ModifyVerify callback to allow only numeric input
static void numeric_verify_cb(Widget w, XtPointer client_data, XtPointer call_data) {
    XmTextVerifyCallbackStruct *cbs = (XmTextVerifyCallbackStruct *)call_data;
    if (cbs->text && cbs->text->ptr) {
        for (int i = 0; i < cbs->text->length; i++) {
            if (!isdigit(cbs->text->ptr[i])) {
                cbs->doit = False; // Reject non-digit input
                return;
            }
        }
    }
}


void settings_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    if (settings_shell == NULL) {
        settings_shell = XtVaCreatePopupShell("settingsShell", topLevelShellWidgetClass, app_shell, XmNtitle, "MotifGPT Settings", XmNwidth, 550, XmNheight, 550, NULL);
        Widget dialog_form = XtVaCreateManagedWidget("dialogForm", xmFormWidgetClass, settings_shell, XmNverticalSpacing, 5, XmNhorizontalSpacing, 5, NULL);
        Widget tab_button_rc = XtVaCreateManagedWidget("tabButtonRc", xmRowColumnWidgetClass, dialog_form, XmNorientation, XmHORIZONTAL, XmNradioBehavior, True, XmNindicatorType, XmONE_OF_MANY, XmNentryAlignment, XmALIGNMENT_CENTER, XmNpacking, XmPACK_TIGHT, XmNspacing, 0, XmNtopAttachment, XmATTACH_FORM, XmNtopOffset, 5, XmNleftAttachment, XmATTACH_FORM, XmNleftOffset, 5, XmNrightAttachment, XmATTACH_FORM, XmNrightOffset, 5, XmNshadowThickness, 0, NULL);
        Widget general_tab_btn = XtVaCreateManagedWidget("General", xmToggleButtonWidgetClass, tab_button_rc, XmNindicatorOn, False, XmNshadowThickness, 2, NULL);
        Widget gemini_tab_btn = XtVaCreateManagedWidget("Gemini", xmToggleButtonWidgetClass, tab_button_rc, XmNindicatorOn, False, XmNshadowThickness, 2, NULL);
        Widget openai_tab_btn = XtVaCreateManagedWidget("OpenAI", xmToggleButtonWidgetClass, tab_button_rc, XmNindicatorOn, False, XmNshadowThickness, 2, NULL);
        XtAddCallback(general_tab_btn, XmNvalueChangedCallback, settings_tab_change_callback, (XtPointer)0);
        XtAddCallback(gemini_tab_btn, XmNvalueChangedCallback, settings_tab_change_callback, (XtPointer)1);
        XtAddCallback(openai_tab_btn, XmNvalueChangedCallback, settings_tab_change_callback, (XtPointer)2);
        Widget content_frame = XtVaCreateManagedWidget("contentFrame", xmFrameWidgetClass, dialog_form, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, tab_button_rc, XmNtopOffset, 5, XmNleftAttachment, XmATTACH_FORM, XmNleftOffset, 5, XmNrightAttachment, XmATTACH_FORM, XmNrightOffset, 5, XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, 45, XmNshadowType, XmSHADOW_ETCHED_IN, NULL);

        settings_general_tab_content = XtVaCreateWidget("generalTab", xmFormWidgetClass, content_frame, XmNmarginWidth, 10, XmNmarginHeight, 10, XmNtopOffset, 10, XmNleftOffset, 10, XmNrightOffset, 10, XmNbottomOffset, 10, NULL);
        Widget provider_label = XtVaCreateManagedWidget("Model API Provider:", xmLabelWidgetClass, settings_general_tab_content, XmNtopAttachment, XmATTACH_FORM, XmNleftAttachment, XmATTACH_FORM, XmNalignment, XmALIGNMENT_BEGINNING, NULL);
        Widget provider_radio_box = XmCreateRadioBox(settings_general_tab_content, "providerRadioBox", NULL, 0);
        XtVaSetValues(provider_radio_box, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, provider_label, XmNleftAttachment, XmATTACH_FORM, XmNorientation, XmHORIZONTAL, NULL);
        provider_gemini_rb = XtVaCreateManagedWidget("Gemini", xmToggleButtonWidgetClass, provider_radio_box, NULL);
        provider_openai_rb = XtVaCreateManagedWidget("OpenAI-compatible", xmToggleButtonWidgetClass, provider_radio_box, NULL);
        XtManageChild(provider_radio_box);
        Widget history_label = XtVaCreateManagedWidget("Message History Length:", xmLabelWidgetClass, settings_general_tab_content, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, provider_radio_box, XmNtopOffset, 15, XmNleftAttachment, XmATTACH_FORM, XmNalignment, XmALIGNMENT_BEGINNING, NULL);
        history_length_text = XtVaCreateManagedWidget("historyLengthText", xmTextFieldWidgetClass, settings_general_tab_content, XmNcolumns, 5, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, provider_radio_box, XmNtopOffset, 10, XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, history_label, XmNleftOffset, 5, NULL);
        XtAddCallback(history_length_text, XmNmodifyVerifyCallback, numeric_verify_cb, NULL);
        XtAddEventHandler(history_length_text, KeyPressMask, False, app_text_key_press_handler, NULL);
        XtAddEventHandler(history_length_text, ButtonPressMask, False, popup_handler, NULL);
        XtAddCallback(history_length_text, XmNfocusCallback, focus_callback, NULL);
        disable_history_limit_toggle = XtVaCreateManagedWidget("Disable Message History FIFO Limit", xmToggleButtonWidgetClass, settings_general_tab_content, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, history_length_text, XmNtopOffset, 10, XmNleftAttachment, XmATTACH_FORM, NULL);
        XtAddCallback(disable_history_limit_toggle, XmNvalueChangedCallback, settings_disable_history_limit_toggle_cb, NULL);
        enter_sends_message_toggle = XtVaCreateManagedWidget("Enter key sends message (unchecked) / inserts newline (checked)", xmToggleButtonWidgetClass, settings_general_tab_content, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, disable_history_limit_toggle, XmNtopOffset, 10, XmNleftAttachment, XmATTACH_FORM, NULL);

        settings_gemini_tab_content = XtVaCreateWidget("geminiTab", xmFormWidgetClass, content_frame, XmNmarginWidth, 10, XmNmarginHeight, 10, XmNtopOffset, 10, XmNleftOffset, 10, XmNrightOffset, 10, XmNbottomOffset, 10, NULL);
        Widget gemini_api_label = XtVaCreateManagedWidget("API Key:", xmLabelWidgetClass, settings_gemini_tab_content, XmNtopAttachment, XmATTACH_FORM, XmNleftAttachment, XmATTACH_FORM, XmNalignment, XmALIGNMENT_BEGINNING, NULL);
        gemini_api_key_text = XtVaCreateManagedWidget("geminiApiKeyText", xmTextFieldWidgetClass, settings_gemini_tab_content, XmNcolumns, 40, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, gemini_api_label, XmNtopOffset, 2, XmNleftAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM, NULL);
        XtAddCallback(gemini_api_key_text, XmNfocusCallback, settings_text_field_focus_in_cb, (XtPointer)DEFAULT_GEMINI_KEY_PLACEHOLDER);
        XtAddCallback(gemini_api_key_text, XmNlosingFocusCallback, settings_text_field_focus_out_cb, (XtPointer)DEFAULT_GEMINI_KEY_PLACEHOLDER);
        XtAddEventHandler(gemini_api_key_text, KeyPressMask, False, app_text_key_press_handler, NULL);
        XtAddEventHandler(gemini_api_key_text, ButtonPressMask, False, popup_handler, NULL);
        Widget gemini_model_label = XtVaCreateManagedWidget("Model ID:", xmLabelWidgetClass, settings_gemini_tab_content, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, gemini_api_key_text, XmNtopOffset, 5, XmNleftAttachment, XmATTACH_FORM, XmNalignment, XmALIGNMENT_BEGINNING, NULL);
        gemini_model_text = XtVaCreateManagedWidget("geminiModelText", xmTextFieldWidgetClass, settings_gemini_tab_content, XmNcolumns, 40, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, gemini_model_label, XmNtopOffset, 2, XmNleftAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM, NULL);
        XtAddCallback(gemini_model_text, XmNfocusCallback, settings_text_field_focus_in_cb, (XtPointer)DEFAULT_GEMINI_MODEL);
        XtAddCallback(gemini_model_text, XmNlosingFocusCallback, settings_text_field_focus_out_cb, (XtPointer)DEFAULT_GEMINI_MODEL);
        XtAddEventHandler(gemini_model_text, KeyPressMask, False, app_text_key_press_handler, NULL);
        XtAddEventHandler(gemini_model_text, ButtonPressMask, False, popup_handler, NULL);
        Widget gemini_get_models_btn = XtVaCreateManagedWidget("Get Models", xmPushButtonWidgetClass, settings_gemini_tab_content, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, gemini_model_text, XmNtopOffset, 5, XmNleftAttachment, XmATTACH_FORM, NULL);
        XtAddCallback(gemini_get_models_btn, XmNactivateCallback, settings_get_models_callback, (XtPointer)0);
        Widget gemini_use_model_btn = XtVaCreateManagedWidget("Use Selected", xmPushButtonWidgetClass, settings_gemini_tab_content, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, gemini_model_text, XmNtopOffset, 5, XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, gemini_get_models_btn, XmNleftOffset, 10, NULL);
        XtAddCallback(gemini_use_model_btn, XmNactivateCallback, settings_use_selected_model_callback, (XtPointer)0);
        gemini_model_list = XmCreateScrolledList(settings_gemini_tab_content, "geminiModelList", NULL, 0);
        XtVaSetValues(XtParent(gemini_model_list), XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, gemini_get_models_btn, XmNtopOffset,5, XmNleftAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM,NULL);
        XtManageChild(gemini_model_list);

        settings_openai_tab_content = XtVaCreateWidget("openaiTab", xmFormWidgetClass, content_frame, XmNmarginWidth, 10, XmNmarginHeight, 10, XmNtopOffset, 10, XmNleftOffset, 10, XmNrightOffset, 10, XmNbottomOffset, 10, NULL);
        Widget openai_api_label = XtVaCreateManagedWidget("API Key:", xmLabelWidgetClass, settings_openai_tab_content, XmNtopAttachment, XmATTACH_FORM, XmNleftAttachment, XmATTACH_FORM, XmNalignment, XmALIGNMENT_BEGINNING, NULL);
        openai_api_key_text = XtVaCreateManagedWidget("openaiApiKeyText", xmTextFieldWidgetClass, settings_openai_tab_content, XmNcolumns, 40, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, openai_api_label, XmNtopOffset, 2, XmNleftAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM, NULL);
        XtAddCallback(openai_api_key_text, XmNfocusCallback, settings_text_field_focus_in_cb, (XtPointer)DEFAULT_OPENAI_KEY_PLACEHOLDER);
        XtAddCallback(openai_api_key_text, XmNlosingFocusCallback, settings_text_field_focus_out_cb, (XtPointer)DEFAULT_OPENAI_KEY_PLACEHOLDER);
        XtAddEventHandler(openai_api_key_text, KeyPressMask, False, app_text_key_press_handler, NULL);
        XtAddEventHandler(openai_api_key_text, ButtonPressMask, False, popup_handler, NULL);
        Widget openai_base_url_label = XtVaCreateManagedWidget("API Base URL (optional):", xmLabelWidgetClass, settings_openai_tab_content, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, openai_api_key_text, XmNtopOffset, 5, XmNleftAttachment, XmATTACH_FORM, XmNalignment, XmALIGNMENT_BEGINNING, NULL);
        openai_base_url_text = XtVaCreateManagedWidget("openaiBaseUrlText", xmTextFieldWidgetClass, settings_openai_tab_content, XmNcolumns, 40, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, openai_base_url_label, XmNtopOffset, 2, XmNleftAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM, NULL);
        XtAddCallback(openai_base_url_text, XmNfocusCallback, openai_base_url_focus_in_cb, NULL);
        XtAddCallback(openai_base_url_text, XmNlosingFocusCallback, openai_base_url_focus_out_cb, NULL);
        XtAddEventHandler(openai_base_url_text, KeyPressMask, False, app_text_key_press_handler, NULL);
        XtAddEventHandler(openai_base_url_text, ButtonPressMask, False, popup_handler, NULL);
        Widget openai_model_label = XtVaCreateManagedWidget("Model ID:", xmLabelWidgetClass, settings_openai_tab_content, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, openai_base_url_text, XmNtopOffset, 5, XmNleftAttachment, XmATTACH_FORM, XmNalignment, XmALIGNMENT_BEGINNING, NULL);
        openai_model_text = XtVaCreateManagedWidget("openaiModelText", xmTextFieldWidgetClass, settings_openai_tab_content, XmNcolumns, 40, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, openai_model_label, XmNtopOffset, 2, XmNleftAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM, NULL);
        XtAddCallback(openai_model_text, XmNfocusCallback, settings_text_field_focus_in_cb, (XtPointer)DEFAULT_OPENAI_MODEL);
        XtAddCallback(openai_model_text, XmNlosingFocusCallback, settings_text_field_focus_out_cb, (XtPointer)DEFAULT_OPENAI_MODEL);
        XtAddEventHandler(openai_model_text, KeyPressMask, False, app_text_key_press_handler, NULL);
        XtAddEventHandler(openai_model_text, ButtonPressMask, False, popup_handler, NULL);
        Widget openai_get_models_btn = XtVaCreateManagedWidget("Get Models", xmPushButtonWidgetClass, settings_openai_tab_content, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, openai_model_text, XmNtopOffset, 5, XmNleftAttachment, XmATTACH_FORM, NULL);
        XtAddCallback(openai_get_models_btn, XmNactivateCallback, settings_get_models_callback, (XtPointer)1);
        Widget openai_use_model_btn = XtVaCreateManagedWidget("Use Selected", xmPushButtonWidgetClass, settings_openai_tab_content, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, openai_model_text, XmNtopOffset, 5, XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, openai_get_models_btn, XmNleftOffset, 10, NULL);
        XtAddCallback(openai_use_model_btn, XmNactivateCallback, settings_use_selected_model_callback, (XtPointer)1);
        openai_model_list = XmCreateScrolledList(settings_openai_tab_content, "openaiModelList", NULL, 0);
        XtVaSetValues(XtParent(openai_model_list), XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, openai_get_models_btn, XmNtopOffset,5, XmNleftAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM,NULL);
        XtManageChild(openai_model_list);

        Widget button_rc_bottom = XtVaCreateManagedWidget("buttonRcBottom", xmRowColumnWidgetClass, dialog_form,
                                                   XmNorientation, XmHORIZONTAL, XmNpacking, XmPACK_TIGHT,
                                                   XmNentryAlignment, XmALIGNMENT_CENTER, XmNspacing, 10,
                                                   XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, 5,
                                                   XmNrightAttachment, XmATTACH_FORM, XmNrightOffset, 5,
                                                   NULL);
        Widget ok_button = XtVaCreateManagedWidget("OK", xmPushButtonWidgetClass, button_rc_bottom, NULL);
        Widget cancel_button = XtVaCreateManagedWidget("Cancel", xmPushButtonWidgetClass, button_rc_bottom, NULL);
        Widget apply_button = XtVaCreateManagedWidget("Apply", xmPushButtonWidgetClass, button_rc_bottom, NULL);
        XtAddCallback(ok_button, XmNactivateCallback, settings_ok_callback, NULL);
        XtAddCallback(cancel_button, XmNactivateCallback, settings_cancel_callback, NULL);
        XtAddCallback(apply_button, XmNactivateCallback, settings_apply_callback, NULL);
        XtVaSetValues(dialog_form, XmNdefaultButton, ok_button, NULL);

        XmToggleButtonSetState(general_tab_btn, True, True);
    }
    populate_settings_dialog();
    XtManageChild(settings_shell);
    XtPopup(settings_shell, XtGrabNone);
}

int main(int argc, char **argv) {
    XtAppContext app_context;
    Widget main_window, menu_bar, main_form;
    Widget chat_area_paned, input_form, bottom_buttons_form;
    Widget file_menu, file_cascade, quit_button_widget, clear_chat_button, settings_button, file_sep_exit;
    Widget edit_menu, edit_cascade;
    Widget cut_button, copy_button, paste_button, select_all_button, edit_sep;
    XmString acc_text_ctrl_q, acc_text_ctrl_o, acc_text_ctrl_x, acc_text_ctrl_c, acc_text_ctrl_v, acc_text_ctrl_a;

    // Set locale for UTF-8 handling *before* XtAppInitialize
    if (setlocale(LC_ALL, "") == NULL) {
        fprintf(stderr, "Warning: Could not set locale.\n");
    }


    if (ensure_config_dir_exists() != 0) {
        fprintf(stderr, "Warning: Could not create/access config directory. Settings may not persist.\n");
    }
    load_settings();

    current_assistant_response_capacity = 1024;
    current_assistant_response_buffer = malloc(current_assistant_response_capacity);
    if (!current_assistant_response_buffer) { perror("malloc assistant_buffer"); return 1; }
    current_assistant_response_buffer[0] = '\0';

    if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK) { fprintf(stderr, "Fatal: curl_global_init failed.\n"); return 1; }
    initialize_dp_context();

    if (pipe(pipe_fds) == -1) {
        perror("Fatal: pipe failed");
        if(dp_ctx) dp_destroy_context(dp_ctx); curl_global_cleanup(); return 1;
    }
    if (fcntl(pipe_fds[0], F_SETFL, O_NONBLOCK) == -1) {
        perror("Fatal: fcntl failed"); close(pipe_fds[0]); close(pipe_fds[1]);
        if(dp_ctx) dp_destroy_context(dp_ctx); curl_global_cleanup(); return 1;
    }

    app_shell = XtAppInitialize(&app_context, "MotifGPT", NULL, 0, &argc, argv, NULL, NULL, 0);
    XtAddCallback(app_shell, XmNdestroyCallback, quit_callback, NULL);

    Display *dpy = XtDisplay(app_shell);
    Colormap cmap = DefaultColormap(dpy, DefaultScreen(dpy));
    XColor xcolor_grey_val, xcolor_exact_grey;
    if (XAllocNamedColor(dpy, cmap, "grey70", &xcolor_grey_val, &xcolor_exact_grey)) {
        grey_fg_color = xcolor_grey_val.pixel;
    } else {
        XColor screen_def_grey;
        screen_def_grey.red = screen_def_grey.green = screen_def_grey.blue = (unsigned short)(0.7 * 65535);
        if (XAllocColor(dpy, cmap, &screen_def_grey)) {
            grey_fg_color = screen_def_grey.pixel;
        } else {
             grey_fg_color = WhitePixel(dpy, DefaultScreen(dpy)) / 2 + BlackPixel(dpy, DefaultScreen(dpy)) / 2 ;
        }
    }
    Widget temp_tf = XmCreateTextField(app_shell, "tempTf", NULL, 0);
    XtVaGetValues(temp_tf, XmNforeground, &normal_fg_color, NULL);
    XtDestroyWidget(temp_tf);


    main_window = XmCreateMainWindow(app_shell, "mainWindow", NULL, 0); XtManageChild(main_window);
    menu_bar = XmCreateMenuBar(main_window, "menuBar", NULL, 0); XtManageChild(menu_bar);

    file_menu = XmCreatePulldownMenu(menu_bar, "fileMenu", NULL, 0);
    file_cascade = XtVaCreateManagedWidget("File", xmCascadeButtonWidgetClass, menu_bar, XmNsubMenuId, file_menu, XmNmnemonic, XK_F, NULL);
    acc_text_ctrl_o = XmStringCreateLocalized("Ctrl+O");
    settings_button = XtVaCreateManagedWidget("Settings...", xmPushButtonWidgetClass, file_menu, XmNmnemonic, XK_S, XmNaccelerator, "Ctrl<Key>o", XmNacceleratorText, acc_text_ctrl_o, NULL);
    XmStringFree(acc_text_ctrl_o); XtAddCallback(settings_button, XmNactivateCallback, settings_callback, NULL);
    clear_chat_button = XtVaCreateManagedWidget("Clear Chat", xmPushButtonWidgetClass, file_menu, XmNmnemonic, XK_C, NULL);
    XtAddCallback(clear_chat_button, XmNactivateCallback, clear_chat_callback, NULL);
    file_sep_exit = XtVaCreateManagedWidget("fileSeparatorExit", xmSeparatorWidgetClass, file_menu, NULL);
    acc_text_ctrl_q = XmStringCreateLocalized("Ctrl+Q");
    quit_button_widget = XtVaCreateManagedWidget("Exit", xmPushButtonWidgetClass, file_menu, XmNmnemonic, XK_x, XmNaccelerator, "Ctrl<Key>q", XmNacceleratorText, acc_text_ctrl_q, NULL);
    XmStringFree(acc_text_ctrl_q); XtAddCallback(quit_button_widget, XmNactivateCallback, quit_callback, NULL);

    edit_menu = XmCreatePulldownMenu(menu_bar, "editMenu", NULL, 0);
    edit_cascade = XtVaCreateManagedWidget("Edit", xmCascadeButtonWidgetClass, menu_bar, XmNsubMenuId, edit_menu, XmNmnemonic, XK_E, NULL);
    acc_text_ctrl_x = XmStringCreateLocalized("Ctrl+X");
    cut_button = XtVaCreateManagedWidget("Cut", xmPushButtonWidgetClass, edit_menu, XmNmnemonic, XK_t, XmNaccelerator, "Ctrl<Key>x", XmNacceleratorText, acc_text_ctrl_x, NULL);
    XmStringFree(acc_text_ctrl_x); XtAddCallback(cut_button, XmNactivateCallback, cut_callback, NULL);
    acc_text_ctrl_c = XmStringCreateLocalized("Ctrl+C");
    copy_button = XtVaCreateManagedWidget("Copy", xmPushButtonWidgetClass, edit_menu, XmNmnemonic, XK_C, XmNaccelerator, "Ctrl<Key>c", XmNacceleratorText, acc_text_ctrl_c, NULL);
    XmStringFree(acc_text_ctrl_c); XtAddCallback(copy_button, XmNactivateCallback, copy_callback, NULL);
    acc_text_ctrl_v = XmStringCreateLocalized("Ctrl+V");
    paste_button = XtVaCreateManagedWidget("Paste", xmPushButtonWidgetClass, edit_menu, XmNmnemonic, XK_P, XmNaccelerator, "Ctrl<Key>v", XmNacceleratorText, acc_text_ctrl_v, NULL);
    XmStringFree(acc_text_ctrl_v); XtAddCallback(paste_button, XmNactivateCallback, paste_callback, NULL);
    edit_sep = XtVaCreateManagedWidget("editSeparator", xmSeparatorWidgetClass, edit_menu, NULL);
    acc_text_ctrl_a = XmStringCreateLocalized("Ctrl+A");
    select_all_button = XtVaCreateManagedWidget("Select All", xmPushButtonWidgetClass, edit_menu, XmNmnemonic, XK_A, XmNaccelerator, "Ctrl<Key>a", XmNacceleratorText, acc_text_ctrl_a, NULL);
    XmStringFree(acc_text_ctrl_a); XtAddCallback(select_all_button, XmNactivateCallback, select_all_callback, NULL);

    main_form = XtVaCreateWidget("mainForm", xmFormWidgetClass, main_window, XmNwidth, 600, XmNheight, 450, NULL); XtManageChild(main_form);
    chat_area_paned = XtVaCreateManagedWidget("chatAreaPaned", xmPanedWindowWidgetClass, main_form, XmNtopAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM, XmNleftAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM, XmNsashWidth, 1, XmNsashHeight, 1, NULL);

    Arg sw_args[5]; int sw_ac = 0;
    XtSetArg(sw_args[sw_ac], XmNpaneMinimum, 100); sw_ac++;
    XtSetArg(sw_args[sw_ac], XmNpaneMaximum, 1000); sw_ac++; // Allow it to grow large
    XtSetArg(sw_args[sw_ac], XmNscrollingPolicy, XmAUTOMATIC); sw_ac++;
    Widget scrolled_conv_win = XmCreateScrolledWindow(chat_area_paned, "scrolledConvWin", sw_args, sw_ac);
    XtManageChild(scrolled_conv_win);

    conversation_text = XmCreateText(scrolled_conv_win, "conversationText", NULL, 0);
    XtVaSetValues(conversation_text, XmNeditMode, XmMULTI_LINE_EDIT, XmNeditable, False, XmNcursorPositionVisible, False, XmNwordWrap, True, XmNscrollHorizontal, False, XmNrows, 15, XmNbackground, WhitePixelOfScreen(XtScreen(conversation_text)), XmNresizeWidth, False, NULL);
    XtManageChild(conversation_text);
    XmScrolledWindowSetAreas(scrolled_conv_win, NULL, NULL, conversation_text);
    XtAddCallback(conversation_text, XmNfocusCallback, focus_callback, NULL);
    XtAddEventHandler(conversation_text, ButtonPressMask, False, popup_handler, NULL);
    XtAddEventHandler(conversation_text, KeyPressMask, False, app_text_key_press_handler, NULL);


    input_form = XtVaCreateWidget("inputForm", xmFormWidgetClass, chat_area_paned,
                                   XmNpaneMinimum, 120, // Ensure minimum height for input area
                                   XmNpaneMaximum, 250,
                                   XmNallowResize, True, // Allow this pane to be resized
                                   XmNfractionBase, 10, NULL);
    XtManageChild(input_form);

    bottom_buttons_form = XtVaCreateManagedWidget("bottomButtonsForm", xmFormWidgetClass, input_form, XmNbottomAttachment, XmATTACH_FORM, XmNleftAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM, XmNheight, 35, NULL);
    attach_image_button = XtVaCreateManagedWidget("Attach Image...", xmPushButtonWidgetClass, bottom_buttons_form, XmNleftAttachment, XmATTACH_FORM, XmNleftOffset, 5, XmNtopAttachment, XmATTACH_FORM, XmNtopOffset, 2, XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, 2, NULL);
    XtAddCallback(attach_image_button, XmNactivateCallback, attach_image_callback, NULL);
    send_button = XtVaCreateManagedWidget("Send", xmPushButtonWidgetClass, bottom_buttons_form, XmNrightAttachment, XmATTACH_FORM, XmNrightOffset, 5, XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, attach_image_button, XmNleftOffset, 5, XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, 2, XmNtopAttachment, XmATTACH_FORM, XmNtopOffset, 2, XmNdefaultButtonShadowThickness, 1, NULL);
    XtAddCallback(send_button, XmNactivateCallback, send_message_callback, NULL);
    XtVaSetValues(input_form, XmNdefaultButton, send_button, NULL);

    // Reset sw_ac for the next ScrolledWindow
    sw_ac = 0;
    XtSetArg(sw_args[sw_ac], XmNscrollingPolicy, XmAUTOMATIC); sw_ac++;
    Widget scrolled_input_win = XmCreateScrolledWindow(input_form, "scrolledInputWin", sw_args, sw_ac);
    XtVaSetValues(scrolled_input_win, XmNtopAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_WIDGET, XmNbottomWidget, bottom_buttons_form, XmNleftAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM, NULL);
    XtManageChild(scrolled_input_win);

    input_text = XmCreateText(scrolled_input_win, "inputText", NULL, 0);
    XtVaSetValues(input_text, XmNeditMode, XmMULTI_LINE_EDIT, XmNrows, 3, XmNwordWrap, True, XmNbackground, WhitePixelOfScreen(XtScreen(input_text)), XmNresizeWidth, False, NULL);
    XtManageChild(input_text);
    XmScrolledWindowSetAreas(scrolled_input_win, NULL, NULL, input_text);
    XtAddEventHandler(input_text, KeyPressMask, False, input_text_key_press_handler, NULL);
    XtAddEventHandler(input_text, KeyPressMask, True, app_text_key_press_handler, NULL); // Pass True so both can see Ctrl events
    XtAddCallback(input_text, XmNfocusCallback, focus_callback, NULL);
    XtAddEventHandler(input_text, ButtonPressMask, False, popup_handler, NULL);

    popup_menu = create_text_popup_menu(main_window);

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
    if (pipe_fds[0] != -1) close(pipe_fds[0]); if (pipe_fds[1] != -1) close(pipe_fds[1]);
    if (settings_shell) XtDestroyWidget(settings_shell);
    return 0;
}

