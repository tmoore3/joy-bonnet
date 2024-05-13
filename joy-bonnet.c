#include <fcntl.h>
#include <linux/input.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BTN_SOUTH_BIT_POS       0
#define BTN_EAST_BIT_POS        1
#define BTN_C_BIT_POS           2
#define BTN_NORTH_BIT_POS       3
#define BTN_WEST_BIT_POS        4              
#define BTN_Z_BIT_POS           5
#define BTN_TL_BIT_POS          6
#define BTN_TR_BIT_POS          7
#define BTN_TL2_BIT_POS         8
#define BTN_TR2_BIT_POS         9
#define BTN_SELECT_BIT_POS      10
#define BTN_START_BIT_POS       11
#define BTN_MODE_BIT_POS        12
#define BTN_THUMBL_BIT_POS      13
#define BTN_THUMBR_BIT_POS      14

#define GAMEPAD_GADGET "/dev/hidg0"
#define JOYSTICK "/dev/input/by-path/platform-joy-bonnet-stick-event"
#define BUTTONS "/dev/input/by-path/platform-joy-bonnet-buttons-event-joystick"

static volatile sig_atomic_t g_exit = false;
static volatile bool g_update_report = false;

typedef struct __attribute__((packed)) {
    uint8_t report_id;
    uint16_t x;
    uint16_t y;
    uint16_t buttons;
} multiplayer_gamepad_report_t;

void sigint_handler(int sig)
{
    g_exit = true;
}

void *handle_buttons(void *data)
{
    // Store the value argument passed to this thread
    multiplayer_gamepad_report_t *report = (multiplayer_gamepad_report_t *)data;
    int buttons_fd;

    buttons_fd = open(BUTTONS, O_RDONLY);
    if (buttons_fd < 0)
    {
        perror("Failed to open joy bonnet input device");
        exit(EXIT_FAILURE);
    }

    while (!g_exit)
    {
        struct input_event ev;
        int rc = read(buttons_fd, &ev, sizeof(ev));
        if (rc < 0)
        {
            perror("Failed to read input event");
            exit(EXIT_FAILURE);
        }
        else if (rc != sizeof(ev))
        {
            perror("Unexpected input event size\n");
            exit(EXIT_FAILURE);
        }
        else if (ev.type == EV_KEY)
        {
            switch (ev.code)
            {
            case BTN_NORTH:
                report->buttons = report->buttons & ~(1 << BTN_NORTH_BIT_POS) | (ev.value << BTN_NORTH_BIT_POS);
                break;
            case BTN_EAST:
                report->buttons = report->buttons & ~(1 << BTN_EAST_BIT_POS) | (ev.value << BTN_EAST_BIT_POS);
                break;
            case BTN_SOUTH:
                report->buttons = report->buttons & ~(1 << BTN_SOUTH_BIT_POS) | (ev.value << BTN_SOUTH_BIT_POS);
                break;
            case BTN_WEST:
                report->buttons = report->buttons & ~(1 << BTN_WEST_BIT_POS) | (ev.value << BTN_WEST_BIT_POS);
                break;
            case BTN_SELECT:
                report->buttons = report->buttons & ~(1 << BTN_SELECT_BIT_POS) | (ev.value << BTN_SELECT_BIT_POS);
                break;
            case BTN_START:
                report->buttons = report->buttons & ~(1 << BTN_START_BIT_POS) | (ev.value << BTN_START_BIT_POS);
                break;
            case BTN_THUMBL:
                report->buttons = report->buttons & ~(1 << BTN_THUMBL_BIT_POS) | (ev.value << BTN_THUMBL_BIT_POS);
                break;
            case BTN_THUMBR:
                report->buttons = report->buttons & ~(1 << BTN_THUMBR_BIT_POS) | (ev.value << BTN_THUMBR_BIT_POS);
                break;
            default:
                break;
            }
            g_update_report = true;
        }
    }
    close(buttons_fd);
}

void *handle_joystick(void *data)
{
    // Store the value argument passed to this thread
    multiplayer_gamepad_report_t *report = (multiplayer_gamepad_report_t *)data;
    int joystick_fd;

    joystick_fd = open(JOYSTICK, O_RDONLY);
    if (joystick_fd < 0)
    {
        perror("Failed to open joy bonnet input device");
        exit(EXIT_FAILURE);
    }

    while (!g_exit)
    {
        struct input_event ev;
        int rc = read(joystick_fd, &ev, sizeof(ev));
        if (rc < 0)
        {
            perror("Failed to read input event");
            exit(EXIT_FAILURE);
        }
        else if (rc != sizeof(ev))
        {
            perror("Unexpected input event size\n");
            exit(EXIT_FAILURE);
        }
        else if (ev.type == EV_ABS)
        {
            switch (ev.code)
            {
            case ABS_X:
                report->x = ev.value;
                break;
            case ABS_Y:
                report->y = ev.value;
                break;
            default:
                break;
            }
            g_update_report = true;
        }
    }
    close(joystick_fd);
}

int main()
{
    struct sigaction sa_int;
    multiplayer_gamepad_report_t report = {0};
    pthread_t tid1, tid2;
    int gamepad_fd;

    /* Setup SIGINT signal handler */
    sa_int.sa_handler = &sigint_handler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    if (sigaction(SIGINT, &sa_int, NULL) == -1)
    {
        perror("Failed to setup SIGINT handler");
        return EXIT_FAILURE;
    }

    // Init HID report
    report.report_id = 1;
    report.x = 0;
    report.y = 0;
    report.buttons = 0;

    pthread_create(&tid1, NULL, handle_buttons, (void *)&report);
    pthread_create(&tid2, NULL, handle_joystick, (void *)&report);

    gamepad_fd = open(GAMEPAD_GADGET, O_WRONLY);
    if (gamepad_fd < 0)
    {
        perror("Failed to open gamepad input device");
        return EXIT_FAILURE;
    }

    g_update_report = false;
    while (!g_exit)
    {
        if (g_update_report == true)
        {
            if (write(gamepad_fd, (unsigned char *)&report, sizeof(report)) < 0)
            {
                perror("Failed to write to the device file");
                close(gamepad_fd);
                return EXIT_FAILURE;
            }
            g_update_report = false;
        }
    }
    close(gamepad_fd);

    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);

    return EXIT_SUCCESS;
}