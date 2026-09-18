/*
 * SPDX-FileCopyrightText: 2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.settings.trigger

import android.content.Context
import org.lineageos.settings.utils.*

object TriggerController {

    const val KEY_TRIGGER_ENABLE = "trigger_enable"

    const val TRIGGER_BUTTON1_ENABLE_NODE = "/proc/nubia_key/sar0/mode_operation"
    const val TRIGGER_BUTTON2_ENABLE_NODE = "/proc/nubia_key/sar1/mode_operation"

    const val TRIGGER_SLEEP_MODE = "2"
    const val TRIGGER_WAKE_MODE = "1"

    /*
     * Enable or disable the trigger buttons
     * @return puts the value in place
     */
    fun setTriggerEnabled(context: Context, enabled: Boolean) {
        putInt(context, KEY_TRIGGER_ENABLE, if (enabled) 1 else 0)
        writeLine(
            TRIGGER_BUTTON1_ENABLE_NODE,
            if (enabled) TRIGGER_WAKE_MODE else TRIGGER_SLEEP_MODE,
        )
        writeLine(
            TRIGGER_BUTTON2_ENABLE_NODE,
            if (enabled) TRIGGER_WAKE_MODE else TRIGGER_SLEEP_MODE,
        )
    }

    /*
     * Restore settings on boot / resume
     * @return puts the value in place
     */
    fun restoreSettings(context: Context) {
        val triggerEnabled = getInt(context, KEY_TRIGGER_ENABLE, 0) == 1

        setTriggerEnabled(context, triggerEnabled)
    }
}
