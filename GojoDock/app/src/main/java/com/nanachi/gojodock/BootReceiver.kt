package com.nanachi.gojodock

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.provider.Settings

/** Brings the head back after a reboot or an app update. */
class BootReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        val prefs = Prefs(context)
        val wanted = when (intent.action) {
            Intent.ACTION_BOOT_COMPLETED -> prefs.enabled && prefs.autoStart
            Intent.ACTION_MY_PACKAGE_REPLACED -> prefs.enabled
            else -> false
        }
        if (wanted && Settings.canDrawOverlays(context)) BubbleService.start(context)
    }
}
