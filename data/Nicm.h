// SPDX-FileCopyrightText: 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

struct rmnet_ioctl_extended_s {
    uint32_t extended_ioctl;
    union {
        uint32_t data;
        char if_name[16];
        struct {
            uint32_t consumer_pipe_num;
            uint32_t producer_pipe_num;
        } ipa_ep_pair;
        uint8_t raw[20];
    } u;
};

static_assert(sizeof(rmnet_ioctl_extended_s) == 24);
static_assert(offsetof(rmnet_ioctl_extended_s, u) == 4);

namespace Nicm {

class NetDevice {
  public:
    std::string getDeviceName() const;
};

class PhysicalDevice : public NetDevice {
  public:
    int nicmExtendedIoctl(rmnet_ioctl_extended_s* data, unsigned int request);
    int setLinkLayerProtocol(int protocol);
    int openPort();
};

}
