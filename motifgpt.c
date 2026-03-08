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
#include <stdint.h>
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
#include <time.h>
#include <ctype.h>
#include <stdint.h>

#include "disasterparty.h"
#include <curl/curl.h>
#include "utils.h"
#include "motifgpt_config.h"
#include "motifgpt_history.h"
#include "motifgpt_chat.h"
#include "buffer_utils.h"

// --- Configuration ---
#define DEFAULT_PROVIDER DP_PROVIDER_GOOGLE_GEMINI
#define DEFAULT_GEMINI_MODEL "gemini-2.0-flash"
#define DEFAULT_OPENAI_MODEL "gpt-4.1-nano"
#define DEFAULT_OPENAI_BASE_URL "https://api.openai.com/v1"
#define DEFAULT_ANTHROPIC_MODEL "claude-3-haiku-20240307"
#define DEFAULT_GEMINI_KEY_PLACEHOLDER "AIkeygoesherexxx..."
#define DEFAULT_OPENAI_KEY_PLACEHOLDER "sk-yourkeygoesherexxxx..."
#define DEFAULT_ANTHROPIC_KEY_PLACEHOLDER "sk-ant-yourkeygoesherexxxx..."
#define USER_NICKNAME "User"
#define ASSISTANT_NICKNAME "Assistant"
#define DEFAULT_MAX_HISTORY_MESSAGES 100
#define INTERNAL_MAX_HISTORY_CAPACITY 10000
#define CONFIG_DIR_MODE 0755
#define CONFIG_FILE_NAME "settings.conf"
#define CACHE_DIR_NAME "cache"

#define KEY_PROVIDER "provider"
#define KEY_GEMINI_API_KEY "gemini_api_key"
#define KEY_GEMINI_MODEL "gemini_model"
#define KEY_OPENAI_API_KEY "openai_api_key"
#define KEY_OPENAI_MODEL "openai_model"
#define KEY_OPENAI_BASE_URL "openai_base_url"
#define KEY_ANTHROPIC_API_KEY "anthropic_api_key"
#define KEY_ANTHROPIC_MODEL "anthropic_model"
#define KEY_MAX_HISTORY "max_history"
#define KEY_SYSTEM_PROMPT "system_prompt"
#define KEY_HISTORY_LIMITS_DISABLED "history_limits_disabled"
#define KEY_ENTER_SENDS_MESSAGE "enter_sends_message"
#define KEY_APPEND_DEFAULT_SYSTEM_PROMPT "append_default_system_prompt"

#define VAL_PROVIDER_GEMINI "gemini"
#define VAL_PROVIDER_OPENAI "openai"
#define VAL_PROVIDER_ANTHROPIC "anthropic"
#define VAL_TRUE "true"
#define VAL_FALSE "false"

// UI Spacing
#define UI_SPACING_SMALL 2
#define UI_SPACING_MEDIUM 5
#define UI_SPACING_LARGE 10
#define UI_SPACING_XLARGE 15

// Buffer sizes
#define API_KEY_BUF_SIZE 256
#define MODEL_ID_BUF_SIZE 128
#define API_URL_BUF_SIZE 256
#define SYSTEM_PROMPT_BUF_SIZE 2048
#define THREAD_SYSTEM_PROMPT_BUF_SIZE (SYSTEM_PROMPT_BUF_SIZE * 2)
#define DISPLAY_MSG_BUF_SIZE (2048 + PATH_MAX)
#define DEFAULT_MAX_TOKENS 2048

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
Widget settings_general_tab_content, settings_gemini_tab_content, settings_openai_tab_content, settings_anthropic_tab_content;
Widget settings_current_tab_content = NULL;
Widget provider_gemini_rb, provider_openai_rb, provider_anthropic_rb;
Widget gemini_api_key_text, gemini_model_text, gemini_model_list;
Widget openai_api_key_text, openai_model_text, openai_base_url_text, openai_model_list;
Widget anthropic_api_key_text, anthropic_model_text, anthropic_model_list;
Widget history_length_text;
Widget disable_history_limit_toggle;
Widget enter_sends_message_toggle;
Widget system_prompt_text;
Widget append_prompt_toggle;

dp_provider_type_t current_api_provider = DEFAULT_PROVIDER;
char current_gemini_api_key[API_KEY_BUF_SIZE] = "";
char current_gemini_model[MODEL_ID_BUF_SIZE] = DEFAULT_GEMINI_MODEL;
char current_openai_api_key[API_KEY_BUF_SIZE] = "";
char current_openai_model[MODEL_ID_BUF_SIZE] = DEFAULT_OPENAI_MODEL;
char current_openai_base_url[API_URL_BUF_SIZE] = "";
char current_anthropic_api_key[API_KEY_BUF_SIZE] = "";
char current_anthropic_model[MODEL_ID_BUF_SIZE] = DEFAULT_ANTHROPIC_MODEL;
Boolean enter_key_sends_message = True;
char current_system_prompt[SYSTEM_PROMPT_BUF_SIZE] = "";
Boolean append_default_system_prompt = True;

char attached_image_path[PATH_MAX] = "";
char attached_image_mime_type[64] = "";
char *attached_image_base64_data = NULL;

dp_context_t *dp_ctx = NULL;
char current_assistant_prefix[64];

Pixel normal_fg_color, grey_fg_color;

typedef enum {
    PIPE_MSG_TOKEN, PIPE_MSG_STREAM_END, PIPE_MSG_ERROR,
    PIPE_MSG_MODEL_LIST_ITEM, PIPE_MSG_MODEL_LIST_END, PIPE_MSG_MODEL_LIST_ERROR
} pipe_message_type_t;
typedef struct { pipe_message_type_t type; char data[512]; } pipe_message_t;
typedef struct { dp_request_config_t config; char system_prompt_buffer[THREAD_SYSTEM_PROMPT_BUF_SIZE]; char temp_history_filename[PATH_MAX]; } llm_thread_data_t;
typedef struct { dp_provider_type_t provider; char api_key_for_list[API_KEY_BUF_SIZE]; char base_url_for_list[API_URL_BUF_SIZE]; } get_models_thread_data_t;

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
static int ends_with_ignore_case(const char *str, const char *suffix);
void clear_chat_callback(Widget, XtPointer, XtPointer);
void *perform_llm_request_thread(void*);
void initialize_dp_context();
void load_settings();
void save_settings();
void attach_image_callback(Widget, XtPointer, XtPointer);
void file_selection_ok_callback(Widget, XtPointer, XtPointer);
void open_chat_callback(Widget, XtPointer, XtPointer);
void save_chat_as_callback(Widget, XtPointer, XtPointer);
void file_selection_open_ok_callback(Widget, XtPointer, XtPointer);
void file_selection_save_as_ok_callback(Widget, XtPointer, XtPointer);
void render_all_history();
void append_to_conversation(const char* text);
void append_to_conversation_ex(const char* text, Boolean scroll);
unsigned char* read_file_to_buffer(const char*, size_t*);
char* base64_encode(const unsigned char*, size_t);
static void popup_handler(Widget, XtPointer, XEvent*, Boolean*);
Widget create_text_popup_menu(Widget);
static void numeric_verify_cb(Widget, XtPointer, XtPointer);

void cut_callback(Widget, XtPointer, XtPointer); void copy_callback(Widget, XtPointer, XtPointer);
void paste_callback(Widget, XtPointer, XtPointer); void select_all_callback(Widget, XtPointer, XtPointer);
void settings_callback(Widget, XtPointer, XtPointer); void settings_tab_change_callback(Widget, XtPointer, XtPointer);
Boolean apply_settings_safe();
void settings_apply_callback(Widget, XtPointer, XtPointer); void settings_ok_callback(Widget, XtPointer, XtPointer);
void settings_cancel_callback(Widget, XtPointer, XtPointer); void settings_get_models_callback(Widget, XtPointer, XtPointer);
void *perform_get_models_thread(void*); void settings_use_selected_model_callback(Widget, XtPointer, XtPointer);
void populate_settings_dialog(); void retrieve_settings_from_dialog();
Widget create_provider_settings_tab(Widget parent, const char *prefix,
                                    Widget *api_key_text_w, const char *api_key_placeholder,
                                    Widget *model_id_text_w, const char *model_id_placeholder,
                                    Widget *model_list_w,
                                    Widget *base_url_text_w, const char *base_url_placeholder,
                                    int tab_index);
void create_general_tab(Widget parent);
void settings_disable_history_limit_toggle_cb(Widget, XtPointer, XtPointer);
static void openai_base_url_focus_in_cb(Widget, XtPointer, XtPointer);
static void openai_base_url_focus_out_cb(Widget, XtPointer, XtPointer);
void setup_ui(void);


void append_to_conversation_ex(const char* text, Boolean scroll) {
    if (!conversation_text || !XtIsManaged(conversation_text)) return;
    XmTextPosition pos = XmTextGetLastPosition(conversation_text);
    XmTextInsert(conversation_text, pos, (char*)text);
    if (scroll) {
        XmTextShowPosition(conversation_text, XmTextGetLastPosition(conversation_text));
    }
}

void append_to_conversation(const char* text) {
    append_to_conversation_ex(text, True);
}

void handle_pipe_input(XtPointer client_data, int *source, XtInputId *id) {
    pipe_message_t msg;
    char batch_buffer[8192];
    size_t batch_len = 0;
    const size_t BATCH_CAPACITY = sizeof(batch_buffer) - 1; // Leave room for null terminator

    batch_buffer[0] = '\0';

    // Safety break to prevent infinite loop if pipe is flooded faster than we can read
    int max_reads = 1000;

    while (max_reads-- > 0) {
        ssize_t nbytes = read(pipe_fds[0], &msg, sizeof(pipe_message_t));

        if (nbytes == sizeof(pipe_message_t)) {
             if (msg.type == PIPE_MSG_TOKEN) {
                 if (assistant_is_replying && !prefix_already_added_for_current_reply) {
                     size_t prefix_len = strlen(current_assistant_prefix);
                     if (batch_len + prefix_len > BATCH_CAPACITY) {
                         if (batch_len > 0) {
                             append_to_conversation(batch_buffer);
                             batch_buffer[0] = '\0';
                             batch_len = 0;
                         }
                     }
                     if (batch_len + prefix_len <= BATCH_CAPACITY) {
                         strcpy(batch_buffer + batch_len, current_assistant_prefix);
                         batch_len += prefix_len;
                     } else {
                         append_to_conversation(current_assistant_prefix);
                     }
                     prefix_already_added_for_current_reply = true;
                 }

                 size_t token_len = strlen(msg.data);
                 if (batch_len + token_len > BATCH_CAPACITY) {
                     append_to_conversation(batch_buffer);
                     batch_buffer[0] = '\0';
                     batch_len = 0;
                 }

                 if (token_len > BATCH_CAPACITY) {
                     append_to_conversation(msg.data);
                 } else {
                     strcpy(batch_buffer + batch_len, msg.data);
                     batch_len += token_len;
                 }
             } else {
                 if (batch_len > 0) {
                     append_to_conversation(batch_buffer);
                     batch_buffer[0] = '\0';
                     batch_len = 0;
                 }

                 switch (msg.type) {
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
                            Widget list_to_update = NULL;
                            if (settings_current_tab_content == settings_gemini_tab_content) list_to_update = gemini_model_list;
                            else if (settings_current_tab_content == settings_openai_tab_content) list_to_update = openai_model_list;
                            else if (settings_current_tab_content == settings_anthropic_tab_content) list_to_update = anthropic_model_list;

                            if (list_to_update) {
                                XmString item = XmStringCreateLocalized(msg.data);
                                XmListAddItemUnselected(list_to_update, item, 0); XmStringFree(item);
                            }
                        }
                        break;
                     case PIPE_MSG_MODEL_LIST_END: printf("Model listing complete.\n"); break;
                     case PIPE_MSG_MODEL_LIST_ERROR: show_error_dialog(msg.data); break;
                     default: break;
                 }
             }
        } else if (nbytes == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else {
                perror("handle_pipe_input: read");
                 XtRemoveInput(*id);
                 break;
            }
        } else if (nbytes == 0) {
             fprintf(stderr, "handle_pipe_input: EOF on pipe.\n");
             XtRemoveInput(*id);
             break;
        } else {
             fprintf(stderr, "handle_pipe_input: Partial read from pipe (%ld bytes).\n", nbytes);
             break;
        }
    }

    if (batch_len > 0) {
        append_to_conversation(batch_buffer);
    }
}

void *perform_llm_request_thread(void *arg) {
    llm_thread_data_t *thread_data = (llm_thread_data_t *)arg;
    dp_response_t response_status = {0};

    // Deserialize chat history from temp file
    if (strlen(thread_data->temp_history_filename) > 0) {
        dp_message_t *loaded_messages = NULL;
        size_t num_loaded = 0;
        if (dp_deserialize_messages_from_file(thread_data->temp_history_filename, &loaded_messages, &num_loaded) == 0) {
            thread_data->config.messages = loaded_messages;
            thread_data->config.num_messages = num_loaded;
            unlink(thread_data->temp_history_filename); // Remove file immediately after loading
        } else {
             write_pipe_message(PIPE_MSG_ERROR, "Failed to load chat history for request.");
             unlink(thread_data->temp_history_filename);
             free(thread_data);
             pthread_detach(pthread_self());
             return NULL;
        }
    }

    printf("Thread: LLM request with %d messages.\n", (int)thread_data->config.num_messages);

    // Point the config's system_prompt to the buffer inside the struct
    if (strlen(thread_data->system_prompt_buffer) > 0) {
        thread_data->config.system_prompt = thread_data->system_prompt_buffer;
    } else {
        thread_data->config.system_prompt = NULL;
    }

    pthread_mutex_lock(&dp_mutex);
    int ret = -1;
    if (dp_ctx) {
        ret = dp_perform_streaming_completion(dp_ctx, &thread_data->config, stream_handler, NULL, &response_status);
    }
    pthread_mutex_unlock(&dp_mutex);

    if (ret != 0) {
        char err_buf[1024];
        if (ret == -1 && response_status.http_status_code == 0) {
             snprintf(err_buf, sizeof(err_buf), "LLM Request Failed: Context unavailable.");
        } else {
             snprintf(err_buf, sizeof(err_buf), "LLM Request Failed (Thread) (HTTP %ld): %s",
                 response_status.http_status_code, response_status.error_message ? response_status.error_message : "DP error in thread.");
        }
        write_pipe_message(PIPE_MSG_ERROR, err_buf);
    }
    dp_free_response_content(&response_status);

    // Free the thread-specific history
    if (thread_data->config.messages) {
        dp_free_messages(thread_data->config.messages, thread_data->config.num_messages);
        free(thread_data->config.messages);
    }

    free(thread_data);
    pthread_detach(pthread_self());
    return NULL;
}

void start_llm_request() {
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

    pthread_mutex_lock(&dp_mutex);
    if (!dp_ctx) {
        show_error_dialog("LLM context not initialized. Please check API Key and Model ID in Settings.");
        if(input_string_raw) XtFree(input_string_raw);
        pthread_mutex_unlock(&dp_mutex);
        return;
    }
    pthread_mutex_unlock(&dp_mutex);

    char display_msg_text_part[1024] = "";
    if (input_string_raw) {
         snprintf(display_msg_text_part, sizeof(display_msg_text_part), "%s: %s", USER_NICKNAME, input_string_raw);
    } else {
         snprintf(display_msg_text_part, sizeof(display_msg_text_part), "%s: ", USER_NICKNAME);
    }
    char full_display_msg[DISPLAY_MSG_BUF_SIZE]; strcpy(full_display_msg, display_msg_text_part);
    if (attached_image_base64_data) {
        char path_copy[PATH_MAX]; strncpy(path_copy, attached_image_path, PATH_MAX); path_copy[PATH_MAX-1] = '\0';
        snprintf(full_display_msg, sizeof(full_display_msg), "%s [Image Attached: %s]\n", display_msg_text_part, basename(path_copy));
    } else {
        snprintf(full_display_msg, sizeof(full_display_msg), "%s\n", display_msg_text_part);
    }
    append_to_conversation(full_display_msg);
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
    thread_data->system_prompt_buffer[0] = '\0';

    if (current_api_provider == DP_PROVIDER_GOOGLE_GEMINI) {
        thread_data->config.model = current_gemini_model;
    } else if (current_api_provider == DP_PROVIDER_OPENAI_COMPATIBLE) {
        thread_data->config.model = current_openai_model;
    } else if (current_api_provider == DP_PROVIDER_ANTHROPIC) {
        thread_data->config.model = current_anthropic_model;
    }

    generate_system_prompt(thread_data->system_prompt_buffer, sizeof(thread_data->system_prompt_buffer), current_system_prompt, append_default_system_prompt);
    // The config.system_prompt pointer will be set inside the thread to point to system_prompt_buffer.
    // This avoids passing a pointer to a stack variable to the new thread.

    thread_data->config.temperature = 0.7; thread_data->config.max_tokens = DEFAULT_MAX_TOKENS;
    thread_data->config.stream = true;

    // Serialize chat history to temp file to avoid race condition.
    // We use file serialization because dp_message_t is opaque and we cannot safely deep-copy it in memory without library headers.
    char temp_filename[PATH_MAX];
    strcpy(temp_filename, "/tmp/motifgpt_hist_XXXXXX");
    int fd = mkstemp(temp_filename);
    if (fd != -1) {
        close(fd);
        if (dp_serialize_messages_to_file(chat_history, chat_history_count, temp_filename) == 0) {
             strncpy(thread_data->temp_history_filename, temp_filename, PATH_MAX - 1);
             thread_data->temp_history_filename[PATH_MAX - 1] = '\0';
             thread_data->config.messages = NULL; // Will be loaded in thread
             thread_data->config.num_messages = 0;
        } else {
             perror("dp_serialize_messages_to_file");
             unlink(temp_filename);
             free(thread_data);
             show_error_dialog("Failed to serialize chat history for thread.");
             return;
        }
    } else {
        perror("mkstemp");
        free(thread_data);
        show_error_dialog("Failed to create temp file for history.");
        return;
    }

    snprintf(current_assistant_prefix, sizeof(current_assistant_prefix), "%s: ", ASSISTANT_NICKNAME);
    pthread_t tid;
    if (pthread_create(&tid, NULL, perform_llm_request_thread, thread_data) != 0) {
        perror("pthread_create llm_request");
        if (strlen(thread_data->temp_history_filename) > 0) unlink(thread_data->temp_history_filename);
        free(thread_data);
        show_error_dialog("Failed to start LLM request thread.");
    }
}

void send_message_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    start_llm_request();
}

void quit_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    printf("Exiting MotifGPT...\n"); save_settings(); free_chat_history();
    if (current_assistant_response_buffer) free(current_assistant_response_buffer);
    pthread_mutex_lock(&dp_mutex);
    if (dp_ctx) dp_destroy_context(dp_ctx);
    pthread_mutex_unlock(&dp_mutex);
    curl_global_cleanup();
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
                 start_llm_request();
                *continue_to_dispatch = False;
            } else { // Enter alone
                if (enter_key_sends_message) { // Check the setting
                    start_llm_request();
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
    // No need to call general focus_callback on losing focus unless specifically required
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

void load_settings() {
    char *settings_file = get_config_path(CONFIG_FILE_NAME);
    if (!settings_file) { fprintf(stderr, "Could not determine settings file path.\n"); return; }
    FILE *fp = fopen(settings_file, "r");
    if (!fp) {
        printf("No settings file (%s). Using defaults/environment variables.\n", settings_file);
        const char* ge = getenv("GEMINI_API_KEY"); if (ge) snprintf(current_gemini_api_key, sizeof(current_gemini_api_key), "%s", ge); else current_gemini_api_key[0] = '\0';
        const char* oe = getenv("OPENAI_API_KEY"); if (oe) snprintf(current_openai_api_key, sizeof(current_openai_api_key), "%s", oe); else current_openai_api_key[0] = '\0';
        current_max_history_messages = DEFAULT_MAX_HISTORY_MESSAGES;
        history_limits_disabled = False;
        enter_key_sends_message = True;
        return;
    }
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        char *key = strtok(line, "="); char *value = strtok(NULL, "\n");
        if (key && value) {
            if (strcmp(key, KEY_PROVIDER) == 0) {
                if (strcmp(value, VAL_PROVIDER_GEMINI) == 0) current_api_provider = DP_PROVIDER_GOOGLE_GEMINI;
                else if (strcmp(value, VAL_PROVIDER_OPENAI) == 0) current_api_provider = DP_PROVIDER_OPENAI_COMPATIBLE;
                else if (strcmp(value, VAL_PROVIDER_ANTHROPIC) == 0) current_api_provider = DP_PROVIDER_ANTHROPIC;
            } else if (strcmp(key, KEY_GEMINI_API_KEY) == 0) snprintf(current_gemini_api_key, sizeof(current_gemini_api_key), "%s", value);
            else if (strcmp(key, KEY_GEMINI_MODEL) == 0) snprintf(current_gemini_model, sizeof(current_gemini_model), "%s", value);
            else if (strcmp(key, KEY_OPENAI_API_KEY) == 0) snprintf(current_openai_api_key, sizeof(current_openai_api_key), "%s", value);
            else if (strcmp(key, KEY_OPENAI_MODEL) == 0) snprintf(current_openai_model, sizeof(current_openai_model), "%s", value);
            else if (strcmp(key, KEY_OPENAI_BASE_URL) == 0) snprintf(current_openai_base_url, sizeof(current_openai_base_url), "%s", value);
            else if (strcmp(key, KEY_ANTHROPIC_API_KEY) == 0) snprintf(current_anthropic_api_key, sizeof(current_anthropic_api_key), "%s", value);
            else if (strcmp(key, KEY_ANTHROPIC_MODEL) == 0) snprintf(current_anthropic_model, sizeof(current_anthropic_model), "%s", value);
            else if (strcmp(key, KEY_MAX_HISTORY) == 0) current_max_history_messages = atoi(value);
            else if (strcmp(key, KEY_SYSTEM_PROMPT) == 0) snprintf(current_system_prompt, sizeof(current_system_prompt), "%s", value);
            else if (strcmp(key, KEY_HISTORY_LIMITS_DISABLED) == 0) history_limits_disabled = (strcmp(value, VAL_TRUE) == 0);
            else if (strcmp(key, KEY_ENTER_SENDS_MESSAGE) == 0) enter_key_sends_message = (strcmp(value, VAL_TRUE) == 0);
            else if (strcmp(key, KEY_APPEND_DEFAULT_SYSTEM_PROMPT) == 0) append_default_system_prompt = (strcmp(value, VAL_TRUE) == 0);
        }
    }
    fclose(fp); printf("Settings loaded from %s\n", settings_file);
}

void save_settings() {
    if (ensure_config_dir_exists() != 0) { fprintf(stderr, "Config dir error. Settings not saved.\n"); return; }
    char *settings_file = get_config_path(CONFIG_FILE_NAME);
    if (!settings_file) { fprintf(stderr, "Settings file path error. Not saved.\n"); return; }
    int fd = open(settings_file, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd == -1) {
        char err_msg[PATH_MAX + 100]; snprintf(err_msg, sizeof(err_msg), "open for writing: %s", settings_file);
        perror(err_msg); return;
    }
    if (fchmod(fd, 0600) == -1) {
        perror("fchmod settings file"); close(fd); return;
    }
    FILE *fp = fdopen(fd, "w");
    if (!fp) {
        perror("fdopen settings file"); close(fd); return;
    }
    const char* provider_str = VAL_PROVIDER_GEMINI;
    if (current_api_provider == DP_PROVIDER_OPENAI_COMPATIBLE) provider_str = VAL_PROVIDER_OPENAI;
    else if (current_api_provider == DP_PROVIDER_ANTHROPIC) provider_str = VAL_PROVIDER_ANTHROPIC;
    fprintf(fp, "%s=%s\n", KEY_PROVIDER, provider_str);

    fprintf(fp, "%s=%s\n", KEY_GEMINI_API_KEY, current_gemini_api_key);
    fprintf(fp, "%s=%s\n", KEY_GEMINI_MODEL, current_gemini_model);
    fprintf(fp, "%s=%s\n", KEY_OPENAI_API_KEY, current_openai_api_key);
    fprintf(fp, "%s=%s\n", KEY_OPENAI_MODEL, current_openai_model);
    fprintf(fp, "%s=%s\n", KEY_OPENAI_BASE_URL, current_openai_base_url);
    fprintf(fp, "%s=%s\n", KEY_ANTHROPIC_API_KEY, current_anthropic_api_key);
    fprintf(fp, "%s=%s\n", KEY_ANTHROPIC_MODEL, current_anthropic_model);

    fprintf(fp, "%s=%d\n", KEY_MAX_HISTORY, current_max_history_messages);
    fprintf(fp, "%s=%s\n", KEY_SYSTEM_PROMPT, current_system_prompt);
    fprintf(fp, "%s=%s\n", KEY_APPEND_DEFAULT_SYSTEM_PROMPT, append_default_system_prompt ? VAL_TRUE : VAL_FALSE);
    fprintf(fp, "%s=%s\n", KEY_HISTORY_LIMITS_DISABLED, history_limits_disabled ? VAL_TRUE : VAL_FALSE);
    fprintf(fp, "%s=%s\n", KEY_ENTER_SENDS_MESSAGE, enter_key_sends_message ? VAL_TRUE : VAL_FALSE);
    fclose(fp); printf("Settings saved to %s\n", settings_file);
}

static int ends_with_ignore_case(const char *str, const char *suffix) {
    if (!str || !suffix) return 0;
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str_len) return 0;

    const char *p_str = str + str_len - suffix_len;
    const char *p_suffix = suffix;

    while (*p_suffix) {
        if (tolower((unsigned char)*p_str) != tolower((unsigned char)*p_suffix)) {
            return 0;
        }
        p_str++;
        p_suffix++;
    }
    return 1;
}

void file_selection_ok_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    XmFileSelectionBoxCallbackStruct *cbs = (XmFileSelectionBoxCallbackStruct *)call_data;
    char *filename = NULL; XmStringGetLtoR(cbs->value, XmFONTLIST_DEFAULT_TAG, &filename);
    if (!filename || strlen(filename) == 0) { XtFree(filename); return; }

    size_t file_size;
    unsigned char *file_buffer = read_file_to_buffer(filename, &file_size);

    if (!file_buffer) {
        show_error_dialog("Could not read image file.");
        XtFree(filename);
        attached_image_path[0] = '\0';
        return;
    }

    const char* mime_type = get_image_mime_type(file_buffer, file_size);
    if (!mime_type) {
        show_error_dialog("Unsupported image type or invalid file content (PNG, JPG, GIF required).");
        free(file_buffer);
        XtFree(filename);
        attached_image_path[0] = '\0';
        return;
    }

    strncpy(attached_image_path, filename, PATH_MAX -1);
    attached_image_path[PATH_MAX-1] = '\0';
    strncpy(attached_image_mime_type, mime_type, sizeof(attached_image_mime_type) - 1);
    attached_image_mime_type[sizeof(attached_image_mime_type) - 1] = '\0';
    XtFree(filename);

    if (attached_image_base64_data) free(attached_image_base64_data);
    attached_image_base64_data = base64_encode(file_buffer, file_size);
    free(file_buffer);

    if (!attached_image_base64_data) {
        show_error_dialog("Could not Base64 encode image.");
        attached_image_path[0] = '\0';
        return;
    }

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

void file_selection_open_ok_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    XmFileSelectionBoxCallbackStruct *cbs = (XmFileSelectionBoxCallbackStruct *)call_data;
    char *filename = NULL;
    XmStringGetLtoR(cbs->value, XmFONTLIST_DEFAULT_TAG, &filename);
    if (!filename || strlen(filename) == 0) { if(filename) XtFree(filename); return; }

    dp_message_t* loaded_messages = NULL;
    size_t num_loaded = 0;
    if (dp_deserialize_messages_from_file(filename, &loaded_messages, &num_loaded) == 0) {
        free_chat_history(); // Clear existing history
        chat_history = loaded_messages;
        chat_history_count = num_loaded;
        chat_history_capacity = num_loaded; // Set capacity to what was loaded
        render_all_history();
        append_to_conversation("\n--- Conversation Loaded ---\n");
    } else {
        show_error_dialog("Failed to load or parse conversation file.");
    }

    if(filename) XtFree(filename);
    XtUnmanageChild(w);
}

void file_selection_save_as_ok_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    XmFileSelectionBoxCallbackStruct *cbs = (XmFileSelectionBoxCallbackStruct *)call_data;
    char *filename = NULL;
    XmStringGetLtoR(cbs->value, XmFONTLIST_DEFAULT_TAG, &filename);
    if (!filename || strlen(filename) == 0) { if(filename) XtFree(filename); return; }

    if (dp_serialize_messages_to_file(chat_history, chat_history_count, filename) == 0) {
        char success_msg[PATH_MAX + 50];
        snprintf(success_msg, sizeof(success_msg), "\n--- Conversation Saved to: %s ---\n", basename(filename));
        append_to_conversation(success_msg);
    } else {
        show_error_dialog("Failed to save conversation to file.");
    }

    if(filename) XtFree(filename);
    XtUnmanageChild(w);
}

void open_chat_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    static Widget file_selector = NULL;
    if (!file_selector) {
        file_selector = XmCreateFileSelectionDialog(app_shell, "openChatSelector", NULL, 0);
        XtAddCallback(file_selector, XmNokCallback, file_selection_open_ok_callback, NULL);
        XtAddCallback(file_selector, XmNcancelCallback, (XtCallbackProc)XtUnmanageChild, NULL);
        XtUnmanageChild(XmFileSelectionBoxGetChild(file_selector, XmDIALOG_HELP_BUTTON));
        XmString title = XmStringCreateLocalized("Open Conversation");
        XtVaSetValues(file_selector, XmNdialogTitle, title, NULL);
        XmStringFree(title);
    }
    XtManageChild(file_selector);
}

void save_chat_as_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    static Widget file_selector = NULL;
    if (!file_selector) {
        file_selector = XmCreateFileSelectionDialog(app_shell, "saveChatSelector", NULL, 0);
        XtAddCallback(file_selector, XmNokCallback, file_selection_save_as_ok_callback, NULL);
        XtAddCallback(file_selector, XmNcancelCallback, (XtCallbackProc)XtUnmanageChild, NULL);
        XtUnmanageChild(XmFileSelectionBoxGetChild(file_selector, XmDIALOG_HELP_BUTTON));
        XmString title = XmStringCreateLocalized("Save Conversation As...");
        XmString save_text = XmStringCreateLocalized("conversation.json");
        XtVaSetValues(file_selector, XmNdialogTitle, title, XmNtextString, save_text, NULL);
        XmStringFree(title);
        XmStringFree(save_text);
    }
    XtManageChild(file_selector);
}


void initialize_dp_context() {
    if (dp_ctx) { dp_destroy_context(dp_ctx); dp_ctx = NULL; }
    const char* key_to_use = NULL;
    const char* model_to_use = NULL;

    if (current_api_provider == DP_PROVIDER_GOOGLE_GEMINI) {
        key_to_use = current_gemini_api_key;
        model_to_use = current_gemini_model;
    } else if (current_api_provider == DP_PROVIDER_OPENAI_COMPATIBLE) {
        key_to_use = current_openai_api_key;
        model_to_use = current_openai_model;
    } else if (current_api_provider == DP_PROVIDER_ANTHROPIC) {
        key_to_use = current_anthropic_api_key;
        model_to_use = current_anthropic_model;
    }

    if (!key_to_use || !model_to_use) {
        fprintf(stderr, "Provider not correctly set, cannot initialize context.\n");
        return;
    }




    const char* base_url_to_use = NULL;

    if (current_api_provider == DP_PROVIDER_OPENAI_COMPATIBLE) {
        if (strlen(current_openai_base_url) > 0 && strcmp(current_openai_base_url, DEFAULT_OPENAI_BASE_URL) != 0) {
            base_url_to_use = current_openai_base_url;
        }
    }

    Boolean key_is_placeholder = False;
    Boolean model_is_placeholder = False;

    if (settings_shell) {
        Pixel fg;
        if (current_api_provider == DP_PROVIDER_GOOGLE_GEMINI) {
            if(gemini_api_key_text && XtIsManaged(gemini_api_key_text)) { // Check if widget is valid
                XtVaGetValues(gemini_api_key_text, XmNforeground, &fg, NULL);
                if (strcmp(key_to_use, DEFAULT_GEMINI_KEY_PLACEHOLDER) == 0 && fg == grey_fg_color) key_is_placeholder = True;
            } else if (strcmp(key_to_use, DEFAULT_GEMINI_KEY_PLACEHOLDER) == 0) key_is_placeholder = True; // Fallback if widget not ready

            if(gemini_model_text && XtIsManaged(gemini_model_text)) {
                XtVaGetValues(gemini_model_text, XmNforeground, &fg, NULL);
                if (strcmp(model_to_use, DEFAULT_GEMINI_MODEL) == 0 && fg == grey_fg_color) model_is_placeholder = True;
            } else if (strcmp(model_to_use, DEFAULT_GEMINI_MODEL) == 0) model_is_placeholder = True;

        } else if (current_api_provider == DP_PROVIDER_OPENAI_COMPATIBLE) {
            if(openai_api_key_text && XtIsManaged(openai_api_key_text)) {
                XtVaGetValues(openai_api_key_text, XmNforeground, &fg, NULL);
                if (strcmp(key_to_use, DEFAULT_OPENAI_KEY_PLACEHOLDER) == 0 && fg == grey_fg_color) key_is_placeholder = True;
            } else if (strcmp(key_to_use, DEFAULT_OPENAI_KEY_PLACEHOLDER) == 0) key_is_placeholder = True;

            if(openai_model_text && XtIsManaged(openai_model_text)) {
                XtVaGetValues(openai_model_text, XmNforeground, &fg, NULL);
                if (strcmp(model_to_use, DEFAULT_OPENAI_MODEL) == 0 && fg == grey_fg_color) model_is_placeholder = True;
            } else if (strcmp(model_to_use, DEFAULT_OPENAI_MODEL) == 0) model_is_placeholder = True;
        }
        else if (current_api_provider == DP_PROVIDER_ANTHROPIC) {
            if(anthropic_api_key_text && XtIsManaged(anthropic_api_key_text)) {
                XtVaGetValues(anthropic_api_key_text, XmNforeground, &fg, NULL);
                if (strcmp(key_to_use, DEFAULT_ANTHROPIC_KEY_PLACEHOLDER) == 0 && fg == grey_fg_color) key_is_placeholder = True;
            } else if (strcmp(key_to_use, DEFAULT_ANTHROPIC_KEY_PLACEHOLDER) == 0) key_is_placeholder = True;

            if(anthropic_model_text && XtIsManaged(anthropic_model_text)) {
                XtVaGetValues(anthropic_model_text, XmNforeground, &fg, NULL);
                if (strcmp(model_to_use, DEFAULT_ANTHROPIC_MODEL) == 0 && fg == grey_fg_color) model_is_placeholder = True;
            } else if (strcmp(model_to_use, DEFAULT_ANTHROPIC_MODEL) == 0) model_is_placeholder = True;
        }
    } else { // Fallback if settings_shell not created yet (initial load)
        if (current_api_provider == DP_PROVIDER_GOOGLE_GEMINI) {
            if (strcmp(key_to_use, DEFAULT_GEMINI_KEY_PLACEHOLDER) == 0) key_is_placeholder = True;
            if (strcmp(model_to_use, DEFAULT_GEMINI_MODEL) == 0) model_is_placeholder = True;
        } else if (current_api_provider == DP_PROVIDER_OPENAI_COMPATIBLE) {
            if (strcmp(key_to_use, DEFAULT_OPENAI_KEY_PLACEHOLDER) == 0) key_is_placeholder = True;
            if (strcmp(model_to_use, DEFAULT_OPENAI_MODEL) == 0) model_is_placeholder = True;
        }
    }


    if (strlen(key_to_use) == 0 || key_is_placeholder) {
        fprintf(stderr, "API Key not set or is placeholder. LLM disabled until configured in Settings.\n"); return;
    }
    if (strlen(model_to_use) == 0 || model_is_placeholder) {
         fprintf(stderr, "Model ID not set or is placeholder. LLM disabled until configured in Settings.\n"); return;
    }

    dp_ctx = dp_init_context(current_api_provider, key_to_use, base_url_to_use);
    if (!dp_ctx) { fprintf(stderr, "Failed to init LLM context with current settings.\n"); }
    else {
        printf("LLM context (re)initialized. Provider: %s, Model: %s, Base URL: %s\n",
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
        case 3: settings_current_tab_content = settings_anthropic_tab_content; break;
        default: return;
    }
    if (settings_current_tab_content) XtManageChild(settings_current_tab_content);
}

static void update_settings_text_field(Widget tf, const char *value, const char *placeholder) {
    if (strlen(value) == 0) {
        XmTextFieldSetString(tf, (char*)placeholder);
        XtVaSetValues(tf, XmNforeground, grey_fg_color, NULL);
    } else {
        XmTextFieldSetString(tf, (char*)value);
        XtVaSetValues(tf, XmNforeground, normal_fg_color, NULL);
    }
}

void populate_settings_dialog() {
    XmToggleButtonSetState(provider_gemini_rb, current_api_provider == DP_PROVIDER_GOOGLE_GEMINI, False);
    XmToggleButtonSetState(provider_openai_rb, current_api_provider == DP_PROVIDER_OPENAI_COMPATIBLE, False);
    XmToggleButtonSetState(provider_anthropic_rb, current_api_provider == DP_PROVIDER_ANTHROPIC, False);

    char hist_len_str[10];
    snprintf(hist_len_str, sizeof(hist_len_str), "%d", current_max_history_messages);
    XmTextFieldSetString(history_length_text, hist_len_str);
    XmToggleButtonSetState(disable_history_limit_toggle, history_limits_disabled, False);
    XmToggleButtonSetState(enter_sends_message_toggle, enter_key_sends_message, False);

    XmTextSetString(system_prompt_text, current_system_prompt);
    XmToggleButtonSetState(append_prompt_toggle, append_default_system_prompt, False);


    XtSetSensitive(history_length_text, !history_limits_disabled);

    // Gemini
    update_settings_text_field(gemini_api_key_text, current_gemini_api_key, DEFAULT_GEMINI_KEY_PLACEHOLDER);
    update_settings_text_field(gemini_model_text, current_gemini_model, DEFAULT_GEMINI_MODEL);
    XmListDeleteAllItems(gemini_model_list);

    // OpenAI
    update_settings_text_field(openai_api_key_text, current_openai_api_key, DEFAULT_OPENAI_KEY_PLACEHOLDER);
    update_settings_text_field(openai_model_text, current_openai_model, DEFAULT_OPENAI_MODEL);
    update_settings_text_field(openai_base_url_text, current_openai_base_url, DEFAULT_OPENAI_BASE_URL);
    XmListDeleteAllItems(openai_model_list);

    // Anthropic
    update_settings_text_field(anthropic_api_key_text, current_anthropic_api_key, DEFAULT_ANTHROPIC_KEY_PLACEHOLDER);
    update_settings_text_field(anthropic_model_text, current_anthropic_model, DEFAULT_ANTHROPIC_MODEL);
    XmListDeleteAllItems(anthropic_model_list);
}

static void retrieve_text_field_value(Widget w, char *buffer, size_t buffer_size, const char *placeholder, Boolean clear_if_placeholder) {
    char *tmp = XmTextFieldGetString(w);
    Pixel fg_color;
    XtVaGetValues(w, XmNforeground, &fg_color, NULL);

    if (clear_if_placeholder && placeholder && strcmp(tmp, placeholder) == 0 && fg_color == grey_fg_color) {
        if (buffer_size > 0) buffer[0] = '\0';
    } else {
        strncpy(buffer, tmp, buffer_size - 1);
        if (buffer_size > 0) buffer[buffer_size - 1] = '\0';
    }
    XtFree(tmp);
}

void retrieve_settings_from_dialog() {
    if (XmToggleButtonGetState(provider_gemini_rb)) current_api_provider = DP_PROVIDER_GOOGLE_GEMINI;
    else if (XmToggleButtonGetState(provider_openai_rb)) current_api_provider = DP_PROVIDER_OPENAI_COMPATIBLE;
    else if (XmToggleButtonGetState(provider_anthropic_rb)) current_api_provider = DP_PROVIDER_ANTHROPIC;

    char *hist_len_str = XmTextFieldGetString(history_length_text);
    current_max_history_messages = atoi(hist_len_str);
    if (current_max_history_messages <= 0 && !history_limits_disabled) current_max_history_messages = DEFAULT_MAX_HISTORY_MESSAGES;
    XtFree(hist_len_str);
    history_limits_disabled = XmToggleButtonGetState(disable_history_limit_toggle);
    enter_key_sends_message = XmToggleButtonGetState(enter_sends_message_toggle);

    retrieve_text_field_value(gemini_api_key_text, current_gemini_api_key, sizeof(current_gemini_api_key), DEFAULT_GEMINI_KEY_PLACEHOLDER, True);
    retrieve_text_field_value(gemini_model_text, current_gemini_model, sizeof(current_gemini_model), DEFAULT_GEMINI_MODEL, False);

    retrieve_text_field_value(openai_api_key_text, current_openai_api_key, sizeof(current_openai_api_key), DEFAULT_OPENAI_KEY_PLACEHOLDER, True);
    retrieve_text_field_value(openai_model_text, current_openai_model, sizeof(current_openai_model), DEFAULT_OPENAI_MODEL, False);
    retrieve_text_field_value(openai_base_url_text, current_openai_base_url, sizeof(current_openai_base_url), DEFAULT_OPENAI_BASE_URL, True);

    retrieve_text_field_value(anthropic_api_key_text, current_anthropic_api_key, sizeof(current_anthropic_api_key), DEFAULT_ANTHROPIC_KEY_PLACEHOLDER, True);
    retrieve_text_field_value(anthropic_model_text, current_anthropic_model, sizeof(current_anthropic_model), DEFAULT_ANTHROPIC_MODEL, False);

    char *tmp = XmTextGetString(system_prompt_text);
    strncpy(current_system_prompt, tmp, sizeof(current_system_prompt)-1); current_system_prompt[sizeof(current_system_prompt)-1]='\0';
    XtFree(tmp);
    append_default_system_prompt = XmToggleButtonGetState(append_prompt_toggle);
}

Boolean apply_settings_safe() {
    if (pthread_mutex_trylock(&dp_mutex) != 0) {
        show_error_dialog("Cannot apply settings while an LLM request is in progress.");
        return False;
    }
    retrieve_settings_from_dialog();
    initialize_dp_context_unsafe();
    save_settings();
    pthread_mutex_unlock(&dp_mutex);
    return True;
}

void settings_apply_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    apply_settings_safe();
}

void settings_ok_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    if (apply_settings_safe()) {
        XtUnmanageChild(settings_shell);
    }
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
        } else { snprintf(err_buf, sizeof(err_buf), "%s", "Error Get Models: dp_list_models failed critically."); }
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
    } else if (tab_type == 1) {
        provider_for_list = DP_PROVIDER_OPENAI_COMPATIBLE; api_key_str = XmTextFieldGetString(openai_api_key_text);
        base_url_str = XmTextFieldGetString(openai_base_url_text);
        XmListDeleteAllItems(openai_model_list);
    } else { // tab_type == 2
        provider_for_list = DP_PROVIDER_ANTHROPIC; api_key_str = XmTextFieldGetString(anthropic_api_key_text);
        XmListDeleteAllItems(anthropic_model_list);
    }

    Pixel key_fg;
    if (tab_type == 0) XtVaGetValues(gemini_api_key_text, XmNforeground, &key_fg, NULL);
    else if (tab_type == 1) XtVaGetValues(openai_api_key_text, XmNforeground, &key_fg, NULL);
    else XtVaGetValues(anthropic_api_key_text, XmNforeground, &key_fg, NULL);

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
    Widget source_list = NULL;
    Widget target_text = NULL;

    if (tab_type == 0) { source_list = gemini_model_list; target_text = gemini_model_text; }
    else if (tab_type == 1) { source_list = openai_model_list; target_text = openai_model_text; }
    else if (tab_type == 2) { source_list = anthropic_model_list; target_text = anthropic_model_text; }

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

void render_all_history() {
    // 1. Calculate required buffer size
    size_t total_len = 0;
    for (int i = 0; i < chat_history_count; i++) {
        const char* nick = (chat_history[i].role == DP_ROLE_USER) ? USER_NICKNAME : ASSISTANT_NICKNAME;
        total_len += strlen(nick) + 2; // "Nick: "

        for (size_t j = 0; j < chat_history[i].num_parts; j++) {
            dp_content_part_t* part = &chat_history[i].parts[j];
            if (part->type == DP_CONTENT_PART_TEXT) {
                if (part->text) total_len += strlen(part->text);
            } else if (part->type == DP_CONTENT_PART_IMAGE_BASE64) {
                total_len += strlen(" [Image Attached]");
            }
        }
        total_len += 1; // "\n"
    }

    // 2. Allocate buffer
    char* full_history = malloc(total_len + 1);
    if (!full_history) {
        perror("malloc render_all_history");
        return;
    }

    // 3. Construct string
    char* current_pos = full_history;
    for (int i = 0; i < chat_history_count; i++) {
        const char* nick = (chat_history[i].role == DP_ROLE_USER) ? USER_NICKNAME : ASSISTANT_NICKNAME;

        size_t nick_len = strlen(nick);
        memcpy(current_pos, nick, nick_len);
        current_pos += nick_len;
        memcpy(current_pos, ": ", 2);
        current_pos += 2;

        for (size_t j = 0; j < chat_history[i].num_parts; j++) {
            dp_content_part_t* part = &chat_history[i].parts[j];
            if (part->type == DP_CONTENT_PART_TEXT && part->text) {
                size_t text_len = strlen(part->text);
                memcpy(current_pos, part->text, text_len);
                current_pos += text_len;
            } else if (part->type == DP_CONTENT_PART_IMAGE_BASE64) {
                 const char* img_msg = " [Image Attached]";
                 size_t img_len = strlen(img_msg);
                 memcpy(current_pos, img_msg, img_len);
                 current_pos += img_len;
            }
        }
        *current_pos = '\n';
        current_pos++;
    }
    *current_pos = '\0';

    // 4. Update UI once
    XmTextSetString(conversation_text, full_history);
    XmTextShowPosition(conversation_text, XmTextGetLastPosition(conversation_text));

    // 5. Cleanup
    free(full_history);
}


void create_general_tab(Widget parent) {
    settings_general_tab_content = XtVaCreateWidget("generalTab", xmFormWidgetClass, parent, XmNmarginWidth, 10, XmNmarginHeight, 10, XmNtopOffset, 10, XmNleftOffset, 10, XmNrightOffset, 10, XmNbottomOffset, 10, NULL);
    Widget provider_label = XtVaCreateManagedWidget("Model API Provider:", xmLabelWidgetClass, settings_general_tab_content, XmNtopAttachment, XmATTACH_FORM, XmNleftAttachment, XmATTACH_FORM, XmNalignment, XmALIGNMENT_BEGINNING, NULL);
    Widget provider_radio_box = XmCreateRadioBox(settings_general_tab_content, "providerRadioBox", NULL, 0);
    XtVaSetValues(provider_radio_box, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, provider_label, XmNleftAttachment, XmATTACH_FORM, XmNorientation, XmHORIZONTAL, NULL);
    provider_gemini_rb = XtVaCreateManagedWidget("Gemini", xmToggleButtonWidgetClass, provider_radio_box, NULL);
    provider_openai_rb = XtVaCreateManagedWidget("OpenAI-compatible", xmToggleButtonWidgetClass, provider_radio_box, NULL);
    provider_anthropic_rb = XtVaCreateManagedWidget("Anthropic", xmToggleButtonWidgetClass, provider_radio_box, NULL);
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

    Widget sys_prompt_label = XtVaCreateManagedWidget("System Prompt:", xmLabelWidgetClass, settings_general_tab_content, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, enter_sends_message_toggle, XmNtopOffset, 15, XmNleftAttachment, XmATTACH_FORM, XmNalignment, XmALIGNMENT_BEGINNING, NULL);
    Widget scrolled_prompt_win = XmCreateScrolledWindow(settings_general_tab_content, "scrolledPromptWin", NULL, 0);
    XtVaSetValues(scrolled_prompt_win, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, sys_prompt_label, XmNleftAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, 30, XmNscrollingPolicy, XmAUTOMATIC, NULL);
    system_prompt_text = XmCreateText(scrolled_prompt_win, "systemPromptText", NULL, 0);
    XtVaSetValues(system_prompt_text, XmNeditMode, XmMULTI_LINE_EDIT, XmNwordWrap, True, XmNscrollHorizontal, False, NULL);
    XtManageChild(system_prompt_text);
    XtManageChild(scrolled_prompt_win);
    XmScrolledWindowSetAreas(scrolled_prompt_win, NULL, NULL, system_prompt_text);
    append_prompt_toggle = XtVaCreateManagedWidget("Append to MotifGPT default system prompt", xmToggleButtonWidgetClass, settings_general_tab_content, XmNbottomAttachment, XmATTACH_FORM, XmNleftAttachment, XmATTACH_FORM, NULL);
}

Widget create_provider_settings_tab(Widget parent, const char *prefix,
                                    Widget *api_key_text_w, const char *api_key_placeholder,
                                    Widget *model_id_text_w, const char *model_id_placeholder,
                                    Widget *model_list_w,
                                    Widget *base_url_text_w, const char *base_url_placeholder,
                                    int tab_index) {
    char name_buffer[256];

    snprintf(name_buffer, sizeof(name_buffer), "%sTab", prefix);
    Widget tab = XtVaCreateWidget(name_buffer, xmFormWidgetClass, parent,
                                  XmNmarginWidth, 10, XmNmarginHeight, 10,
                                  XmNtopOffset, 10, XmNleftOffset, 10, XmNrightOffset, 10, XmNbottomOffset, 10,
                                  NULL);

    Widget api_label = XtVaCreateManagedWidget("API Key:", xmLabelWidgetClass, tab,
                                               XmNtopAttachment, XmATTACH_FORM,
                                               XmNleftAttachment, XmATTACH_FORM,
                                               XmNalignment, XmALIGNMENT_BEGINNING,
                                               NULL);

    snprintf(name_buffer, sizeof(name_buffer), "%sApiKeyText", prefix);
    *api_key_text_w = XtVaCreateManagedWidget(name_buffer, xmTextFieldWidgetClass, tab,
                                              XmNcolumns, 40,
                                              XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, api_label, XmNtopOffset, 2,
                                              XmNleftAttachment, XmATTACH_FORM,
                                              XmNrightAttachment, XmATTACH_FORM,
                                              NULL);
    XtAddCallback(*api_key_text_w, XmNfocusCallback, settings_text_field_focus_in_cb, (XtPointer)api_key_placeholder);
    XtAddCallback(*api_key_text_w, XmNlosingFocusCallback, settings_text_field_focus_out_cb, (XtPointer)api_key_placeholder);
    XtAddEventHandler(*api_key_text_w, KeyPressMask, False, app_text_key_press_handler, NULL);
    XtAddEventHandler(*api_key_text_w, ButtonPressMask, False, popup_handler, NULL);

    Widget last_widget = *api_key_text_w;

    if (base_url_text_w != NULL) {
        Widget base_url_label = XtVaCreateManagedWidget("API Base URL (optional):", xmLabelWidgetClass, tab,
                                                        XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, last_widget, XmNtopOffset, 5,
                                                        XmNleftAttachment, XmATTACH_FORM,
                                                        XmNalignment, XmALIGNMENT_BEGINNING,
                                                        NULL);
        snprintf(name_buffer, sizeof(name_buffer), "%sBaseUrlText", prefix);
        *base_url_text_w = XtVaCreateManagedWidget(name_buffer, xmTextFieldWidgetClass, tab,
                                                   XmNcolumns, 40,
                                                   XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, base_url_label, XmNtopOffset, 2,
                                                   XmNleftAttachment, XmATTACH_FORM,
                                                   XmNrightAttachment, XmATTACH_FORM,
                                                   NULL);
        if (base_url_placeholder) {
             XtAddCallback(*base_url_text_w, XmNfocusCallback, settings_text_field_focus_in_cb, (XtPointer)base_url_placeholder);
             XtAddCallback(*base_url_text_w, XmNlosingFocusCallback, settings_text_field_focus_out_cb, (XtPointer)base_url_placeholder);
        }
        XtAddEventHandler(*base_url_text_w, KeyPressMask, False, app_text_key_press_handler, NULL);
        XtAddEventHandler(*base_url_text_w, ButtonPressMask, False, popup_handler, NULL);
        last_widget = *base_url_text_w;
    }

    Widget model_label = XtVaCreateManagedWidget("Model ID:", xmLabelWidgetClass, tab,
                                                 XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, last_widget, XmNtopOffset, 5,
                                                 XmNleftAttachment, XmATTACH_FORM,
                                                 XmNalignment, XmALIGNMENT_BEGINNING,
                                                 NULL);

    snprintf(name_buffer, sizeof(name_buffer), "%sModelText", prefix);
    *model_id_text_w = XtVaCreateManagedWidget(name_buffer, xmTextFieldWidgetClass, tab,
                                               XmNcolumns, 40,
                                               XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, model_label, XmNtopOffset, 2,
                                               XmNleftAttachment, XmATTACH_FORM,
                                               XmNrightAttachment, XmATTACH_FORM,
                                               NULL);
    XtAddCallback(*model_id_text_w, XmNfocusCallback, settings_text_field_focus_in_cb, (XtPointer)model_id_placeholder);
    XtAddCallback(*model_id_text_w, XmNlosingFocusCallback, settings_text_field_focus_out_cb, (XtPointer)model_id_placeholder);
    XtAddEventHandler(*model_id_text_w, KeyPressMask, False, app_text_key_press_handler, NULL);
    XtAddEventHandler(*model_id_text_w, ButtonPressMask, False, popup_handler, NULL);

    Widget get_models_btn = XtVaCreateManagedWidget("Get Models", xmPushButtonWidgetClass, tab,
                                                    XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, *model_id_text_w, XmNtopOffset, 10,
                                                    XmNleftAttachment, XmATTACH_FORM,
                                                    NULL);
    XtAddCallback(get_models_btn, XmNactivateCallback, settings_get_models_callback, (XtPointer)(long)tab_index);

    Widget use_model_btn = XtVaCreateManagedWidget("Use Selected", xmPushButtonWidgetClass, tab,
                                                   XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, *model_id_text_w, XmNtopOffset, 5,
                                                   XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, get_models_btn, XmNleftOffset, 10,
                                                   NULL);
    XtAddCallback(use_model_btn, XmNactivateCallback, settings_use_selected_model_callback, (XtPointer)(long)tab_index);

    snprintf(name_buffer, sizeof(name_buffer), "%sModelList", prefix);
    *model_list_w = XmCreateScrolledList(tab, name_buffer, NULL, 0);
    XtVaSetValues(XtParent(*model_list_w), XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, get_models_btn, XmNtopOffset, 5, XmNleftAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM, NULL);
    XtManageChild(*model_list_w);

    return tab;
}

void settings_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    if (settings_shell == NULL) {
        settings_shell = XtVaCreatePopupShell("settingsShell", topLevelShellWidgetClass, app_shell, XmNtitle, "MotifGPT Settings", XmNwidth, 550, XmNheight, 600, NULL);
        Widget dialog_form = XtVaCreateManagedWidget("dialogForm", xmFormWidgetClass, settings_shell, XmNverticalSpacing, UI_SPACING_MEDIUM, XmNhorizontalSpacing, UI_SPACING_MEDIUM, NULL);
        Widget tab_button_rc = XtVaCreateManagedWidget("tabButtonRc", xmRowColumnWidgetClass, dialog_form, XmNorientation, XmHORIZONTAL, XmNradioBehavior, True, XmNindicatorType, XmONE_OF_MANY, XmNentryAlignment, XmALIGNMENT_CENTER, XmNpacking, XmPACK_TIGHT, XmNspacing, 0, XmNtopAttachment, XmATTACH_FORM, XmNtopOffset, UI_SPACING_MEDIUM, XmNleftAttachment, XmATTACH_FORM, XmNleftOffset, UI_SPACING_MEDIUM, XmNrightAttachment, XmATTACH_FORM, XmNrightOffset, UI_SPACING_MEDIUM, XmNshadowThickness, 0, NULL);
        Widget general_tab_btn = XtVaCreateManagedWidget("General", xmToggleButtonWidgetClass, tab_button_rc, XmNindicatorOn, False, XmNshadowThickness, 2, NULL);
        Widget gemini_tab_btn = XtVaCreateManagedWidget("Gemini", xmToggleButtonWidgetClass, tab_button_rc, XmNindicatorOn, False, XmNshadowThickness, 2, NULL);
        Widget openai_tab_btn = XtVaCreateManagedWidget("OpenAI", xmToggleButtonWidgetClass, tab_button_rc, XmNindicatorOn, False, XmNshadowThickness, 2, NULL);
        Widget anthropic_tab_btn = XtVaCreateManagedWidget("Anthropic", xmToggleButtonWidgetClass, tab_button_rc, XmNindicatorOn, False, XmNshadowThickness, 2, NULL);
        XtAddCallback(general_tab_btn, XmNvalueChangedCallback, settings_tab_change_callback, (XtPointer)0);
        XtAddCallback(gemini_tab_btn, XmNvalueChangedCallback, settings_tab_change_callback, (XtPointer)1);
        XtAddCallback(openai_tab_btn, XmNvalueChangedCallback, settings_tab_change_callback, (XtPointer)2);
        XtAddCallback(anthropic_tab_btn, XmNvalueChangedCallback, settings_tab_change_callback, (XtPointer)3);
        Widget content_frame = XtVaCreateManagedWidget("contentFrame", xmFrameWidgetClass, dialog_form, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, tab_button_rc, XmNtopOffset, UI_SPACING_MEDIUM, XmNleftAttachment, XmATTACH_FORM, XmNleftOffset, UI_SPACING_MEDIUM, XmNrightAttachment, XmATTACH_FORM, XmNrightOffset, UI_SPACING_MEDIUM, XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, 45, XmNshadowType, XmSHADOW_ETCHED_IN, NULL);

        create_general_tab(content_frame);
        settings_gemini_tab_content = create_provider_settings_tab(content_frame, "gemini", &gemini_api_key_text, DEFAULT_GEMINI_KEY_PLACEHOLDER, &gemini_model_text, DEFAULT_GEMINI_MODEL, &gemini_model_list, NULL, NULL, 0);
        settings_openai_tab_content = create_provider_settings_tab(content_frame, "openai", &openai_api_key_text, DEFAULT_OPENAI_KEY_PLACEHOLDER, &openai_model_text, DEFAULT_OPENAI_MODEL, &openai_model_list, &openai_base_url_text, DEFAULT_OPENAI_BASE_URL, 1);
        settings_anthropic_tab_content = create_provider_settings_tab(content_frame, "anthropic", &anthropic_api_key_text, DEFAULT_ANTHROPIC_KEY_PLACEHOLDER, &anthropic_model_text, DEFAULT_ANTHROPIC_MODEL, &anthropic_model_list, NULL, NULL, 2);

        Widget button_rc_bottom = XtVaCreateManagedWidget("buttonRcBottom", xmRowColumnWidgetClass, dialog_form,
                                                   XmNorientation, XmHORIZONTAL, XmNpacking, XmPACK_TIGHT,
                                                   XmNentryAlignment, XmALIGNMENT_CENTER, XmNspacing, UI_SPACING_LARGE,
                                                   XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, UI_SPACING_MEDIUM,
                                                   XmNrightAttachment, XmATTACH_FORM, XmNrightOffset, UI_SPACING_MEDIUM,
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

void setup_ui(void) {
    Widget main_window, menu_bar, main_form;
    Widget chat_area_paned, input_form, bottom_buttons_form, open_chat_button, save_chat_as_button, file_sep_open;
    Widget file_menu, file_cascade, quit_button_widget, clear_chat_button, settings_button, file_sep_exit;
    Widget edit_menu, edit_cascade;
    Widget cut_button, copy_button, paste_button, select_all_button, edit_sep;
    XmString acc_text_ctrl_q, acc_text_ctrl_o, acc_text_ctrl_x, acc_text_ctrl_c, acc_text_ctrl_v, acc_text_ctrl_a;

    Widget temp_tf = XmCreateTextField(app_shell, "tempTf", NULL, 0);
    XtVaGetValues(temp_tf, XmNforeground, &normal_fg_color, NULL);
    XtDestroyWidget(temp_tf);


    main_window = XmCreateMainWindow(app_shell, "mainWindow", NULL, 0); XtManageChild(main_window);
    menu_bar = XmCreateMenuBar(main_window, "menuBar", NULL, 0); XtManageChild(menu_bar);

    file_menu = XmCreatePulldownMenu(menu_bar, "fileMenu", NULL, 0);
    file_cascade = XtVaCreateManagedWidget("File", xmCascadeButtonWidgetClass, menu_bar, XmNsubMenuId, file_menu, XmNmnemonic, XK_F, NULL);
    open_chat_button = XtVaCreateManagedWidget("Open Conversation...", xmPushButtonWidgetClass, file_menu, XmNmnemonic, XK_O, NULL);
    XtAddCallback(open_chat_button, XmNactivateCallback, open_chat_callback, NULL);
    save_chat_as_button = XtVaCreateManagedWidget("Save Conversation As...", xmPushButtonWidgetClass, file_menu, XmNmnemonic, XK_S, NULL);
    XtAddCallback(save_chat_as_button, XmNactivateCallback, save_chat_as_callback, NULL);
    file_sep_open = XtVaCreateManagedWidget("fileSeparatorOpen", xmSeparatorWidgetClass, file_menu, NULL);
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
    Widget scrolled_conv_win = XmCreateScrolledWindow(chat_area_paned, "scrolledConvWin", NULL, 0);
    XtVaSetValues(scrolled_conv_win, XmNpaneMinimum, 100, XmNpaneMaximum, 1000, XmNscrollingPolicy, XmAUTOMATIC, NULL);
    conversation_text = XmCreateText(scrolled_conv_win, "conversationText", NULL, 0);
    XtVaSetValues(conversation_text, XmNeditMode, XmMULTI_LINE_EDIT, XmNeditable, False, XmNcursorPositionVisible, False, XmNwordWrap, True, XmNscrollHorizontal, False, XmNrows, 15, XmNbackground, WhitePixelOfScreen(XtScreen(conversation_text)), XmNresizeWidth, False, NULL);
    XtManageChild(conversation_text); XtManageChild(scrolled_conv_win);
    XmScrolledWindowSetAreas(scrolled_conv_win, NULL, NULL, conversation_text);
    XtAddCallback(conversation_text, XmNfocusCallback, focus_callback, NULL);
    XtAddEventHandler(conversation_text, ButtonPressMask, False, popup_handler, NULL);
    XtAddEventHandler(conversation_text, KeyPressMask, False, app_text_key_press_handler, NULL);


    input_form = XtVaCreateWidget("inputForm", xmFormWidgetClass, chat_area_paned, XmNpaneMinimum, 120, XmNpaneMaximum, 250, XmNfractionBase, 10, NULL); XtManageChild(input_form);
    bottom_buttons_form = XtVaCreateManagedWidget("bottomButtonsForm", xmFormWidgetClass, input_form, XmNbottomAttachment, XmATTACH_FORM, XmNleftAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM, XmNheight, 35, NULL);
    attach_image_button = XtVaCreateManagedWidget("Attach Image...", xmPushButtonWidgetClass, bottom_buttons_form, XmNleftAttachment, XmATTACH_FORM, XmNleftOffset, 5, XmNtopAttachment, XmATTACH_FORM, XmNtopOffset, 2, XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, 2, NULL);
    XtAddCallback(attach_image_button, XmNactivateCallback, attach_image_callback, NULL);
    send_button = XtVaCreateManagedWidget("Send", xmPushButtonWidgetClass, bottom_buttons_form, XmNrightAttachment, XmATTACH_FORM, XmNrightOffset, 5, XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, attach_image_button, XmNleftOffset, 5, XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, 2, XmNtopAttachment, XmATTACH_FORM, XmNtopOffset, 2, XmNdefaultButtonShadowThickness, 1, NULL);
    XtAddCallback(send_button, XmNactivateCallback, send_message_callback, NULL);
    XtVaSetValues(input_form, XmNdefaultButton, send_button, NULL);

    Widget scrolled_input_win = XmCreateScrolledWindow(input_form, "scrolledInputWin", NULL, 0);
    XtVaSetValues(scrolled_input_win, XmNtopAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_WIDGET, XmNbottomWidget, bottom_buttons_form, XmNleftAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM, XmNscrollingPolicy, XmAUTOMATIC, NULL);
    input_text = XmCreateText(scrolled_input_win, "inputText", NULL, 0);
    XtVaSetValues(input_text, XmNeditMode, XmMULTI_LINE_EDIT, XmNrows, 3, XmNwordWrap, True, XmNbackground, WhitePixelOfScreen(XtScreen(input_text)), XmNresizeWidth, False, NULL);
    XtManageChild(input_text); XtManageChild(scrolled_input_win);
    XmScrolledWindowSetAreas(scrolled_input_win, NULL, NULL, input_text);
    XtAddEventHandler(input_text, KeyPressMask, False, input_text_key_press_handler, NULL);
    XtAddEventHandler(input_text, KeyPressMask, True, app_text_key_press_handler, NULL);
    XtAddCallback(input_text, XmNfocusCallback, focus_callback, NULL);
    XtAddEventHandler(input_text, ButtonPressMask, False, popup_handler, NULL);

    popup_menu = create_text_popup_menu(main_window);

    XmMainWindowSetAreas(main_window, menu_bar, NULL, NULL, NULL, main_form);
}

int main(int argc, char **argv) {
    XtAppContext app_context;

    if (ensure_config_dir_exists() != 0) {
        fprintf(stderr, "Warning: Could not create/access config directory. Settings may not persist.\n");
    }
    load_settings();

    init_assistant_buffer();

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

    setup_ui();

    XtAppAddInput(app_context, pipe_fds[0], (XtPointer)XtInputReadMask, handle_pipe_input, NULL);
    XtRealizeWidget(app_shell);
    focused_text_widget = input_text;
    append_to_conversation("Welcome to MotifGPT! Type message, Shift+Enter for newline, Enter to send.\n");
    XtAppMainLoop(app_context);

    free_assistant_buffer();
    free_chat_history();
    if (dp_ctx) dp_destroy_context(dp_ctx);
    curl_global_cleanup();
    if (pipe_fds[0] != -1) close(pipe_fds[0]); if (pipe_fds[1] != -1) close(pipe_fds[1]);
    if (settings_shell) XtDestroyWidget(settings_shell);
    return 0;
}
