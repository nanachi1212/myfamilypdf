package com.nanachi.gojodock

import android.content.ActivityNotFoundException
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.content.pm.ResolveInfo
import android.graphics.drawable.Drawable
import android.net.Uri
import android.util.Log

/** An app that shows up in the launcher, offered in the "新增 App" picker. */
class AppEntry(val label: String, val packageName: String, val activity: String, val info: ResolveInfo)

object Shortcuts {

    /** The first of the item's packages that is installed, or null. */
    fun installedPackage(pm: PackageManager, item: ShortcutItem): String? =
        item.packages.firstOrNull { pm.getLaunchIntentForPackage(it) != null }

    /** Opens the item; returns false when nothing on the phone can open it. */
    fun launch(context: Context, item: ShortcutItem): Boolean {
        val intent = intentFor(context.packageManager, item) ?: return false
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_RESET_TASK_IF_NEEDED)
        return try {
            context.startActivity(intent)
            true
        } catch (e: ActivityNotFoundException) {
            Log.w(TAG, "Cannot open ${item.label}", e)
            false
        } catch (e: SecurityException) {
            Log.w(TAG, "Not allowed to open ${item.label}", e)
            false
        }
    }

    private fun intentFor(pm: PackageManager, item: ShortcutItem): Intent? {
        val activity = item.activity
        if (activity != null && item.packages.size == 1) {
            val exact = Intent(Intent.ACTION_MAIN)
                .addCategory(Intent.CATEGORY_LAUNCHER)
                .setClassName(item.packages[0], activity)
            if (pm.resolveActivity(exact, 0) != null) return exact
        }
        for (pkg in item.packages) pm.getLaunchIntentForPackage(pkg)?.let { return it }
        return item.url?.let { Intent(Intent.ACTION_VIEW, Uri.parse(it)) }
    }

    fun icon(context: Context, item: ShortcutItem): Drawable {
        val pm = context.packageManager
        val pkg = installedPackage(pm, item)
        if (pkg != null) {
            try {
                val activity = item.activity
                return if (activity != null && pkg == item.packages[0]) {
                    pm.getActivityIcon(ComponentName(pkg, activity))
                } else {
                    pm.getApplicationIcon(pkg)
                }
            } catch (e: PackageManager.NameNotFoundException) {
                // Fall through to the letter icon.
            }
        }
        return LetterIcon.forLabel(context, item.label)
    }

    /** Every launchable app except this one, sorted by name. Slow: call off the main thread. */
    fun launchableApps(context: Context): List<AppEntry> {
        val pm = context.packageManager
        val query = Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_LAUNCHER)
        @Suppress("DEPRECATION")
        return pm.queryIntentActivities(query, 0)
            .filter { it.activityInfo.packageName != context.packageName }
            .map { AppEntry(it.loadLabel(pm).toString(), it.activityInfo.packageName, it.activityInfo.name, it) }
            .sortedBy { it.label.lowercase() }
    }

    private const val TAG = "GojoDock"
}
