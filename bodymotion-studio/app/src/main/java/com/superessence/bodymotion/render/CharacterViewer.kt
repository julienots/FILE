package com.superessence.bodymotion.render

import android.annotation.SuppressLint
import android.content.Context
import android.view.MotionEvent
import android.view.TextureView
import com.google.android.filament.Camera
import com.google.android.filament.Engine
import com.google.android.filament.EntityManager
import com.google.android.filament.IndirectLight
import com.google.android.filament.LightManager
import com.google.android.filament.Renderer
import com.google.android.filament.Scene
import com.google.android.filament.Skybox
import com.google.android.filament.SwapChain
import com.google.android.filament.View
import com.google.android.filament.Viewport
import com.google.android.filament.android.DisplayHelper
import com.google.android.filament.android.UiHelper
import com.google.android.filament.gltfio.AssetLoader
import com.google.android.filament.gltfio.FilamentAsset
import com.google.android.filament.gltfio.ResourceLoader
import com.google.android.filament.gltfio.UbershaderProvider
import com.google.android.filament.utils.GestureDetector
import com.google.android.filament.utils.Manipulator
import com.superessence.bodymotion.math.Mat4
import com.superessence.bodymotion.math.Quat
import com.superessence.bodymotion.math.Vec3
import com.superessence.bodymotion.retarget.PoseEvaluator
import com.superessence.bodymotion.retarget.SkeletalPose
import com.superessence.bodymotion.rig.Skeleton
import java.nio.ByteBuffer
import java.nio.ByteOrder
import kotlin.math.max

/**
 * Viewport 3D temps réel (moteur Filament) : matériaux PBR, éclairage dynamique, ombres,
 * anti-aliasing, caméra orbitale. La pose du personnage est appliquée directement aux os du modèle.
 */
@SuppressLint("ClickableViewAccessibility")
class CharacterViewer(private val ctx: Context, private val textureView: TextureView, quality: Int) {
    val engine: Engine = Engine.create()
    private val renderer: Renderer = engine.createRenderer()
    private val scene: Scene = engine.createScene()
    private val view: View = engine.createView()
    private val cameraEntity = EntityManager.get().create()
    private val camera: Camera = engine.createCamera(cameraEntity)
    private val uiHelper = UiHelper(UiHelper.ContextErrorPolicy.DONT_CHECK)
    private val displayHelper = DisplayHelper(ctx)
    private var swapChain: SwapChain? = null
    private val materialProvider = UbershaderProvider(engine)
    private val assetLoader = AssetLoader(engine, materialProvider, EntityManager.get())
    private val resourceLoader = ResourceLoader(engine, true)
    private val sun = EntityManager.get().create()
    private val fill = EntityManager.get().create()
    private val ibl: IndirectLight
    private val skybox: Skybox
    private var manipulator: Manipulator = buildManipulator(Vec3(0.0, 0.9, 0.0), 3.2, Vec3(0.0, 0.0, 1.0))
    private var gestures = GestureDetector(textureView, manipulator)

    private var character: FilamentAsset? = null
    private var ground: FilamentAsset? = null
    private var skeleton: Skeleton? = null
    private var nodeEntities = IntArray(0)
    private var morphEntities = HashMap<Int, Int>()
    private var morphCounts = HashMap<Int, Int>()
    private var rootTransform = Mat4()
    var viewWidth = 1; private set
    var viewHeight = 1; private set
    private val eye = DoubleArray(3); private val target = DoubleArray(3); private val up = DoubleArray(3)
    private var destroyed = false

    init {
        scene.skybox = Skybox.Builder().color(0.035f, 0.04f, 0.05f, 1f).build(engine).also { skybox = it }
        ibl = IndirectLight.Builder()
            .irradiance(3, floatArrayOf(
                0.62f, 0.64f, 0.70f, // L00 : ambiance neutre légèrement froide
                0.10f, 0.12f, 0.16f, // L1-1 (haut)
                0.06f, 0.05f, 0.04f, // L10
                -0.03f, -0.02f, 0.0f, // L11
                0f, 0f, 0f, 0f, 0f, 0f, 0f, 0f, 0f, 0f, 0f, 0f, 0f, 0f, 0f,
            ))
            .intensity(32000f).build(engine)
        scene.indirectLight = ibl
        LightManager.Builder(LightManager.Type.DIRECTIONAL).color(1f, 0.97f, 0.93f).intensity(95000f)
            .direction(-0.35f, -1f, -0.55f).castShadows(quality >= 1)
            .shadowOptions(LightManager.ShadowOptions().apply { mapSize = if (quality >= 2) 2048 else 1024})
            .build(engine, sun)
        scene.addEntity(sun)
        // Lumière de contour cyan (accent de l'identité visuelle), sans ombre.
        LightManager.Builder(LightManager.Type.DIRECTIONAL).color(0.55f, 0.85f, 1f).intensity(22000f).direction(0.6f, -0.2f, 0.8f).castShadows(false).build(engine, fill)
        scene.addEntity(fill)
        view.scene = scene
        view.camera = camera
        applyQuality(quality)
        renderer.clearOptions = Renderer.ClearOptions().apply { clear = true; clearColor = floatArrayOf(0.035f, 0.04f, 0.05f, 1f) }
        uiHelper.renderCallback = object : UiHelper.RendererCallback {
            override fun onNativeWindowChanged(surface: android.view.Surface) {
                swapChain?.let { engine.destroySwapChain(it) }
                swapChain = engine.createSwapChain(surface)
                textureView.display?.let { displayHelper.attach(renderer, it) }
            }
            override fun onDetachedFromSurface() {
                displayHelper.detach()
                swapChain?.let { engine.destroySwapChain(it); engine.flushAndWait() }
                swapChain = null
            }
            override fun onResized(width: Int, height: Int) {
                viewWidth = max(1, width); viewHeight = max(1, height)
                view.viewport = Viewport(0, 0, viewWidth, viewHeight)
                updateProjection()
                manipulator.setViewport(viewWidth, viewHeight)
            }
        }
        uiHelper.attachTo(textureView)
        textureView.setOnTouchListener { _, e -> onTouch(e); true }
    }

    fun applyQuality(q: Int) {
        view.antiAliasing = if (q >= 1) View.AntiAliasing.FXAA else View.AntiAliasing.NONE
        view.multiSampleAntiAliasingOptions = View.MultiSampleAntiAliasingOptions().apply { enabled = q >= 2; sampleCount = 4 }
        view.ambientOcclusionOptions = View.AmbientOcclusionOptions().apply { enabled = q >= 2 }
        view.setShadowingEnabled(q >= 1)
        view.dynamicResolutionOptions = View.DynamicResolutionOptions().apply { enabled = q == 0 }
    }

    private fun updateProjection() {
        camera.setProjection(38.0, viewWidth.toDouble() / viewHeight, 0.03, 200.0, Camera.Fov.VERTICAL)
    }

    private fun buildManipulator(center: Vec3, dist: Double, forward: Vec3): Manipulator {
        val home = center + Vec3(forward.x * dist + dist * 0.25, dist * 0.18, forward.z * dist + 0.0)
        return Manipulator.Builder()
            .targetPosition(center.x.toFloat(), center.y.toFloat(), center.z.toFloat())
            .orbitHomePosition(home.x.toFloat(), home.y.toFloat(), home.z.toFloat())
            .upVector(0f, 1f, 0f)
            .zoomSpeed(0.012f)
            .orbitSpeed(0.006f, 0.006f)
            .viewport(viewWidth, viewHeight)
            .build(Manipulator.Mode.ORBIT)
    }

    fun onTouch(e: MotionEvent) = gestures.onTouchEvent(e)

    fun resetCamera() { manipulator.jumpToBookmark(manipulator.homeBookmark) }

    private fun loadAsset(bytes: ByteArray): FilamentAsset? {
        val buf = ByteBuffer.allocateDirect(bytes.size).order(ByteOrder.nativeOrder()).put(bytes).also { it.flip() }
        val a = assetLoader.createAsset(buf) ?: return null
        resourceLoader.loadResources(a)
        a.releaseSourceData()
        return a
    }

    /** Charge le personnage (GLB interne) et le sol. Le personnage est mis à l'échelle ~1,8 m, pieds au sol. */
    fun setCharacter(glb: ByteArray, skel: Skeleton, forward: Vec3) {
        character?.let { scene.removeEntities(it.entities); assetLoader.destroyAsset(it) }
        val a = loadAsset(glb) ?: throw IllegalStateException("Le moteur 3D n'a pas pu charger ce modèle.")
        character = a; skeleton = skel
        scene.addEntities(a.entities)
        val rm = engine.renderableManager
        for (e in a.entities) {
            if (rm.hasComponent(e)) { val i = rm.getInstance(e); rm.setCastShadows(i, true); rm.setReceiveShadows(i, true) }
        }
        // Correspondance index de nœud -> entité (les noms ont été rendus uniques à l'import).
        val byName = HashMap<String, Int>()
        for (e in a.entities) a.getName(e)?.let { byName.putIfAbsent(it, e) }
        nodeEntities = IntArray(skel.nodes.size) { byName[skel.nodes[it].name] ?: 0 }
        morphEntities.clear(); morphCounts.clear()
        for (m in skel.morphTargets) {
            val ent = nodeEntities[m.nodeIndex]
            val target = if (rm.hasComponent(ent)) ent else a.entities.firstOrNull { rm.hasComponent(it) && a.getName(it) == skel.nodes[m.nodeIndex].name } ?: continue
            morphEntities[m.nodeIndex] = target; morphCounts[m.nodeIndex] = m.targetNames.size
        }
        // Normalisation : hauteur des articulations -> ~1,75 m, centre au sol en (0, 0, 0).
        val h = skel.restHeight()
        val minY = skel.nodes.filter { it.isJoint }.minOfOrNull { skel.restPos(it.index).y } ?: 0.0
        val box = a.boundingBox
        val c = box.center
        val s = 1.75 / h
        val toeOffset = (minY - (c[1] - box.halfExtent[1])).coerceAtLeast(0.0) // semelle sous la cheville
        rootTransform = Mat4.fromTrs(Vec3(-c[0] * s, -(minY - toeOffset) * s, -c[2] * s), Quat.IDENTITY, Vec3(s, s, s))
        val tm = engine.transformManager
        tm.setTransform(tm.getInstance(a.root), rootTransform.toFloatArray())

        if (ground == null) {
            ground = loadAsset(ctx.assets.open("ground.glb").use { it.readBytes() })?.also { g ->
                scene.addEntities(g.entities)
                for (e in g.entities) if (rm.hasComponent(e)) { val i = rm.getInstance(e); rm.setCastShadows(i, false); rm.setReceiveShadows(i, true) }
            }
        }
        manipulator = buildManipulator(Vec3(0.0, 0.95, 0.0), 3.6, forward.let { Vec3(it.x, 0.0, it.z).normalized().takeIf { v -> v != Vec3.ZERO } ?: Vec3.Z })
        manipulator.setViewport(viewWidth, viewHeight)
        gestures = GestureDetector(textureView, manipulator)
    }

    /** Applique une pose (rotations locales + bassin) aux os du modèle. */
    fun applyPose(pose: SkeletalPose?) {
        val a = character ?: return
        val sk = skeleton ?: return
        val tm = engine.transformManager
        val p = pose
        for (i in sk.nodes.indices) {
            val e = nodeEntities[i]; if (e == 0) continue
            val n = sk.nodes[i]
            if (!n.isJoint && (p == null || i != p.hipsNode)) continue
            val r = p?.local?.get(i) ?: n.rest.r
            val t = if (p != null && i == p.hipsNode) p.hipsLocalT else n.rest.t
            tm.setTransform(tm.getInstance(e), Mat4.fromTrs(t, r, n.rest.s).toFloatArray())
        }
        a.instance.animator.updateBoneMatrices()
    }

    fun applyMorphs(nodeTargets: Map<Int, FloatArray>) {
        val rm = engine.renderableManager
        for ((node, w) in nodeTargets) {
            val e = morphEntities[node] ?: continue
            if (!rm.hasComponent(e)) continue
            rm.setMorphWeights(rm.getInstance(e), w, 0)
        }
    }

    fun morphCount(node: Int) = morphCounts[node] ?: 0

    /** Projette les articulations à l'écran (pour l'affichage du squelette). */
    fun projectJoints(pose: SkeletalPose?, indices: IntArray): FloatArray {
        val sk = skeleton ?: return FloatArray(0)
        val g = if (pose != null) PoseEvaluator.globals(sk, pose) else sk.restGlobal
        val view = DoubleArray(16); val proj = DoubleArray(16)
        camera.getViewMatrix(view); camera.getProjectionMatrix(proj)
        val vp = Mat4(proj) * Mat4(view) * rootTransform
        val out = FloatArray(indices.size * 3)
        for ((k, i) in indices.withIndex()) {
            val w = g[i].translation()
            val x = vp[0, 0] * w.x + vp[0, 1] * w.y + vp[0, 2] * w.z + vp[0, 3]
            val y = vp[1, 0] * w.x + vp[1, 1] * w.y + vp[1, 2] * w.z + vp[1, 3]
            val ww = vp[3, 0] * w.x + vp[3, 1] * w.y + vp[3, 2] * w.z + vp[3, 3]
            if (ww <= 1e-6) { out[k * 3 + 2] = 0f; continue }
            out[k * 3] = ((x / ww * 0.5 + 0.5) * viewWidth).toFloat()
            out[k * 3 + 1] = ((1 - (y / ww * 0.5 + 0.5)) * viewHeight).toFloat()
            out[k * 3 + 2] = 1f
        }
        return out
    }

    fun render(frameTimeNanos: Long) {
        if (destroyed || !uiHelper.isReadyToRender) return
        val sc = swapChain ?: return
        resourceLoader.asyncUpdateLoad()
        manipulator.getLookAt(eye, target, up)
        camera.lookAt(eye[0], eye[1], eye[2], target[0], target[1], target[2], up[0], up[1], up[2])
        if (renderer.beginFrame(sc, frameTimeNanos)) {
            renderer.render(view)
            renderer.endFrame()
        }
    }

    fun destroy() {
        if (destroyed) return
        destroyed = true
        uiHelper.detach()
        character?.let { assetLoader.destroyAsset(it) }
        ground?.let { assetLoader.destroyAsset(it) }
        materialProvider.destroyMaterials()
        materialProvider.destroy()
        assetLoader.destroy()
        resourceLoader.destroy()
        engine.destroyEntity(sun); engine.destroyEntity(fill)
        engine.destroyIndirectLight(ibl)
        engine.destroySkybox(skybox)
        engine.destroyRenderer(renderer)
        engine.destroyView(view)
        engine.destroyScene(scene)
        engine.destroyCameraComponent(cameraEntity)
        EntityManager.get().destroy(cameraEntity); EntityManager.get().destroy(sun); EntityManager.get().destroy(fill)
        engine.destroy()
    }
}
