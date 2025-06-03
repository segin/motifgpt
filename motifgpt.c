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
#include <Xm/MessageB.h> // For dialogs

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // For pipe
#include <fcntl.h>  // For fcntl
#include <errno.h>  // For errno

#include "disasterparty.h" // Assuming disasterparty.h is in include path
#include <curl/curl.h>     // For curl_global_init/cleanup

// --- Configuration ---
// Change these to use a different provider or model
#define CHATBOT_PROVIDER DP_PROVIDER_GOOGLE_GEMINI // or DP_PROVIDER_OPENAI_COMPATIBLE
#define CHATBOT_MODEL "gemini-2.0-flash" // e.g., "gemini-2.0-flash" or "gpt-4.1-nano"
#define USER_NICKNAME "User"
#define ASSISTANT_NICKNAME "Assistant"
// --- End Configuration ---

// Globals
Widget app_shell;
Widget conversation_text;
Widget input_text;
Widget send_button;

dp_context_t *dp_ctx = NULL;
char *api_key = NULL;

// For streaming from disasterparty callback to main thread
int pipe_fds[2]; // pipe_fds[0] is read end, pipe_fds[1] is write end
bool assistant_is_replying = false;
char current_assistant_prefix[64];
bool prefix_already_added_for_current_reply = false;


// Placeholder XBM data for icons (simple 32x32 "U" and "A")
#define user_icon_width 32
#define user_icon_height 32
static unsigned char user_icon_bits[] = { // Represents "U"
   0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0xff, 0x07, 0x02, 0x00, 0x00, 0x04,
   0x02, 0x00, 0x00, 0x04, 0x02, 0x00, 0x00, 0x04, 0x02, 0x00, 0x00, 0x04,
   0x02, 0x00, 0x00, 0x04, 0x02, 0x00, 0x00, 0x04, 0x02, 0x00, 0x00, 0x04,
   0xfe, 0xff, 0xff, 0x07, 0x00, 0x00, 0x00, 0x00};

#define assistant_icon_width 32
#define assistant_icon_height 32
static unsigned char assistant_icon_bits[] = { // Represents "A"
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x30, 0x00,
   0x00, 0x3c, 0x78, 0x00, 0x00, 0x66, 0xcc, 0x00, 0x00, 0xc3, 0x86, 0x00,
   0x01, 0xff, 0xfe, 0x01, 0x01, 0xff, 0xfe, 0x01, 0x03, 0x80, 0x00, 0x03,
   0x02, 0x00, 0x00, 0x02, 0x06, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00};


// Function Prototypes
void send_message_callback(Widget w, XtPointer client_data, XtPointer call_data);
int stream_handler(const char* token, void* user_data, bool is_final, const char* error_during_stream);
void handle_pipe_input(XtPointer client_data, int *source, XtInputId *id);
void quit_callback(Widget w, XtPointer client_data, XtPointer call_data);
void show_error_dialog(const char* message);
static void handle_input_key_press(Widget w, XtPointer client_data, XEvent *event, Boolean *continue_to_dispatch);


/**
 * @brief Appends text to the main conversation text widget.
 * @param text The string to append.
 */
void append_to_conversation(const char* text) {
    if (!conversation_text || !XtIsManaged(conversation_text)) return;
    XmTextPosition current_pos = XmTextGetLastPosition(conversation_text);
    XmTextInsert(conversation_text, current_pos, (char*)text);
    current_pos = XmTextGetLastPosition(conversation_text);
    XmTextShowPosition(conversation_text, current_pos);
}

/**
 * @brief Callback for the disasterparty library to handle streamed LLM responses.
 */
int stream_handler(const char* token, void* user_data, bool is_final, const char* error_during_stream) {
    if (error_during_stream) {
        char error_buf[512];
        snprintf(error_buf, sizeof(error_buf), "Stream Error: %s\n", error_during_stream);
        if (write(pipe_fds[1], error_buf, strlen(error_buf)) == -1) {
            perror("stream_handler: write error to pipe");
        }
        assistant_is_replying = false;
        prefix_already_added_for_current_reply = false;
        return 1; // Stop streaming
    }

    if (!assistant_is_replying) {
        assistant_is_replying = true;
        // Prefix will be added by handle_pipe_input on first token
    }

    if (token) {
        if (write(pipe_fds[1], token, strlen(token)) == -1) {
            perror("stream_handler: write token to pipe");
            assistant_is_replying = false;
            prefix_already_added_for_current_reply = false;
            return 1; // Stop streaming
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
    return 0; // Continue streaming
}

/**
 * @brief XtAppAddInput callback to handle data read from the pipe.
 */
void handle_pipe_input(XtPointer client_data, int *source, XtInputId *id) {
    char buffer[256];
    ssize_t nbytes;

    nbytes = read(pipe_fds[0], buffer, sizeof(buffer) - 1);

    if (nbytes > 0) {
        buffer[nbytes] = '\0';

        if (strstr(buffer, "[END_OF_STREAM]")) {
            char *actual_content = strtok(buffer, "\n");
             if (actual_content && strcmp(actual_content, "[END_OF_STREAM]") != 0) {
                 append_to_conversation(actual_content);
             }
            append_to_conversation("\n");
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

/**
 * @brief Callback for the "Send" button.
 */
void send_message_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    char *input_string_raw = XmTextGetString(input_text);
    if (!input_string_raw || strlen(input_string_raw) == 0) {
        XtFree(input_string_raw);
        return;
    }
    if (input_string_raw[strlen(input_string_raw)-1] == '\n') {
        input_string_raw[strlen(input_string_raw)-1] = '\0';
    }
    if (strlen(input_string_raw) == 0) {
        XtFree(input_string_raw);
        return;
    }

    char display_user_msg[1024];
    snprintf(display_user_msg, sizeof(display_user_msg), "%s: %s\n", USER_NICKNAME, input_string_raw);
    append_to_conversation(display_user_msg);
    XmTextSetString(input_text, "");

    if (!dp_ctx) {
        show_error_dialog("Disaster Party context not initialized. Cannot send message.");
        XtFree(input_string_raw);
        return;
    }

    dp_request_config_t request_config = {0};
    request_config.model = CHATBOT_MODEL;
    request_config.temperature = 0.7;
    request_config.max_tokens = 1024;
    request_config.stream = true;

    dp_message_t messages[1];
    request_config.messages = messages;
    request_config.num_messages = 1;

    messages[0].role = DP_ROLE_USER;
    messages[0].num_parts = 0;
    messages[0].parts = NULL;
    if (!dp_message_add_text_part(&messages[0], input_string_raw)) {
        show_error_dialog("Failed to add text part to LLM message structure.");
        XtFree(input_string_raw);
        dp_free_messages(messages, request_config.num_messages);
        return;
    }
    XtFree(input_string_raw);

    snprintf(current_assistant_prefix, sizeof(current_assistant_prefix), "%s: ", ASSISTANT_NICKNAME);
    assistant_is_replying = false; 
    prefix_already_added_for_current_reply = false; // Reset for new reply

    dp_response_t response_status = {0};
    int ret = dp_perform_streaming_completion(dp_ctx, &request_config, stream_handler, NULL, &response_status);

    if (ret != 0) {
        char error_buf[1024];
        if (response_status.error_message) {
            snprintf(error_buf, sizeof(error_buf), "LLM Request Failed (Setup) (HTTP %ld): %s", response_status.http_status_code, response_status.error_message);
        } else {
            snprintf(error_buf, sizeof(error_buf), "LLM Request Failed (Setup) (HTTP %ld): Unknown error from Disaster Party.", response_status.http_status_code);
        }
        show_error_dialog(error_buf);
        append_to_conversation(error_buf);
    }

    dp_free_messages(messages, request_config.num_messages);
    dp_free_response_content(&response_status);
}

/**
 * @brief Callback for the "Exit" menu item or window close.
 */
void quit_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    printf("Exiting MotifGPT...\n");
    if (dp_ctx) {
        dp_destroy_context(dp_ctx);
    }
    curl_global_cleanup();

    if (pipe_fds[0] != -1) close(pipe_fds[0]);
    if (pipe_fds[1] != -1) close(pipe_fds[1]);

    XtDestroyApplicationContext(XtWidgetToApplicationContext(app_shell));
    exit(0);
}

/**
 * @brief Displays an error message in a Motif error dialog.
 */
void show_error_dialog(const char* message) {
    Widget dialog = XmCreateErrorDialog(app_shell, "errorDialog", NULL, 0);
    XmString xm_msg = XmStringCreateLocalized((char*)message);
    XtVaSetValues(dialog, XmNmessageString, xm_msg, NULL);
    XmStringFree(xm_msg);

    XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_CANCEL_BUTTON));
    XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_HELP_BUTTON));

    XtManageChild(dialog);
}

/**
 * @brief Event handler for key presses in the input_text widget.
 * Handles Shift+Enter for newline and Enter for sending the message.
 */
static void handle_input_key_press(Widget w, XtPointer client_data, XEvent *event, Boolean *continue_to_dispatch) {
    if (event->type == KeyPress) {
        XKeyEvent *key_event = (XKeyEvent *)event;
        KeySym keysym;
        char buffer[1];
        XLookupString(key_event, buffer, 1, &keysym, NULL);

        if (keysym == XK_Return) {
            if (key_event->state & ShiftMask) { // Shift + Enter
                // Insert newline
                XmTextInsert(input_text, XmTextGetCursorPosition(input_text), "\n");
                *continue_to_dispatch = False; // We handled it
            } else { // Enter only
                // Trigger send message
                send_message_callback(send_button, NULL, NULL);
                *continue_to_dispatch = False; // We handled it
            }
        }
    }
}


/**
 * @brief Main function for MotifGPT.
 */
int main(int argc, char **argv) {
    XtAppContext app_context;
    Widget main_window, menu_bar, main_form, icon_panel_form;
    Widget chat_area_paned, input_form, bottom_buttons_form;
    Widget file_menu, file_cascade, quit_button_widget;
    Widget user_icon_label, assistant_icon_label;
    Pixmap user_pixmap, assistant_pixmap;
    XmString acc_text_ctrl_q; // For accelerator text

    // 1. Initialize libcurl
    if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK) {
        fprintf(stderr, "Fatal: Failed to initialize libcurl.\n");
        return 1;
    }

    // 2. Initialize Disaster Party LLM library
    const char* provider_env_key = (CHATBOT_PROVIDER == DP_PROVIDER_GOOGLE_GEMINI) ? "GEMINI_API_KEY" : "OPENAI_API_KEY";
    api_key = getenv(provider_env_key);
    if (!api_key) {
        fprintf(stderr, "Fatal Error: %s environment variable not set.\n", provider_env_key);
        fprintf(stderr, "Please set it to your API key for the selected LLM provider.\n");
        curl_global_cleanup();
        return 1;
    }
    dp_ctx = dp_init_context(CHATBOT_PROVIDER, api_key, NULL);
    if (!dp_ctx) {
        fprintf(stderr, "Fatal: Failed to initialize Disaster Party context.\n");
        curl_global_cleanup();
        return 1;
    }
    printf("Disaster Party initialized with %s, model %s. Library version: %s\n",
           (CHATBOT_PROVIDER == DP_PROVIDER_GOOGLE_GEMINI) ? "Gemini" : "OpenAI-compatible",
           CHATBOT_MODEL, dp_get_version());

    // 3. Create pipe
    if (pipe(pipe_fds) == -1) {
        perror("Fatal: pipe creation failed");
        dp_destroy_context(dp_ctx);
        curl_global_cleanup();
        return 1;
    }
    if (fcntl(pipe_fds[0], F_SETFL, O_NONBLOCK) == -1) {
        perror("Fatal: fcntl O_NONBLOCK failed on pipe");
        close(pipe_fds[0]); close(pipe_fds[1]);
        dp_destroy_context(dp_ctx); curl_global_cleanup();
        return 1;
    }

    // 4. Initialize Motif
    app_shell = XtAppInitialize(&app_context, "MotifGPT", NULL, 0, &argc, argv, NULL, NULL, 0);
    XtAddCallback(app_shell, XmNdestroyCallback, quit_callback, NULL);

    main_window = XmCreateMainWindow(app_shell, "mainWindow", NULL, 0);
    XtManageChild(main_window);

    // --- Menu Bar ---
    menu_bar = XmCreateMenuBar(main_window, "menuBar", NULL, 0);
    XtManageChild(menu_bar);

    file_menu = XmCreatePulldownMenu(menu_bar, "fileMenu", NULL, 0);
    file_cascade = XtVaCreateManagedWidget("File", xmCascadeButtonWidgetClass, menu_bar,
                                           XmNsubMenuId, file_menu,
                                           XmNmnemonic, XK_F, // Alt+F
                                           NULL);
    acc_text_ctrl_q = XmStringCreateLocalized("Ctrl+Q");
    quit_button_widget = XtVaCreateManagedWidget("Exit", xmPushButtonWidgetClass, file_menu,
                                           XmNmnemonic, XK_x, // E[x]it
                                           XmNaccelerator, "Ctrl<Key>q",
                                           XmNacceleratorText, acc_text_ctrl_q,
                                           NULL);
    XmStringFree(acc_text_ctrl_q);
    XtAddCallback(quit_button_widget, XmNactivateCallback, quit_callback, NULL);

    // --- Main Layout Form ---
    main_form = XtVaCreateWidget("mainForm", xmFormWidgetClass, main_window,
                                 XmNwidth, 600, XmNheight, 450,
                                 NULL);
    XtManageChild(main_form);

    // --- Icon Panel (Left Side) ---
    icon_panel_form = XtVaCreateManagedWidget("iconPanelForm", xmFormWidgetClass, main_form,
                                              XmNwidth, 80,
                                              XmNtopAttachment, XmATTACH_FORM,
                                              XmNbottomAttachment, XmATTACH_FORM,
                                              XmNleftAttachment, XmATTACH_FORM,
                                              XmNbackground, WhitePixelOfScreen(XtScreen(main_form)),
                                              NULL);

    user_pixmap = XCreatePixmapFromBitmapData(XtDisplay(icon_panel_form),
                                              RootWindowOfScreen(XtScreen(icon_panel_form)),
                                              (char*)user_icon_bits, user_icon_width, user_icon_height,
                                              BlackPixelOfScreen(XtScreen(icon_panel_form)),
                                              WhitePixelOfScreen(XtScreen(icon_panel_form)),
                                              DefaultDepthOfScreen(XtScreen(icon_panel_form)));
    user_icon_label = XtVaCreateManagedWidget("userIcon", xmLabelWidgetClass, icon_panel_form,
                                              XmNlabelType, XmPIXMAP, XmNlabelPixmap, user_pixmap,
                                              XmNtopAttachment, XmATTACH_FORM, XmNtopOffset, 10,
                                              XmNleftAttachment, XmATTACH_FORM, XmNleftOffset, 10,
                                              XmNrightAttachment, XmATTACH_FORM, XmNrightOffset, 10, NULL);

    assistant_pixmap = XCreatePixmapFromBitmapData(XtDisplay(icon_panel_form),
                                             RootWindowOfScreen(XtScreen(icon_panel_form)),
                                             (char*)assistant_icon_bits, assistant_icon_width, assistant_icon_height,
                                             BlackPixelOfScreen(XtScreen(icon_panel_form)),
                                             WhitePixelOfScreen(XtScreen(icon_panel_form)),
                                             DefaultDepthOfScreen(XtScreen(icon_panel_form)));
    assistant_icon_label = XtVaCreateManagedWidget("assistantIcon", xmLabelWidgetClass, icon_panel_form,
                                             XmNlabelType, XmPIXMAP, XmNlabelPixmap, assistant_pixmap,
                                             XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, user_icon_label, XmNtopOffset, 10,
                                             XmNleftAttachment, XmATTACH_FORM, XmNleftOffset, 10,
                                             XmNrightAttachment, XmATTACH_FORM, XmNrightOffset, 10, NULL);

    // --- Chat Area Paned Window ---
    chat_area_paned = XtVaCreateManagedWidget("chatAreaPaned", xmPanedWindowWidgetClass, main_form,
                                              XmNtopAttachment, XmATTACH_FORM,
                                              XmNbottomAttachment, XmATTACH_FORM,
                                              XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, icon_panel_form,
                                              XmNrightAttachment, XmATTACH_FORM,
                                              XmNsashWidth, 1, XmNsashHeight, 1, NULL);

    Widget scrolled_conv_win = XmCreateScrolledWindow(chat_area_paned, "scrolledConvWin", NULL, 0);
    XtVaSetValues(scrolled_conv_win, XmNpaneMinimum, 100, XmNpaneMaximum, 1000, NULL);
    conversation_text = XmCreateText(scrolled_conv_win, "conversationText", NULL, 0);
    XtVaSetValues(conversation_text,
                  XmNeditMode, XmMULTI_LINE_EDIT, XmNeditable, False,
                  XmNcursorPositionVisible, False, XmNwordWrap, True, // Word wrap enabled
                  XmNscrollHorizontal, False, XmNrows, 15,
                  XmNbackground, WhitePixelOfScreen(XtScreen(conversation_text)), NULL);
    XtManageChild(conversation_text);
    XtManageChild(scrolled_conv_win);
    XmScrolledWindowSetAreas(scrolled_conv_win, NULL, NULL, conversation_text);

    // --- Input Area Form ---
    input_form = XtVaCreateWidget("inputForm", xmFormWidgetClass, chat_area_paned,
                                   XmNpaneMinimum, 100, XmNpaneMaximum, 200,
                                   XmNfractionBase, 10, NULL);
    XtManageChild(input_form);

    // --- Bottom Buttons Form (now only Send button) ---
    bottom_buttons_form = XtVaCreateManagedWidget("bottomButtonsForm", xmFormWidgetClass, input_form,
                                                 XmNbottomAttachment, XmATTACH_FORM,
                                                 XmNleftAttachment, XmATTACH_FORM, // Send button will attach to left
                                                 XmNrightAttachment, XmATTACH_FORM,
                                                 NULL);

    send_button = XtVaCreateManagedWidget("Send", xmPushButtonWidgetClass, bottom_buttons_form,
                                          XmNrightAttachment, XmATTACH_FORM, // Align Send to the right
                                          XmNleftAttachment, XmATTACH_FORM,   // Stretch Send button
                                          XmNbottomAttachment, XmATTACH_FORM,
                                          XmNtopAttachment, XmATTACH_FORM,
                                          XmNdefaultButtonShadowThickness, 1, NULL);
    XtAddCallback(send_button, XmNactivateCallback, send_message_callback, NULL);
    // Setting default button for the form to enable Enter key submission
    XtVaSetValues(input_form, XmNdefaultButton, send_button, NULL);


    // --- Input Text Area ---
    Widget scrolled_input_win = XmCreateScrolledWindow(input_form, "scrolledInputWin", NULL, 0);
    XtVaSetValues(scrolled_input_win,
                  XmNtopAttachment, XmATTACH_FORM,
                  XmNbottomAttachment, XmATTACH_WIDGET, XmNbottomWidget, bottom_buttons_form,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_FORM, NULL);

    input_text = XmCreateText(scrolled_input_win, "inputText", NULL, 0);
    XtVaSetValues(input_text,
                  XmNeditMode, XmMULTI_LINE_EDIT, XmNrows, 3, XmNwordWrap, True, // Word wrap enabled
                  XmNbackground, WhitePixelOfScreen(XtScreen(input_text)), // White background
                  NULL);
    XtManageChild(input_text);
    XtManageChild(scrolled_input_win);
    XmScrolledWindowSetAreas(scrolled_input_win, NULL, NULL, input_text);

    // Add event handler for Shift+Enter and Enter in input_text
    XtAddEventHandler(input_text, KeyPressMask, False, handle_input_key_press, NULL);


    // 5. Set MainWindow areas
    XmMainWindowSetAreas(main_window, menu_bar, NULL, NULL, NULL, main_form);

    // 6. Register pipe's read end
    XtAppAddInput(app_context, pipe_fds[0], (XtPointer)XtInputReadMask, handle_pipe_input, NULL);

    // 7. Realize and loop
    XtRealizeWidget(app_shell);
    append_to_conversation("Welcome to MotifGPT! Type your message and press Send or Enter.\n");
    XtAppMainLoop(app_context);

    if (dp_ctx) dp_destroy_context(dp_ctx);
    curl_global_cleanup();
    if (pipe_fds[0] != -1) close(pipe_fds[0]);
    if (pipe_fds[1] != -1) close(pipe_fds[1]);

    return 0;
}

