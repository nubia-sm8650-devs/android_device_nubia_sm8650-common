/*
 * SPDX-FileCopyrightText: 2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.settings

import android.app.Activity
import android.content.ActivityNotFoundException
import android.content.ComponentName
import android.content.Intent
import android.os.Bundle
import org.lineageos.settings.fan.FanActivity
import org.lineageos.settings.fan.FanTileService
import org.lineageos.settings.trigger.TriggerActivity
import org.lineageos.settings.trigger.TriggerTileService

class TileActivity : Activity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val sourceClass =
            intent.getParcelableExtra(Intent.EXTRA_COMPONENT_NAME, ComponentName::class.java)

        val targetActivity =
            when (sourceClass?.className) {
                FanTileService::class.java.name -> FanActivity::class.java
                TriggerTileService::class.java.name -> TriggerActivity::class.java
                else -> null
            }

        targetActivity?.let { openActivitySafely(Intent(this, it)) } ?: finish()
    }

    private fun openActivitySafely(dest: Intent) {
        try {
            startActivity(
                dest.apply {
                    flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK
                }
            )
        } catch (e: ActivityNotFoundException) {
            // Activity not found, just finish
        }
        finish()
    }
}
