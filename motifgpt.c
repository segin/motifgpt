#include <Xm/Xm.h>
#include <Xm/MainW.h>
#include <Xm/RowColumn.h>
#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/Text.h>
#include <Xm/ScrolledW.h>
#include <Xm/PanedW.h>
#include <Xm/CascadeB.h>
#include <Xm/Separator.h>
#include <Xm/MessageB.h>
#include <Xm/CutPaste.h>
#include <X11/keysym.h> // For XK_Return, XK_KP_Enter, XK_F, etc.
#include <X11/Xlib.h>   // For KeySym, XLookupString, XKeyEvent

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "disasterparty.h"
#include <curl/curl.h>

// --- Configuration ---
#define CHATBOT_PROVIDER DP_PROVIDER_GOOGLE_GEMINI
#define CHATBOT_MODEL "gemini-2.0-flash"
#define USER_NICKNAME "User"
#define ASSISTANT_NICKNAME "Assistant"
#define MAX_HISTORY_MESSAGES 50 // Max messages to keep in history (user + assistant turns)
// --- End Configuration ---

// Globals
Widget app_shell;
Widget conversation_text;
Widget input_text;
Widget send_button;
Widget focused_text_widget = NULL;

dp_context_t *dp_ctx = NULL;
char *api_key = NULL;

int pipe_fds[2];
bool assistant_is_replying = false;
char current_assistant_prefix[64];
bool prefix_already_added_for_current_reply = false;

// Chat History
dp_message_t *chat_history = NULL;
int chat_history_count = 0;
int chat_history_capacity = 0;
char *current_assistant_response_buffer = NULL; // To accumulate full assistant response for history
size_t current_assistant_response_len = 0;
size_t current_assistant_response_capacity = 0;


// Function Prototypes
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

// Edit menu callbacks
void cut_callback(Widget w, XtPointer client_data, XtPointer call_data);
void copy_callback(Widget w, XtPointer client_data, XtPointer call_data);
void paste_callback(Widget w, XtPointer client_data, XtPointer call_data);
void select_all_callback(Widget w, XtPointer client_data, XtPointer call_data);


void append_to_conversation(const char* text) {
    if (!conversation_text || !XtIsManaged(conversation_text)) return;
    XmTextPosition current_pos = XmTextGetLastPosition(conversation_text);
    XmTextInsert(conversation_text, current_pos, (char*)text);
    current_pos = XmTextGetLastPosition(conversation_text);
    XmTextShowPosition(conversation_text, current_pos);
}

// Helper to add to current_assistant_response_buffer
void append_to_assistant_buffer(const char* text) {
    if (!text) return;
    size_t text_len = strlen(text);
    if (current_assistant_response_len + text_len + 1 > current_assistant_response_capacity) {
        current_assistant_response_capacity = (current_assistant_response_len + text_len + 1) * 2;
        char *new_buf = realloc(current_assistant_response_buffer, current_assistant_response_capacity);
        if (!new_buf) {
            perror("Failed to realloc assistant response buffer");
            free(current_assistant_response_buffer);
            current_assistant_response_buffer = NULL;
            current_assistant_response_len = 0;
            current_assistant_response_capacity = 0;
            return;
        }
        current_assistant_response_buffer = new_buf;
    }
    memcpy(current_assistant_response_buffer + current_assistant_response_len, text, text_len);
    current_assistant_response_len += text_len;
    current_assistant_response_buffer[current_assistant_response_len] = '\0';
}


int stream_handler(const char* token, void* user_data, bool is_final, const char* error_during_stream) {
    if (error_during_stream) {
        char error_buf[512];
        snprintf(error_buf, sizeof(error_buf), "Stream Error: %s\n", error_during_stream);
        if (write(pipe_fds[1], error_buf, strlen(error_buf)) == -1) {
            perror("stream_handler: write error to pipe");
        }
        assistant_is_replying = false;
        prefix_already_added_for_current_reply = false;
        if (current_assistant_response_buffer) current_assistant_response_buffer[0] = '\0';
        current_assistant_response_len = 0;
        return 1;
    }

    if (!assistant_is_replying) {
        assistant_is_replying = true;
        if (current_assistant_response_buffer) current_assistant_response_buffer[0] = '\0';
        current_assistant_response_len = 0;
    }

    if (token) {
        append_to_assistant_buffer(token);
        if (write(pipe_fds[1], token, strlen(token)) == -1) {
            perror("stream_handler: write token to pipe");
            assistant_is_replying = false;
            prefix_already_added_for_current_reply = false;
            if (current_assistant_response_buffer) current_assistant_response_buffer[0] = '\0';
            current_assistant_response_len = 0;
            return 1;
        }
    }

    if (is_final) {
        const char* end_marker = "\n[END_OF_STREAM]\n";
        if (write(pipe_fds[1], end_marker, strlen(end_marker)) == -1) {
            perror("stream_handler: write end_marker to pipe");
        }
        assistant_is_replying = false;
        prefix_already_added_for_current_reply = false;
    }
    return 0;
}

void handle_pipe_input(XtPointer client_data, int *source, XtInputId *id) {
    char buffer[256];
    ssize_t nbytes = read(pipe_fds[0], buffer, sizeof(buffer) - 1);

    if (nbytes > 0) {
        buffer[nbytes] = '\0';
        if (strstr(buffer, "[END_OF_STREAM]")) {
            char *actual_content = strtok(buffer, "\n");
             if (actual_content && strcmp(actual_content, "[END_OF_STREAM]") != 0) {
                 append_to_conversation(actual_content);
             }
            append_to_conversation("\n");
            if (current_assistant_response_buffer && current_assistant_response_len > 0) {
                add_message_to_history(DP_ROLE_ASSISTANT, current_assistant_response_buffer);
            }
            if (current_assistant_response_buffer) current_assistant_response_buffer[0] = '\0';
            current_assistant_response_len = 0;

        } else if (strncmp(buffer, "Stream Error:", 13) == 0) {
            show_error_dialog(buffer);
            append_to_conversation(buffer);
        } else {
            if (assistant_is_replying && !prefix_already_added_for_current_reply) {
                 append_to_conversation(current_assistant_prefix);
                 prefix_already_added_for_current_reply = true;
            }
            append_to_conversation(buffer);
        }
    } else if (nbytes == 0) {
        fprintf(stderr, "handle_pipe_input: EOF on pipe.\n");
        XtRemoveInput(*id);
    } else {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("handle_pipe_input: read from pipe");
            XtRemoveInput(*id);
        }
    }
}

void add_message_to_history(dp_message_role_t role, const char* text_content) {
    if (chat_history_count >= MAX_HISTORY_MESSAGES) {
        fprintf(stderr, "Chat history full. Not adding new message.\n");
        return;
    }

    if (chat_history_count >= chat_history_capacity) {
        chat_history_capacity = (chat_history_capacity == 0) ? 10 : chat_history_capacity * 2;
        if (chat_history_capacity > MAX_HISTORY_MESSAGES) chat_history_capacity = MAX_HISTORY_MESSAGES;

        dp_message_t *new_history = realloc(chat_history, chat_history_capacity * sizeof(dp_message_t));
        if (!new_history) {
            perror("Failed to realloc chat history");
            return;
        }
        chat_history = new_history;
    }

    dp_message_t *new_msg = &chat_history[chat_history_count];
    new_msg->role = role;
    new_msg->num_parts = 0;
    new_msg->parts = NULL;

    if (!dp_message_add_text_part(new_msg, text_content)) {
        fprintf(stderr, "Failed to add text part to history message.\n");
    } else {
        chat_history_count++;
    }
}

void free_chat_history() {
    if (chat_history) {
        dp_free_messages(chat_history, chat_history_count);
        free(chat_history);
        chat_history = NULL;
    }
    chat_history_count = 0;
    chat_history_capacity = 0;
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
        show_error_dialog("Disaster Party context not initialized.");
        XtFree(input_string_raw); return;
    }

    dp_request_config_t request_config = {0};
    request_config.model = CHATBOT_MODEL;
    request_config.temperature = 0.7;
    request_config.max_tokens = 1024;
    request_config.stream = true;
    request_config.messages = chat_history;
    request_config.num_messages = chat_history_count;

    XtFree(input_string_raw);


    snprintf(current_assistant_prefix, sizeof(current_assistant_prefix), "%s: ", ASSISTANT_NICKNAME);
    assistant_is_replying = false;
    prefix_already_added_for_current_reply = false;

    dp_response_t response_status = {0};
    int ret = dp_perform_streaming_completion(dp_ctx, &request_config, stream_handler, NULL, &response_status);

    if (ret != 0) {
        char error_buf[1024];
        snprintf(error_buf, sizeof(error_buf), "LLM Request Failed (Setup) (HTTP %ld): %s",
                 response_status.http_status_code,
                 response_status.error_message ? response_status.error_message : "Unknown Disaster Party error.");
        show_error_dialog(error_buf);
        append_to_conversation(error_buf);
    }
    dp_free_response_content(&response_status);
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
    assistant_is_replying = false;
    prefix_already_added_for_current_reply = false;
    if (current_assistant_response_buffer) current_assistant_response_buffer[0] = '\0';
    current_assistant_response_len = 0;
}


void show_error_dialog(const char* message) {
    Widget dialog = XmCreateErrorDialog(app_shell, "errorDialog", NULL, 0);
    XmString xm_msg = XmStringCreateLocalized((char*)message);
    XtVaSetValues(dialog, XmNmessageString, xm_msg, NULL);
    XmStringFree(xm_msg);
    XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_CANCEL_BUTTON));
    XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_HELP_BUTTON));
    XtManageChild(dialog);
}

static void handle_input_key_press(Widget w, XtPointer client_data, XEvent *event, Boolean *continue_to_dispatch) {
    if (event->type == KeyPress) {
        XKeyEvent *key_event = (XKeyEvent *)event;
        KeySym keysym;
        char buffer[1]; // XLookupString needs a buffer, though we only use keysym here
        // Use XLookupString to get the KeySym, respecting modifiers for Shift, etc.
        XLookupString(key_event, buffer, 1, &keysym, NULL);


        if (keysym == XK_Return || keysym == XK_KP_Enter) { // XK_KP_Enter for numpad Enter
            if (key_event->state & ShiftMask) { // Check if Shift key is pressed
                // Insert newline character into the text widget
                XmTextInsert(input_text, XmTextGetCursorPosition(input_text), "\n");
                *continue_to_dispatch = False; // Event handled, don't process further
            } else { // Enter only (no Shift)
                // Trigger the send message callback
                send_message_callback(send_button, NULL, NULL);
                *continue_to_dispatch = False; // Event handled
            }
        }
    }
}

static void focus_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    // The call_data for XmNfocusCallback is XmAnyCallbackStruct.
    XmAnyCallbackStruct *focus_data = (XmAnyCallbackStruct *)call_data;
    if (focus_data->reason == XmCR_FOCUS) { // Check the reason for the callback
        focused_text_widget = w;
    }
}

void cut_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    if (focused_text_widget && XtIsManaged(focused_text_widget) && XmIsText(focused_text_widget)) {
        Time timestamp = XtLastTimestampProcessed(XtDisplay(focused_text_widget));
        XmTextCut(focused_text_widget, timestamp);
    }
}

void copy_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    if (focused_text_widget && XtIsManaged(focused_text_widget) && XmIsText(focused_text_widget)) {
        Time timestamp = XtLastTimestampProcessed(XtDisplay(focused_text_widget));
        XmTextCopy(focused_text_widget, timestamp);
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


int main(int argc, char **argv) {
    XtAppContext app_context;
    Widget main_window, menu_bar, main_form;
    Widget chat_area_paned, input_form, bottom_buttons_form;
    Widget file_menu, file_cascade, quit_button_widget, clear_chat_button, file_sep_exit;
    Widget edit_menu, edit_cascade;
    Widget cut_button, copy_button, paste_button, select_all_button, edit_sep;
    XmString acc_text_ctrl_q, acc_text_ctrl_x, acc_text_ctrl_c, acc_text_ctrl_v, acc_text_ctrl_a;

    current_assistant_response_capacity = 1024;
    current_assistant_response_buffer = malloc(current_assistant_response_capacity);
    if (!current_assistant_response_buffer) {
        perror("Failed to allocate initial assistant response buffer"); return 1;
    }
    current_assistant_response_buffer[0] = '\0';


    if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK) {
        fprintf(stderr, "Fatal: Failed to initialize libcurl.\n"); return 1;
    }
    const char* provider_env_key = (CHATBOT_PROVIDER == DP_PROVIDER_GOOGLE_GEMINI) ? "GEMINI_API_KEY" : "OPENAI_API_KEY";
    api_key = getenv(provider_env_key);
    if (!api_key) {
        fprintf(stderr, "Fatal Error: %s environment variable not set.\n", provider_env_key);
        curl_global_cleanup(); return 1;
    }
    dp_ctx = dp_init_context(CHATBOT_PROVIDER, api_key, NULL);
    if (!dp_ctx) {
        fprintf(stderr, "Fatal: Failed to initialize Disaster Party context.\n");
        curl_global_cleanup(); return 1;
    }
    printf("Disaster Party initialized. Lib version: %s\n", dp_get_version());

    if (pipe(pipe_fds) == -1) {
        perror("Fatal: pipe creation failed");
        dp_destroy_context(dp_ctx); curl_global_cleanup(); return 1;
    }
    if (fcntl(pipe_fds[0], F_SETFL, O_NONBLOCK) == -1) {
        perror("Fatal: fcntl O_NONBLOCK failed");
        close(pipe_fds[0]); close(pipe_fds[1]);
        dp_destroy_context(dp_ctx); curl_global_cleanup(); return 1;
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

