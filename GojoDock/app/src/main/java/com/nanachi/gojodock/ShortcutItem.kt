package com.nanachi.gojodock

import org.json.JSONArray
import org.json.JSONObject
import java.util.UUID

/**
 * One tile in the floating panel: either an installed app or a web link.
 *
 * An app item lists candidate packages and opens the first one installed, so
 * "計算機" works with both Samsung's and Google's calculator. [url] is opened when
 * none of the packages is installed, or is the whole item for a link.
 */
data class ShortcutItem(
    val id: String = UUID.randomUUID().toString(),
    val label: String,
    val packages: List<String> = emptyList(),
    val activity: String? = null,
    val url: String? = null,
) {
    val isLink: Boolean get() = packages.isEmpty()

    fun toJson(): JSONObject = JSONObject().apply {
        put("id", id)
        put("label", label)
        put("packages", JSONArray(packages))
        activity?.let { put("activity", it) }
        url?.let { put("url", it) }
    }

    companion object {
        fun fromJson(o: JSONObject): ShortcutItem {
            val pkgs = o.optJSONArray("packages") ?: JSONArray()
            return ShortcutItem(
                id = o.optString("id").ifEmpty { UUID.randomUUID().toString() },
                label = o.optString("label"),
                packages = List(pkgs.length()) { pkgs.getString(it) },
                activity = if (o.has("activity")) o.getString("activity") else null,
                url = if (o.has("url")) o.getString("url") else null,
            )
        }

        fun listFromJson(raw: String): List<ShortcutItem> {
            val arr = JSONArray(raw)
            return List(arr.length()) { fromJson(arr.getJSONObject(it)) }
        }

        fun listToJson(items: List<ShortcutItem>): String =
            JSONArray(items.map { it.toJson() }).toString()

        fun defaults(): List<ShortcutItem> = listOf(
            ShortcutItem(
                label = "Google 翻譯",
                packages = listOf("com.google.android.apps.translate"),
                url = "https://translate.google.com/",
            ),
            ShortcutItem(
                label = "Gmail",
                packages = listOf("com.google.android.gm"),
                url = "https://mail.google.com/",
            ),
            ShortcutItem(
                label = "Google 地圖",
                packages = listOf("com.google.android.apps.maps"),
                url = "https://maps.google.com/",
            ),
            ShortcutItem(
                label = "計算機",
                packages = listOf("com.sec.android.app.popupcalculator", "com.google.android.calculator"),
            ),
        )
    }
}
