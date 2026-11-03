// SPDX-FileCopyrightText: 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0

#include "Nicm.h"

#include <cerrno>
#include <cstring>
#include <dlfcn.h>

namespace {

constexpr char kDeviceName[] = "rmnet_ipa0";
constexpr unsigned int kExtendedIoctl = 0x89fd;
constexpr char kExtendedIoctlSymbol[] =
        "_ZN4Nicm14PhysicalDevice17nicmExtendedIoctlEP22rmnet_ioctl_extended_sj";
constexpr char kSetLinkLayerProtocolSymbol[] =
        "_ZN4Nicm14PhysicalDevice20setLinkLayerProtocolEi";
constexpr char kOpenPortSymbol[] = "_ZN4Nicm14PhysicalDevice8openPortEv";

enum ExtendedIoctl : uint32_t {
    GET_SUPPORTED_FEATURES = 0x00,
    SET_MRU = 0x01,
    GET_EPID = 0x03,
    GET_DRIVER_NAME = 0x04,
    GET_EP_PAIR = 0x10,
    SET_SLEEP_STATE = 0x14,
    SET_XLAT_DEV_INFO = 0x15,
    GET_SG_SUPPORT = 0x17,
    SET_MTU = 0x20,
    GET_EPID_LL = 0x21,
};

int fail(int error) {
    errno = error;
    return -1;
}

template <const char* Symbol, typename... Args>
int callNext(Args... args) {
    using Function = int (*)(Args...);
    static const auto original = reinterpret_cast<Function>(dlsym(RTLD_NEXT, Symbol));
    return original ? original(args...) : fail(ENOSYS);
}

}

namespace Nicm {

int PhysicalDevice::nicmExtendedIoctl(rmnet_ioctl_extended_s* data, unsigned int request) {
    if (getDeviceName() != kDeviceName) {
        return callNext<kExtendedIoctlSymbol>(this, data, request);
    }

    if (request != kExtendedIoctl) {
        return fail(EOPNOTSUPP);
    }
    if (!data) {
        return fail(EFAULT);
    }

    switch (data->extended_ioctl) {
        case GET_SUPPORTED_FEATURES:
            data->u.data = 0;
            break;
        case GET_SG_SUPPORT:
        case GET_EPID:
            data->u.data = 1;
            break;
        case GET_EPID_LL:
            data->u.data = 5;
            break;
        case GET_EP_PAIR:
            data->u.ipa_ep_pair.consumer_pipe_num = 2;
            data->u.ipa_ep_pair.producer_pipe_num = 24;
            break;
        case GET_DRIVER_NAME:
            std::strncpy(data->u.if_name, kDeviceName, sizeof(data->u.if_name));
            break;
        case SET_MRU:
        case SET_SLEEP_STATE:
        case SET_XLAT_DEV_INFO:
        case SET_MTU:
            break;
        default:
            return fail(EINVAL);
    }

    return 0;
}

int PhysicalDevice::setLinkLayerProtocol(int protocol) {
    if (getDeviceName() != kDeviceName) {
        return callNext<kSetLinkLayerProtocolSymbol>(this, protocol);
    }

    return protocol == 2 ? 0 : fail(EINVAL);
}

int PhysicalDevice::openPort() {
    if (getDeviceName() != kDeviceName) {
        return callNext<kOpenPortSymbol>(this);
    }

    return 0;
}

}
