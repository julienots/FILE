// LOST KINGDOM — decodes a GLB compressed with EXT_meshopt_compression + KHR_mesh_quantization into a plain
// float GLB (node transforms baked) that tools/simplify_glb.py, import_prop.py and import_vehicle.py can read.
// Setup once: npm install @gltf-transform/core@4 @gltf-transform/extensions@4 @gltf-transform/functions@4 meshoptimizer@0.21
// Usage: node tools/decode_glb.mjs in.glb out.glb
// Decodes EXT_meshopt_compression + KHR_mesh_quantization into a plain float GLB (node transforms baked).
import { NodeIO } from '@gltf-transform/core';
import { ALL_EXTENSIONS } from '@gltf-transform/extensions';
import { dequantize, flatten, unpartition } from '@gltf-transform/functions';
import { MeshoptDecoder } from 'meshoptimizer';
await MeshoptDecoder.ready;
const io = new NodeIO().registerExtensions(ALL_EXTENSIONS).registerDependencies({ 'meshopt.decoder': MeshoptDecoder });
const doc = await io.read(process.argv[2]);
await doc.transform(dequantize());
// bake node transforms into vertices so the result has identity nodes
for (const node of doc.getRoot().listNodes()) {
  const mesh = node.getMesh(); if (!mesh) continue;
  const m = node.getWorldMatrix();
  for (const prim of mesh.listPrimitives()) {
    const pos = prim.getAttribute('POSITION'); const n = prim.getAttribute('NORMAL');
    const v = [0, 0, 0];
    for (let i = 0; i < pos.getCount(); i++) { pos.getElement(i, v); const x = v[0], y = v[1], z = v[2];
      pos.setElement(i, [m[0]*x + m[4]*y + m[8]*z + m[12], m[1]*x + m[5]*y + m[9]*z + m[13], m[2]*x + m[6]*y + m[10]*z + m[14]]); }
  }
  node.setMatrix([1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1]);
}
for (const ext of doc.getRoot().listExtensionsUsed()) if (ext.extensionName === 'EXT_meshopt_compression' || ext.extensionName === 'KHR_mesh_quantization') ext.dispose();
await doc.transform(unpartition());
await io.write(process.argv[3], doc);
console.log('decoded', process.argv[3]);
