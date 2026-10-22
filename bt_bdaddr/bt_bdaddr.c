/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "bt_bdaddr"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <log/log.h>

#define BTPROTO_HCI 1
#define HCI_DEV_NONE 0xffff
#define HCI_CHANNEL_CONTROL 3

#define MGMT_OP_SET_PUBLIC_ADDRESS 0x0039
#define MGMT_OP_READ_UNCONF_INDEX_LIST 0x0036
#define MGMT_EV_CMD_COMPLETE 0x0001
#define MGMT_EV_CMD_STATUS 0x0002

#define BT_NV_SIZE 9
#define BT_NV_ADDR_LEN 6
#define WAIT_TRIES 120
#define WAIT_MS 500

struct sockaddr_hci {
    sa_family_t hci_family;
    uint16_t hci_dev;
    uint16_t hci_channel;
};

struct mgmt_hdr {
    uint16_t opcode;
    uint16_t index;
    uint16_t len;
} __attribute__((packed));

static int read_bt_nv(const char* path, uint8_t addr[BT_NV_ADDR_LEN]) {
    uint8_t buf[BT_NV_SIZE];
    ssize_t len;
    int fd;

    fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        ALOGE("open %s: %s", path, strerror(errno));
        return -1;
    }
    len = read(fd, buf, sizeof(buf));
    close(fd);

    if (len != BT_NV_SIZE || buf[2] != BT_NV_ADDR_LEN) {
        ALOGE("%s holds no address", path);
        return -1;
    }

    memcpy(addr, buf + 3, BT_NV_ADDR_LEN);
    for (int i = 0; i < BT_NV_ADDR_LEN; i++)
        if (addr[i])
            return 0;

    ALOGE("%s holds an empty address", path);
    return -1;
}

/* Send a command and return the status of its completion, or -1. */
static int mgmt_cmd(int sk, uint16_t opcode, uint16_t index, const void* param, uint16_t plen,
                    void* rsp, size_t* rlen) {
    uint8_t buf[512];
    struct mgmt_hdr* hdr = (struct mgmt_hdr*)buf;

    hdr->opcode = opcode;
    hdr->index = index;
    hdr->len = plen;
    if (plen)
        memcpy(buf + sizeof(*hdr), param, plen);

    if (write(sk, buf, sizeof(*hdr) + plen) < 0) {
        ALOGE("mgmt write: %s", strerror(errno));
        return -1;
    }

    for (;;) {
        struct pollfd pfd = {.fd = sk, .events = POLLIN};
        ssize_t len;
        uint8_t* ev;

        if (poll(&pfd, 1, 2000) <= 0)
            return -1;

        len = read(sk, buf, sizeof(buf));
        if (len < (ssize_t)(sizeof(*hdr) + 3))
            continue;

        ev = buf + sizeof(*hdr);
        if ((hdr->opcode != MGMT_EV_CMD_COMPLETE && hdr->opcode != MGMT_EV_CMD_STATUS) ||
            (ev[0] | ev[1] << 8) != opcode)
            continue;

        if (rsp && rlen) {
            size_t n = len - sizeof(*hdr) - 3;

            *rlen = n < *rlen ? n : *rlen;
            memcpy(rsp, ev + 3, *rlen);
        }
        return ev[2];
    }
}

int main(int argc, char** argv) {
    struct sockaddr_hci sa = {
            .hci_family = AF_BLUETOOTH,
            .hci_dev = HCI_DEV_NONE,
            .hci_channel = HCI_CHANNEL_CONTROL,
    };
    uint8_t addr[BT_NV_ADDR_LEN];
    int sk;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <bt_nv.bin>\n", argv[0]);
        return 1;
    }

    if (read_bt_nv(argv[1], addr))
        return 1;

    sk = socket(AF_BLUETOOTH, SOCK_RAW | SOCK_CLOEXEC, BTPROTO_HCI);
    if (sk < 0 || bind(sk, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
        ALOGE("mgmt socket: %s", strerror(errno));
        return 1;
    }

    for (int i = 0; i < WAIT_TRIES; i++) {
        uint8_t list[2 + 2 * 16];
        size_t len = sizeof(list);
        uint16_t count;

        if (mgmt_cmd(sk, MGMT_OP_READ_UNCONF_INDEX_LIST, HCI_DEV_NONE, NULL, 0, list, &len) == 0 &&
            len >= 2) {
            count = list[0] | list[1] << 8;
            if (count && len >= 4) {
                uint16_t index = list[2] | list[3] << 8;
                int status = mgmt_cmd(sk, MGMT_OP_SET_PUBLIC_ADDRESS, index, addr,
                                      sizeof(addr), NULL, NULL);

                ALOGI("hci%u address %02x:%02x:%02x:%02x:%02x:%02x status %d", index, addr[5],
                      addr[4], addr[3], addr[2], addr[1], addr[0], status);
                close(sk);
                return status ? 1 : 0;
            }
        }

        usleep(WAIT_MS * 1000);
    }

    ALOGI("no unconfigured controller");
    close(sk);
    return 0;
}
