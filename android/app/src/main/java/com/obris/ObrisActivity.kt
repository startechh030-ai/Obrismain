package com.obris

import android.content.res.AssetManager
import android.os.Bundle
import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
import androidx.appcompat.app.AppCompatActivity

/**
 * Obris — Lightweight GLB Renderer for Kotlin
 *
 * This is a test wrapper activity that demonstrates the full API
 * of libs.obris.so. It can render:
 *   - GLB models with PBR materials (via Filament)
 *   - HDR environment maps (IBL)
 *   - Directional/point lights
 *   - Sound effects (via miniaudio)
 *   - JSON asset manifest reading
 *   - Encryption/decryption (via libsodium)
 *
 * Camera controls (for testing):
 *   - Drag to orbit
 *   - Pinch to zoom
 */
class ObrisActivity : AppCompatActivity() {

    private lateinit var surfaceView: SurfaceView

    private var nativePtr: Long = 0

    // ── Native methods (libs.obris.so) ─────────────────────────
    companion object {
        init { System.loadLibrary("obris_shared") }

        // Lifecycle
        private external fun nativeCreate(
            surface: android.view.Surface,
            assetManager: AssetManager,
            width: Int, height: Int,
            iblPath: String?
        ): Long

        private external fun nativeDestroy()
        private external fun nativeRenderFrame()
        private external fun nativeResize(w: Int, h: Int)

        // Camera
        private external fun nativeSetCamera(
            x: Float, y: Float, z: Float,
            tx: Float, ty: Float, tz: Float,
            fov: Float
        )

        // Lights
        private external fun nativeAddLight(
            type: Int,
            r: Float, g: Float, b: Float,
            intensity: Float,
            dx: Float, dy: Float, dz: Float
        ): Int

        // Models
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

        // IBL
        private external fun nativeLoadIBL(path: String): Boolean
        private external fun nativeSetIBLIntensity(intensity: Float)
        private external fun nativeSetIBLRotation(degrees: Float)

        // Audio
        private external fun nativeLoadSound(path: String): Int
        private external fun nativePlaySound(sound: Int, volume: Float, looping: Boolean): Int
        private external fun nativeStopSound(playback: Int)
        private external fun nativeSetMasterVolume(volume: Float)

        // JSON
        private external fun nativeLoadJSON(path: String): String?
        private external fun nativeJSONGetString(json: String, key: String): String?
        private external fun nativeJSONGetFloat(json: String, key: String): Float
        private external fun nativeJSONGetInt(json: String, key: String): Int

        // Encryption
        private external fun nativeEncrypt(key: ByteArray, data: ByteArray): ByteArray?
        private external fun nativeDecrypt(key: ByteArray, data: ByteArray): ByteArray?
    }

    // ── Lifecycle ─────────────────────────────────────────────
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        surfaceView = SurfaceView(this)
        surfaceView.holder.addCallback(surfaceCallback)
        setContentView(surfaceView)
    }

    override fun onResume() {
        super.onResume()
        // Engine is created in surfaceCreated callback
    }

    override fun onPause() {
        super.onPause()
        // Rendering loop pauses
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

            // Create the native engine
            nativePtr = nativeCreate(
                holder.surface,
                assets,
                w, h,
                "environments/lobby_hdr.ktx"  // Default IBL — change as needed
            )

            // Setup default scene
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
        // Camera — lobby-style close camera
        nativeSetCamera(
            0.0f, 2.5f, 5.0f,   // eye
            0.0f, 1.0f, 0.0f,   // target
            60.0f                // FOV
        )

        // Main directional light (sun)
        nativeAddLight(
            0,                     // directional
            1.0f, 0.95f, 0.85f,   // warm white
            80000.0f,              // intensity
            -0.5f, -1.0f, -0.3f   // direction
        )

        // Fill light
        nativeAddLight(
            0,                     // directional
            0.3f, 0.4f, 0.6f,     // cool blue
            20000.0f,
            0.5f, 0.5f, 0.5f
        )

        // Set IBL intensity
        nativeSetIBLIntensity(0.6f)

        // Background color — dark blue-gray
        // (set via native)
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
                    // Orbit
                    orbitYaw += dx * 0.3f
                    orbitPitch = (orbitPitch + dy * 0.3f).coerceIn(-89f, 89f)

                    val radYaw = Math.toRadians(orbitYaw.toDouble())
                    val radPitch = Math.toRadians(orbitPitch.toDouble())

                    val x = (orbitDistance * Math.cos(radPitch) * Math.sin(radYaw)).toFloat()
                    val y = (orbitDistance * Math.sin(radPitch)).toFloat()
                    val z = (orbitDistance * Math.cos(radPitch) * Math.cos(radYaw)).toFloat()

                    nativeSetCamera(x, y + 1.5f, z, 0f, 1f, 0f, 60f)
                } else if (event.pointerCount == 2) {
                    // Pinch zoom
                    // (simplified — just use dy)
                    orbitDistance = (orbitDistance - dy * 0.02f).coerceIn(1f, 20f)
                }

                lastTouchX = event.x
                lastTouchY = event.y
            }
        }
        return true
    }

    // ── API wrappers for Kotlin consumers ─────────────────────
    // These are the public API that your Kotlin code will call.

    fun loadCharacterModel(path: String): Int {
        // Character at origin, life-size
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
