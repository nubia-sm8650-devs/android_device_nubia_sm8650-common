/*
 * SPDX-FileCopyrightText: 2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "LockoutTracker.h"
#include "fingerprint.h"

#include <aidl/android/hardware/biometrics/fingerprint/ISessionCallback.h>
#include <android/binder_to_string.h>

#include <atomic>
#include <condition_variable>
#include <future>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace aidl::android::hardware::biometrics::fingerprint {

class FingerprintEngine {
  public:
    FingerprintEngine();
    ~FingerprintEngine();

    void setSessionCallback(ISessionCallback* cb);
    void setActiveGroup(int userId);

    void generateChallengeImpl();
    void revokeChallengeImpl(int64_t challenge);

    void enrollImpl(const keymaster::HardwareAuthToken& hat, const std::future<void>& cancel);
    void authenticateImpl(int64_t operationId, const std::future<void>& cancel);
    void detectInteractionImpl(const std::future<void>& cancel);

    void enumerateEnrollmentsImpl();
    void removeEnrollmentsImpl(const std::vector<int32_t>& enrollmentIds);

    void getAuthenticatorIdImpl();
    void invalidateAuthenticatorIdImpl();

    void resetLockoutImpl(const keymaster::HardwareAuthToken& /*hat*/);

    ndk::ScopedAStatus onPointerDownImpl(int32_t pointerId, int32_t x, int32_t y, float minor,
                                         float major);
    ndk::ScopedAStatus onPointerUpImpl(int32_t pointerId);
    ndk::ScopedAStatus onUiReadyImpl();

  protected:
    bool handleAcquiredOrErrorMessage(fingerprint_msg_t& msg, bool& exit);

    static void onMessageWrapper(const fingerprint_msg_t* msg);
    void fodUiThreadLoop();
    void notifyFodUi(bool ready);
    void onMessage(const fingerprint_msg_t* msg);
    std::thread waitForCancel(const std::future<void>& cancel, std::atomic<bool>& stopFlag);
    void discardStaleMessages();
    fingerprint_msg_t popMessage();
    fingerprint_msg_t waitForMessage();
    fingerprint_msg_t waitForReply();

    void printError(std::pair<Error, int32_t> ec);
    std::pair<Error, int32_t> convertError(int32_t error);
    std::pair<AcquiredInfo, int32_t> convertAcquiredInfo(int32_t info);

    std::mutex mMutex;
    std::mutex mMessageMutex;
    std::condition_variable mMessageCond;
    std::queue<fingerprint_msg_t> mMessageQueue;

    ISessionCallback* mCb;

    std::thread mFodUiThread;
    std::atomic<bool> mFodUiThreadStop{false};

  private:
    fingerprint_device_t* openHal(const char* class_name, const char* module_id);
    fingerprint_device_t* mDevice;

  protected:
    void clearLockout(bool dueToTimeout = false);
    bool checkSensorLockout();
    void lockoutTimerExpired();
    void startLockoutTimer(int64_t timeout);
    bool isLockoutTimerSupported;
    std::atomic<bool> isLockoutTimerStarted;
    std::atomic<bool> isLockoutTimerAborted;

    LockoutTracker mLockoutTracker;
};

}  // namespace aidl::android::hardware::biometrics::fingerprint
