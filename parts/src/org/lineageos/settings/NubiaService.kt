/*
 * SPDX-FileCopyrightText: 2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.settings

import android.app.Service
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.IBinder
import org.lineageos.settings.fan.FanController
import org.lineageos.settings.utils.*

class NubiaService : Service() {

    private val receiver =
        object : BroadcastReceiver() {
            override fun onReceive(context: Context, intent: Intent) {
                when (intent.action) {
                    Intent.ACTION_SCREEN_OFF -> {
                        if (getInt(context, FanController.KEY_FAN_ENABLE, 0) == 0) return
                        val speed =
                            getInt(
                                context,
                                FanController.KEY_FAN_SPEED,
                                FanController.FAN_DEFAULT_SPEED,
                            )
                        putInt(context, FanController.KEY_FAN_SPEED_RECEIVER, speed)
                        FanController.setFanSpeed(context, 0)
                    }
                    Intent.ACTION_SCREEN_ON -> {
                        if (getInt(context, FanController.KEY_FAN_ENABLE, 0) == 0) return
                        val speed =
                            getInt(
                                context,
                                FanController.KEY_FAN_SPEED_RECEIVER,
                                FanController.FAN_DEFAULT_SPEED,
                            )
                        FanController.setFanSpeed(context, speed)
                    }
                }
            }
        }

    override fun onCreate() {
        super.onCreate()

        registerReceiver(
            receiver,
            IntentFilter().apply {
                addAction(Intent.ACTION_SCREEN_OFF)
                addAction(Intent.ACTION_SCREEN_ON)
            },
        )
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int) = START_STICKY

    override fun onDestroy() {
        runCatching { unregisterReceiver(receiver) }
        super.onDestroy()
    }

    override fun onBind(intent: Intent?): IBinder? = null
}
