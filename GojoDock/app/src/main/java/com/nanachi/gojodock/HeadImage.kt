package com.nanachi.gojodock

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.BitmapShader
import android.graphics.Canvas
import android.graphics.ImageDecoder
import android.graphics.Matrix
import android.graphics.Paint
import android.graphics.Shader
import android.graphics.drawable.BitmapDrawable
import android.graphics.drawable.Drawable
import android.net.Uri
import android.util.Log
import java.io.File
import kotlin.math.max
import kotlin.math.min

/** The picture used for the floating head: one of the two built-in Gojo heads or the user's photo. */
object HeadImage {
    const val STYLE_SMILE = "smile"
    const val STYLE_BLINDFOLD = "blindfold"
    const val STYLE_CUSTOM = "custom"

    private const val FILE_NAME = "custom_head.png"
    private const val OUT_SIZE = 320

    private fun file(context: Context) = File(context.filesDir, FILE_NAME)

    fun hasCustom(context: Context) = file(context).exists()

    fun load(context: Context, style: String): Drawable {
        if (style == STYLE_CUSTOM) {
            BitmapFactory.decodeFile(file(context).path)?.let {
                return BitmapDrawable(context.resources, it)
            }
        }
        val res = if (style == STYLE_BLINDFOLD) R.drawable.head_blindfold else R.drawable.head_smile
        return context.getDrawable(res)!!
    }

    /** Decodes the picked image, crops the centre square into a round badge and stores it. */
    fun saveCustom(context: Context, uri: Uri): Boolean = try {
        val source = ImageDecoder.createSource(context.contentResolver, uri)
        val bitmap = ImageDecoder.decodeBitmap(source) { decoder, info, _ ->
            decoder.allocator = ImageDecoder.ALLOCATOR_SOFTWARE
            val longest = max(info.size.width, info.size.height)
            if (longest > 1600) decoder.setTargetSampleSize(longest / 1600)
        }
        val badge = roundBadge(bitmap, context.getColor(R.color.head_rim), context.getColor(R.color.launcher_bg))
        file(context).outputStream().use { badge.compress(Bitmap.CompressFormat.PNG, 100, it) }
        true
    } catch (e: Exception) {
        Log.w("GojoDock", "Could not read the picked image", e)
        false
    }

    private fun roundBadge(src: Bitmap, rimColor: Int, diskColor: Int): Bitmap {
        val side = min(src.width, src.height)
        val square = Bitmap.createBitmap(src, (src.width - side) / 2, (src.height - side) / 2, side, side)
        val rim = OUT_SIZE * 0.045f
        val inner = (OUT_SIZE - 2 * rim).toInt()
        val scaled = Bitmap.createScaledBitmap(square, inner, inner, true)

        val out = Bitmap.createBitmap(OUT_SIZE, OUT_SIZE, Bitmap.Config.ARGB_8888)
        val canvas = Canvas(out)
        val c = OUT_SIZE / 2f
        canvas.drawCircle(c, c, c, Paint(Paint.ANTI_ALIAS_FLAG).apply { color = rimColor })
        // Shows through transparent pictures such as cut-out stickers.
        canvas.drawCircle(c, c, c - rim, Paint(Paint.ANTI_ALIAS_FLAG).apply { color = diskColor })
        val photo = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            shader = BitmapShader(scaled, Shader.TileMode.CLAMP, Shader.TileMode.CLAMP).apply {
                setLocalMatrix(Matrix().apply { setTranslate(rim, rim) })
            }
        }
        canvas.drawCircle(c, c, c - rim, photo)
        return out
    }
}
