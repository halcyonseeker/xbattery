/* 
 * xbattery.c 
 * A simple battery status monitor
 * https://git.sr.ht/~thalia/xbattery
 *
 */

#define _GNU_SOURCE             /* asprintf() */

#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>

#include <freetype2/ft2build.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/Xos.h>
#include <X11/Xutil.h>

#define RGB(r, g, b) (b + (g<<8) + (r<<16))

/* Core variables */
typedef struct {
    char *name;
    int  percent;
    char *status;
    char *remaining;
} Battery;
Battery* battery;

/* X variables */
Display *dis;
int screen;
Window win;
GC gc;
XftDraw *draw;
XftFont *text_font, *emoji_font;
XftColor xftc;
int x11_fd;

/* Function signatures */
void die(char *what, char *happened);
void free_battery(void);
void display(void);
void init_x(void);
void init_batteries(void);
unsigned long color_for_percentage(int percent);
void parse_acpi_to_struct(void);
int main(int argc, char *argv[]);

/**
 * Helper Functions
 */
void
die(char *what, char *happened)
{
    fprintf(stderr, "%s: %s\n", what, happened);
    exit(EXIT_FAILURE);
}

void
free_and_close(void)
{
    if (battery->name != NULL)
        free(battery->name);
    if (battery->status != NULL)
        free(battery->status);
    if (battery->remaining != NULL)
        free(battery->remaining);
    free(battery);

    XFreeGC(dis, gc);
    XDestroyWindow(dis, win);
    XCloseDisplay(dis);
}

unsigned long
color_for_percentage(int p)
{
    unsigned long color;
    if (0 <= p && p <= 14) 
        color = RGB(255, 51, 0);   /* red */
    else if (15 <= p && p <= 29)
        color = RGB(255, 153, 0);  /* orange */
    else if (30 <= p && p <= 49)
        color = RGB(255, 255, 0);  /* yellow */
    else if (50 <= p && p <= 74)
        color = RGB(153, 255, 51); /* light green */
    else if (75 <= p && p <= 100)
        color = RGB(51, 204, 51);  /* green */
    return color;
}

/**
 * For ACPI systems, read data into the global battery struct.
 * This is called every few seconds by display()
 */
void
parse_acpi_to_struct(void)
{
    FILE *acpi_info;
    char line[256];

    acpi_info = fopen("/sys/class/power_supply/BAT0/uevent", "r");
    if (acpi_info == NULL)
        die("fopen", "I couldn't open BAT#'s usevent file");

    while (fgets(line, sizeof(line), acpi_info) != NULL) {
        if ((strchr(line, '=') == NULL) || (strchr(line, '\n') == NULL))
            continue;

        char key[127], val[127];
        int key_len = 0, val_len = 0;

        /* uevent has a simple key=value\n syntax*/
        for (int i = 0; i < 127; i++) {
            key[i] = '0';
            val[i] = '0';
        }
        for (; line[key_len] != '='; key_len++)
            key[key_len] = line[key_len];
        key[key_len++] = '\0';
        for (; line[key_len] != '\n'; key_len++, val_len++)
            val[val_len] = line[key_len];
        val[val_len++] = '\0';

        /* Add the values to the appropriate Battery fields */
        if (strcmp(key, "POWER_SUPPLY_NAME") == 0) {
            if (battery->name != NULL)
                free(battery->name);
            battery->name = (char*)calloc(val_len, sizeof(char));
            if (battery->name == NULL)
                die("calloc", "I couldn't get memory for the battery's name");
            strcpy(battery->name, val);
        } else if (strcmp(key, "POWER_SUPPLY_STATUS") == 0) {
            if (battery->status != NULL)
                free(battery->status);
            battery->status = (char*)calloc(val_len, sizeof(char));
            if (battery->status == NULL)
                die("calloc", "I couldn't get memory for the battery's status");
            strcpy(battery->status, val);
        } else if (strcmp(key, "POWER_SUPPLY_CAPACITY") == 0) {
            battery->percent = atoi(val);
        }

        /* TODO
         * Figure out how to compute the time remaining. For now this just 
         * encodes the current percentage as a string. */
        asprintf(&battery->remaining, "%i%%", battery->percent);
    }
    fclose(acpi_info);
}

/**
 * Run a loop to query system info and update the X window
 * The arguments are the width and height of the window.
 */
void
display(void)
{
    int percent = battery->percent;
    unsigned long color = color_for_percentage(percent);

    /* get current window attributes */
    XWindowAttributes *attrs = malloc(sizeof(XWindowAttributes));
    XGetWindowAttributes(dis, win, attrs);

    int percentage_width = (attrs->width / 100.0) * percent;

    /* Add colors to the window */
    XClearWindow(dis, win);
    XSetForeground(dis, gc, color);
    XDrawRectangle(dis, win, gc, 0, 0, attrs->width, attrs->height);
    XFillRectangle(dis, win, gc, 0, 0, percentage_width, attrs->height);
    XSetForeground(dis, gc, RGB(10, 10, 10));

    int x = 5;
    int y = (attrs->height / 2) + 10;

    /* Add text to the window */
    if (strcmp(battery->status, "Charging") == 0) {
        char *icon = "🔌: ";
        XftDrawStringUtf8(draw, &xftc, emoji_font, x, y, (XftChar8*)icon,
                        strlen(icon));
        x += 35;
        XftDrawStringUtf8(draw, &xftc, text_font, x, y,
                        (XftChar8*)battery->remaining,
                        strlen(battery->remaining));
    } else if (strcmp(battery->status, "Discharging") == 0) {
        char *icon = "🔋: ";
        XftDrawStringUtf8(draw, &xftc, emoji_font, x, y, (XftChar8*)icon,
                        strlen(icon));
        x += 35;
        XftDrawStringUtf8(draw, &xftc, text_font, x, y,
                        (XftChar8*)battery->remaining,
                        strlen(battery->remaining));
    }
    free(attrs);
}

/**
 * X11 init
 */
void init_x(void)
{
    unsigned long black, white;
    int width = 200, height = 70;

    dis = XOpenDisplay((char *)0);
    screen = DefaultScreen(dis);
    black = BlackPixel(dis, screen), white = WhitePixel(dis, screen);
    win = XCreateSimpleWindow(dis, DefaultRootWindow(dis), 0, 0, width, height, 5,
                              black, white);
    XSetStandardProperties(dis, win, "XBattery", "XBattery", None, NULL, 0, NULL);

    //XSelectInput(dis, win, ExposureMask | ResizeRedirectMask);
    XSelectInput(dis, win, ExposureMask | StructureNotifyMask);
    gc = XCreateGC(dis, win, 0, 0);
    XSetBackground(dis, gc, white);
    XSetForeground(dis, gc, black);
    XSetFillStyle(dis, gc, FillSolid);
    XClearWindow(dis, win);
    XMapRaised(dis, win);

    /*  Font  */
    draw = XftDrawCreate(dis, win, DefaultVisual(dis, 0), DefaultColormap(dis, 0));
    text_font = XftFontOpenName(dis, DefaultScreen(dis), "DejaVu Sans Mono-14");
    emoji_font = XftFontOpenName(dis, DefaultScreen(dis), "Noto Emoji-14");

    /* Background */
    XRenderColor color = { 0, 0, 0, 0xFFFF };
    XftColorAllocValue(dis, DefaultVisual(dis, DefaultScreen(dis)),
                       DefaultColormap(dis, DefaultScreen(dis)), &color, &xftc);
}

/**
 * Setup the batteries but don't read them yet
 */
void
init_batteries(void)
{
    int bats = 0;
    DIR *acpi_dir;
    struct dirent *dirp;

    if ((acpi_dir = opendir("/sys/class/power_supply")) != NULL) {
        /* 
         * Make sure there are batteries attached. This is not the best
         * way to do this, but it will be useful later when multiple
         * batteries are supported 
         */
        while ((dirp = readdir(acpi_dir)) != NULL)
            if (strncmp(dirp->d_name, "BAT", 3) == 0)
                bats++;
        if (bats == 0)
            die("init", "I couldn't find any batteries");

        battery = calloc(1, sizeof(Battery));
        if (battery == NULL)
            die("calloc", "I couldn't allocate core for the battery");
                
        closedir(acpi_dir);
    } else {
        die("opendir", "I failed to open /sys/class/power_supply");
    }
}

int
main(int argc, char *argv[])
{
    struct timeval tv;
    fd_set in_fds;

    /* Start the window */
    init_x();

    /* Get a file descriptor for the window */
    x11_fd = ConnectionNumber(dis);

    /* Set up the battery info struct */
    init_batteries();
    parse_acpi_to_struct();

    /* Main loop reads battery information and draws the X window */
    for (;;) {
        /* Create a file description set containing x11_fd */
        FD_ZERO(&in_fds);
        FD_SET(x11_fd, &in_fds);

        /* Set a timer to refresh every 10 seconds */
        tv.tv_usec = 0;
        tv.tv_sec = 10;

        /* respond to the timer or a resize event */
        if (select(x11_fd + 1, &in_fds, 0, 0, &tv)) {
            /* Handle the event here */
            printf("handle event\n");
            display();
        } else {
            /* Handle timer here */
            parse_acpi_to_struct();
            printf("%s: %s - %i, %s\n", battery->name, battery->status,
                    battery->percent, battery->remaining);
            display();
        }
    }

    free_and_close();
    return EXIT_SUCCESS;
}
