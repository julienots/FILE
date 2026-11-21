import * as THREE from 'three';

/*
 * DIRECTION ARTISTIQUE « JEU » : ombrage cartoon global.
 * On modifie les morceaux de shader de Three.js AVANT toute compilation, donc tous les matériaux
 * standard / lambert du monde (terrain, maisons, personnages, arbres, herbe) partagent :
 *  - un éclairage en paliers (zone éclairée franche, transition courte, ombre propre) ;
 *  - un liseré de lumière sur les bords (rim light) qui détache les formes du décor.
 * Les contours noirs sont ajoutés en post-traitement (atmosphere.js).
 */
let applied = false;
export function applyToonShading() {
  if (applied) return; applied = true;
  const SC = THREE.ShaderChunk;
  const line = 'float dotNL = saturate( dot( geometryNormal, directLight.direction ) );';
  // matériaux standard : paliers marqués, on garde 20 % de dégradé pour ne pas « aplatir » les volumes
  SC.lights_physical_pars_fragment = SC.lights_physical_pars_fragment.replace(line,
    `${line}
	dotNL = mix( smoothstep( 0.0, 0.05, dotNL ) * 0.6 + smoothstep( 0.32, 0.42, dotNL ) * 0.4, dotNL, 0.2 );`);
  // lambert (herbe, fleurs) : paliers plus doux
  SC.lights_lambert_pars_fragment = SC.lights_lambert_pars_fragment.replace(line,
    `${line}
	dotNL = mix( smoothstep( 0.0, 0.1, dotNL ) * 0.7 + smoothstep( 0.3, 0.5, dotNL ) * 0.3, dotNL, 0.45 );`);
  // liseré de lumière froide sur les silhouettes
  SC.lights_fragment_end = SC.lights_fragment_end + `
{
	vec3 gdV = normalize( vViewPosition );
	float gdRim = pow( 1.0 - saturate( dot( normalize( geometryNormal ), gdV ) ), 3.0 );
	reflectedLight.indirectDiffuse += diffuseColor.rgb * gdRim * 0.5 + vec3( 0.55, 0.65, 0.8 ) * gdRim * 0.06;
}`;
}
