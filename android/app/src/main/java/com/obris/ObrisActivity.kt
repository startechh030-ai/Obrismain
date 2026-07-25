package com.obris

import android.content.res.AssetManager
import android.os.Bundle
import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
import androidx.appcompat.app.AppCompatActivity

/**
 * Obris — Lightweight GLB Renderer for Kotlin
 */
class ObrisActivity : AppCompatActivity() {

    private lateinit var surfaceView: SurfaceView
    private var nativePtr: Long = 0

    // ── Load library (class-level, NOT in companion object) ────
    init { System.loadLibrary("obris_shared") }

    // ── Native methods (class-level, NOT companion) ────────────
    // This avoids JNI name mangling with _00024Companion

    private external fun nativeCreate(
        surface: android.view.Surface,
        assetManager: AssetManager,
        width: Int, height: Int,
        iblPath: String?
    ): Long

    private external fun nativeDestroy()
    private external fun nativeRenderFrame()
    private external fun nativeResize(w: Int, h: Int)

    private external fun nativeSetCamera(
        x: Float, y: Float, z: Float,
        tx: Float, ty: Float, tz: Float,
        fov: Float
    )

    private external fun nativeAddLight(
        type: Int,
        r: Float, g: Float, b: Float,
        intensity: Float,
        dx: Float, dy: Float, dz: Float
    ): Int

    private external fun nativeLoadModel(
        path: String,
        px: Float, py: Float, pz: Float,
        rx: Float, ry: Float, rz: Float, rw: Float,
        sx: Float, sy: Float, sz: Float
    ): Int

    private external fun nativeUnloadModel(handle: Int)
    private external fun nativeSetModelTransform(
        handle: Int,
        px: Float, py: Float, pz: Float,
        rx: Float, ry: Float, rz: Float, rw: Float,
        sx: Float, sy: Float, sz: Float
    )
    private external fun nativeSetModelVisible(handle: Int, visible: Boolean)

    private external fun nativeLoadIBL(path: String): Boolean
    private external fun nativeSetIBLIntensity(intensity: Float)
    private external fun nativeSetIBLRotation(degrees: Float)

    private external fun nativeLoadSound(path: String): Int
    private external fun nativePlaySound(sound: Int, volume: Float, looping: Boolean): Int
    private external fun nativeStopSound(playback: Int)
    private external fun nativeSetMasterVolume(volume: Float)

    private external fun nativeLoadJSON(path: String): String?
    private external fun nativeJSONGetString(json: String, key: String): String?
    private external fun nativeJSONGetFloat(json: String, key: String): Float
    private external fun nativeJSONGetInt(json: String, key: String): Int

    private external fun nativeEncrypt(key: ByteArray, data: ByteArray): ByteArray?
    private external fun nativeDecrypt(key: ByteArray, data: ByteArray): ByteArray?

    // ── Lifecycle ─────────────────────────────────────────────
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        surfaceView = SurfaceView(this)
        surfaceView.holder.addCallback(surfaceCallback)
        setContentView(surfaceView)
    }

    override fun onResume() {
        super.onResume()
    }

    override fun onPause() {
        super.onPause()
    }

    override fun onDestroy() {
        if (nativePtr != 0L) {
            nativeDestroy()
            nativePtr = 0L
        }
        super.onDestroy()
    }

    // ── Surface Callback ──────────────────────────────────────
    private val surfaceCallback = object : SurfaceHolder.Callback {
        override fun surfaceCreated(holder: SurfaceHolder) {
            val w = holder.surfaceFrame.width().coerceAtLeast(720)
            val h = holder.surfaceFrame.height().coerceAtLeast(1280)

            nativePtr = nativeCreate(
                holder.surface,
                this@ObrisActivity.assets,
                w, h,
                null
            )

            setupDefaultScene()
        }

        override fun surfaceChanged(holder: SurfaceHolder, format: Int, w: Int, h: Int) {
            if (nativePtr != 0L) nativeResize(w, h)
        }

        override fun surfaceDestroyed(holder: SurfaceHolder) {
            // Engine handles this
        }
    }

    // ── Default Scene Setup ────────────────────────────────────
    private fun setupDefaultScene() {
        nativeSetCamera(0f, 2.5f, 5f, 0f, 1f, 0f, 60f)

        nativeAddLight(0, 1f, 0.95f, 0.85f, 80000f, -0.5f, -1f, -0.3f)
        nativeAddLight(0, 0.3f, 0.4f, 0.6f, 20000f, 0.5f, 0.5f, 0.5f)

        nativeSetIBLIntensity(0.6f)
    }

    // ── Touch Controls (orbit camera) ─────────────────────────
    private var lastTouchX = 0f
    private var lastTouchY = 0f
    private var orbitYaw = 0f
    private var orbitPitch = -15f
    private var orbitDistance = 5f

    override fun onTouchEvent(event: MotionEvent): Boolean {
        when (event.action) {
            MotionEvent.ACTION_DOWN -> {
                lastTouchX = event.x
                lastTouchY = event.y
            }
            MotionEvent.ACTION_MOVE -> {
                val dx = event.x - lastTouchX
                val dy = event.y - lastTouchY

                if (event.pointerCount == 1) {
                    orbitYaw += dx * 0.3f
                    orbitPitch = (orbitPitch + dy * 0.3f).coerceIn(-89f, 89f)

                    val radYaw = Math.toRadians(orbitYaw.toDouble())
                    val radPitch = Math.toRadians(orbitPitch.toDouble())

                    val x = (orbitDistance * Math.cos(radPitch) * Math.sin(radYaw)).toFloat()
                    val y = (orbitDistance * Math.sin(radPitch)).toFloat()
                    val z = (orbitDistance * Math.cos(radPitch) * Math.cos(radYaw)).toFloat()

                    nativeSetCamera(x, y + 1.5f, z, 0f, 1f, 0f, 60f)
                } else if (event.pointerCount == 2) {
                    orbitDistance = (orbitDistance - dy * 0.02f).coerceIn(1f, 20f)
                }

                lastTouchX = event.x
                lastTouchY = event.y
            }
        }
        return true
    }

    // ── Public API wrappers ───────────────────────────────────
    fun loadCharacterModel(path: String): Int {
        return nativeLoadModel(path, 0f, 0f, 0f, 0f, 0f, 0f, 1f, 1f, 1f, 1f)
    }

    fun loadSceneProp(path: String, x: Float, y: Float, z: Float): Int {
        return nativeLoadModel(path, x, y, z, 0f, 0f, 0f, 1f, 1f, 1f, 1f)
    }

    fun playLobbyMusic(path: String) {
        val sound = nativeLoadSound(path)
        if (sound != 0) nativePlaySound(sound, 0.5f, true)
    }

    fun loadManifest(path: String): String? {
        return nativeLoadJSON(path)
    }
}
