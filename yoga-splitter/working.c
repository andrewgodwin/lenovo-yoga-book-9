#include <linux/uinput.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

// Coordinate Maxima from your HID Decode
#define MAX_X 11440
#define MAX_Y 7148

void setup_abs(int fd, unsigned int chan, int max) {
    struct uinput_abs_setup abs = {0};
    abs.code = chan;
    abs.absinfo.minimum = 0;
    abs.absinfo.maximum = max;
    ioctl(fd, UI_ABS_SETUP, &abs);
}

int create_virtual_device(const char* name) {
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    ioctl(fd, UI_SET_KEYBIT, BTN_TOUCH);
    ioctl(fd, UI_SET_EVBIT, EV_ABS);
    ioctl(fd, UI_SET_PROPBIT, INPUT_PROP_DIRECT);

    // Standard and Multi-Touch axes
    ioctl(fd, UI_SET_ABSBIT, ABS_X);
    ioctl(fd, UI_SET_ABSBIT, ABS_Y);
    //ioctl(fd, UI_SET_ABSBIT, ABS_MT_POSITION_X);
    //ioctl(fd, UI_SET_ABSBIT, ABS_MT_POSITION_Y);

    setup_abs(fd, ABS_X, MAX_X);
    setup_abs(fd, ABS_Y, MAX_Y);
    //setup_abs(fd, ABS_MT_POSITION_X, MAX_X);
    //setup_abs(fd, ABS_MT_POSITION_Y, MAX_Y);

    struct uinput_setup us = {0};
    strncpy(us.name, name, UINPUT_MAX_NAME_SIZE);
    ioctl(fd, UI_DEV_SETUP, &us);
    ioctl(fd, UI_DEV_CREATE);
    return fd;
}

void emit(int fd, int type, int code, int val) {
    struct input_event ev = {0};
    ev.type = type; ev.code = code; ev.value = val;
    write(fd, &ev, sizeof(ev));
}

int main() {
    // Note: Change /dev/hidraw5 to your current node if it changed
    int raw_fd = open("/dev/hidraw2", O_RDONLY);
    if (raw_fd < 0) { perror("Failed to open hidraw"); return 1; }

    int top_fd = create_virtual_device("Yoga Top Touch");
    int bot_fd = create_virtual_device("Yoga Bottom Touch");

    unsigned char buf[128]; // Larger buffer to accommodate the 87-byte report
    printf("Yoga Book 9i Bridge Active. Monitoring hidraw...\n");

    while (read(raw_fd, buf, sizeof(buf)) > 0) {
        int target_fd = 0;
        if (buf[0] == 48) target_fd = top_fd;      // 0x30
        else if (buf[0] == 56) target_fd = bot_fd; // 0x38

        if (target_fd) {
            // Updated Offsets based on your hid-recorder output
            int touch = buf[1] & 0x01;
            int x = buf[2] | (buf[3] << 8);
            int y = buf[4] | (buf[5] << 8);

            // Sync both Standard and MT coordinates for compatibility
            emit(target_fd, EV_ABS, ABS_X, x);
            emit(target_fd, EV_ABS, ABS_Y, y);
            //emit(target_fd, EV_ABS, ABS_MT_POSITION_X, x);
            //emit(target_fd, EV_ABS, ABS_MT_POSITION_Y, y);
            emit(target_fd, EV_KEY, BTN_TOUCH, touch);
            emit(target_fd, EV_SYN, SYN_REPORT, 0);
        }
    }
    return 0;
}
