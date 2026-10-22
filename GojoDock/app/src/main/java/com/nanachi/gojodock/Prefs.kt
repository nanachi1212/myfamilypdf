package com.nanachi.gojodock

import android.content.Context
import android.content.SharedPreferences

/** All settings live here; [BubbleService] listens to changes made by [MainActivity]. */
class Prefs(context: Context) {
    val sp: SharedPreferences = context.getSharedPreferences("gojo_dock", Context.MODE_PRIVATE)

    var items: List<ShortcutItem>
        get() {
            val raw = sp.getString(KEY_ITEMS, null)
                ?: return ShortcutItem.defaults().also { items = it }
            return try {
                ShortcutItem.listFromJson(raw)
            } catch (e: org.json.JSONException) {
                ShortcutItem.defaults()
            }
        }
        set(value) = sp.edit().putString(KEY_ITEMS, ShortcutItem.listToJson(value)).apply()

    /** The user wants the head on screen; checked on boot and when the app opens. */
    var enabled: Boolean
        get() = sp.getBoolean(KEY_ENABLED, true)
        set(value) = sp.edit().putBoolean(KEY_ENABLED, value).apply()

    var headSizeDp: Int
        get() = sp.getInt(KEY_SIZE, SIZE_DEFAULT).coerceIn(SIZE_MIN, SIZE_MAX)
        set(value) = sp.edit().putInt(KEY_SIZE, value).apply()

    /** Opacity in percent while the head sits idle at the edge. */
    var idleAlpha: Int
        get() = sp.getInt(KEY_ALPHA, ALPHA_DEFAULT).coerceIn(ALPHA_MIN, 100)
        set(value) = sp.edit().putInt(KEY_ALPHA, value).apply()

    var autoTuck: Boolean
        get() = sp.getBoolean(KEY_TUCK, true)
        set(value) = sp.edit().putBoolean(KEY_TUCK, value).apply()

    var autoStart: Boolean
        get() = sp.getBoolean(KEY_AUTO_START, true)
        set(value) = sp.edit().putBoolean(KEY_AUTO_START, value).apply()

    /** One of [HeadImage.STYLE_SMILE], [HeadImage.STYLE_BLINDFOLD], [HeadImage.STYLE_CUSTOM]. */
    var headStyle: String
        get() = sp.getString(KEY_HEAD_STYLE, HeadImage.STYLE_SMILE) ?: HeadImage.STYLE_SMILE
        set(value) = sp.edit().putString(KEY_HEAD_STYLE, value).putLong(KEY_HEAD_VERSION, System.currentTimeMillis()).apply()

    /** Re-read the head picture even if the style did not change (new custom photo). */
    fun touchHead() = sp.edit().putLong(KEY_HEAD_VERSION, System.currentTimeMillis()).apply()

    val onRight: Boolean get() = sp.getBoolean(KEY_ON_RIGHT, true)

    /** Vertical position as a fraction of the screen height, so it survives rotation. */
    val yFraction: Float get() = sp.getFloat(KEY_Y, 0.58f)

    fun savePosition(onRight: Boolean, yFraction: Float) =
        sp.edit().putBoolean(KEY_ON_RIGHT, onRight).putFloat(KEY_Y, yFraction).apply()

    companion object {
        const val KEY_ITEMS = "items"
        const val KEY_ENABLED = "enabled"
        const val KEY_SIZE = "head_size_dp"
        const val KEY_ALPHA = "idle_alpha"
        const val KEY_TUCK = "auto_tuck"
        const val KEY_AUTO_START = "auto_start"
        const val KEY_HEAD_STYLE = "head_style"
        const val KEY_HEAD_VERSION = "head_version"
        const val KEY_ON_RIGHT = "on_right"
        const val KEY_Y = "y_fraction"

        const val SIZE_MIN = 32
        const val SIZE_MAX = 64
        const val SIZE_DEFAULT = 44
        const val ALPHA_MIN = 30
        const val ALPHA_DEFAULT = 75
    }
}
