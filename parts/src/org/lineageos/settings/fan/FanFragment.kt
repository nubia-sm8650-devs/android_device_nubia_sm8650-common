/*
 * SPDX-FileCopyrightText: 2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.settings.fan

import android.os.Bundle
import androidx.preference.Preference
import com.android.settingslib.widget.MainSwitchPreference
import com.android.settingslib.widget.SettingsBasePreferenceFragment
import com.android.settingslib.widget.SliderPreference
import org.lineageos.settings.R
import org.lineageos.settings.utils.*

class FanFragment : SettingsBasePreferenceFragment(), Preference.OnPreferenceChangeListener {

    private lateinit var mFanSpeedBar: SliderPreference

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        addPreferencesFromResource(R.xml.fan_preferences)

        val fanEnabled = getInt(requireContext(), FanController.KEY_FAN_ENABLE, 0) == 1
        val savedSpeed =
            getInt(requireContext(), FanController.KEY_FAN_SPEED, FanController.FAN_DEFAULT_SPEED)

        findPreference<MainSwitchPreference>(FanController.KEY_FAN_ENABLE)!!.apply {
            setChecked(fanEnabled)
            onPreferenceChangeListener = this@FanFragment
        }

        mFanSpeedBar =
            findPreference<SliderPreference>(FanController.KEY_FAN_SPEED)!!.apply {
                setMin(FanController.FAN_MIN_SPEED)
                setMax(FanController.FAN_MAX_SPEED)
                setSliderIncrement(1)
                setValue(savedSpeed)
                setShowSliderValue(true)
                isEnabled = fanEnabled
                onPreferenceChangeListener = this@FanFragment
            }
    }

    override fun onPreferenceChange(preference: Preference, newValue: Any): Boolean {
        return when (preference.key) {
            FanController.KEY_FAN_ENABLE -> {
                val isEnabled = newValue as Boolean
                val speed =
                    getInt(
                        requireContext(),
                        FanController.KEY_FAN_SPEED,
                        FanController.FAN_DEFAULT_SPEED,
                    )

                mFanSpeedBar.isEnabled = isEnabled
                FanController.applySettings(requireContext(), isEnabled, speed)
                true
            }

            FanController.KEY_FAN_SPEED -> {
                val speed = newValue as Int

                FanController.setFanSpeed(requireContext(), speed)
                true
            }

            else -> false
        }
    }
}
