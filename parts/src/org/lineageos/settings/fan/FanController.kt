/*
 * SPDX-FileCopyrightText: 2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.settings.fan

import android.content.Context
import org.lineageos.settings.utils.*

object FanController {

    const val KEY_FAN_ENABLE = "fan_enable"
    const val KEY_FAN_SPEED = "fan_speed_level"
    const val KEY_FAN_SPEED_RECEIVER = "fan_speed_receiver"

    const val FAN_ENABLE_NODE = "/sys/kernel/fan/fan_enable"
    const val FAN_SPEED_NODE = "/sys/kernel/fan/fan_speed_level"

    const val FAN_MIN_SPEED = 1
    const val FAN_MAX_SPEED = 5
    const val FAN_DEFAULT_SPEED = 3

    /*
     * Enable or disable the fan
     * @return puts the value in place
     */
    fun setFanEnabled(context: Context, enabled: Boolean) {
        putInt(context, KEY_FAN_ENABLE, if (enabled) 1 else 0)
        writeLine(FAN_ENABLE_NODE, if (enabled) "1" else "0")
    }

    /*
     * Set the fan speed level
     * @return puts the value in place
     */
    fun setFanSpeed(context: Context, speed: Int) {
        putInt(context, KEY_FAN_SPEED, speed)
        writeLine(FAN_SPEED_NODE, speed.toString())
    }

    /*
     * Apply both fan state and speed settings
     * @return puts the value in place
     */
    fun applySettings(context: Context, enabled: Boolean, speed: Int) {
        setFanEnabled(context, enabled)
        setFanSpeed(context, speed)
    }

    /*
     * Restore settings on boot / resume
     * @return puts the value in place
     */
    fun restoreSettings(context: Context) {
        val fanEnabled = getInt(context, KEY_FAN_ENABLE, 0) == 1
        val fanSpeed = getInt(context, KEY_FAN_SPEED, FAN_DEFAULT_SPEED)

        applySettings(context, fanEnabled, fanSpeed)
    }
}
