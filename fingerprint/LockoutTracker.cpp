//
// SPDX-FileCopyrightText: 2025 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
//

#include "LockoutTracker.h"

#include <util/Util.h>

#include <algorithm>

namespace aidl::android::hardware::biometrics::fingerprint {

void LockoutTracker::reset(bool dueToTimeout) {
    if (!dueToTimeout) {
        mFailedCount = 0;
    }
    mFailedCountTimed = 0;
    mLockoutTimedStart = 0;
    mCurrentMode = LockoutMode::kNone;
}

void LockoutTracker::addFailedAttempt() {
    mFailedCount++;
    mFailedCountTimed++;
    if (mFailedCount >= LOCKOUT_PERMANENT_THRESHOLD) {
        mCurrentMode = LockoutMode::kPermanent;
    } else if (mFailedCountTimed >= LOCKOUT_TIMED_THRESHOLD) {
        if (mCurrentMode == LockoutMode::kNone) {
            mCurrentMode = LockoutMode::kTimed;
            mLockoutTimedStart = Util::getSystemNanoTime();
        }
    }
}

LockoutTracker::LockoutMode LockoutTracker::getMode() {
    if (mCurrentMode == LockoutMode::kTimed) {
        if (Util::hasElapsed(mLockoutTimedStart, LOCKOUT_TIMED_DURATION)) {
            mCurrentMode = LockoutMode::kNone;
            mLockoutTimedStart = 0;
        }
    }

    return mCurrentMode;
}

int64_t LockoutTracker::getLockoutTimeLeft() {
    int64_t res = 0;

    if (mLockoutTimedStart > 0) {
        auto now = Util::getSystemNanoTime();
        auto elapsed = (now - mLockoutTimedStart) / 1000000LL;
        res = std::max<int64_t>(LOCKOUT_TIMED_DURATION - elapsed, 0);
        LOG(INFO) << "elapsed=" << elapsed << " now = " << now
                  << " mLockoutTimedStart=" << mLockoutTimedStart << " res=" << res;
    }

    return res;
}

}  // namespace aidl::android::hardware::biometrics::fingerprint
