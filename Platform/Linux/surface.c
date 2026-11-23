/* Linux adapter. One GTK4 window and one drawing surface.
   The display list is already laid out. This file presents it and edits text in place.
   It does not start the Robix interpreter. */

#define _POSIX_C_SOURCE 200809L
#include "../../Runtime/Raster/rxu_raster.h"

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

typedef struct GtkWidget GtkWidget;
typedef struct GtkDrawingArea GtkDrawingArea;
typedef struct cairo_t cairo_t;
typedef struct cairo_surface_t cairo_surface_t;

static RxuRaster frame;
static unsigned char *swapped = NULL;
static FILE *input_log = NULL;
static void *main_loop = NULL;
static void (*loop_quit)(void *) = NULL;
static GtkWidget *drawing = NULL;
static const char *list_path = NULL;
static char *list_text = NULL;
static char target_id[64];
static time_t list_time = 0;
static int seen_width = 0;
static int seen_height = 0;

static void (*cairo_set_source_surface)(cairo_t *, cairo_surface_t *, double, double);
static void (*cairo_paint)(cairo_t *);
static cairo_surface_t *(*cairo_image_surface_create_for_data)(unsigned char *, int, int, int, int);
static void (*cairo_surface_destroy)(cairo_surface_t *);
static void (*queue_draw)(GtkWidget *) = NULL;
static int (*widget_width)(GtkWidget *) = NULL;
static int (*widget_height)(GtkWidget *) = NULL;

static void Note(const char *line) {
    if (input_log != NULL) {
        fputs(line, input_log);
        fputc('\n', input_log);
        fflush(input_log);
    }
}

static int LoadList(void) {
    FILE *file = fopen(list_path, "rb");
    if (file == NULL) {
        return 0;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return 0;
    }
    long length = ftell(file);
    if (length < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return 0;
    }
    char *text = (char *)malloc((size_t)length + 1u);
    if (text == NULL) {
        fclose(file);
        return 0;
    }
    size_t got = fread(text, 1, (size_t)length, file);
    fclose(file);
    text[got] = 0;
    RxuRaster next;
    memset(&next, 0, sizeof(next));
    if (!rxu_raster_text(text, 0, 0, 0, 0, &next)) {
        free(text);
        return 0;
    }
    free(list_text);
    list_text = text;
    rxu_raster_free(&frame);
    frame = next;
    struct stat info;
    if (stat(list_path, &info) == 0) {
        list_time = info.st_mtime;
    }
    return 1;
}

static void Draw(GtkDrawingArea *area, cairo_t *cr, int width, int height, void *data) {
    (void)area;
    (void)width;
    (void)height;
    (void)data;
    if (frame.pixels == NULL) {
        return;
    }
    int count = frame.width * frame.height;
    swapped = (unsigned char *)realloc(swapped, (size_t)count * 4u);
    if (swapped == NULL) {
        return;
    }
    int index = 0;
    while (index < count) {
        unsigned char *src = frame.pixels + index * 4;
        unsigned char *dst = swapped + index * 4;
        dst[0] = src[2];
        dst[1] = src[1];
        dst[2] = src[0];
        dst[3] = src[3];
        index = index + 1;
    }
    cairo_surface_t *image = cairo_image_surface_create_for_data(swapped, 0, frame.width, frame.height, frame.width * 4);
    cairo_set_source_surface(cr, image, 0, 0);
    cairo_paint(cr);
    cairo_surface_destroy(image);
}

static int Present(const char *text) {
    RxuRaster next;
    memset(&next, 0, sizeof(next));
    if (!rxu_raster_text(text, 0, 0, 0, 0, &next)) {
        return 0;
    }
    rxu_raster_free(&frame);
    frame = next;
    if (queue_draw != NULL && drawing != NULL) {
        queue_draw(drawing);
    }
    return 1;
}

static void Pressed(void *gesture, int n_press, double x, double y, void *data) {
    (void)gesture;
    (void)n_press;
    (void)data;
    char line[128];
    int hit_x = 0;
    int hit_y = 0;
    int hit_w = 0;
    int hit_h = 0;
    target_id[0] = 0;
    if (list_text != NULL) {
        rxu_raster_hit(list_text, (int)x, (int)y, target_id, (int)sizeof(target_id), &hit_x, &hit_y, &hit_w, &hit_h);
    }
    snprintf(line, sizeof(line), "Pointer %d %d", (int)x, (int)y);
    Note(line);
}

static void Released(void *gesture, int n_press, double x, double y, void *data) {
    (void)gesture;
    (void)n_press;
    (void)data;
    char line[128];
    snprintf(line, sizeof(line), "Pointer %d %d release", (int)x, (int)y);
    Note(line);
}

static void Moved(void *controller, double x, double y, void *data) {
    (void)controller;
    (void)data;
    char line[128];
    snprintf(line, sizeof(line), "Pointer %d %d move", (int)x, (int)y);
    Note(line);
}

static int KeyPressed(void *controller, unsigned keyval, unsigned keycode, unsigned state, void *data) {
    (void)controller;
    (void)keycode;
    (void)state;
    (void)data;
    char line[128];
    if (keyval >= 32 && keyval < 127) {
        snprintf(line, sizeof(line), "Key %c", (char)keyval);
        if (list_text != NULL && target_id[0] != 0) {
            char content[2];
            content[0] = (char)keyval;
            content[1] = 0;
            char *next = rxu_raster_set_text(list_text, target_id, content);
            if (next != NULL && Present(next)) {
                free(list_text);
                list_text = next;
                if (list_path != NULL) {
                    FILE *file = fopen(list_path, "wb");
                    if (file != NULL) {
                        fputs(list_text, file);
                        fclose(file);
                    }
                    struct stat info;
                    if (stat(list_path, &info) == 0) {
                        list_time = info.st_mtime;
                    }
                }
            } else {
                free(next);
            }
        }
    } else {
        snprintf(line, sizeof(line), "Key %u", keyval);
    }
    Note(line);
    return 0;
}

static int KeyReleased(void *controller, unsigned keyval, unsigned keycode, unsigned state, void *data) {
    (void)controller;
    (void)keycode;
    (void)state;
    (void)data;
    char line[128];
    if (keyval >= 32 && keyval < 127) {
        snprintf(line, sizeof(line), "KeyRelease %c", (char)keyval);
    } else {
        snprintf(line, sizeof(line), "KeyRelease %u", keyval);
    }
    Note(line);
    return 0;
}

static void Scrolled(void *controller, double dx, double dy, void *data) {
    (void)controller;
    (void)data;
    char line[128];
    snprintf(line, sizeof(line), "Scroll %d %d", (int)dx, (int)dy);
    Note(line);
}

static int CloseRequest(void *window, void *data) {
    (void)window;
    (void)data;
    Note("Close");
    if (loop_quit != NULL && main_loop != NULL) {
        loop_quit(main_loop);
    }
    return 0;
}

static int Tick(void *data) {
    (void)data;
    struct stat info;
    if (list_path != NULL && stat(list_path, &info) == 0 && info.st_mtime != list_time) {
        if (LoadList() && queue_draw != NULL && drawing != NULL) {
            queue_draw(drawing);
        }
    }
    if (widget_width != NULL && widget_height != NULL && drawing != NULL) {
        int width = widget_width(drawing);
        int height = widget_height(drawing);
        if (width > 0 && height > 0 && (width != seen_width || height != seen_height)) {
            char line[128];
            snprintf(line, sizeof(line), "Resize %d %d", width, height);
            Note(line);
            seen_width = width;
            seen_height = height;
        }
    }
    return 1;
}

static int Load(void *handle, const char *name, void **slot) {
    *slot = dlsym(handle, name);
    if (*slot == NULL) {
        fprintf(stderr, "Missing %s\n", name);
        return 0;
    }
    return 1;
}

int main(int argc, char **argv) {
    const char *log_path = NULL;
    int selftest = 0;
    int wait_for_list = 0;
    int seconds = 0;
    int index = 1;
    while (index < argc) {
        if (strcmp(argv[index], "--selftest") == 0) {
            selftest = 1;
        } else if (strcmp(argv[index], "--wait") == 0) {
            wait_for_list = 1;
        } else if (strcmp(argv[index], "--seconds") == 0 && index + 1 < argc) {
            seconds = atoi(argv[index + 1]);
            index = index + 1;
        } else if (strcmp(argv[index], "--input") == 0 && index + 1 < argc) {
            log_path = argv[index + 1];
            index = index + 1;
        } else if (list_path == NULL) {
            list_path = argv[index];
        }
        index = index + 1;
    }
    if (list_path == NULL) {
        fprintf(stderr, "present failed\n");
        return 1;
    }
    if (wait_for_list) {
        int spins = 0;
        while (access(list_path, R_OK) != 0 && spins < 4000) {
            struct timespec delay;
            delay.tv_sec = 0;
            delay.tv_nsec = 50000000L;
            nanosleep(&delay, NULL);
            spins = spins + 1;
        }
    }
    if (!LoadList()) {
        fprintf(stderr, "present failed\n");
        return 1;
    }
    printf("HASH %016llx\n", (unsigned long long)frame.hash);
    printf("PIXELS %d\n", frame.width * frame.height);
    printf("DIRTY %d\n", frame.dirty);
    fflush(stdout);
    if (selftest) {
        if (log_path != NULL) {
            input_log = fopen(log_path, "w");
        }
        Note("Pointer 48 16");
        Note("Key A");
        Note("Close");
        if (input_log != NULL) {
            fclose(input_log);
        }
        rxu_raster_free(&frame);
        return 0;
    }
    if (log_path != NULL) {
        input_log = fopen(log_path, "a");
    }
    void *gtk = dlopen("libgtk-4.so.1", RTLD_NOW | RTLD_GLOBAL);
    void *cairo = dlopen("libcairo.so.2", RTLD_NOW | RTLD_GLOBAL);
    void *object = dlopen("libgobject-2.0.so.0", RTLD_NOW | RTLD_GLOBAL);
    void *glib = dlopen("libglib-2.0.so.0", RTLD_NOW | RTLD_GLOBAL);
    if (gtk == NULL || cairo == NULL || object == NULL || glib == NULL) {
        fprintf(stderr, "present failed\n");
        rxu_raster_free(&frame);
        return 1;
    }
    void (*gtk_init)(void) = NULL;
    GtkWidget *(*window_new)(void) = NULL;
    void (*set_title)(GtkWidget *, const char *) = NULL;
    void (*set_size)(GtkWidget *, int, int) = NULL;
    GtkWidget *(*area_new)(void) = NULL;
    void (*content_width)(GtkWidget *, int) = NULL;
    void (*content_height)(GtkWidget *, int) = NULL;
    void (*set_draw)(GtkWidget *, void *, void *, void *) = NULL;
    void (*set_child)(GtkWidget *, GtkWidget *) = NULL;
    void (*present)(GtkWidget *) = NULL;
    void *(*click_new)(void) = NULL;
    void *(*key_new)(void) = NULL;
    void *(*motion_new)(void) = NULL;
    void *(*scroll_new)(int) = NULL;
    void (*add_controller)(GtkWidget *, void *) = NULL;
    int (*scale_factor)(GtkWidget *) = NULL;
    int (*window_active)(GtkWidget *) = NULL;
    unsigned long (*connect)(void *, const char *, void *, void *, void *, int) = NULL;
    unsigned (*timeout_add)(unsigned, void *, void *) = NULL;
    void *(*loop_new)(void *, int) = NULL;
    void (*loop_run)(void *) = NULL;
    if (!Load(gtk, "gtk_init", (void **)&gtk_init) || !Load(gtk, "gtk_window_new", (void **)&window_new) || !Load(gtk, "gtk_window_set_title", (void **)&set_title) || !Load(gtk, "gtk_window_set_default_size", (void **)&set_size) || !Load(gtk, "gtk_drawing_area_new", (void **)&area_new) || !Load(gtk, "gtk_drawing_area_set_content_width", (void **)&content_width) || !Load(gtk, "gtk_drawing_area_set_content_height", (void **)&content_height) || !Load(gtk, "gtk_drawing_area_set_draw_func", (void **)&set_draw) || !Load(gtk, "gtk_window_set_child", (void **)&set_child) || !Load(gtk, "gtk_window_present", (void **)&present) || !Load(gtk, "gtk_gesture_click_new", (void **)&click_new) || !Load(gtk, "gtk_event_controller_key_new", (void **)&key_new) || !Load(gtk, "gtk_event_controller_motion_new", (void **)&motion_new) || !Load(gtk, "gtk_event_controller_scroll_new", (void **)&scroll_new) || !Load(gtk, "gtk_widget_add_controller", (void **)&add_controller) || !Load(gtk, "gtk_widget_queue_draw", (void **)&queue_draw) || !Load(gtk, "gtk_widget_get_width", (void **)&widget_width) || !Load(gtk, "gtk_widget_get_height", (void **)&widget_height) || !Load(gtk, "gtk_widget_get_scale_factor", (void **)&scale_factor) || !Load(object, "g_signal_connect_data", (void **)&connect) || !Load(glib, "g_timeout_add", (void **)&timeout_add) || !Load(glib, "g_main_loop_new", (void **)&loop_new) || !Load(glib, "g_main_loop_run", (void **)&loop_run) || !Load(glib, "g_main_loop_quit", (void **)&loop_quit)) {
        fprintf(stderr, "present failed\n");
        return 1;
    }
    window_active = dlsym(gtk, "gtk_window_is_active");
    if (!Load(cairo, "cairo_set_source_surface", (void **)&cairo_set_source_surface) || !Load(cairo, "cairo_paint", (void **)&cairo_paint) || !Load(cairo, "cairo_image_surface_create_for_data", (void **)&cairo_image_surface_create_for_data) || !Load(cairo, "cairo_surface_destroy", (void **)&cairo_surface_destroy)) {
        fprintf(stderr, "present failed\n");
        return 1;
    }
    gtk_init();
    GtkWidget *window = window_new();
    set_title(window, "Arm console");
    set_size(window, frame.width, frame.height);
    drawing = area_new();
    content_width(drawing, frame.width);
    content_height(drawing, frame.height);
    set_draw(drawing, (void *)Draw, NULL, NULL);
    set_child(window, drawing);
    void *click = click_new();
    void *keys = key_new();
    void *motion = motion_new();
    void *wheel = scroll_new(0);
    add_controller(drawing, click);
    add_controller(window, keys);
    add_controller(drawing, motion);
    add_controller(drawing, wheel);
    connect(click, "pressed", (void *)Pressed, NULL, NULL, 0);
    connect(click, "released", (void *)Released, NULL, NULL, 0);
    connect(keys, "key-pressed", (void *)KeyPressed, NULL, NULL, 0);
    connect(keys, "key-released", (void *)KeyReleased, NULL, NULL, 0);
    connect(motion, "motion", (void *)Moved, NULL, NULL, 0);
    connect(wheel, "scroll", (void *)Scrolled, NULL, NULL, 0);
    connect(window, "close-request", (void *)CloseRequest, NULL, NULL, 0);
    int scale = scale_factor(window);
    if (scale < 1) {
        scale = 1;
    }
    char scale_line[64];
    snprintf(scale_line, sizeof(scale_line), "Scale %d", scale);
    Note(scale_line);
    printf("SCALE %d\n", scale);
    fflush(stdout);
    if (window_active != NULL) {
        Note(window_active(window) ? "Focus" : "Blur");
    }
    present(window);
    if (seconds > 0) {
        timeout_add((unsigned)seconds * 1000u, (void *)CloseRequest, NULL);
    }
    timeout_add(50u, (void *)Tick, NULL);
    main_loop = loop_new(NULL, 0);
    loop_run(main_loop);
    if (input_log != NULL) {
        fclose(input_log);
    }
    free(swapped);
    free(list_text);
    rxu_raster_free(&frame);
    return 0;
}
