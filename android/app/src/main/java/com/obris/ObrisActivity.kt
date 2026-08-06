package com.obris

import android.content.res.AssetManager
import android.os.Bundle
import android.util.Log
import android.view.Choreographer
import android.view.MotionEvent
import android.view.SurfaceView
import androidx.appcompat.app.AppCompatActivity
import com.google.android.filament.*
import com.google.android.filament.gltfio.AssetLoader
import com.google.android.filament.gltfio.FilamentAsset
import com.google.android.filament.gltfio.Gltfio
import com.google.android.filament.gltfio.MaterialProvider
import com.google.android.filament.gltfio.ResourceLoader
import com.google.android.filament.utils.UiHelper
import java.io.InputStream
import java.nio.ByteBuffer

/**
 * Obris — Lightweight GLB Renderer for Kotlin + Native C++
 */
class ObrisActivity : AppCompatActivity() {

    private lateinit var surfaceView: SurfaceView
    private lateinit var uiHelper: UiHelper

    private var engine: Engine? = null
    private var renderer: Renderer? = null
    private var scene: Scene? = null
    private var view: View? = null
    private var camera: Camera? = null

    private var swapChain: SwapChain? = null
    private var skybox: Skybox? = null

    private var assetLoader: AssetLoader? = null
    private var materialProvider: MaterialProvider? = null
    private var loadedAsset: FilamentAsset? = null

    private var cameraEntity = 0
    private var isRendering = false
    private var nativePtr: Long = 0

    // Load native C++ obris_shared library
    init {
        try { System.loadLibrary("obris_shared") } catch (t: Throwable) { Log.e("Obris", "loadLibrary error: ${t.message}") }
    }

    private external fun nativeCreate(assetManager: AssetManager): Long
    private external fun nativeDestroy()

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

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Init Filament native libraries in Android JVM
        Filament.init()
        Gltfio.init()

        // Init native C++ Obris engine
        nativePtr = nativeCreate(assets)

        surfaceView = SurfaceView(this)
        setContentView(surfaceView)

        setupFilament()
        setupUiHelper()
    }

    private fun setupFilament() {
        val e = Engine.create()
        engine = e

        val r = e.createRenderer()
        renderer = r

        val s = e.createScene()
        scene = s

        val v = e.createView()
        view = v

        val camEnt = EntityManager.get().create()
        cameraEntity = camEnt
        val cam = e.createCamera(camEnt)
        camera = cam

        v.scene = s
        v.camera = cam

        // Studio Gray Viewport
        r.clearOptions = r.clearOptions.apply {
            clearColor = floatArrayOf(0.22f, 0.23f, 0.25f, 1.0f)
            clear = true
        }

        val sb = Skybox.Builder()
            .color(0.22f, 0.23f, 0.25f, 1.0f)
            .build(e)
        s.skybox = sb
        skybox = sb

        // Lighting
        val sun = EntityManager.get().create()
        LightManager.Builder(LightManager.Type.DIRECTIONAL)
            .color(1.0f, 0.98f, 0.94f)
            .intensity(110000.0f)
            .direction(-0.6f, -1.0f, -0.4f)
            .castShadows(true)
            .build(e, sun)
        s.addEntity(sun)

        val fill = EntityManager.get().create()
        LightManager.Builder(LightManager.Type.DIRECTIONAL)
            .color(0.5f, 0.65f, 0.85f)
            .intensity(35000.0f)
            .direction(0.6f, 0.8f, 0.5f)
            .castShadows(false)
            .build(e, fill)
        s.addEntity(fill)

        // Setup gltfio Loader
        val matProvider = MaterialProvider.createUbershaderProvider(e)
        materialProvider = matProvider
        assetLoader = AssetLoader(e, matProvider, EntityManager.get())

        // Load model Project 9.glb from assets!
        loadGlbModel("Project 9.glb")
    }

    private fun loadGlbModel(assetPath: String) {
        try {
            val input: InputStream = assets.open(assetPath)
            val bytes = input.readBytes()
            input.close()

            val buffer = ByteBuffer.allocateDirect(bytes.size).put(bytes)
            buffer.rewind()

            val loader = assetLoader
            val e = engine
            val s = scene

            if (loader != null && e != null && s != null) {
                val asset = loader.createAsset(buffer)
                if (asset != null) {
                    loadedAsset = asset
                    val resourceLoader = ResourceLoader(e)
                    resourceLoader.loadResources(asset)
                    s.addEntities(asset.entities)
                    Log.i("Obris", "Loaded GLB model: $assetPath (${asset.entities.size} entities)")
                }
            }
        } catch (e: Exception) {
            Log.e("Obris", "Failed to load GLB asset $assetPath: ${e.message}", e)
        }
    }

    private fun setupUiHelper() {
        uiHelper = UiHelper(UiHelper.ContextErrorPolicy.DONT_CHECK)
        uiHelper.renderCallback = object : UiHelper.RendererCallback {
            override fun onNativeWindowChanged(surface: android.view.Surface) {
                val e = engine ?: return
                swapChain?.let { e.destroySwapChain(it) }
                swapChain = e.createSwapChain(surface)
            }

            override fun onDetachedFromUnunderlyingWindow() {
                val e = engine ?: return
                swapChain?.let {
                    e.destroySwapChain(it)
                    e.flushFrame()
                }
                swapChain = null
            }

            override fun onResized(width: Int, height: Int) {
                val v = view ?: return
                val cam = camera ?: return
                v.viewport = Viewport(0, 0, width, height)
                val aspect = width.toDouble() / height.toDouble()
                cam.setProjection(60.0, aspect, 0.1, 1000.0)
                cam.lookAt(0.0, 2.5, 5.0, 0.0, 0.5, 0.0, 0.0, 1.0, 0.0)
            }
        }
        uiHelper.attachTo(surfaceView)
    }

    // Frame Loop
    private val frameCallback = object : Choreographer.FrameCallback {
        override fun doFrame(frameTimeNanos: Long) {
            if (isRendering) {
                val r = renderer
                val v = view
                val sc = swapChain
                if (uiHelper.isReadyToRender && r != null && v != null && sc != null) {
                    if (r.beginFrame(sc, frameTimeNanos)) {
                        r.render(v)
                        r.endFrame()
                    }
                }
                Choreographer.getInstance().postFrameCallback(this)
            }
        }
    }

    override fun onResume() {
        super.onResume()
        isRendering = true
        Choreographer.getInstance().postFrameCallback(frameCallback)
    }

    override fun onPause() {
        isRendering = false
        super.onPause()
    }

    override fun onDestroy() {
        isRendering = false
        uiHelper.detach()

        val e = engine
        if (e != null) {
            loadedAsset?.let {
                scene?.removeEntities(it.entities)
                assetLoader?.destroyAsset(it)
            }
            assetLoader?.destroy()
            materialProvider?.destroy()

            skybox?.let { e.destroySkybox(it) }
            view?.let { e.destroyView(it) }
            scene?.let { e.destroyScene(it) }
            renderer?.let { e.destroyRenderer(it) }
            swapChain?.let { e.destroySwapChain(it) }

            e.destroy()
        }

        if (nativePtr != 0L) {
            nativeDestroy()
            nativePtr = 0L
        }

        super.onDestroy()
    }

    // Touch controls
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

                    val x = orbitDistance * Math.cos(radPitch) * Math.sin(radYaw)
                    val y = orbitDistance * Math.sin(radPitch)
                    val z = orbitDistance * Math.cos(radPitch) * Math.cos(radYaw)

                    camera?.lookAt(x, y + 0.5, z, 0.0, 0.5, 0.0, 0.0, 1.0, 0.0)
                } else if (event.pointerCount == 2) {
                    orbitDistance = (orbitDistance - dy * 0.02f).coerceIn(1f, 20f)
                }

                lastTouchX = event.x
                lastTouchY = event.y
            }
        }
        return true
    }

    // Public API wrappers for Native Audio / JSON / Crypto
    fun playLobbySound(path: String) {
        val sound = nativeLoadSound(path)
        if (sound != 0) nativePlaySound(sound, 0.5f, true)
    }

    fun loadManifest(path: String): String? {
        return nativeLoadJSON(path)
    }
}
