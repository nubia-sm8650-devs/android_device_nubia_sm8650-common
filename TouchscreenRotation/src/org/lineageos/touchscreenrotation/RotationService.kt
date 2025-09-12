/*
 * SPDX-FileCopyrightText: 2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.touchscreenrotation

import android.app.Service
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.IBinder
import android.util.Log
import android.view.Surface
import android.view.WindowManager
import java.io.FileWriter
import java.io.IOException

class RotationService : Service() {
    private lateinit var rotationReceiver: BroadcastReceiver
    private var rotation = 0

    override fun onCreate() {
        super.onCreate()

        Log.i(TAG, "Service created")

        rotationReceiver = object : BroadcastReceiver() {
            override fun onReceive(context: Context, intent: Intent) {
                val newRotation = getRotation(context)
                if (newRotation != rotation) {
                    rotation = newRotation
                    writeRotation(newRotation)
                }
            }
        }

        val filter = IntentFilter(Intent.ACTION_CONFIGURATION_CHANGED)
        registerReceiver(rotationReceiver, filter)
    }

    private fun writeRotation(rotation: Int) {
        Log.i(TAG, "Write rotation: $rotation")
        try {
            FileWriter(ROTATION_PATH).use { writer ->
                writer.write(rotation.toString())
            }
        } catch (e: IOException) {
            Log.e(TAG, "Failed to write to $ROTATION_PATH", e)
        }
    }

    override fun onDestroy() {
        unregisterReceiver(rotationReceiver)
        Log.i(TAG, "Service destroyed")
        super.onDestroy()
    }

    override fun onBind(intent: Intent): IBinder? {
        return null
    }

    companion object {
        private const val TAG = "RotationService"
        private const val ROTATION_PATH = "/sys/devices/platform/goodix_ts.0/rotation"

        @JvmStatic
        fun getRotation(context: Context): Int {
            val wm = context.getSystemService(Context.WINDOW_SERVICE) as WindowManager
            val rotation = wm.defaultDisplay.rotation

            return when (rotation) {
                Surface.ROTATION_0 -> 0
                Surface.ROTATION_90 -> 90
                Surface.ROTATION_180 -> 180
                else -> 270
            }
        }
    }
}
