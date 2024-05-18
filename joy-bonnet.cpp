
#include <atomic>
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>
#include <libevdev/libevdev.h>

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

std::atomic<bool> g_stop(false);
static volatile bool g_update_report = false;

typedef struct __attribute__((packed)) {
    uint8_t report_id;
    uint16_t x;
    uint16_t y;
    uint16_t buttons;
} multiplayer_gamepad_report_t;

void signal_handler(int sig)
{
    g_stop = true;
}

struct libevdev* find_device_by_name(const std::string& requested_name) {
	struct libevdev *dev = nullptr;

	for (int i = 0;; i++) {
		std::string path = "/dev/input/event" + std::to_string(i);
		int fd = open(path.c_str(), O_RDWR|O_CLOEXEC);
		if (fd == -1) {
			break; // no more character devices
		}
		if (libevdev_new_from_fd(fd, &dev) == 0) {
			std::string name = libevdev_get_name(dev);
			if (name == requested_name) {
				return dev;
			}
			libevdev_free(dev);
			dev = nullptr;
		}
		close(fd);
	}

	return nullptr;
}

void process_events(struct libevdev *dev, multiplayer_gamepad_report_t *report) {
	struct input_event ev = {};
	int status = 0;
	auto is_error = [](int v) { return v < 0 && v != -EAGAIN; };
	auto has_next_event = [](int v) { return v >= 0; };
	const auto flags = LIBEVDEV_READ_FLAG_NORMAL | LIBEVDEV_READ_FLAG_BLOCKING;

	while (status = libevdev_next_event(dev, flags, &ev), !is_error(status) || !g_stop) {
		if (!has_next_event(status)) continue;

        switch (ev.type)
        {
        case EV_KEY:
            switch (ev.code)
            {
            case BTN_SOUTH:
            case BTN_EAST:
            case BTN_NORTH:
            case BTN_WEST:
            case BTN_SELECT:
            case BTN_START:
            case BTN_THUMBL:
            case BTN_THUMBR:
                report->buttons = report->buttons & ~(1 << ev.code - BTN_SOUTH) | (ev.value << ev.code - BTN_SOUTH);
                g_update_report = true;
                break;
            default:
                break;
            }
            break;
        case EV_ABS:
            switch (ev.code)
            {
            case ABS_X:
                report->x = ev.value;
                g_update_report = true;
                break;
            case ABS_Y:
                report->y = ev.value;
                g_update_report = true;
                break;
            default:
                break;
            }
        default:
            break;
        }
	}
}

int main() {
    struct sigaction sa_int;
    multiplayer_gamepad_report_t report = {0};
    int gamepad_fd;

    // /* Setup SIGINT signal handler */
    std::signal(SIGINT, signal_handler);

    gamepad_fd = open(GAMEPAD_GADGET, O_WRONLY);
    if (gamepad_fd < 0)
    {
        std::perror("Failed to open gamepad input device");
        return EXIT_FAILURE;
    }

    struct libevdev *buttons = find_device_by_name("joy-bonnet-buttons");
    struct libevdev *joystick = find_device_by_name("joy-bonnet-stick");

	if (buttons == nullptr || joystick == nullptr) {
		std::cerr << "Couldn't find joystick and/or buttons!" << std::endl;
		return EXIT_FAILURE;
	}

	libevdev_grab(joystick, LIBEVDEV_GRAB);
	libevdev_grab(buttons, LIBEVDEV_GRAB);

    // Init HID report
    report.report_id = 1;
    report.x = 0;
    report.y = 0;
    report.buttons = 0;

	std::thread joystick_thread(process_events, joystick, &report);
	std::thread buttons_thread(process_events, buttons, &report);

    g_update_report = false;
    while (!g_stop)
    {
        if (g_update_report == true)
        {
            if (write(gamepad_fd, (unsigned char *)&report, sizeof(report)) < 0)
            {
                std::perror("Failed to write to the device file");
                close(gamepad_fd);
                return -1;
            }
            g_update_report = false;
        }
    }

    joystick_thread.join();
    buttons_thread.join();

	libevdev_free(joystick);
	libevdev_free(buttons);
	return 0;
}
