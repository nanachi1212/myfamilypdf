package com.nanachi.gojodock

import android.Manifest
import android.annotation.SuppressLint
import android.app.Activity
import android.app.AlertDialog
import android.content.ActivityNotFoundException
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.Drawable
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.PowerManager
import android.provider.MediaStore
import android.provider.Settings
import android.text.Editable
import android.text.InputType
import android.text.TextUtils
import android.text.TextWatcher
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.view.WindowInsets
import android.widget.BaseAdapter
import android.widget.EditText
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.ListView
import android.widget.ScrollView
import android.widget.SeekBar
import android.widget.Switch
import android.widget.TextView
import android.widget.Toast
import java.util.Collections

/** Settings: permissions, the list of shortcuts and how the head looks. */
class MainActivity : Activity() {

    private lateinit var prefs: Prefs
    private lateinit var content: LinearLayout
    private var waitingForOverlay = false

    private val textColor get() = getColor(R.color.text)
    private val subColor get() = getColor(R.color.subtext)
    private val accent get() = getColor(R.color.accent)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        prefs = Prefs(this)
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.VANILLA_ICE_CREAM) {
            // Android 15+ always draws edge to edge; older versions need to be asked.
            @Suppress("DEPRECATION")
            window.setDecorFitsSystemWindows(false)
        }
        content = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(16), dp(12), dp(16), dp(32))
        }
        val scroll = ScrollView(this).apply {
            clipToPadding = false
            addView(content)
            setOnApplyWindowInsetsListener { v, insets ->
                val i = insets.getInsets(WindowInsets.Type.systemBars() or WindowInsets.Type.displayCutout())
                v.setPadding(i.left, i.top, i.right, i.bottom)
                insets
            }
        }
        setContentView(scroll)
        if (intent.getBooleanExtra(EXTRA_ADD_APP, false)) content.post { pickApp() }
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        if (intent.getBooleanExtra(EXTRA_ADD_APP, false)) content.post { pickApp() }
    }

    override fun onResume() {
        super.onResume()
        val canDraw = Settings.canDrawOverlays(this)
        if (waitingForOverlay && canDraw) prefs.enabled = true
        waitingForOverlay = false
        // Opening the app brings the head back if One UI closed it.
        if (prefs.enabled && canDraw && !BubbleService.running) BubbleService.start(this)
        if (prefs.enabled && canDraw && needsNotificationPermission() && !prefs.sp.getBoolean(KEY_ASKED_NOTIFICATIONS, false)) {
            askNotifications()
        }
        render()
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        render()
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        val uri = data?.data
        if (requestCode != REQ_IMAGE || resultCode != RESULT_OK || uri == null) return
        Thread {
            val ok = HeadImage.saveCustom(this, uri)
            runOnUiThread {
                if (ok) {
                    prefs.headStyle = HeadImage.STYLE_CUSTOM
                    prefs.touchHead()
                    toast("頭像換好了")
                } else {
                    toast("這張圖片讀不到，換一張試試")
                }
                render()
            }
        }.start()
    }

    private fun render() {
        content.removeAllViews()
        header()
        statusSection()
        itemsSection()
        lookSection()
        tipsSection()
    }

    // ---- sections ------------------------------------------------------------------

    private fun header() {
        val row = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(dp(4), dp(8), 0, dp(4))
        }
        val avatar = ImageView(this)
        avatar.setImageDrawable(HeadImage.load(this, prefs.headStyle))
        row.addView(avatar, LinearLayout.LayoutParams(dp(56), dp(56)))
        val texts = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(14), 0, 0, 0)
        }
        texts.addView(label(getString(R.string.app_name), 24f, textColor, bold = true))
        texts.addView(label("點頭像展開 · 拖曳移動 · 放開自動貼邊", 13f, subColor))
        row.addView(texts)
        content.addView(row)
    }

    private fun statusSection() {
        val box = section("懸浮頭像")
        val canDraw = Settings.canDrawOverlays(this)
        box.addView(switchRow("顯示懸浮頭像", "關掉後頭像和通知都會收起來", prefs.enabled && canDraw) { on ->
            if (on) turnOn() else turnOff()
        })
        if (!canDraw) {
            box.addView(stepRow(
                "第 1 步：允許「顯示在其他應用程式上層」",
                "沒有這個權限，頭像沒辦法浮在其他 App 上面。打開後回到這裡就會出現。",
                "前往開啟",
            ) { openOverlaySettings() })
        }
        if (needsNotificationPermission()) {
            box.addView(stepRow(
                "允許通知（建議）",
                "通知列會有一則安靜的常駐提醒，可以一鍵收起頭像。",
                "允許",
            ) { askNotifications() })
        }
        val power = getSystemService(PowerManager::class.java)!!
        if (!power.isIgnoringBatteryOptimizations(packageName)) {
            box.addView(stepRow(
                "電池設為「不受限制」（Samsung 建議）",
                "One UI 省電會關掉背景 App，設定後頭像比較不會突然不見。",
                "前往設定",
            ) { openBatterySettings() })
        }
    }

    private fun itemsSection() {
        val box = section("功能清單")
        box.addView(label("點名稱可以改名，↑↓ 調整順序，✕ 移除。", 12f, subColor), lp(bottom = 4))
        val items = prefs.items
        if (items.isEmpty()) box.addView(label("目前沒有功能，按下面的按鈕新增。", 14f, subColor), lp(top = 8, bottom = 8))
        items.forEachIndexed { index, _ -> box.addView(itemRow(items, index)) }

        val buttons = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            setPadding(0, dp(10), 0, 0)
        }
        buttons.addView(pill("＋ 新增 App") { pickApp() }, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        buttons.addView(View(this), LinearLayout.LayoutParams(dp(10), 1))
        buttons.addView(pill("＋ 新增網址") { addLink() }, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        box.addView(buttons)
        box.addView(pill("還原預設功能", filled = false) { confirmRestore() }, lp(top = 10))
    }

    private fun lookSection() {
        val box = section("外觀")
        box.addView(label("頭像圖片", 15f, textColor), lp(top = 4))
        val styles = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            setPadding(0, dp(6), 0, dp(4))
        }
        styles.addView(styleChoice(HeadImage.STYLE_SMILE, "雙眼笑"))
        styles.addView(styleChoice(HeadImage.STYLE_BLINDFOLD, "掀眼罩"))
        if (HeadImage.hasCustom(this)) styles.addView(styleChoice(HeadImage.STYLE_CUSTOM, "我的圖片"))
        box.addView(styles)
        box.addView(pill("從相簿選一張當頭像", filled = false) { pickImage() }, lp(top = 4))
        box.addView(seekRow("頭像大小", Prefs.SIZE_MIN, Prefs.SIZE_MAX, prefs.headSizeDp, { "$it dp" }) {
            prefs.headSizeDp = it
        })
        box.addView(seekRow("閒置時透明度", Prefs.ALPHA_MIN, 100, prefs.idleAlpha, { "$it%" }) {
            prefs.idleAlpha = it
        })
        box.addView(switchRow("閒置時收進側邊", "2.5 秒沒碰就藏一半到螢幕邊，不擋畫面", prefs.autoTuck) {
            prefs.autoTuck = it
        })
        box.addView(switchRow("開機後自動出現", null, prefs.autoStart) { prefs.autoStart = it })
    }

    private fun tipsSection() {
        val box = section("小提示")
        listOf(
            "點頭像：展開功能；點空白處或返回手勢：收起。",
            "按住頭像拖曳可以移動，放開會自動貼到左邊或右邊。",
            "Samsung 側邊面板的把手預設在右邊，若擋到可以把頭像拖到左邊。",
            "頭像不見了：打開「悟醬快捷」就會自動回來。",
        ).forEach { box.addView(label("‧ $it", 13f, subColor), lp(top = 4)) }
    }

    // ---- shortcut list -------------------------------------------------------------

    private fun itemRow(items: List<ShortcutItem>, index: Int): View {
        val item = items[index]
        val row = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(0, dp(6), 0, dp(6))
        }
        val icon = ImageView(this)
        icon.setImageDrawable(Shortcuts.icon(this, item))
        row.addView(icon, LinearLayout.LayoutParams(dp(40), dp(40)))

        val texts = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(12), dp(4), dp(4), dp(4))
            background = ripple(getColor(R.color.ripple), dp(12).toFloat())
            setOnClickListener { rename(item) }
        }
        texts.addView(label(item.label, 16f, textColor).apply {
            maxLines = 1
            ellipsize = TextUtils.TruncateAt.END
        })
        describe(item)?.let {
            texts.addView(label(it, 12f, subColor).apply {
                maxLines = 1
                ellipsize = TextUtils.TruncateAt.END
            })
        }
        row.addView(texts, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        row.addView(smallButton("↑", index > 0, accent) { move(index, -1) })
        row.addView(smallButton("↓", index < items.lastIndex, accent) { move(index, 1) })
        row.addView(smallButton("✕", true, getColor(R.color.danger)) { confirmRemove(item) })
        return row
    }

    private fun describe(item: ShortcutItem): String? {
        if (item.isLink) return item.url
        if (Shortcuts.installedPackage(packageManager, item) != null) return null
        return if (item.url != null) "沒裝這個 App，會改用網頁開啟" else "手機上找不到這個 App"
    }

    private fun move(index: Int, delta: Int) {
        val list = prefs.items.toMutableList()
        Collections.swap(list, index, index + delta)
        prefs.items = list
        render()
    }

    private fun confirmRemove(item: ShortcutItem) {
        AlertDialog.Builder(this)
            .setMessage("從頭像移除「${item.label}」？")
            .setPositiveButton("移除") { _, _ ->
                prefs.items = prefs.items.filter { it.id != item.id }
                render()
            }
            .setNegativeButton("取消", null)
            .show()
    }

    private fun rename(item: ShortcutItem) {
        val input = EditText(this).apply {
            setText(item.label)
            isSingleLine = true
            setSelection(text.length)
        }
        AlertDialog.Builder(this)
            .setTitle("改名")
            .setView(padded(input))
            .setPositiveButton("儲存") { _, _ ->
                val name = input.text.toString().trim()
                if (name.isNotEmpty()) {
                    prefs.items = prefs.items.map { if (it.id == item.id) it.copy(label = name) else it }
                    render()
                }
            }
            .setNegativeButton("取消", null)
            .show()
    }

    private fun confirmRestore() {
        AlertDialog.Builder(this)
            .setMessage("還原成預設的 Google 翻譯、Gmail、Google 地圖、計算機？自己加的會被移除。")
            .setPositiveButton("還原") { _, _ ->
                prefs.items = ShortcutItem.defaults()
                render()
            }
            .setNegativeButton("取消", null)
            .show()
    }

    private fun pickApp() {
        val adapter = AppAdapter()
        val search = EditText(this).apply {
            hint = "搜尋 App 名稱"
            isSingleLine = true
        }
        val loading = label("讀取 App 清單中…", 14f, subColor).apply { setPadding(dp(4), dp(16), dp(4), dp(16)) }
        val list = ListView(this)
        list.adapter = adapter
        list.divider = null
        val box = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(20), dp(8), dp(20), 0)
        }
        box.addView(search)
        box.addView(loading)
        box.addView(list, LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, (resources.displayMetrics.heightPixels * 0.5f).toInt(),
        ))
        val dialog = AlertDialog.Builder(this)
            .setTitle("選擇要放進頭像的 App")
            .setView(box)
            .setNegativeButton("關閉", null)
            .create()
        list.setOnItemClickListener { _, _, position, _ ->
            val app = adapter.getItem(position)
            prefs.items = prefs.items + ShortcutItem(
                label = app.label,
                packages = listOf(app.packageName),
                activity = app.activity,
            )
            toast("已加入「${app.label}」")
            dialog.dismiss()
            render()
        }
        search.addTextChangedListener(object : TextWatcher {
            override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
            override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {
                adapter.filter(s?.toString().orEmpty())
            }

            override fun afterTextChanged(s: Editable?) {}
        })
        dialog.show()
        Thread {
            val apps = Shortcuts.launchableApps(this)
            runOnUiThread {
                if (!isDestroyed && dialog.isShowing) {
                    loading.visibility = View.GONE
                    adapter.setApps(apps, search.text.toString())
                }
            }
        }.start()
    }

    private fun addLink() {
        val name = EditText(this).apply {
            hint = "名稱，例如：YouTube"
            isSingleLine = true
        }
        val url = EditText(this).apply {
            hint = "網址，例如：youtube.com"
            isSingleLine = true
            inputType = InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_URI
        }
        val box = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(20), dp(8), dp(20), 0)
        }
        box.addView(name)
        box.addView(url)
        AlertDialog.Builder(this)
            .setTitle("新增網址捷徑")
            .setView(box)
            .setPositiveButton("新增") { _, _ ->
                val link = normalizeUrl(url.text.toString())
                if (link == null) {
                    toast("網址看起來不正確")
                } else {
                    val label = name.text.toString().trim().ifEmpty { Uri.parse(link).host ?: link }
                    prefs.items = prefs.items + ShortcutItem(label = label, url = link)
                    render()
                }
            }
            .setNegativeButton("取消", null)
            .show()
    }

    private fun normalizeUrl(raw: String): String? {
        val text = raw.trim()
        if (text.isEmpty() || text == "https://" || text.contains(' ')) return null
        return if (SCHEME.containsMatchIn(text)) text else "https://$text"
    }

    // ---- head look -----------------------------------------------------------------

    private fun styleChoice(style: String, name: String): View {
        val selected = prefs.headStyle == style
        val radius = dp(16).toFloat()
        val col = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER_HORIZONTAL
            setPadding(dp(10), dp(8), dp(10), dp(8))
            val frame = if (selected) roundRect(getColor(R.color.ripple), radius, accent, dp(2)) else null
            background = ripple(getColor(R.color.ripple), radius, frame)
            setOnClickListener {
                prefs.headStyle = style
                render()
            }
        }
        val image = ImageView(this)
        image.setImageDrawable(HeadImage.load(this, style))
        col.addView(image, LinearLayout.LayoutParams(dp(60), dp(60)))
        col.addView(label(name, 13f, if (selected) accent else subColor).apply { gravity = Gravity.CENTER }, lp(top = 4))
        col.layoutParams = LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT).apply {
            marginEnd = dp(8)
        }
        return col
    }

    private fun pickImage() {
        val fallback = Intent(Intent.ACTION_OPEN_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE).setType("image/*")
        val picker = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            Intent(MediaStore.ACTION_PICK_IMAGES).setType("image/*")
        } else {
            fallback
        }
        try {
            startActivityForResult(picker, REQ_IMAGE)
        } catch (e: ActivityNotFoundException) {
            startActivityForResult(fallback, REQ_IMAGE)
        }
    }

    // ---- permissions ---------------------------------------------------------------

    private fun turnOn() {
        if (!Settings.canDrawOverlays(this)) {
            openOverlaySettings()
            render()
            return
        }
        prefs.enabled = true
        BubbleService.start(this)
        render()
    }

    private fun turnOff() {
        prefs.enabled = false
        BubbleService.stop(this)
        render()
    }

    private fun openOverlaySettings() {
        waitingForOverlay = true
        toast("找到「${getString(R.string.app_name)}」，打開「允許」開關後返回", long = true)
        if (!startSafely(Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION, Uri.parse("package:$packageName")))) {
            startSafely(Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION))
        }
    }

    @SuppressLint("BatteryLife")
    private fun openBatterySettings() {
        val ask = Intent(Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS, Uri.parse("package:$packageName"))
        if (!startSafely(ask)) {
            startSafely(Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS, Uri.parse("package:$packageName")))
        }
    }

    private fun needsNotificationPermission() =
        Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU &&
            checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED

    private fun askNotifications() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) return
        if (prefs.sp.getBoolean(KEY_ASKED_NOTIFICATIONS, false)) {
            // Android stops showing the prompt after it was declined; go to the app's page instead.
            startSafely(Intent(Settings.ACTION_APP_NOTIFICATION_SETTINGS).putExtra(Settings.EXTRA_APP_PACKAGE, packageName))
        } else {
            prefs.sp.edit().putBoolean(KEY_ASKED_NOTIFICATIONS, true).apply()
            requestPermissions(arrayOf(Manifest.permission.POST_NOTIFICATIONS), REQ_NOTIFICATIONS)
        }
    }

    private fun startSafely(intent: Intent): Boolean = try {
        startActivity(intent)
        true
    } catch (e: ActivityNotFoundException) {
        false
    }

    // ---- small view helpers --------------------------------------------------------

    private fun section(title: String): LinearLayout {
        val box = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            background = roundRect(getColor(R.color.card), dp(20).toFloat())
            setPadding(dp(16), dp(14), dp(16), dp(14))
        }
        box.addView(label(title, 13f, accent, bold = true), lp(bottom = 4))
        content.addView(box, lp(top = 14))
        return box
    }

    private fun label(text: CharSequence, size: Float, color: Int, bold: Boolean = false) = TextView(this).apply {
        this.text = text
        textSize = size
        setTextColor(color)
        if (bold) typeface = Typeface.DEFAULT_BOLD
    }

    private fun pill(text: String, filled: Boolean = true, onClick: () -> Unit) = TextView(this).apply {
        this.text = text
        textSize = 14f
        gravity = Gravity.CENTER
        setPadding(dp(14), dp(11), dp(14), dp(11))
        val radius = dp(22).toFloat()
        if (filled) {
            setTextColor(getColor(R.color.on_accent))
            background = ripple(getColor(R.color.ripple), radius, roundRect(accent, radius))
        } else {
            setTextColor(accent)
            background = ripple(getColor(R.color.ripple), radius, roundRect(Color.TRANSPARENT, radius, accent, dp(1)))
        }
        setOnClickListener { onClick() }
    }

    private fun smallButton(text: String, enabled: Boolean, color: Int, onClick: () -> Unit) = TextView(this).apply {
        this.text = text
        textSize = 18f
        gravity = Gravity.CENTER
        setTextColor(color)
        isEnabled = enabled
        alpha = if (enabled) 1f else 0.25f
        background = ripple(getColor(R.color.ripple), dp(20).toFloat())
        setOnClickListener { onClick() }
        layoutParams = LinearLayout.LayoutParams(dp(40), dp(40))
    }

    private fun switchRow(title: String, sub: String?, checked: Boolean, onChange: (Boolean) -> Unit): View {
        val row = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(0, dp(8), 0, dp(8))
        }
        val texts = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        texts.addView(label(title, 16f, textColor))
        if (sub != null) texts.addView(label(sub, 12f, subColor))
        row.addView(texts, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        val toggle = Switch(this)
        toggle.isChecked = checked
        toggle.setOnCheckedChangeListener { _, on -> onChange(on) }
        row.addView(toggle)
        row.setOnClickListener { toggle.toggle() }
        return row
    }

    private fun stepRow(title: String, sub: String, button: String, onClick: () -> Unit): View {
        val box = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            background = roundRect(getColor(R.color.divider), dp(14).toFloat())
            setPadding(dp(14), dp(12), dp(14), dp(12))
        }
        box.addView(label(title, 15f, textColor, bold = true))
        box.addView(label(sub, 12f, subColor), lp(top = 2))
        box.addView(pill(button, onClick = onClick), LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT,
        ).apply { topMargin = dp(8) })
        box.layoutParams = lp(top = 8)
        return box
    }

    private fun seekRow(
        title: String,
        min: Int,
        max: Int,
        value: Int,
        format: (Int) -> String,
        onChange: (Int) -> Unit,
    ): View {
        val box = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(0, dp(12), 0, dp(4))
        }
        val top = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }
        val valueText = label(format(value), 14f, accent)
        top.addView(label(title, 15f, textColor), LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        top.addView(valueText)
        box.addView(top)
        val bar = SeekBar(this)
        bar.min = min
        bar.max = max
        bar.progress = value
        bar.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
                if (!fromUser) return
                valueText.text = format(progress)
                onChange(progress)
            }

            override fun onStartTrackingTouch(seekBar: SeekBar?) {}
            override fun onStopTrackingTouch(seekBar: SeekBar?) {}
        })
        box.addView(bar, lp(top = 4))
        return box
    }

    private fun padded(view: View) = LinearLayout(this).apply {
        setPadding(dp(20), dp(8), dp(20), 0)
        addView(view, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT))
    }

    private fun lp(top: Int = 0, bottom: Int = 0) = LinearLayout.LayoutParams(
        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT,
    ).apply {
        topMargin = dp(top)
        bottomMargin = dp(bottom)
    }

    private fun toast(text: String, long: Boolean = false) =
        Toast.makeText(this, text, if (long) Toast.LENGTH_LONG else Toast.LENGTH_SHORT).show()

    /** Installed apps for the picker, filtered by the search box. */
    private inner class AppAdapter : BaseAdapter() {
        private var all: List<AppEntry> = emptyList()
        private var shown: List<AppEntry> = emptyList()
        private val icons = HashMap<String, Drawable>()
        private val added = prefs.items.flatMap { it.packages }.toSet()

        fun setApps(apps: List<AppEntry>, query: String) {
            all = apps
            filter(query)
        }

        fun filter(query: String) {
            val q = query.trim().lowercase()
            shown = if (q.isEmpty()) all else all.filter { q in it.label.lowercase() || q in it.packageName }
            notifyDataSetChanged()
        }

        override fun getCount() = shown.size

        override fun getItem(position: Int): AppEntry = shown[position]

        override fun getItemId(position: Int) = position.toLong()

        override fun getView(position: Int, convertView: View?, parent: ViewGroup?): View {
            val row = convertView as? LinearLayout ?: LinearLayout(this@MainActivity).apply {
                orientation = LinearLayout.HORIZONTAL
                gravity = Gravity.CENTER_VERTICAL
                setPadding(dp(4), dp(8), dp(4), dp(8))
                addView(ImageView(context), LinearLayout.LayoutParams(dp(36), dp(36)))
                addView(TextView(context).apply {
                    textSize = 15f
                    setPadding(dp(14), 0, 0, 0)
                })
            }
            val app = shown[position]
            val icon = icons.getOrPut("${app.packageName}/${app.activity}") { app.info.loadIcon(packageManager) }
            (row.getChildAt(0) as ImageView).setImageDrawable(icon)
            (row.getChildAt(1) as TextView).apply {
                text = if (app.packageName in added) "${app.label}  ✓ 已加入" else app.label
                setTextColor(textColor)
            }
            return row
        }
    }

    companion object {
        const val EXTRA_ADD_APP = "add_app"
        private const val REQ_IMAGE = 1
        private const val REQ_NOTIFICATIONS = 2
        private const val KEY_ASKED_NOTIFICATIONS = "asked_notifications"
        private val SCHEME = Regex("^[a-zA-Z][a-zA-Z0-9+.-]*:")
    }
}
