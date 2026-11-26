// Matériau PBR du terrain : mélange de 11 textures photo selon pente, altitude, régions,
// chemins, champs, brûlures et terre remuée. Roche en projection triplanaire, anti-répétition.
import * as THREE from 'three';
import { G, WORLD, Q } from './core.js';

export function makeTerrainMaterial(tex) {
  const m = new THREE.MeshStandardMaterial({ roughness: 0.92, metalness: 0 });
  const low = Q().label === 'Bas';
  m.onBeforeCompile = (sh) => {
    sh.uniforms.uTime = G.uniforms.time;
    sh.uniforms.tDiff = { value: tex.diff };
    sh.uniforms.tNor = { value: tex.nor };
    sh.defines = sh.defines || {};
    if (low) sh.defines.LOWQ = 1;
    sh.vertexShader = sh.vertexShader
      .replace('#include <common>', `#include <common>
        attribute vec4 splat; attribute vec4 fx; attribute float heat;
        varying vec4 vSplat; varying vec4 vFx; varying float vHeat; varying vec3 vWPos; varying vec3 vWN;`)
      .replace('#include <begin_vertex>', `#include <begin_vertex>
        vSplat = splat; vFx = fx; vHeat = heat;
        vWPos = (modelMatrix * vec4(transformed, 1.0)).xyz; vWN = normalize(mat3(modelMatrix) * objectNormal);`);
    sh.fragmentShader = sh.fragmentShader
      .replace('#include <common>', `#include <common>
        precision highp sampler2DArray;
        uniform sampler2DArray tDiff; uniform sampler2DArray tNor; uniform float uTime;
        varying vec4 vSplat; varying vec4 vFx; varying float vHeat; varying vec3 vWPos; varying vec3 vWN;
        float h21(vec2 p){ return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
        float vn(vec2 p){ vec2 i = floor(p), f = fract(p); f = f*f*(3.0-2.0*f);
          return mix(mix(h21(i), h21(i+vec2(1,0)), f.x), mix(h21(i+vec2(0,1)), h21(i+vec2(1,1)), f.x), f.y); }
        // couche plane (projection verticale) avec anti-répétition à deux échelles
        void layerTop(float L, float scale, float w, inout vec3 col, inout vec3 nrm, inout float tot, float macro){
          if (w < 0.002) return;
          vec2 uv = vWPos.xz * scale;
          vec4 d = texture(tDiff, vec3(uv, L));
          vec3 n = texture(tNor, vec3(uv, L)).xyz * 2.0 - 1.0;
          #ifndef LOWQ
            vec2 uv2 = mat2(0.8, -0.6, 0.6, 0.8) * vWPos.xz * scale * 0.37 + 0.31;
            vec4 d2 = texture(tDiff, vec3(uv2, L));
            vec3 n2 = texture(tNor, vec3(uv2, L)).xyz * 2.0 - 1.0;
            d = mix(d, d2, macro * 0.6); n = normalize(mix(n, n2, macro * 0.6));
          #endif
          col += d.rgb * w; nrm += vec3(n.x, 0.0, -n.y) * w; tot += w;
        }
        void layerTri(float L, float scale, float w, vec3 N, inout vec3 col, inout vec3 nrm, inout float tot){
          if (w < 0.002) return;
          vec3 bw = pow(abs(N), vec3(4.0)); bw /= (bw.x + bw.y + bw.z);
          vec3 p = vWPos * scale;
          vec4 dx = texture(tDiff, vec3(p.zy, L)), dy = texture(tDiff, vec3(p.xz, L)), dz = texture(tDiff, vec3(p.xy, L));
          vec3 nx = texture(tNor, vec3(p.zy, L)).xyz * 2.0 - 1.0, ny = texture(tNor, vec3(p.xz, L)).xyz * 2.0 - 1.0, nz = texture(tNor, vec3(p.xy, L)).xyz * 2.0 - 1.0;
          col += (dx.rgb * bw.x + dy.rgb * bw.y + dz.rgb * bw.z) * w;
          vec3 n = vec3(0.0, nx.y, nx.x) * sign(N.x) * bw.x + vec3(ny.x, 0.0, -ny.y) * bw.y + vec3(nz.x, nz.y, 0.0) * sign(N.z) * bw.z;
          nrm += n * w; tot += w;
        }`)
      .replace('#include <map_fragment>', `
        vec3 Ng = normalize(vWN);
        float camD = length(vWPos - cameraPosition);
        float nz1 = vn(vWPos.xz * 0.045), nz2 = vn(vWPos.xz * 0.21 + 7.0);
        float macro = smoothstep(0.25, 0.75, vn(vWPos.xz * 0.018 + 3.0));
        float slope = 1.0 - Ng.y;
        float h = vWPos.y;
        float wRock = smoothstep(0.2, 0.36, slope + (nz1 - 0.5) * 0.14);
        wRock = max(wRock, smoothstep(4.0, 12.0, vFx.y) * 0.9);
        float wSnow = smoothstep(150.0, 185.0, h + nz1 * 30.0) * (1.0 - smoothstep(0.45, 0.7, slope));
        float wSand = 1.0 - smoothstep(${(WORLD.water + 0.4).toFixed(1)}, ${(WORLD.water + 2.6).toFixed(1)}, h + nz2 * 1.2);
        float wDug = smoothstep(0.25, 1.8, vFx.y) * (1.0 - wRock);
        float wFar = smoothstep(120.0, 420.0, camD);
        vec3 col = vec3(0.0), nrm = vec3(0.0); float tot = 0.0;
        // herbe et sous-bois (base)
        float forest = clamp(vSplat.x + (nz1 - 0.5) * 0.3, 0.0, 1.0);
        layerTop(0.0, 0.22, (1.0 - forest) * (1.0 - wFar * 0.75), col, nrm, tot, macro);
        layerTop(1.0, 0.25, forest * (1.0 - wFar * 0.6), col, nrm, tot, macro);
        layerTop(10.0, 0.012, wFar * (0.75 - forest * 0.15) + 0.0001, col, nrm, tot, 0.0);
        col /= max(tot, 1e-4); nrm /= max(tot, 1e-4);
        // couches superposées
        vec3 c2; vec3 n2; float t2;
        #define OVER(L, S, W) { float w_ = (W); if (w_ > 0.002) { c2 = vec3(0.0); n2 = vec3(0.0); t2 = 0.0; layerTop(L, S, 1.0, c2, n2, t2, macro); col = mix(col, c2, w_); nrm = mix(nrm, n2, w_); } }
        OVER(4.0, 0.2, wSand)
        OVER(8.0, 0.3, vSplat.z)
        OVER(5.0, 0.3, smoothstep(0.15, 0.7, vSplat.y + (nz2 - 0.5) * 0.35) * 0.8)
        OVER(9.0, 0.3, vSplat.w)
        OVER(7.0, 0.12, vFx.z)
        OVER(5.0, 0.2, wDug)
        #ifndef LOWQ
          if (wRock > 0.002) { c2 = vec3(0.0); n2 = vec3(0.0); t2 = 0.0; layerTri(2.0, 0.07, 1.0, Ng, c2, n2, t2); col = mix(col, c2, wRock); nrm = mix(nrm, n2, wRock); }
        #else
          OVER(2.0, 0.07, wRock)
        #endif
        OVER(3.0, 0.12, wSnow * (1.0 - wRock * 0.6))
        OVER(6.0, 0.2, vFx.x)
        // variation de couleur à grande échelle (évite l'aspect uniforme)
        col *= 0.82 + 0.3 * vn(vWPos.xz * 0.006) + 0.1 * nz1;
        col *= 1.0 - vFx.x * 0.55;
        diffuseColor.rgb *= col;
        float terrRough = mix(0.95, 0.82, wRock); terrRough = mix(terrRough, 0.55, wSnow); terrRough = mix(terrRough, 0.6, wSand * smoothstep(${(WORLD.water + 1.2).toFixed(1)}, ${(WORLD.water).toFixed(1)}, h));
        vec3 terrN = normalize(Ng + nrm * (1.0 - wFar * 0.7) * 0.9);`)
      .replace('#include <roughnessmap_fragment>', 'float roughnessFactor = terrRough;')
      .replace('#include <normal_fragment_maps>', 'normal = normalize((viewMatrix * vec4(terrN, 0.0)).xyz);')
      .replace('#include <emissivemap_fragment>', `#include <emissivemap_fragment>
        float pulse = 0.75 + 0.25 * sin(uTime * 2.3 + vWPos.x * 0.2 + vWPos.z * 0.17);
        float lavaN = vn(vWPos.xz * 0.6 + uTime * 0.15);
        totalEmissiveRadiance += vec3(1.0, 0.32, 0.06) * vHeat * vHeat * (1.6 + lavaN) * pulse;`);
  };
  return m;
}
