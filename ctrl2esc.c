// compile:
//     gcc ctrl2esc.c -o ctrl2esc -I/usr/include/libevdev-1.0 -levdev -ludev
// run:
//     sudo nice -n -20 ./ctrl2esc >ctrl2esc.log 2>ctrl2esc.err &

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

#include <libudev.h>
#include <sys/select.h>

#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>

// clang-format off
const struct input_event
esc_up            = {.type = EV_KEY, .code = KEY_ESC,        .value = 0},
ctrl_up           = {.type = EV_KEY, .code = KEY_LEFTCTRL,   .value = 0},
capslock_up       = {.type = EV_KEY, .code = KEY_CAPSLOCK,   .value = 0},
leftalt_up        = {.type = EV_KEY, .code = KEY_LEFTALT,    .value = 0},
rightalt_up       = {.type = EV_KEY, .code = KEY_RIGHTALT,   .value = 0},
leftshift_up      = {.type = EV_KEY, .code = KEY_LEFTSHIFT,  .value = 0},
rightshift_up     = {.type = EV_KEY, .code = KEY_RIGHTSHIFT, .value = 0},
tilde_up          = {.type = EV_KEY, .code = KEY_GRAVE,      .value = 0},
esc_down          = {.type = EV_KEY, .code = KEY_ESC,        .value = 1},
ctrl_down         = {.type = EV_KEY, .code = KEY_LEFTCTRL,   .value = 1},
capslock_down     = {.type = EV_KEY, .code = KEY_CAPSLOCK,   .value = 1},
leftalt_down      = {.type = EV_KEY, .code = KEY_LEFTALT,    .value = 1},
rightalt_down     = {.type = EV_KEY, .code = KEY_RIGHTALT,   .value = 1},
leftshift_down    = {.type = EV_KEY, .code = KEY_LEFTSHIFT,  .value = 1},
rightshift_down   = {.type = EV_KEY, .code = KEY_RIGHTSHIFT, .value = 1},
tilde_down        = {.type = EV_KEY, .code = KEY_GRAVE,      .value = 1},
esc_repeat        = {.type = EV_KEY, .code = KEY_ESC,        .value = 2},
ctrl_repeat       = {.type = EV_KEY, .code = KEY_LEFTCTRL,   .value = 2},
capslock_repeat   = {.type = EV_KEY, .code = KEY_CAPSLOCK,   .value = 2},
leftalt_repeat    = {.type = EV_KEY, .code = KEY_LEFTALT,    .value = 2},
rightalt_repeat   = {.type = EV_KEY, .code = KEY_RIGHTALT,   .value = 2},
leftshift_repeat  = {.type = EV_KEY, .code = KEY_LEFTSHIFT,  .value = 2},
rightshift_repeat = {.type = EV_KEY, .code = KEY_RIGHTSHIFT, .value = 2},
tilde_repeat      = {.type = EV_KEY, .code = KEY_GRAVE,      .value = 2},
ev_syn_report     = {.type = EV_SYN, .code = SYN_REPORT,     .value = 0};
// clang-format on

int equal(const struct input_event *first, const struct input_event *second) {
    return first->type == second->type && first->code == second->code &&
           first->value == second->value;
}

int eventmap(const struct input_event *input, struct input_event output[]) {
    static bool capslock_is_down, leftshift_is_down, rightshift_is_down,
               leftalt_is_down, rightalt_is_down, esc_is_down, other_modifiers;
    static struct input_event prev_key_event;
    int num_events;

    if (input->type == EV_MSC && input->code == MSC_SCAN)
        return 0;

    output[0] = *input;
    num_events = 1;
    if (input->type != EV_KEY);
    else if (equal(input, &esc_down)) output[0] = tilde_down;
    else if (equal(input, &esc_up)) output[0] = tilde_up;
    else if (equal(input, &esc_repeat)) output[0] = tilde_repeat;
    else if (equal(input, &leftalt_down)) leftalt_is_down = true;
    else if (equal(input, &leftalt_up)) leftalt_is_down = false;
    else if (equal(input, &rightalt_down)) rightalt_is_down = true;
    else if (equal(input, &rightalt_up)) rightalt_is_down = false;
    else if (equal(input, &leftshift_down)) leftshift_is_down = true;
    else if (equal(input, &leftshift_up)) leftshift_is_down = false;
    else if (equal(input, &rightshift_down)) rightshift_is_down = true;
    else if (equal(input, &rightshift_up)) rightshift_is_down = false;
    else if (equal(input, &ctrl_down))
      other_modifiers = leftalt_is_down || rightalt_is_down ||
          leftshift_is_down || rightshift_is_down || esc_is_down;
    else if (equal(input, &ctrl_repeat)) num_events = 0;
    else if (equal(input, &ctrl_up)) {
      if (!other_modifiers && equal(&prev_key_event, &ctrl_down)) {
        output[0] = ctrl_up;
        output[1] = ev_syn_report;
        output[2] = esc_down;
        output[3] = ev_syn_report;
        output[4] = esc_up;
        num_events = 5;
      }
    }
    if (input->type == EV_KEY && num_events > 0)
      prev_key_event = output[num_events-1];
    return num_events;
}

int eventmap_loop(const char *devnode) {
    int result = 0;
    int fd     = open(devnode, O_RDONLY);
    if (fd < 0)
        return 0;

    struct libevdev *dev;
    if (libevdev_new_from_fd(fd, &dev) < 0)
        goto teardown_fd;

    sleep(1);

    if (libevdev_grab(dev, LIBEVDEV_GRAB) < 0)
        goto teardown_dev;
    if (libevdev_enable_event_type(dev, EV_KEY) < 0)
        goto teardown_grab;
    if (libevdev_enable_event_code(dev, EV_KEY, KEY_ESC, NULL) < 0)
        goto teardown_grab;
    if (libevdev_enable_event_code(dev, EV_KEY, KEY_CAPSLOCK, NULL) < 0)
        goto teardown_grab;
    if (libevdev_enable_event_code(dev, EV_KEY, KEY_LEFTCTRL, NULL) < 0)
        goto teardown_grab;
    if (libevdev_disable_event_code(dev, EV_KEY, KEY_WLAN) < 0)
        goto teardown_grab;

    struct libevdev_uinput *udev;
    if (libevdev_uinput_create_from_device(dev, LIBEVDEV_UINPUT_OPEN_MANAGED,
                                           &udev) < 0)
        goto teardown_grab;

    for (;;) {
        struct input_event input;
        int rc = libevdev_next_event(
            dev, LIBEVDEV_READ_FLAG_NORMAL | LIBEVDEV_READ_FLAG_BLOCKING,
            &input);

        while (rc == LIBEVDEV_READ_STATUS_SYNC)
            rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &input);

        if (rc == -EAGAIN)
            continue;

        if (rc != LIBEVDEV_READ_STATUS_SUCCESS)
            break;

        struct input_event output[10];
        int num_events = eventmap(&input, output);
        for (int i = 0; i < num_events; i++) {
            if (libevdev_uinput_write_event( udev, output[i].type,
                                    output[i].code, output[i].value) < 0)
              goto teardown_udev;
        }
    }

    result = 1;

teardown_udev:
    libevdev_uinput_destroy(udev);
teardown_grab:
    libevdev_grab(dev, LIBEVDEV_UNGRAB);
teardown_dev:
    libevdev_free(dev);
teardown_fd:
    close(fd);

    return result;
}

void eventmap_exec(const char *self_path, const char *devnode) {
    switch (fork()) {
        case -1:
            fprintf(stderr, "Fork failed on %s %s (%s)\n", self_path, devnode,
                    strerror(errno));
            break;
        case 0: {
            char *command[] = {(char *)self_path, (char *)devnode, NULL};
            execvp(command[0], command);
            fprintf(stderr, "Exec failed on %s %s (%s)\n", self_path, devnode,
                    strerror(errno));
        } break;
    }
}

int should_grab(struct udev_device *device, int initial_scan) {
    if (device == NULL)
        return 0;

    const char virtual_devices_directory[] = "/sys/devices/virtual/input/";
    if (strncmp(udev_device_get_syspath(device), virtual_devices_directory,
                sizeof(virtual_devices_directory) - 1) == 0)
        return 0;

    if (!initial_scan) {
        const char *action = udev_device_get_action(device);
        if (!action || strcmp(action, "add"))
            return 0;
    }

    const char input_prefix[] = "/dev/input/event";
    const char *devnode       = udev_device_get_devnode(device);
    if (!devnode || strncmp(devnode, input_prefix, sizeof(input_prefix) - 1))
        return 0;

    int fd = open(devnode, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Failed to open %s (%s)\n", devnode, strerror(errno));
        return 0;
    }

    struct libevdev *dev;
    if (libevdev_new_from_fd(fd, &dev) < 0) {
        fprintf(stderr, "Failed to create evdev device from %s (%s)\n", devnode,
                strerror(errno));
        close(fd);
        return 0;
    }

    int should_be_grabbed =
        libevdev_has_event_type(dev, EV_KEY) &&
        (libevdev_has_event_code(dev, EV_KEY, KEY_ESC) ||
         libevdev_has_event_code(dev, EV_KEY, KEY_CAPSLOCK));

    libevdev_free(dev);
    close(fd);

    return should_be_grabbed;
}

void kill_zombies(__attribute__((unused)) int signum) {
    int status;
    while (waitpid(-1, &status, WNOHANG) > 0)
        ;
}

int main(int argc, const char *argv[]) {
    int initial_scan;

    if (argc > 2) {
        fprintf(stderr, "usage: ctrl2esc [device-path]\n");
        return EXIT_FAILURE;
    }

    if (argc == 2)
        return !eventmap_loop(argv[1]);

    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_flags   = SA_NOCLDSTOP;
    sa.sa_handler = &kill_zombies;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("Couldn't summon zombie killer");
        return EXIT_FAILURE;
    }

    struct udev *udev = udev_new();
    if (!udev) {
        perror("Can't create udev");
        return EXIT_FAILURE;
    }

    struct udev_enumerate *enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, "input");
    udev_enumerate_scan_devices(enumerate);
    struct udev_list_entry *devices = udev_enumerate_get_list_entry(enumerate);
    struct udev_list_entry *dev_list_entry;
    udev_list_entry_foreach(dev_list_entry, devices) {
        struct udev_device *device = udev_device_new_from_syspath(
            udev, udev_list_entry_get_name(dev_list_entry));
        if (device) {
            if (should_grab(device, initial_scan = 1))
                eventmap_exec(argv[0], udev_device_get_devnode(device));
            udev_device_unref(device);
        }
    }
    udev_enumerate_unref(enumerate);

    struct udev_monitor *monitor = udev_monitor_new_from_netlink(udev, "udev");
    if (!monitor) {
        perror("Can't create monitor");
        return EXIT_FAILURE;
    }

    udev_monitor_filter_add_match_subsystem_devtype(monitor, "input", NULL);
    udev_monitor_enable_receiving(monitor);
    int fd = udev_monitor_get_fd(monitor);
    for (;;) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        if (select(fd + 1, &fds, NULL, NULL, NULL) > 0 && FD_ISSET(fd, &fds)) {
            struct udev_device *device = udev_monitor_receive_device(monitor);
            if (device) {
                if (should_grab(device, initial_scan = 0))
                    eventmap_exec(argv[0], udev_device_get_devnode(device));
                udev_device_unref(device);
            }
        }
    }

    udev_monitor_unref(monitor);
    udev_unref(udev);
}
