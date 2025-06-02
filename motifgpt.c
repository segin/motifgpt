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
#define USER_NICKNAME "User"         // Changed from "You"
#define ASSISTANT_NICKNAME "Assistant" // Changed from "Bot"
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
bool assistant_is_replying = false; // Changed from bot_is_replying
char current_assistant_prefix[64]; // Changed from current_bot_prefix

// Placeholder XBM data for icons (simple 32x32 "U" and "A")
// These are monochrome bitmaps.
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


/**
 * @brief Appends text to the main conversation text widget.
 * @param text The string to append.
 */
void append_to_conversation(const char* text) {
    if (!conversation_text || !XtIsManaged(conversation_text)) return;
    XmTextPosition current_pos = XmTextGetLastPosition(conversation_text);
    XmTextInsert(conversation_text, current_pos, (char*)text); // XmTextInsert needs char*
    current_pos = XmTextGetLastPosition(conversation_text);
    XmTextShowPosition(conversation_text, current_pos); // Auto-scroll to the end
}

/**
 * @brief Callback for the disasterparty library to handle streamed LLM responses.
 * This function is called by the disasterparty library (potentially in a different thread).
 * It writes tokens/errors to a pipe to communicate with the Motif main loop.
 *
 * @param token The token received from the LLM.
 * @param user_data User-defined data (not used in this example).
 * @param is_final True if this is the final part of the stream.
 * @param error_during_stream Error message if an error occurred during streaming.
 * @return 0 to continue streaming, non-zero to stop.
 */
int stream_handler(const char* token, void* user_data, bool is_final, const char* error_during_stream) {
    if (error_during_stream) {
        char error_buf[512];
        snprintf(error_buf, sizeof(error_buf), "Stream Error: %s\n", error_during_stream);
        // Write error to the pipe for the main thread to display
        if (write(pipe_fds[1], error_buf, strlen(error_buf)) == -1) {
            perror("stream_handler: write error to pipe");
        }
        assistant_is_replying = false; // Reset assistant replying state
        return 1; // Stop streaming on error
    }

    if (!assistant_is_replying) { // First token for this assistant reply
        assistant_is_replying = true;
        // The assistant's prefix (e.g., "Assistant: ") will be added by handle_pipe_input
        // when it first receives data for this reply.
    }

    if (token) {
        // Write the received token to the pipe
        if (write(pipe_fds[1], token, strlen(token)) == -1) {
            perror("stream_handler: write token to pipe");
            assistant_is_replying = false;
            return 1; // Stop streaming if pipe write fails
        }
    }

    if (is_final) {
        // Write a special marker to indicate the end of the stream
        const char* end_marker = "\n[END_OF_STREAM]\n";
        if (write(pipe_fds[1], end_marker, strlen(end_marker)) == -1) {
            perror("stream_handler: write end_marker to pipe");
        }
        assistant_is_replying = false; // Reset assistant replying state
    }
    return 0; // Continue streaming
}

/**
 * @brief XtAppAddInput callback to handle data read from the pipe.
 * This function is called by the Motif main loop when data is available on the pipe.
 * It reads data sent by stream_handler and updates the conversation_text widget.
 *
 * @param client_data User-defined data (not used).
 * @param source Pointer to the file descriptor of the pipe.
 * @param id Pointer to the XtInputId.
 */
void handle_pipe_input(XtPointer client_data, int *source, XtInputId *id) {
    char buffer[256]; // Buffer to read data from the pipe
    ssize_t nbytes;

    nbytes = read(pipe_fds[0], buffer, sizeof(buffer) - 1);

    if (nbytes > 0) {
        buffer[nbytes] = '\0'; // Null-terminate the read data

        // Check for the end-of-stream marker
        if (strstr(buffer, "[END_OF_STREAM]")) {
            char *actual_content = strtok(buffer, "\n"); // Remove the marker itself if it's part of the buffer
             if (actual_content && strcmp(actual_content, "[END_OF_STREAM]") != 0) {
                 append_to_conversation(actual_content); // Append any remaining content before the marker
             }
            append_to_conversation("\n"); // Ensure a newline after the assistant's full message
            // assistant_is_replying was already set to false in stream_handler
        } else if (strncmp(buffer, "Stream Error:", 13) == 0) {
            // If it's a stream error message from stream_handler
            show_error_dialog(buffer);      // Show error in a dialog
            append_to_conversation(buffer); // Also log error in the chat window
        } else {
            // Regular token from the stream
            if (assistant_is_replying) {
                 char* current_chat_content = XmTextGetString(conversation_text);
                 XmTextPosition last_pos = XmTextGetLastPosition(conversation_text);
                 bool prefix_needed = true;

                 // Check if the prefix was already added for this turn.
                 if (last_pos >= strlen(current_assistant_prefix)) {
                    if (strncmp(current_chat_content + last_pos - strlen(current_assistant_prefix), current_assistant_prefix, strlen(current_assistant_prefix)) == 0) {
                        prefix_needed = false;
                    }
                 }
                 // Also check if the conversation is empty or ends with a newline, implying a new turn
                 if (last_pos == 0 || (last_pos > 0 && current_chat_content[last_pos-1] == '\n')) {
                    prefix_needed = true;
                 }


                 if (prefix_needed) {
                     append_to_conversation(current_assistant_prefix);
                 }
                 XtFree(current_chat_content);
            }
            append_to_conversation(buffer); // Append the token
        }
    } else if (nbytes == 0) { // End-of-file on the pipe (should not happen if write end is kept open)
        fprintf(stderr, "handle_pipe_input: EOF on pipe.\n");
        XtRemoveInput(*id); // Remove the input source
        // Consider re-opening or handling this state if necessary
    } else { // Error reading from pipe
        if (errno != EAGAIN && errno != EWOULDBLOCK) { // Ignore non-blocking "errors"
            perror("handle_pipe_input: read from pipe");
            XtRemoveInput(*id); // Remove on actual error
        }
    }
}

/**
 * @brief Callback for the "Send" button.
 * Gets text from the input_text widget, displays it, and sends it to the LLM
 * via the disasterparty library.
 *
 * @param w The widget that triggered the callback (Send button).
 * @param client_data User-defined data (not used).
 * @param call_data Motif call data (not used).
 */
void send_message_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    char *input_string_raw = XmTextGetString(input_text);
    if (!input_string_raw || strlen(input_string_raw) == 0) {
        XtFree(input_string_raw);
        return;
    }
    // Trim trailing newline if present (often added by multi-line text Enter)
    if (input_string_raw[strlen(input_string_raw)-1] == '\n') {
        input_string_raw[strlen(input_string_raw)-1] = '\0';
    }
    // If after trimming it's empty, do nothing
    if (strlen(input_string_raw) == 0) {
        XtFree(input_string_raw);
        return;
    }


    // Display user's message in the conversation area
    char display_user_msg[1024];
    snprintf(display_user_msg, sizeof(display_user_msg), "%s: %s\n", USER_NICKNAME, input_string_raw);
    append_to_conversation(display_user_msg);
    XmTextSetString(input_text, ""); // Clear the input field

    if (!dp_ctx) {
        show_error_dialog("Disaster Party context not initialized. Cannot send message.");
        XtFree(input_string_raw);
        return;
    }

    // Prepare the request configuration for disasterparty
    dp_request_config_t request_config = {0}; // Initialize to zero
    request_config.model = CHATBOT_MODEL;
    request_config.temperature = 0.7;
    request_config.max_tokens = 1024; // Max tokens for the LLM's response
    request_config.stream = true;     // Enable streaming responses

    // Create the message structure for disasterparty
    dp_message_t messages[1]; // For this simple client, we send one message at a time
    request_config.messages = messages;
    request_config.num_messages = 1;

    messages[0].role = DP_ROLE_USER;
    messages[0].num_parts = 0; // Will be incremented by dp_message_add_text_part
    messages[0].parts = NULL;  // Will be allocated by dp_message_add_text_part
    if (!dp_message_add_text_part(&messages[0], input_string_raw)) {
        show_error_dialog("Failed to add text part to LLM message structure.");
        XtFree(input_string_raw);
        dp_free_messages(messages, request_config.num_messages);
        return;
    }
    XtFree(input_string_raw); // Free the raw string from XmTextGetString

    // Set the prefix for the assistant's upcoming reply
    snprintf(current_assistant_prefix, sizeof(current_assistant_prefix), "%s: ", ASSISTANT_NICKNAME);
    assistant_is_replying = false; // Reset state for the new reply stream

    dp_response_t response_status = {0}; // To capture final status/error from dp_perform_streaming_completion
    int ret = dp_perform_streaming_completion(dp_ctx, &request_config, stream_handler, NULL, &response_status);

    if (ret != 0) { // If setting up the stream failed
        char error_buf[1024];
        if (response_status.error_message) {
            snprintf(error_buf, sizeof(error_buf), "LLM Request Failed (Setup) (HTTP %ld): %s", response_status.http_status_code, response_status.error_message);
        } else {
            snprintf(error_buf, sizeof(error_buf), "LLM Request Failed (Setup) (HTTP %ld): Unknown error from Disaster Party.", response_status.http_status_code);
        }
        show_error_dialog(error_buf);
        append_to_conversation(error_buf); // Log setup error in chat
    }

    dp_free_messages(messages, request_config.num_messages); // Clean up message structures
    dp_free_response_content(&response_status); // Clean up response status structure
}

/**
 * @brief Callback for the "Exit" menu item or window close.
 * Performs cleanup and exits the application.
 *
 * @param w The widget that triggered the callback.
 * @param client_data User-defined data (not used).
 * @param call_data Motif call data (not used).
 */
void quit_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    printf("Exiting MotifGPT...\n"); // Changed name here
    if (dp_ctx) {
        dp_destroy_context(dp_ctx); // Clean up Disaster Party context
    }
    curl_global_cleanup(); // Clean up libcurl

    // Close pipe file descriptors
    if (pipe_fds[0] != -1) close(pipe_fds[0]);
    if (pipe_fds[1] != -1) close(pipe_fds[1]);

    XtDestroyApplicationContext(XtWidgetToApplicationContext(app_shell)); // Clean up Motif app context
    exit(0);
}

/**
 * @brief Displays an error message in a Motif error dialog.
 * @param message The error message string to display.
 */
void show_error_dialog(const char* message) {
    Widget dialog = XmCreateErrorDialog(app_shell, "errorDialog", NULL, 0);
    XmString xm_msg = XmStringCreateLocalized((char*)message); // Create Motif string
    XtVaSetValues(dialog, XmNmessageString, xm_msg, NULL);
    XmStringFree(xm_msg); // Free Motif string

    // Remove Cancel and Help buttons for a simpler dialog
    XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_CANCEL_BUTTON));
    XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_HELP_BUTTON));

    XtManageChild(dialog); // Show the dialog
}

/**
 * @brief Main function for MotifGPT.
 * Initializes Motif, libcurl, Disaster Party, creates the UI, and starts the main loop.
 */
int main(int argc, char **argv) {
    XtAppContext app_context; // Motif application context
    Widget main_window, menu_bar, main_form, icon_panel_form;
    Widget chat_area_paned, input_form, bottom_buttons_form;
    Widget file_menu, file_cascade, quit_button_widget; 
    Widget user_icon_label, assistant_icon_label; // Changed from bot_icon_label
    Widget action_buttons_row_col;
    Pixmap user_pixmap, assistant_pixmap; // Changed from bot_pixmap

    // 1. Initialize libcurl (required by disasterparty)
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


    // 3. Create pipe for communication between disasterparty callback and Motif main loop
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
    app_shell = XtAppInitialize(&app_context, "MotifGPT", NULL, 0, &argc, argv, NULL, NULL, 0); // Changed app class name
    XtAddCallback(app_shell, XmNdestroyCallback, quit_callback, NULL);

    main_window = XmCreateMainWindow(app_shell, "mainWindow", NULL, 0);
    XtManageChild(main_window);

    // --- Menu Bar ---
    menu_bar = XmCreateMenuBar(main_window, "menuBar", NULL, 0);
    XtManageChild(menu_bar);

    file_menu = XmCreatePulldownMenu(menu_bar, "fileMenu", NULL, 0);
    file_cascade = XtVaCreateManagedWidget("File", xmCascadeButtonWidgetClass, menu_bar,
                                           XmNsubMenuId, file_menu, NULL);
    quit_button_widget = XtVaCreateManagedWidget("Exit", xmPushButtonWidgetClass, file_menu, NULL);
    XtAddCallback(quit_button_widget, XmNactivateCallback, quit_callback, NULL);

    // --- Main Layout Form (holds icon panel and chat panel) ---
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
                                              XmNlabelType, XmPIXMAP,
                                              XmNlabelPixmap, user_pixmap,
                                              XmNtopAttachment, XmATTACH_FORM, XmNtopOffset, 10,
                                              XmNleftAttachment, XmATTACH_FORM, XmNleftOffset, 10,
                                              XmNrightAttachment, XmATTACH_FORM, XmNrightOffset, 10,
                                              NULL);

    assistant_pixmap = XCreatePixmapFromBitmapData(XtDisplay(icon_panel_form), // Changed from bot_pixmap
                                             RootWindowOfScreen(XtScreen(icon_panel_form)),
                                             (char*)assistant_icon_bits, assistant_icon_width, assistant_icon_height, // Changed from bot_icon_bits
                                             BlackPixelOfScreen(XtScreen(icon_panel_form)),
                                             WhitePixelOfScreen(XtScreen(icon_panel_form)),
                                             DefaultDepthOfScreen(XtScreen(icon_panel_form)));
    assistant_icon_label = XtVaCreateManagedWidget("assistantIcon", xmLabelWidgetClass, icon_panel_form, // Changed from botIcon
                                             XmNlabelType, XmPIXMAP,
                                             XmNlabelPixmap, assistant_pixmap, // Changed from bot_pixmap
                                             XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, user_icon_label, XmNtopOffset, 10,
                                             XmNleftAttachment, XmATTACH_FORM, XmNleftOffset, 10,
                                             XmNrightAttachment, XmATTACH_FORM, XmNrightOffset, 10,
                                             NULL);


    // --- Chat Area Paned Window (Right Side, for conversation and input) ---
    chat_area_paned = XtVaCreateManagedWidget("chatAreaPaned", xmPanedWindowWidgetClass, main_form,
                                              XmNtopAttachment, XmATTACH_FORM,
                                              XmNbottomAttachment, XmATTACH_FORM,
                                              XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, icon_panel_form,
                                              XmNrightAttachment, XmATTACH_FORM,
                                              XmNsashWidth, 1, XmNsashHeight, 1, 
                                              NULL);

    Widget scrolled_conv_win = XmCreateScrolledWindow(chat_area_paned, "scrolledConvWin", NULL, 0);
    XtVaSetValues(scrolled_conv_win, XmNpaneMinimum, 100, XmNpaneMaximum, 1000, NULL); 
    conversation_text = XmCreateText(scrolled_conv_win, "conversationText", NULL, 0);
    XtVaSetValues(conversation_text,
                  XmNeditMode, XmMULTI_LINE_EDIT, 
                  XmNeditable, False,             
                  XmNcursorPositionVisible, False,
                  XmNwordWrap, True,              
                  XmNscrollHorizontal, False,     
                  XmNrows, 15,                    
                  XmNbackground, WhitePixelOfScreen(XtScreen(conversation_text)), 
                  NULL);
    XtManageChild(conversation_text);
    XtManageChild(scrolled_conv_win);
    XmScrolledWindowSetAreas(scrolled_conv_win, NULL, NULL, conversation_text);


    input_form = XtVaCreateWidget("inputForm", xmFormWidgetClass, chat_area_paned,
                                   XmNpaneMinimum, 100, XmNpaneMaximum, 200, 
                                   XmNfractionBase, 10, 
                                   NULL);
    XtManageChild(input_form);

    bottom_buttons_form = XtVaCreateManagedWidget("bottomButtonsForm", xmFormWidgetClass, input_form,
                                                 XmNbottomAttachment, XmATTACH_FORM, 
                                                 XmNleftAttachment, XmATTACH_FORM,
                                                 XmNrightAttachment, XmATTACH_FORM,
                                                 XmNhorizontalSpacing, 5, 
                                                 NULL);

    action_buttons_row_col = XtVaCreateManagedWidget("actionButtons", xmRowColumnWidgetClass, bottom_buttons_form,
                                                     XmNorientation, XmHORIZONTAL,
                                                     XmNpacking, XmPACK_TIGHT, 
                                                     XmNleftAttachment, XmATTACH_FORM,
                                                     XmNbottomAttachment, XmATTACH_FORM,
                                                     NULL);
    XtVaCreateManagedWidget("Warn", xmPushButtonWidgetClass, action_buttons_row_col, XmNsensitive, False, NULL);
    XtVaCreateManagedWidget("Block", xmPushButtonWidgetClass, action_buttons_row_col, XmNsensitive, False, NULL);
    XtVaCreateManagedWidget("Talk", xmPushButtonWidgetClass, action_buttons_row_col, XmNsensitive, False, NULL);

    send_button = XtVaCreateManagedWidget("Send", xmPushButtonWidgetClass, bottom_buttons_form,
                                          XmNrightAttachment, XmATTACH_FORM, 
                                          XmNbottomAttachment, XmATTACH_FORM,
                                          XmNtopAttachment, XmATTACH_FORM, 
                                          XmNdefaultButtonShadowThickness, 1, 
                                          NULL);
    XtAddCallback(send_button, XmNactivateCallback, send_message_callback, NULL);
    XtVaSetValues(bottom_buttons_form, XmNdefaultButton, send_button, NULL);


    Widget scrolled_input_win = XmCreateScrolledWindow(input_form, "scrolledInputWin", NULL, 0);
    XtVaSetValues(scrolled_input_win,
                  XmNtopAttachment, XmATTACH_FORM, 
                  XmNbottomAttachment, XmATTACH_WIDGET, XmNbottomWidget, bottom_buttons_form, 
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_FORM,
                  NULL);

    input_text = XmCreateText(scrolled_input_win, "inputText", NULL, 0);
    XtVaSetValues(input_text,
                  XmNeditMode, XmMULTI_LINE_EDIT, 
                  XmNrows, 3,                     
                  XmNwordWrap, True,              
                  NULL);
    XtManageChild(input_text);
    XtManageChild(scrolled_input_win);
    XmScrolledWindowSetAreas(scrolled_input_win, NULL, NULL, input_text);

    // 5. Set MainWindow areas
    XmMainWindowSetAreas(main_window, menu_bar, NULL, NULL, NULL, main_form);

    // 6. Register the pipe's read end with the Motif event loop
    XtAppAddInput(app_context, pipe_fds[0], (XtPointer)XtInputReadMask, handle_pipe_input, NULL);

    // 7. Realize the widget tree and start the main event loop
    XtRealizeWidget(app_shell);

    append_to_conversation("Welcome to MotifGPT! Type your message and press Send or Enter.\n"); // Changed name here

    XtAppMainLoop(app_context); 

    if (dp_ctx) dp_destroy_context(dp_ctx);
    curl_global_cleanup();
    if (pipe_fds[0] != -1) close(pipe_fds[0]);
    if (pipe_fds[1] != -1) close(pipe_fds[1]);

    return 0; 
}

