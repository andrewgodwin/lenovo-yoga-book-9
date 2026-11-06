#include <linux/uinput.h>
#include <linux/netlink.h>
#include <linux/genetlink.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <poll.h>

// The EC's BKBD field: bits 4-5 of this byte
#define BKBD_ADDRESS 0xFE0B0423
#define BKBD_SHIFT 4
#define BKBD_MASK 0x3
#define PAGE_MASK 0xFFF

#define ACPI_FAMILY "acpi_event"
#define ACPI_GROUP "acpi_mc_group"

// Events can get missed around suspend, so check occasionally anyway
#define RECHECK_MS 5000

// Map the byte containing BKBD
volatile uint8_t *map_position()
{
    int fd = open("/dev/mem", O_RDONLY | O_SYNC);
    if (fd < 0)
        return NULL;
    void *page = mmap(NULL, PAGE_MASK + 1, PROT_READ, MAP_SHARED, fd, BKBD_ADDRESS & ~PAGE_MASK);
    close(fd);
    if (page == MAP_FAILED)
        return NULL;
    return (volatile uint8_t *)page + (BKBD_ADDRESS & PAGE_MASK);
}

// Look through a netlink message's attributes for one type
struct nlattr *find_attr(char *start, char *end, int type)
{
    while (start + NLA_HDRLEN <= end)
    {
        struct nlattr *attr = (struct nlattr *)start;
        if (attr->nla_len < NLA_HDRLEN)
            return NULL;
        if ((attr->nla_type & NLA_TYPE_MASK) == type)
            return attr;
        start += NLA_ALIGN(attr->nla_len);
    }
    return NULL;
}

// Get a netlink socket that wakes up on every ACPI event. The ACPI events
// group has to be looked up by name, so ask the generic netlink controller.
int open_acpi_events()
{
    int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_GENERIC);
    struct sockaddr_nl addr = {.nl_family = AF_NETLINK};
    if (fd < 0 || bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        return -1;

    struct
    {
        struct nlmsghdr nl;
        struct genlmsghdr genl;
        struct nlattr attr;
        char name[sizeof(ACPI_FAMILY)];
    } request = {0};
    request.nl.nlmsg_len = NLMSG_LENGTH(GENL_HDRLEN + NLA_HDRLEN + sizeof(ACPI_FAMILY));
    request.nl.nlmsg_type = GENL_ID_CTRL;
    request.nl.nlmsg_flags = NLM_F_REQUEST;
    request.genl.cmd = CTRL_CMD_GETFAMILY;
    request.genl.version = 1;
    request.attr.nla_type = CTRL_ATTR_FAMILY_NAME;
    request.attr.nla_len = NLA_HDRLEN + sizeof(ACPI_FAMILY);
    strcpy(request.name, ACPI_FAMILY);
    if (send(fd, &request, request.nl.nlmsg_len, 0) < 0)
        return -1;

    char buf[4096];
    int len = recv(fd, buf, sizeof(buf), 0);
    struct nlmsghdr *reply = (struct nlmsghdr *)buf;
    if (len < (int)NLMSG_LENGTH(GENL_HDRLEN) || !NLMSG_OK(reply, len) || reply->nlmsg_type == NLMSG_ERROR)
        return -1;

    char *end = buf + reply->nlmsg_len;
    struct nlattr *groups = find_attr(buf + NLMSG_LENGTH(GENL_HDRLEN), end, CTRL_ATTR_MCAST_GROUPS);
    if (!groups)
        return -1;

    // Each group is its own nested set of attributes
    char *group = (char *)groups + NLA_HDRLEN;
    char *groups_end = (char *)groups + groups->nla_len;
    while (group + NLA_HDRLEN <= groups_end)
    {
        struct nlattr *entry = (struct nlattr *)group;
        if (entry->nla_len < NLA_HDRLEN)
            break;
        char *entry_end = group + entry->nla_len;
        struct nlattr *name = find_attr(group + NLA_HDRLEN, entry_end, CTRL_ATTR_MCAST_GRP_NAME);
        struct nlattr *id = find_attr(group + NLA_HDRLEN, entry_end, CTRL_ATTR_MCAST_GRP_ID);
        if (name && id && strcmp((char *)name + NLA_HDRLEN, ACPI_GROUP) == 0)
        {
            uint32_t group_id = *(uint32_t *)((char *)id + NLA_HDRLEN);
            if (setsockopt(fd, SOL_NETLINK, NETLINK_ADD_MEMBERSHIP, &group_id, sizeof(group_id)) < 0)
                return -1;
            return fd;
        }
        group += NLA_ALIGN(entry->nla_len);
    }
    return -1;
}

// Make a virtual device with one axis holding the position. An axis means the
// kernel remembers the value for us, and it has none of the side effects of
// switches (for example, logind stops suspending on lid close if SW_DOCK is on)
int create_virtual_device(const char *name)
{
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0)
        return -1;
    ioctl(fd, UI_SET_EVBIT, EV_ABS);
    ioctl(fd, UI_SET_ABSBIT, ABS_MISC);

    struct uinput_abs_setup abs = {0};
    abs.code = ABS_MISC;
    abs.absinfo.maximum = BKBD_MASK;
    ioctl(fd, UI_ABS_SETUP, &abs);

    struct uinput_setup us = {0};
    strncpy(us.name, name, UINPUT_MAX_NAME_SIZE - 1);
    ioctl(fd, UI_DEV_SETUP, &us);
    if (ioctl(fd, UI_DEV_CREATE) < 0)
        return -1;
    return fd;
}

// Wrapper for emitting events
void emit(int fd, int type, int code, int val)
{
    struct input_event ev = {0};
    ev.type = type;
    ev.code = code;
    ev.value = val;
    write(fd, &ev, sizeof(ev));
}

int main()
{
    setlinebuf(stdout);

    volatile uint8_t *bkbd = map_position();
    if (!bkbd)
    {
        perror("Could not map EC memory");
        return 1;
    }
    int events_fd = open_acpi_events();
    if (events_fd < 0)
    {
        printf("Could not listen for ACPI events\n");
        return 1;
    }
    int device_fd = create_virtual_device("Yoga Keyboard Position");
    if (device_fd < 0)
    {
        perror("Could not create input device");
        return 1;
    }

    int position = -1;
    struct pollfd pfd = {.fd = events_fd, .events = POLLIN};
    while (1)
    {
        int value = (*bkbd >> BKBD_SHIFT) & BKBD_MASK;
        if (value != position)
        {
            position = value;
            emit(device_fd, EV_ABS, ABS_MISC, position);
            emit(device_fd, EV_SYN, SYN_REPORT, 0);
            printf("Keyboard position: %i\n", position);
        }

        // Rereading is cheap, so don't bother working out which event this is
        if (poll(&pfd, 1, RECHECK_MS) > 0)
        {
            char buf[4096];
            recv(events_fd, buf, sizeof(buf), MSG_DONTWAIT);
        }
    }
    return 0;
}
