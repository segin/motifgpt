#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Mock Types
typedef void* Widget;
typedef unsigned long Pixel;
typedef int Boolean;
#define True 1
#define False 0
#define XmNforeground "foreground"

Pixel grey_fg_color = 0x808080;
Pixel normal_fg_color = 0x000000;

// Mock Global State
char* mock_text_val = NULL;
Pixel mock_fg_color_val = 0;

// Mock Functions
void XtFree(char* ptr) {
    if (ptr) free(ptr);
}

char* XmTextFieldGetString(Widget w) {
    if (!mock_text_val) return NULL;
    return strdup(mock_text_val);
}

// Mock XtVaGetValues: specifically for fetching foreground color
void XtVaGetValues(Widget w, const char* resource, void* val_addr, void* sentinel) {
    if (strcmp(resource, XmNforeground) == 0) {
        *((Pixel*)val_addr) = mock_fg_color_val;
    }
}

// --- The Function Under Test ---
static void get_setting_value(Widget w, const char* placeholder, char* buffer, size_t buffer_size, Boolean clear_if_placeholder) {
    char *tmp = XmTextFieldGetString(w);
    if (!tmp) return;

    if (clear_if_placeholder) {
        Pixel fg_color;
        XtVaGetValues(w, XmNforeground, &fg_color, NULL);
        if (strcmp(tmp, placeholder) == 0 && fg_color == grey_fg_color) {
            buffer[0] = '\0';
            XtFree(tmp);
            return;
        }
    }

    strncpy(buffer, tmp, buffer_size - 1);
    buffer[buffer_size - 1] = '\0';
    XtFree(tmp);
}
// -------------------------------

void test_normal_text() {
    char buffer[256];
    mock_text_val = "MyAPIKey";
    mock_fg_color_val = normal_fg_color;

    get_setting_value(NULL, "PlaceHolder", buffer, sizeof(buffer), True);
    assert(strcmp(buffer, "MyAPIKey") == 0);

    get_setting_value(NULL, "PlaceHolder", buffer, sizeof(buffer), False);
    assert(strcmp(buffer, "MyAPIKey") == 0);
}

void test_placeholder_clear() {
    char buffer[256];
    mock_text_val = "PlaceHolder";
    mock_fg_color_val = grey_fg_color;

    // clear_if_placeholder = True -> Should clear
    get_setting_value(NULL, "PlaceHolder", buffer, sizeof(buffer), True);
    assert(strcmp(buffer, "") == 0);
}

void test_placeholder_keep() {
    char buffer[256];
    mock_text_val = "PlaceHolder";
    mock_fg_color_val = grey_fg_color;

    // clear_if_placeholder = False -> Should keep (copy placeholder text)
    get_setting_value(NULL, "PlaceHolder", buffer, sizeof(buffer), False);
    assert(strcmp(buffer, "PlaceHolder") == 0);
}

void test_placeholder_text_normal_color() {
    char buffer[256];
    mock_text_val = "PlaceHolder";
    mock_fg_color_val = normal_fg_color;

    // User typed exactly what placeholder is, but color is normal -> Should copy
    get_setting_value(NULL, "PlaceHolder", buffer, sizeof(buffer), True);
    assert(strcmp(buffer, "PlaceHolder") == 0);
}

void test_buffer_overflow() {
    char buffer[5];
    mock_text_val = "123456789";
    mock_fg_color_val = normal_fg_color;

    get_setting_value(NULL, "PlaceHolder", buffer, sizeof(buffer), True);
    // Buffer size 5: can hold 4 chars + null
    assert(strcmp(buffer, "1234") == 0);
    assert(buffer[4] == '\0');
}

int main() {
    test_normal_text();
    test_placeholder_clear();
    test_placeholder_keep();
    test_placeholder_text_normal_color();
    test_buffer_overflow();
    printf("All tests passed!\n");
    return 0;
}
