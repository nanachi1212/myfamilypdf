package com.nanachi.gojodock

import android.content.Context
import android.content.res.ColorStateList
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.ColorFilter
import android.graphics.Paint
import android.graphics.PixelFormat
import android.graphics.Typeface
import android.graphics.drawable.Drawable
import android.graphics.drawable.GradientDrawable
import android.graphics.drawable.RippleDrawable
import kotlin.math.abs
import kotlin.math.min
import kotlin.math.roundToInt

fun Context.dp(value: Number): Int = (value.toFloat() * resources.displayMetrics.density).roundToInt()

fun roundRect(color: Int, radius: Float, strokeColor: Int? = null, strokeWidth: Int = 0) =
    GradientDrawable().apply {
        shape = GradientDrawable.RECTANGLE
        cornerRadius = radius
        setColor(color)
        if (strokeColor != null) setStroke(strokeWidth, strokeColor)
    }

/** Touch feedback; with no [content] the view stays transparent until pressed. */
fun ripple(rippleColor: Int, radius: Float, content: Drawable? = null) =
    RippleDrawable(ColorStateList.valueOf(rippleColor), content, roundRect(Color.WHITE, radius))

/** Round icon with the first letter of a label, used for web links. */
class LetterIcon(label: String, background: Int, foreground: Int) : Drawable() {
    private val letter = label.trim().let {
        if (it.isEmpty()) "?" else String(Character.toChars(it.codePointAt(0))).uppercase()
    }
    private val circle = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = background }
    private val text = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = foreground
        textAlign = Paint.Align.CENTER
        typeface = Typeface.DEFAULT_BOLD
    }

    override fun draw(canvas: Canvas) {
        val b = bounds
        val r = min(b.width(), b.height()) / 2f
        canvas.drawCircle(b.exactCenterX(), b.exactCenterY(), r * 0.92f, circle)
        text.textSize = r * 0.95f
        canvas.drawText(letter, b.exactCenterX(), b.exactCenterY() - (text.descent() + text.ascent()) / 2, text)
    }

    override fun setAlpha(alpha: Int) {
        circle.alpha = alpha
        text.alpha = alpha
    }

    override fun setColorFilter(colorFilter: ColorFilter?) {
        circle.colorFilter = colorFilter
        text.colorFilter = colorFilter
    }

    @Deprecated("Deprecated in Java")
    override fun getOpacity() = PixelFormat.TRANSLUCENT

    companion object {
        private val PALETTE = intArrayOf(R.color.sage, R.color.rose, R.color.oat, R.color.mist)

        fun forLabel(context: Context, label: String) =
            LetterIcon(label, context.getColor(PALETTE[abs(label.hashCode() % PALETTE.size)]), Color.WHITE)
    }
}
