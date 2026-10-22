package com.nanachi.gojodock

import android.animation.Animator
import android.animation.AnimatorListenerAdapter
import android.animation.ValueAnimator
import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.BroadcastReceiver
import android.content.ComponentCallbacks
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.SharedPreferences
import android.content.pm.ServiceInfo
import android.content.res.Configuration
import android.graphics.Insets
import android.graphics.PixelFormat
import android.graphics.Typeface
import android.graphics.drawable.Drawable
import android.graphics.drawable.Icon
import android.hardware.display.DisplayManager
import android.os.Build
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.provider.Settings
import android.text.TextUtils
import android.util.Log
import android.view.ContextThemeWrapper
import android.view.Display
import android.view.Gravity
import android.view.HapticFeedbackConstants
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import android.view.ViewConfiguration
import android.view.ViewGroup
import android.view.WindowInsets
import android.view.WindowManager
import android.view.animation.DecelerateInterpolator
import android.view.animation.OvershootInterpolator
import android.widget.FrameLayout
import android.widget.GridLayout
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import kotlin.math.hypot
import kotlin.math.max
import kotlin.math.roundToInt

/**
 * Keeps the floating head on screen. Tap it to open the shortcut panel, drag it to move;
 * on release it snaps to the nearest side and, after a short idle time, tucks half of
 * itself into the edge and fades so it does not cover what is underneath.
 */
class BubbleService : Service(), SharedPreferences.OnSharedPreferenceChangeListener {

    private lateinit var prefs: Prefs
    private lateinit var windowContext: Context
    private lateinit var ui: Context
    private lateinit var wm: WindowManager
    private val handler = Handler(Looper.getMainLooper())
    private val tuckRunnable = Runnable { tuck() }

    private var head: ImageView? = null
    private var panel: View? = null
    private var mover: ValueAnimator? = null
    private val headParams = overlayParams(focusable = false)

    private val headSize: Int get() = ui.dp(prefs.headSizeDp)

    private val configCallbacks = object : ComponentCallbacks {
        override fun onConfigurationChanged(newConfig: Configuration) {
            handler.post { onScreenChanged() }
        }

        override fun onLowMemory() {}
    }

    private val screenOff = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) = closePanel(animate = false)
    }

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onCreate() {
        super.onCreate()
        prefs = Prefs(this)
        // A window context keeps sizes and rotation right for windows added from a service.
        val display = getSystemService(DisplayManager::class.java)!!.getDisplay(Display.DEFAULT_DISPLAY)
        windowContext = createDisplayContext(display)
            .createWindowContext(WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY, null)
        ui = ContextThemeWrapper(windowContext, R.style.Theme_GojoDock)
        wm = windowContext.getSystemService(WindowManager::class.java)!!
        windowContext.registerComponentCallbacks(configCallbacks)
        registerReceiver(screenOff, IntentFilter(Intent.ACTION_SCREEN_OFF))
        running = true
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == ACTION_STOP) {
            prefs.enabled = false
            stopSelf()
            return START_NOT_STICKY
        }
        val canDraw = Settings.canDrawOverlays(this)
        // The overlay goes up first: Android 15 only lets a restarted service go foreground
        // from the background while its overlay window is visible.
        if (canDraw && head == null) showHead()
        try {
            goForeground()
        } catch (e: Exception) {
            Log.w(TAG, "Not allowed to stay in the foreground", e)
            stopSelf()
            return START_NOT_STICKY
        }
        if (!canDraw) {
            stopSelf()
            return START_NOT_STICKY
        }
        return START_STICKY
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)
        handler.post { onScreenChanged() }
    }

    override fun onDestroy() {
        running = false
        handler.removeCallbacksAndMessages(null)
        mover?.cancel()
        prefs.sp.unregisterOnSharedPreferenceChangeListener(this)
        try {
            unregisterReceiver(screenOff)
        } catch (e: IllegalArgumentException) {
            // Not registered.
        }
        windowContext.unregisterComponentCallbacks(configCallbacks)
        panel?.let { removeWindow(it) }
        panel = null
        head?.let { removeWindow(it) }
        head = null
        super.onDestroy()
    }

    override fun onSharedPreferenceChanged(sp: SharedPreferences?, key: String?) {
        when (key) {
            Prefs.KEY_SIZE, Prefs.KEY_TUCK -> onScreenChanged()
            Prefs.KEY_HEAD_STYLE, Prefs.KEY_HEAD_VERSION -> head?.setImageDrawable(headDrawable())
            Prefs.KEY_ALPHA -> head?.let {
                // Show the new idle opacity right away so the slider can be judged.
                it.alpha = prefs.idleAlpha / 100f
                scheduleTuck()
            }
        }
    }

    // ---- notification ----------------------------------------------------------------

    private fun goForeground() {
        val nm = getSystemService(NotificationManager::class.java)!!
        nm.createNotificationChannel(
            NotificationChannel(CHANNEL_ID, getString(R.string.channel_name), NotificationManager.IMPORTANCE_MIN)
                .apply { setShowBadge(false) },
        )
        val open = PendingIntent.getActivity(
            this, 0, Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT,
        )
        val stop = PendingIntent.getService(
            this, 1, Intent(this, BubbleService::class.java).setAction(ACTION_STOP),
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT,
        )
        val notification = Notification.Builder(this, CHANNEL_ID)
            .setSmallIcon(R.drawable.ic_notification)
            .setContentTitle("悟醬待命中")
            .setContentText("點這裡開設定，或按「收起頭像」暫時關掉")
            .setContentIntent(open)
            .setOngoing(true)
            .addAction(
                Notification.Action.Builder(
                    Icon.createWithResource(this, R.drawable.ic_notification), "收起頭像", stop,
                ).build(),
            )
            .build()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            startForeground(NOTIFICATION_ID, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE)
        } else {
            startForeground(NOTIFICATION_ID, notification)
        }
    }

    // ---- head ----------------------------------------------------------------------

    private fun headDrawable(): Drawable = HeadImage.load(ui, prefs.headStyle)

    private fun showHead() {
        val view = ImageView(ui).apply {
            setImageDrawable(headDrawable())
            scaleType = ImageView.ScaleType.FIT_CENTER
            contentDescription = getString(R.string.head_description)
            setOnTouchListener(HeadTouch())
            setOnClickListener { openPanel() }
        }
        val s = screen()
        headParams.width = headSize
        headParams.height = headSize
        headParams.x = dockX(s, prefs.onRight, tucked = false)
        headParams.y = clampY(s, (prefs.yFraction * s.height).roundToInt())
        wm.addView(view, headParams)
        head = view
        prefs.sp.registerOnSharedPreferenceChangeListener(this)
        scheduleTuck()
    }

    /** Rotation, resolution change or a new head size: put the head back on its side. */
    private fun onScreenChanged() {
        val view = head ?: return
        closePanel(animate = false)
        mover?.cancel()
        val s = screen()
        headParams.width = headSize
        headParams.height = headSize
        headParams.x = dockX(s, prefs.onRight, tucked = false)
        headParams.y = clampY(s, (prefs.yFraction * s.height).roundToInt())
        if (view.isAttachedToWindow) wm.updateViewLayout(view, headParams)
        view.animate().alpha(1f).setDuration(150).start()
        scheduleTuck()
    }

    private inner class HeadTouch : View.OnTouchListener {
        private val slop = ViewConfiguration.get(ui).scaledTouchSlop
        private var downX = 0f
        private var downY = 0f
        private var startX = 0
        private var startY = 0
        private var dragging = false

        override fun onTouch(v: View, e: MotionEvent): Boolean {
            when (e.actionMasked) {
                MotionEvent.ACTION_DOWN -> {
                    mover?.cancel()
                    handler.removeCallbacks(tuckRunnable)
                    v.animate().alpha(1f).scaleX(0.9f).scaleY(0.9f).setDuration(90).start()
                    downX = e.rawX
                    downY = e.rawY
                    startX = headParams.x
                    startY = headParams.y
                    dragging = false
                }
                MotionEvent.ACTION_MOVE -> {
                    val dx = e.rawX - downX
                    val dy = e.rawY - downY
                    if (!dragging && hypot(dx, dy) > slop) dragging = true
                    if (dragging) {
                        headParams.x = startX + dx.roundToInt()
                        headParams.y = startY + dy.roundToInt()
                        wm.updateViewLayout(v, headParams)
                    }
                }
                MotionEvent.ACTION_UP -> {
                    v.animate().scaleX(1f).scaleY(1f).setDuration(120).start()
                    if (dragging) snapToEdge() else v.performClick()
                }
                MotionEvent.ACTION_CANCEL -> {
                    v.animate().scaleX(1f).scaleY(1f).setDuration(120).start()
                    snapToEdge()
                }
            }
            return true
        }
    }

    private fun snapToEdge() {
        val s = screen()
        val right = headParams.x + headSize / 2 > s.width / 2
        val y = clampY(s, headParams.y)
        prefs.savePosition(right, y.toFloat() / s.height)
        moveHead(dockX(s, right, tucked = false), y, OvershootInterpolator(1.1f)) { scheduleTuck() }
    }

    private fun scheduleTuck() {
        handler.removeCallbacks(tuckRunnable)
        handler.postDelayed(tuckRunnable, IDLE_DELAY_MS)
    }

    private fun tuck() {
        val view = head ?: return
        if (panel != null) return
        view.animate().alpha(prefs.idleAlpha / 100f).setDuration(400).start()
        if (prefs.autoTuck) {
            moveHead(dockX(screen(), prefs.onRight, tucked = true), headParams.y, DecelerateInterpolator())
        }
    }

    private fun moveHead(x: Int, y: Int, interpolator: android.animation.TimeInterpolator, onEnd: (() -> Unit)? = null) {
        val view = head ?: return
        mover?.cancel()
        val fromX = headParams.x
        val fromY = headParams.y
        mover = ValueAnimator.ofFloat(0f, 1f).apply {
            duration = 240
            this.interpolator = interpolator
            addUpdateListener {
                val f = it.animatedFraction
                headParams.x = fromX + ((x - fromX) * f).roundToInt()
                headParams.y = fromY + ((y - fromY) * f).roundToInt()
                if (view.isAttachedToWindow) wm.updateViewLayout(view, headParams)
            }
            addListener(object : AnimatorListenerAdapter() {
                private var cancelled = false

                override fun onAnimationCancel(animation: Animator) {
                    cancelled = true
                }

                override fun onAnimationEnd(animation: Animator) {
                    if (!cancelled) onEnd?.invoke()
                }
            })
            start()
        }
    }

    // ---- screen geometry -----------------------------------------------------------------

    private class Screen(val width: Int, val height: Int, val insets: Insets)

    private fun screen(): Screen {
        val metrics = wm.currentWindowMetrics
        // Status bar, gesture bar and the punch-hole camera are kept clear.
        val insets = metrics.windowInsets.getInsetsIgnoringVisibility(
            WindowInsets.Type.systemBars() or WindowInsets.Type.displayCutout(),
        )
        return Screen(metrics.bounds.width(), metrics.bounds.height(), insets)
    }

    private fun dockX(s: Screen, right: Boolean, tucked: Boolean): Int {
        val size = headSize
        val hidden = if (tucked) (size * TUCK_FRACTION).roundToInt() else 0
        return if (right) s.width - s.insets.right - size + hidden else s.insets.left - hidden
    }

    private fun clampY(s: Screen, y: Int): Int {
        val margin = ui.dp(8)
        val top = s.insets.top + margin
        val bottom = s.height - s.insets.bottom - headSize - margin
        return y.coerceIn(top, max(top, bottom))
    }

    // ---- panel -----------------------------------------------------------------------

    private fun openPanel() {
        if (panel != null) return
        val view = head ?: return
        handler.removeCallbacks(tuckRunnable)
        mover?.cancel()

        val s = screen()
        val size = headSize
        val right = prefs.onRight
        val headX = dockX(s, right, tucked = false)
        val headY = headParams.y
        val gap = ui.dp(10)
        val margin = ui.dp(12)

        val root = PanelRoot(ui) { closePanel() }
        val scrim = View(ui).apply {
            setBackgroundColor(ui.getColor(R.color.scrim))
            setOnClickListener { closePanel() }
        }
        root.addView(scrim, FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT))

        // A copy of the head sits exactly where the real one is and closes the panel.
        val clone = ImageView(ui).apply {
            setImageDrawable(headDrawable())
            contentDescription = "收起"
            setOnClickListener { closePanel() }
            translationX = (headParams.x - headX).toFloat()
        }
        root.addView(clone, FrameLayout.LayoutParams(size, size).apply {
            leftMargin = headX
            topMargin = headY
        })

        val available = if (right) {
            headX - gap - (s.insets.left + margin)
        } else {
            s.width - s.insets.right - margin - (headX + size + gap)
        }
        val padding = ui.dp(12)
        val columns = ((available - 2 * padding) / ui.dp(CELL_DP)).coerceIn(2, 4)
        val maxHeight = s.height - s.insets.top - s.insets.bottom - 2 * margin
        val (card, scroll) = buildCard(columns, padding)
        val unbounded = View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED)
        card.measure(unbounded, unbounded)
        if (card.measuredHeight > maxHeight) {
            scroll.layoutParams.height = max(ui.dp(CELL_DP), scroll.measuredHeight - (card.measuredHeight - maxHeight))
            card.measure(unbounded, unbounded)
        }
        val cardW = card.measuredWidth
        val cardH = card.measuredHeight
        val cardX = if (right) headX - gap - cardW else headX + size + gap
        val cardY = (headY + size / 2 - cardH / 2)
            .coerceIn(s.insets.top + margin, max(s.insets.top + margin, s.height - s.insets.bottom - margin - cardH))
        root.addView(card, FrameLayout.LayoutParams(cardW, cardH).apply {
            leftMargin = max(cardX, s.insets.left + margin)
            topMargin = cardY
        })

        val lp = overlayParams(focusable = true).apply {
            width = s.width
            height = s.height
        }
        view.performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
        wm.addView(root, lp)
        panel = root
        view.visibility = View.INVISIBLE

        clone.animate().translationX(0f).setDuration(160).start()
        scrim.alpha = 0f
        scrim.animate().alpha(1f).setDuration(160).start()
        card.pivotX = if (right) cardW.toFloat() else 0f
        card.pivotY = (headY + size / 2 - cardY).coerceIn(0, cardH).toFloat()
        card.alpha = 0f
        card.scaleX = 0.85f
        card.scaleY = 0.85f
        card.animate().alpha(1f).scaleX(1f).scaleY(1f).setDuration(180)
            .setInterpolator(DecelerateInterpolator()).start()
    }

    private fun closePanel(animate: Boolean = true) {
        val root = panel ?: return
        panel = null
        head?.let { view ->
            headParams.x = dockX(screen(), prefs.onRight, tucked = false)
            if (view.isAttachedToWindow) wm.updateViewLayout(view, headParams)
            view.alpha = 1f
            view.visibility = View.VISIBLE
        }
        if (animate) {
            root.animate().alpha(0f).setDuration(120).withEndAction { removeWindow(root) }.start()
        } else {
            removeWindow(root)
        }
        scheduleTuck()
    }

    private fun buildCard(columns: Int, padding: Int): Pair<LinearLayout, ScrollView> {
        val c = ui
        val textColor = c.getColor(R.color.text)
        val card = LinearLayout(c).apply {
            orientation = LinearLayout.VERTICAL
            background = roundRect(c.getColor(R.color.panel_bg), c.dp(24).toFloat(), c.getColor(R.color.panel_stroke), c.dp(1))
            elevation = c.dp(8).toFloat()
            setPadding(padding, c.dp(10), padding, padding)
            isClickable = true // taps on the card must not reach the scrim
        }

        val header = LinearLayout(c).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(c.dp(6), 0, 0, c.dp(4))
        }
        header.addView(TextView(c).apply {
            text = "悟の快捷"
            textSize = 15f
            typeface = Typeface.DEFAULT_BOLD
            setTextColor(textColor)
        }, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        header.addView(TextView(c).apply {
            text = "編輯"
            textSize = 13f
            setTextColor(c.getColor(R.color.accent))
            setPadding(c.dp(12), c.dp(6), c.dp(12), c.dp(6))
            background = ripple(c.getColor(R.color.ripple), c.dp(14).toFloat())
            setOnClickListener {
                closePanel(animate = false)
                openSettings(addApp = false)
            }
        })
        card.addView(header)

        val grid = GridLayout(c).apply { columnCount = columns }
        val items = prefs.items
        for (item in items) {
            grid.addView(tile(Shortcuts.icon(c, item), item.label) {
                closePanel(animate = false)
                if (!Shortcuts.launch(this, item)) {
                    Toast.makeText(this, "找不到「${item.label}」，請按「編輯」重新選擇", Toast.LENGTH_SHORT).show()
                }
            })
        }
        grid.addView(tile(LetterIcon("+", c.getColor(R.color.accent), c.getColor(R.color.on_accent)), "新增") {
            closePanel(animate = false)
            openSettings(addApp = true)
        })
        val scroll = ScrollView(c).apply {
            isVerticalScrollBarEnabled = false
            addView(grid)
        }
        card.addView(scroll, LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT))
        return card to scroll
    }

    private fun tile(icon: Drawable, label: String, onClick: () -> Unit): View {
        val c = ui
        return LinearLayout(c).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER_HORIZONTAL
            setPadding(c.dp(4), c.dp(8), c.dp(4), c.dp(6))
            background = ripple(c.getColor(R.color.ripple), c.dp(16).toFloat())
            contentDescription = label
            setOnClickListener { onClick() }
            addView(ImageView(c).apply { setImageDrawable(icon) }, LinearLayout.LayoutParams(c.dp(46), c.dp(46)))
            addView(TextView(c).apply {
                text = label
                textSize = 12f
                maxLines = 1
                ellipsize = TextUtils.TruncateAt.END
                gravity = Gravity.CENTER
                setTextColor(c.getColor(R.color.text))
            }, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT).apply {
                topMargin = c.dp(4)
            })
            layoutParams = GridLayout.LayoutParams().apply {
                width = c.dp(CELL_DP)
                height = ViewGroup.LayoutParams.WRAP_CONTENT
            }
        }
    }

    private fun openSettings(addApp: Boolean) {
        startActivity(
            Intent(this, MainActivity::class.java)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                .putExtra(MainActivity.EXTRA_ADD_APP, addApp),
        )
    }

    private fun removeWindow(view: View) {
        if (view.isAttachedToWindow) wm.removeView(view)
    }

    /** Full-screen panel window that closes on the back gesture. */
    private class PanelRoot(context: Context, private val onBack: () -> Unit) : FrameLayout(context) {
        override fun dispatchKeyEvent(event: KeyEvent): Boolean {
            if (event.keyCode == KeyEvent.KEYCODE_BACK) {
                if (event.action == KeyEvent.ACTION_UP) onBack()
                return true
            }
            return super.dispatchKeyEvent(event)
        }
    }

    companion object {
        const val ACTION_STOP = "com.nanachi.gojodock.STOP"
        private const val TAG = "GojoDock"
        private const val CHANNEL_ID = "bubble"
        private const val NOTIFICATION_ID = 1
        private const val IDLE_DELAY_MS = 2500L
        private const val TUCK_FRACTION = 0.4f
        private const val CELL_DP = 72

        @Volatile
        var running = false
            private set

        fun start(context: Context) {
            try {
                context.startForegroundService(Intent(context, BubbleService::class.java))
            } catch (e: Exception) {
                Log.w(TAG, "Could not start the floating head", e)
            }
        }

        fun stop(context: Context) {
            context.stopService(Intent(context, BubbleService::class.java))
        }

        /** Window coordinates are measured from the physical top-left corner of the screen. */
        private fun overlayParams(focusable: Boolean) = WindowManager.LayoutParams(
            WindowManager.LayoutParams.WRAP_CONTENT,
            WindowManager.LayoutParams.WRAP_CONTENT,
            WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
            WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN or
                WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS or
                (if (focusable) 0 else WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE),
            PixelFormat.TRANSLUCENT,
        ).apply {
            gravity = Gravity.TOP or Gravity.START
            layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS
            setFitInsetsTypes(0)
            title = "GojoDock"
        }
    }
}
