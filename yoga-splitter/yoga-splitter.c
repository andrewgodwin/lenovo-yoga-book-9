#include <linux/uinput.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <poll.h>

#define MAX_X 11440
#define MAX_Y 7148
#define MAX_SLOTS 4
#define TARGET_VID "17EF"
#define TARGET_PID "6161"
#define TARGET_MODALIAS "hid:b0003g0004v000017EFp00006161"
#define SLOT_TIMEOUT_MS 500
#define DEBUG false

// Track which Contact IDs are currently touching the glass
bool slot_active[2][MAX_SLOTS] = { {false} };

// Track last update time for each slot (in milliseconds)
long long slot_last_update[2][MAX_SLOTS] = { {0} };

long long get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// Find the correct hidraw node by scanning /sys
char* find_device() {
    static char path[64];
    DIR *dir = opendir("/sys/class/hidraw");
    if (!dir) return NULL;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.') continue;

        char uevent_path[128];
        snprintf(uevent_path, sizeof(uevent_path), "/sys/class/hidraw/%s/device/uevent", ent->d_name);

        FILE *f = fopen(uevent_path, "r");
        if (f) {
            char line[256];
            bool match_vid = false, match_pid = false, match_name = false;

            while (fgets(line, sizeof(line), f)) {
                if (strstr(line, "HID_ID=")) {
                    if (strstr(line, TARGET_VID)) match_vid = true;
                    if (strstr(line, TARGET_PID)) match_pid = true;
                }
                // Check the descriptive name to avoid the wrong hidraw node
                if (strstr(line, "MODALIAS=") && strstr(line, TARGET_MODALIAS)) {
                    match_name = true;
                }
            }
            fclose(f);

            if (match_vid && match_pid && match_name) {
                snprintf(path, sizeof(path), "/dev/%s", ent->d_name);
                closedir(dir);
                return path;
            }
        }
    }
    closedir(dir);
    return NULL;
}

// Send positional limits
void setup_abs(int fd, unsigned int chan, int min, int max) {
    struct uinput_abs_setup abs = {0};
    abs.code = chan;
    abs.absinfo.minimum = min;
    abs.absinfo.maximum = max;
    ioctl(fd, UI_ABS_SETUP, &abs);
}

// Make virtual touchscreen devices with all the right parameters set
int create_virtual_device(const char* name) {
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    ioctl(fd, UI_SET_KEYBIT, BTN_TOUCH);
    ioctl(fd, UI_SET_EVBIT, EV_ABS);
    ioctl(fd, UI_SET_PROPBIT, INPUT_PROP_DIRECT);

    ioctl(fd, UI_SET_ABSBIT, ABS_MT_SLOT);
    ioctl(fd, UI_SET_ABSBIT, ABS_MT_TRACKING_ID);
    ioctl(fd, UI_SET_ABSBIT, ABS_MT_POSITION_X);
    ioctl(fd, UI_SET_ABSBIT, ABS_MT_POSITION_Y);
    ioctl(fd, UI_SET_ABSBIT, ABS_X);
    ioctl(fd, UI_SET_ABSBIT, ABS_Y);

    setup_abs(fd, ABS_X, 0, MAX_X);
    setup_abs(fd, ABS_Y, 0, MAX_Y);
    setup_abs(fd, ABS_MT_SLOT, 0, MAX_SLOTS - 1);
    setup_abs(fd, ABS_MT_TRACKING_ID, 0, 65535);
    setup_abs(fd, ABS_MT_POSITION_X, 0, MAX_X);
    setup_abs(fd, ABS_MT_POSITION_Y, 0, MAX_Y);

    struct uinput_setup us = {0};
    strncpy(us.name, name, UINPUT_MAX_NAME_SIZE);
    ioctl(fd, UI_DEV_SETUP, &us);
    ioctl(fd, UI_DEV_CREATE);
    return fd;
}

// Wrapper for emitting events
void emit(int fd, int type, int code, int val) {
    struct input_event ev = {0};
    ev.type = type; ev.code = code; ev.value = val;
    write(fd, &ev, sizeof(ev));
}

int main() {
    // Create virtual input devices for top and bottom screen
    int fds[2];
    fds[0] = create_virtual_device("Yoga Top Multitouch");
    fds[1] = create_virtual_device("Yoga Bottom Multitouch");

    // An eight-finger touch USB-resets the device, so an outer loop to keep re-finding it if that happens
    while (1) {
        // Find the HID device
        char* dev_path = find_device();
        if (!dev_path) {
            printf("Waiting for Yoga Book Ingenic device...\n");
            sleep(2);
            continue;
        }
        int raw_fd = open(dev_path, O_RDONLY);
        if (raw_fd < 0) {
            sleep(1);
            continue;
        }
        printf("Connected to %s. Tracking active.\n", dev_path);

        unsigned char buf[128];
        struct pollfd pfd = { .fd = raw_fd, .events = POLLIN };

        while (1) {
            int poll_ret = poll(&pfd, 1, 100);  // 100ms poll timeout
            if (poll_ret < 0) break;

            // Check for timed-out slots on both screens
            long long now = get_time_ms();
            for (int dev_idx = 0; dev_idx < 2; dev_idx++) {
                bool any_timed_out = false;
                for (int slot = 0; slot < MAX_SLOTS; slot++) {
                    if (slot_active[dev_idx][slot]) {
                        if (now - slot_last_update[dev_idx][slot] > SLOT_TIMEOUT_MS) {
                            emit(fds[dev_idx], EV_ABS, ABS_MT_SLOT, slot);
                            emit(fds[dev_idx], EV_ABS, ABS_MT_TRACKING_ID, -1);
                            printf("Finger %i, Screen %i: Timeout lifted\n", slot, dev_idx);
                            slot_active[dev_idx][slot] = false;
                            any_timed_out = true;
                        }
                    }
                }
                // Send SYN_REPORT if any slots timed out
                if (any_timed_out) {
                    bool still_active = false;
                    for (int slot = 0; slot < MAX_SLOTS; slot++)
                        if (slot_active[dev_idx][slot]) still_active = true;
                    emit(fds[dev_idx], EV_KEY, BTN_TOUCH, still_active ? 1 : 0);
                    emit(fds[dev_idx], EV_SYN, SYN_REPORT, 0);
                }
            }

            if (poll_ret == 0) continue;  // Timeout, no data
            if (!(pfd.revents & POLLIN)) break;

            if (read(raw_fd, buf, sizeof(buf)) <= 0) break;
            int dev_idx = -1;
            if (buf[0] == 0x30) dev_idx = 0;      // Top Screen
            else if (buf[0] == 0x38) dev_idx = 1; // Bottom Screen

            if (dev_idx != -1) {
                int current_fd = fds[dev_idx];
                int contact_count = buf[55];
                int contacts_seen = 0;
                bool global_touch = false;

                for (int i = 0; i < MAX_SLOTS; i++) {
                    int offset = 1 + (i * 5);
                    unsigned char status = buf[offset];

                    // Bit 0 is Tip Switch, Bits 3-7 are Contact ID
                    bool tip_switch = status & 0x01;
                    int contact_id = (status >> 3);

                    // We use the hardware's Contact ID to select the MT Slot
                    // Most firmwares use 1-based IDs, we normalize to 0-based
                    int slot = (contact_id > 0) ? (contact_id - 1) : i;
                    if (slot >= MAX_SLOTS) continue;

                    emit(current_fd, EV_ABS, ABS_MT_SLOT, slot);

                    if (tip_switch) {
                        // The device is a bit odd - as well as ignoring any active slots beyond contact_count,
                        // we also have to specifically only trust slot 1 if contact_count is 1.
                        contacts_seen++;
                        if (contacts_seen > contact_count) continue;
                        if (contact_count == 1 && slot > 0) continue;

                        // Read coords out of the slot
                        int x = buf[offset + 1] | (buf[offset + 2] << 8);
                        int y = buf[offset + 3] | (buf[offset + 4] << 8);

                        // Top digitizer is inverted compared to the screen
                        if (dev_idx == 0) {
                            x = MAX_X - x;
                            y = MAX_Y - y;
                        }

                        // Pass through the slot IDs as the tracking IDs to the event subsystem
                        if (!slot_active[dev_idx][slot]) {
                            emit(current_fd, EV_ABS, ABS_MT_TRACKING_ID, slot + (dev_idx * 10) + 1);
                            slot_active[dev_idx][slot] = true;
                        }
                        slot_last_update[dev_idx][slot] = get_time_ms();
                        emit(current_fd, EV_ABS, ABS_MT_POSITION_X, x);
                        emit(current_fd, EV_ABS, ABS_MT_POSITION_Y, y);
                        if (DEBUG) printf("Finger %i, CID %i, CC %i, Screen %i: X %i Y %i\n", slot, contact_id, contact_count, dev_idx, x, y);

                        // Touches in the first slot should also emit non-multitouch position events
                        if (slot == 0) {
                            emit(current_fd, EV_ABS, ABS_X, x);
                            emit(current_fd, EV_ABS, ABS_Y, y);
                        }
                        global_touch = true;
                    } else {
                        if (slot_active[dev_idx][slot]) {
                            emit(current_fd, EV_ABS, ABS_MT_TRACKING_ID, -1);
                            if (DEBUG) printf("Finger %i, CID %i, CC %i, Screen %i: Lifted\n", slot, contact_id, contact_count, dev_idx);
                            slot_active[dev_idx][slot] = false;
                        }
                    }
                }
                emit(current_fd, EV_KEY, BTN_TOUCH, global_touch ? 1 : 0);
                emit(current_fd, EV_SYN, SYN_REPORT, 0);
            }
        }
    }
    return 0;
}
