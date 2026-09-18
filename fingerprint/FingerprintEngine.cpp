/*
 * SPDX-FileCopyrightText: 2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "FingerprintEngine.h"
#include "Legacy2Aidl.h"
#include "fingerprint.h"

#include <android-base/logging.h>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <chrono>
#include <mutex>

using namespace std::chrono;

namespace aidl::android::hardware::biometrics::fingerprint {

static FingerprintEngine* sInstance;

namespace {
constexpr const char* FOD_UI_PATH = "/sys/devices/platform/soc/soc:qcom,dsi-display-primary/fod_ui";

constexpr int FOD_UI_POLL_TIMEOUT_MS = 1000;
constexpr int FOD_UI_READY_COMMAND = 30;

static bool readBool(int fd, bool fallback) {
    char c;

    if (lseek(fd, 0, SEEK_SET) < 0) {
        PLOG(ERROR) << "failed to seek fod_ui";
        return fallback;
    }

    if (read(fd, &c, sizeof(char)) != 1) {
        PLOG(ERROR) << "failed to read fod_ui";
        return fallback;
    }

    return c != '0';
}

};  // namespace

FingerprintEngine::FingerprintEngine() : mDevice(openHal(nullptr, "fingerprint.gf95xx")) {
    sInstance = this;

    mFodUiThread = std::thread([this]() { fodUiThreadLoop(); });
}

void FingerprintEngine::notifyFodUi(bool ready) {
    if (mDevice == nullptr) {
        return;
    }

    LOG(INFO) << "fodUiReady: " << ready;

    int error = mDevice->sendCustomizedCommand(mDevice, FOD_UI_READY_COMMAND, ready);
    if (error) {
        LOG(ERROR) << "sendCustomizedCommand failed: " << error;
    }
}

void FingerprintEngine::fodUiThreadLoop() {
    int fd = open(FOD_UI_PATH, O_RDONLY);
    if (fd < 0) {
        PLOG(ERROR) << "failed to open " << FOD_UI_PATH;
        return;
    }

    bool fodUiReady = readBool(fd, false);
    notifyFodUi(fodUiReady);

    struct pollfd fodUiPoll = {
            .fd = fd,
            .events = POLLERR | POLLPRI,
            .revents = 0,
    };

    while (!mFodUiThreadStop.load()) {
        fodUiPoll.revents = 0;

        int rc = poll(&fodUiPoll, 1, FOD_UI_POLL_TIMEOUT_MS);
        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            }
            PLOG(ERROR) << "failed to poll fod_ui, giving up";
            break;
        }

        if (rc == 0) {
            continue;
        }

        bool ready = readBool(fd, fodUiReady);
        if (ready == fodUiReady) {
            continue;
        }

        fodUiReady = ready;
        notifyFodUi(fodUiReady);
    }

    close(fd);
}

FingerprintEngine::~FingerprintEngine() {
    LOG(INFO) << __func__;

    mFodUiThreadStop = true;
    if (mFodUiThread.joinable()) {
        mFodUiThread.join();
    }

    if (mDevice == nullptr) {
        LOG(ERROR) << "No valid device";
        return;
    }
    int err;
    if (0 != (err = mDevice->common.close(reinterpret_cast<hw_device_t*>(mDevice)))) {
        LOG(ERROR) << "Can't close fingerprint module, error: " << err;
        return;
    }
    mDevice = nullptr;
}

void FingerprintEngine::setSessionCallback(ISessionCallback* cb) {
    std::unique_lock<std::mutex> lock(mMutex);

    mCb = cb;
}

fingerprint_device_t* FingerprintEngine::openHal(const char* class_name, const char* module_id) {
    int err;
    const hw_module_t* hw_mdl = nullptr;
    LOG(DEBUG) << "Opening fingerprint hal library...";
    if (0 != (err = hw_get_module_by_class(module_id, class_name, &hw_mdl))) {
        LOG(ERROR) << "Can't open fingerprint HW Module, error: " << err;
        return nullptr;
    }

    if (hw_mdl == nullptr) {
        LOG(ERROR) << "No valid fingerprint module";
        return nullptr;
    }

    fingerprint_module_t const* module = reinterpret_cast<const fingerprint_module_t*>(hw_mdl);
    if (module->common.methods->open == nullptr) {
        LOG(ERROR) << "No valid open method";
        return nullptr;
    }

    hw_device_t* device = nullptr;

    if (0 != (err = module->common.methods->open(hw_mdl, nullptr, &device))) {
        LOG(ERROR) << "Can't open fingerprint methods, error" << err;
        return nullptr;
    }

    fingerprint_device_t* fp_device = reinterpret_cast<fingerprint_device_t*>(device);

    if (0 != (err = fp_device->set_notify(fp_device, FingerprintEngine::onMessageWrapper))) {
        LOG(ERROR) << "Can't register fingerprint module callback, error: " << err;
        return nullptr;
    }

    return fp_device;
}

void FingerprintEngine::setActiveGroup(int userId) {
    LOG(INFO) << __func__;

    mDevice->setActiveGroup(mDevice, userId);
}

void FingerprintEngine::generateChallengeImpl() {
    LOG(INFO) << __func__;

    std::unique_lock<std::mutex> lock(mMutex);
    CHECK(mCb != nullptr);

    uint64_t error = mDevice->generateChallenge(mDevice);
    if (error) {
        auto ec = convertError(error);
        printError(ec);
        mCb->onError(Error::UNABLE_TO_PROCESS, 0 /* vendorError */);
        return;
    }

    while (true) {
        auto msg = waitForMessage();

        if (msg.type != FINGERPRINT_GENERATE_CHALLENGE) {
            LOG(ERROR) << "Unexpected message type: " << msg.type;
            continue;
        }

        LOG(INFO) << "onChallengeGenerated(challenge=" << msg.data.data << ")";

        mCb->onChallengeGenerated(msg.data.data);
        break;
    }
}

void FingerprintEngine::revokeChallengeImpl(int64_t challenge) {
    LOG(INFO) << __func__;

    std::unique_lock<std::mutex> lock(mMutex);
    CHECK(mCb != nullptr);

    uint64_t error = mDevice->revokeChallenge(mDevice, challenge);
    if (error) {
        auto ec = convertError(error);
        printError(ec);
        mCb->onError(Error::UNABLE_TO_PROCESS, 0 /* vendorError */);
        return;
    }

    while (true) {
        auto msg = waitForMessage();

        if (msg.type != FINGERPRINT_REVOKE_CHALLENGE) {
            LOG(ERROR) << "Unexpected message type: " << msg.type;
            continue;
        }

        LOG(INFO) << "onChallengeRevoked(challenge=" << msg.data.data << ")";

        mCb->onChallengeRevoked(msg.data.data);
        break;
    }
}

bool FingerprintEngine::handleAcquiredOrErrorMessage(fingerprint_msg_t& msg, bool& exit) {
    if (msg.type == FINGERPRINT_ERROR) {
        auto ec = convertError(msg.data.error);

        printError(ec);

        if (ec.first == Error::CANCELED) {
            mDevice->cancel();
            auto msg = waitForMessage();
            CHECK(msg.type == FINGERPRINT_ERROR);
            CHECK(msg.data.error == FINGERPRINT_ERROR_CANCELED);
        }

        exit = true;
        mCb->onError(ec.first, ec.second);
        return true;
    } else if (msg.type == FINGERPRINT_ACQUIRED) {
        auto ac = convertAcquiredInfo(msg.data.acquired.acquired_info);
        LOG(INFO) << "onAcquired(" << (int)ac.first << ", " << ac.second << ")";

        if (ac.first == AcquiredInfo::VENDOR) {
            return true;
        }

        mCb->onAcquired(ac.first, ac.second);
        return true;
    }

    return false;
}

std::thread FingerprintEngine::waitForCancel(const std::future<void>& cancel,
                                             std::atomic<bool>& stop) {
    return std::thread([&] {
        while (!stop.load()) {
            if (cancel.wait_for(1s) != std::future_status::ready) continue;

            LOG(INFO) << "Found cancel condition";
            fingerprint_msg_t msg = {
                    .type = FINGERPRINT_ERROR,
                    .data.error = FINGERPRINT_ERROR_CANCELED,
            };
            onMessage(&msg);

            return;
        }
    });
}

void FingerprintEngine::enrollImpl(const keymaster::HardwareAuthToken& hat,
                                   const std::future<void>& cancel) {
    LOG(INFO) << __func__;

    std::unique_lock<std::mutex> lock(mMutex);
    CHECK(mCb != nullptr);

    hw_auth_token_t authToken;
    translate(hat, authToken);
    int error = mDevice->enroll(mDevice, &authToken);
    if (error) {
        auto ec = convertError(error);
        printError(ec);
        mCb->onError(Error::UNABLE_TO_PROCESS, 0 /* vendorError */);
        return;
    }

    std::atomic<bool> stop;
    std::thread cancelThread = waitForCancel(cancel, stop);

    while (true) {
        auto msg = waitForMessage();

        bool exit = false;
        auto handled = handleAcquiredOrErrorMessage(msg, exit);
        if (exit) break;
        if (handled) continue;

        if (msg.type != FINGERPRINT_TEMPLATE_ENROLLING) {
            LOG(ERROR) << "Unexpected message type: " << msg.type;
            continue;
        }

        LOG(INFO) << "onEnrollResult(fid=" << msg.data.enroll.finger.fid
                  << ", rem=" << msg.data.enroll.samples_remaining << ")";

        mCb->onEnrollmentProgress(msg.data.enroll.finger.fid, msg.data.enroll.samples_remaining);

        if (!msg.data.enroll.samples_remaining) {
            break;
        }
    }

    stop.store(true);
    cancelThread.join();
}

void FingerprintEngine::authenticateImpl(int64_t operationId, const std::future<void>& cancel) {
    LOG(INFO) << __func__;

    std::unique_lock<std::mutex> lock(mMutex);
    CHECK(mCb != nullptr);

    int error = mDevice->authenticate(mDevice, operationId);
    if (error) {
        auto ec = convertError(error);
        printError(ec);
        mCb->onError(Error::UNABLE_TO_PROCESS, 0 /* vendorError */);
        return;
    }

    std::atomic<bool> stop;
    std::thread cancelThread = waitForCancel(cancel, stop);

    while (true) {
        auto msg = waitForMessage();

        bool exit = false;
        auto handled = handleAcquiredOrErrorMessage(msg, exit);
        if (exit) break;
        if (handled) continue;

        if (msg.type != FINGERPRINT_AUTHENTICATED) {
            LOG(ERROR) << "Unexpected message type: " << msg.type;
            continue;
        }

        LOG(INFO) << "onAuthenticated(fid=" << msg.data.authenticated.finger.fid << ")";

        if (msg.data.authenticated.finger.fid != 0) {
            const hw_auth_token_t hat = msg.data.authenticated.hat;
            keymaster::HardwareAuthToken authToken;
            translate(hat, authToken);
            mCb->onAuthenticationSucceeded(msg.data.authenticated.finger.fid, authToken);
            mLockoutTracker.reset(true);
            break;
        }

        mCb->onAuthenticationFailed();
        mLockoutTracker.addFailedAttempt();
        if (checkSensorLockout()) {
            break;
        }
    }

    stop.store(true);
    cancelThread.join();
}

void FingerprintEngine::detectInteractionImpl(const std::future<void>& /*cancel*/) {
    LOG(INFO) << __func__;
}

void FingerprintEngine::enumerateEnrollmentsImpl() {
    LOG(INFO) << __func__;

    std::unique_lock<std::mutex> lock(mMutex);
    CHECK(mCb != nullptr);

    std::vector<int32_t> enrollmentIds;

    int error = mDevice->enumerate(mDevice);
    if (error) {
        auto ec = convertError(error);
        printError(ec);
        mCb->onEnrollmentsEnumerated(enrollmentIds);
        return;
    }

    auto msg = waitForMessage();

    if (msg.type != FINGERPRINT_TEMPLATE_ENUMERATING) {
        LOG(ERROR) << "Unexpected message type: " << msg.type;
        return;
    }

    for (unsigned int i = 0; i < NUM_FINGERS; i++) {
        int32_t fid = msg.data.enumerated.fingers[i].fid;

        LOG(INFO) << "onEnumerate(i=" << i << ", fid=" << fid << ")";

        if (fid) {
            enrollmentIds.push_back(fid);
        }
    }

    mCb->onEnrollmentsEnumerated(enrollmentIds);
}

void FingerprintEngine::removeEnrollmentsImpl(const std::vector<int32_t>& enrollmentIds) {
    LOG(INFO) << __func__;

    std::unique_lock<std::mutex> lock(mMutex);
    CHECK(mCb != nullptr);

    int error = mDevice->remove(mDevice, enrollmentIds.data(), enrollmentIds.size());
    if (error) {
        auto ec = convertError(error);
        printError(ec);
        return;
    }

    std::vector<int32_t> removedEnrollmentIds;
    auto msg = waitForMessage();

    if (msg.type == FINGERPRINT_ERROR) {
        auto ec = convertError(msg.data.error);
        printError(ec);
        return;
    }

    if (msg.type != FINGERPRINT_TEMPLATE_REMOVED) {
        LOG(ERROR) << "Unexpected message type: " << msg.type;
        return;
    }

    for (unsigned int i = 0; i < NUM_FINGERS; i++) {
        int32_t fid = msg.data.enumerated.fingers[i].fid;

        LOG(INFO) << "onRemove(i=" << i << ", fid=" << fid << ")";

        if (fid) {
            removedEnrollmentIds.push_back(fid);
        }
    }

    mCb->onEnrollmentsRemoved(removedEnrollmentIds);
}

void FingerprintEngine::getAuthenticatorIdImpl() {
    LOG(INFO) << __func__;

    std::unique_lock<std::mutex> lock(mMutex);
    CHECK(mCb != nullptr);

    uint64_t error = mDevice->getAuthenticatorId(mDevice);
    if (error) {
        auto ec = convertError(error);
        printError(ec);
        mCb->onAuthenticatorIdRetrieved(0);
        return;
    }

    while (true) {
        auto msg = waitForMessage();

        if (msg.type != FINGERPRINT_GET_AUTHENTICATOR_ID) {
            LOG(ERROR) << "Unexpected message type: " << msg.type;
            continue;
        }

        LOG(INFO) << "onAuthenticatorIdRetrieved(authenticatorId=" << msg.data.data << ")";

        mCb->onAuthenticatorIdRetrieved(msg.data.data);
        break;
    }
}

void FingerprintEngine::invalidateAuthenticatorIdImpl() {
    LOG(INFO) << __func__;

    std::unique_lock<std::mutex> lock(mMutex);
    CHECK(mCb != nullptr);

    uint64_t error = mDevice->invalidateAuthenticatorId(mDevice);
    if (error) {
        auto ec = convertError(error);
        printError(ec);
        mCb->onAuthenticatorIdInvalidated(0);
        return;
    }

    while (true) {
        auto msg = waitForMessage();

        if (msg.type != FINGERPRINT_GET_AUTHENTICATOR_ID) {
            LOG(ERROR) << "Unexpected message type: " << msg.type;
            continue;
        }

        LOG(INFO) << "onAuthenticatorIdInvalidated(newAuthenticatorId=" << msg.data.data << ")";

        mCb->onAuthenticatorIdInvalidated(msg.data.data);
        break;
    }
}

void FingerprintEngine::resetLockoutImpl(const keymaster::HardwareAuthToken& /*hat*/) {
    LOG(INFO) << __func__;

    clearLockout();
    if (isLockoutTimerStarted) isLockoutTimerAborted = true;
}

ndk::ScopedAStatus FingerprintEngine::onPointerDownImpl(int32_t /*pointerId*/, int32_t /*x*/,
                                                        int32_t /*y*/, float /*minor*/,
                                                        float /*major*/) {
    LOG(INFO) << __func__;

    int error = mDevice->sendCustomizedCommand(mDevice, 10, 1);
    if (error) {
        auto ec = convertError(error);
        printError(ec);
        return ndk::ScopedAStatus::fromExceptionCode(EX_SERVICE_SPECIFIC);
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus FingerprintEngine::onPointerUpImpl(int32_t /*pointerId*/) {
    LOG(INFO) << __func__;

    int error = mDevice->sendCustomizedCommand(mDevice, 10, 0);
    if (error) {
        auto ec = convertError(error);
        printError(ec);
        return ndk::ScopedAStatus::fromExceptionCode(EX_SERVICE_SPECIFIC);
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus FingerprintEngine::onUiReadyImpl() {
    LOG(INFO) << __func__;
    return ndk::ScopedAStatus::ok();
}

void FingerprintEngine::printError(std::pair<Error, int32_t> ec) {
    LOG(ERROR) << "onError(" << (int)ec.first << ", " << ec.second << ")";
}

std::pair<Error, int32_t> FingerprintEngine::convertError(int32_t code) {
    std::pair<Error, int32_t> res;
    CHECK(code != FINGERPRINT_ERROR_LOCKOUT);
    if (code > FINGERPRINT_ERROR_VENDOR_BASE) {
        res.first = Error::VENDOR;
        res.second = code - FINGERPRINT_ERROR_VENDOR_BASE;
    } else {
        res.first = (Error)code;
        res.second = 0;
    }
    return res;
}

std::pair<AcquiredInfo, int32_t> FingerprintEngine::convertAcquiredInfo(int32_t code) {
    std::pair<AcquiredInfo, int32_t> res;
    if (code > FINGERPRINT_ACQUIRED_VENDOR_BASE) {
        res.first = AcquiredInfo::VENDOR;
        res.second = code - FINGERPRINT_ACQUIRED_VENDOR_BASE;
    } else if (code == FINGERPRINT_ACQUIRED_GOOD) {
        res.first = AcquiredInfo::GOOD;
        res.second = 0;
    } else {
        res.first = AcquiredInfo::UNKNOWN;
        res.second = 0;
    }
    return res;
}

fingerprint_msg_t FingerprintEngine::waitForMessage() {
    LOG(INFO) << __func__;

    std::unique_lock<std::mutex> lock(mMessageMutex);

    mMessageCond.wait(lock, [this] { return !mMessageQueue.empty(); });

    fingerprint_msg_t msg = mMessageQueue.front();
    LOG(INFO) << "Found message type: " << msg.type;
    mMessageQueue.pop();

    return msg;
}

void FingerprintEngine::onMessage(const fingerprint_msg_t* msg) {
    LOG(INFO) << __func__;
    LOG(INFO) << "Received message type: " << msg->type;
    CHECK(msg != nullptr);

    if (msg->type == GF_FINGERPRINT_BIG_DATA) {
        LOG(INFO) << "Skip message type: " << msg->type;
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mMessageMutex);
        mMessageQueue.push(*msg);
    }

    mMessageCond.notify_one();
}

void FingerprintEngine::onMessageWrapper(const fingerprint_msg_t* msg) {
    sInstance->onMessage(msg);
}

void FingerprintEngine::clearLockout(bool dueToTimeout) {
    std::unique_lock<std::mutex> lock(mMutex);
    CHECK(mCb != nullptr);

    mLockoutTracker.reset(dueToTimeout);
    mCb->onLockoutCleared();
}

bool FingerprintEngine::checkSensorLockout() {
    CHECK(mCb != nullptr);

    LockoutTracker::LockoutMode lockoutMode = mLockoutTracker.getMode();
    if (lockoutMode == LockoutTracker::LockoutMode::kPermanent) {
        LOG(ERROR) << "Fail: lockout permanent";
        mCb->onLockoutPermanent();
        isLockoutTimerAborted = true;
        return true;
    } else if (lockoutMode == LockoutTracker::LockoutMode::kTimed) {
        int64_t timeLeft = mLockoutTracker.getLockoutTimeLeft();
        LOG(ERROR) << "Fail: lockout timed " << timeLeft;
        mCb->onLockoutTimed(timeLeft);
        if (!isLockoutTimerStarted) startLockoutTimer(timeLeft);
        return true;
    }

    return false;
}

void FingerprintEngine::startLockoutTimer(int64_t timeout) {
    auto action = std::bind(&FingerprintEngine::lockoutTimerExpired, this);
    std::thread([this, timeout, action]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
        action();
    }).detach();

    isLockoutTimerStarted = true;
}

void FingerprintEngine::lockoutTimerExpired() {
    if (!isLockoutTimerAborted) {
        clearLockout(true);
    }
    isLockoutTimerStarted = false;
    isLockoutTimerAborted = false;
}

}  // namespace aidl::android::hardware::biometrics::fingerprint
