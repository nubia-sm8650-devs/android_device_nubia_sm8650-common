/*
 * SPDX-FileCopyrightText: 2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.settings

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import org.lineageos.settings.fan.FanController
import org.lineageos.settings.trigger.TriggerController

class BootReceiver : BroadcastReceiver() {

    override fun onReceive(context: Context, intent: Intent) {
        if (Intent.ACTION_BOOT_COMPLETED == intent.action) {
            FanController.restoreSettings(context)
            TriggerController.restoreSettings(context)
            context.startService(Intent(context, NubiaService::class.java))
        }
    }
}
