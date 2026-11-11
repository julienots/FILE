// SUPERESSENCE 3D Mobile — relais Claude (MCP Streamable HTTP) ↔ téléphone.
// Claude appelle  POST /superessence/mcp/<secret>        (connecteur personnalisé)
// Le téléphone :  POST /superessence/device/register     (crée l'appareil et son secret)
//                 GET  /superessence/device/<secret>/wait  (attend une commande, ~25 s)
//                 POST /superessence/device/<secret>/result
// Déployer avec « Verify JWT » DÉSACTIVÉ (Claude n'envoie pas de jeton Supabase).

const URL_ = Deno.env.get("SUPABASE_URL")!;
const KEY = Deno.env.get("SUPABASE_SERVICE_ROLE_KEY")!;
const H = { apikey: KEY, Authorization: `Bearer ${KEY}`, "Content-Type": "application/json" };
const CORS = { "Access-Control-Allow-Origin": "*", "Access-Control-Allow-Headers": "content-type, authorization, mcp-session-id, mcp-protocol-version", "Access-Control-Allow-Methods": "GET, POST, OPTIONS" };
const json = (b: unknown, status = 200, extra: Record<string, string> = {}) => new Response(JSON.stringify(b), { status, headers: { "Content-Type": "application/json", ...CORS, ...extra } });
const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

async function db(path: string, init: RequestInit = {}) {
  const r = await fetch(`${URL_}/rest/v1/${path}`, { ...init, headers: { ...H, Prefer: "return=representation", ...(init.headers || {}) } });
  if (!r.ok) throw new Error(`base de données : ${r.status} ${await r.text()}`);
  const t = await r.text();
  return t ? JSON.parse(t) : null;
}
const validSecret = (s: string) => /^[A-Za-z0-9_-]{24,64}$/.test(s);
async function deviceExists(secret: string) {
  if (!validSecret(secret)) return false;
  const rows = await db(`se_devices?secret=eq.${secret}&select=secret`);
  return rows.length > 0;
}

// ------------------------------------------------------------------ outils exposés à Claude
const S = (props: Record<string, unknown>, required: string[] = []) => ({ type: "object", properties: props, required });
const TOOLS = [
  { name: "get_scene_info", description: "État de la scène sur le téléphone : objets (nom, type, triangles, taille, animation, spécification de génération), éclairage.", inputSchema: S({}) },
  { name: "generate_model", description: "Génère un modèle 3D procédural sur le téléphone à partir d'une description FR/EN (créatures, personnages, animaux, bâtiments, véhicules, armes fictives, objets, décors). spec_overrides permet de préciser : height (m), colors {primary, secondary, eyes, horns, wings} en noms ou #rrggbb, features {wings, horns, tails, tentacles, heads, eyes, legs, spikes, scales, claws, fangs}, scales {wings:1.5, body:1.2...}, material, glow_eyes, glow, build, style. quality : draft|standard|high|ultra.", inputSchema: S({ prompt: { type: "string" }, quality: { type: "string", enum: ["draft", "standard", "high", "ultra"] }, spec_overrides: { type: "object" } }, ["prompt"]) },
  { name: "modify_model", description: "Modifie un modèle généré par une instruction en langage naturel (« agrandis ses ailes, ajoute six tentacules et remplace les yeux par des yeux blancs lumineux ») et/ou des changements structurés (changes : {features, colors, scales, height, material, glow_eyes...}).", inputSchema: S({ name: { type: "string" }, instruction: { type: "string" }, changes: { type: "object" } }, ["name"]) },
  { name: "set_material", description: "Change le matériau d'un objet ou d'une pièce (part) : preset (plain, metal, gold, rust, wood, bark, stone, brick, skin, scales, fabric, leather, organic, plastic, glass, ice, crystal, lava, emission, ground, fur) et color, ou description (« peau noire humide aux veines violettes »).", inputSchema: S({ name: { type: "string" }, part: { type: "string" }, preset: { type: "string" }, color: { type: "string" }, description: { type: "string" }, emissive: {}, roughness: { type: "number" }, metallic: { type: "number" } }, ["name"]) },
  { name: "transform_object", description: "Déplace / tourne / met à l'échelle un objet (position [x,y,z] en m, Y vers le haut ; rotation_y en degrés ; scale uniforme).", inputSchema: S({ name: { type: "string" }, position: { type: "array", items: { type: "number" } }, rotation_y: { type: "number" }, scale: { type: "number" } }, ["name"]) },
  { name: "animate", description: "Anime un modèle à squelette avec des mouvements combinables : idle, breathe, walk, run, jump, attack, fly, wing_flap, tail_wave, head_look, tentacle_wave, slither. Pour un objet sans squelette : object_motion spin|bounce|float|pulse|sway.", inputSchema: S({ name: { type: "string" }, motions: { type: "array", items: { type: "string" } }, object_motion: { type: "string" }, amplitude: { type: "number" } }, ["name"]) },
  { name: "stop_animation", description: "Arrête la lecture des animations.", inputSchema: S({}) },
  { name: "add_shape", description: "Ajoute une forme de base : box, sphere, cylinder, cone, torus, plane (color facultative).", inputSchema: S({ shape: { type: "string" }, color: { type: "string" }, name: { type: "string" } }, ["shape"]) },
  { name: "duplicate_object", description: "Duplique un objet.", inputSchema: S({ name: { type: "string" } }, ["name"]) },
  { name: "delete_object", description: "Supprime un objet. Exige confirm=true APRÈS avoir demandé l'accord de l'utilisateur.", inputSchema: S({ name: { type: "string" }, confirm: { type: "boolean" } }, ["name"]) },
  { name: "set_lighting", description: "Éclairage de la scène : studio, outdoor, dramatic, night ; background (#rrggbb) facultatif.", inputSchema: S({ preset: { type: "string" }, background: { type: "string" } }) },
  { name: "view_scene", description: "Capture la vue 3D du téléphone (image) pour vérifier visuellement. Par défaut 4 vues (persp, front, right, top) en mosaïque. name : cadrer un objet ; views : liste parmi persp|front|right|top.", inputSchema: S({ name: { type: "string" }, views: { type: "array", items: { type: "string" } } }) },
  { name: "save_to_library", description: "Enregistre un objet dans la bibliothèque de l'application.", inputSchema: S({ name: { type: "string" } }, ["name"]) },
  { name: "export_model", description: "Exporte un objet (ou la scène) en glb, obj ou stl : le fichier est vérifié puis l'utilisateur choisit où l'enregistrer sur son téléphone.", inputSchema: S({ name: { type: "string" }, format: { type: "string", enum: ["glb", "obj", "stl"] } }) },
  { name: "undo", description: "Annule la dernière opération.", inputSchema: S({}) },
  { name: "build_organic", description: "MODÉLISATION LIBRE d'un corps organique (créature inventée, personnage, animal, plante, monstre…). Surface lisse continue construite autour d'os (capsules effilées) et de volumes (ellipsoïdes), fusionnés en douceur, avec squelette animable. Coordonnées monde en mètres : Y = haut, Z = avant, X = côté (sol à y=0). bones : [{name, head:[x,y,z], tail:[x,y,z], radius:[r_head, r_tail], parent, role}] — rôles pour l'animation : spine, chest, neck, head, tail_1.., leg_L_front_upper/lower/foot, leg_R_back_upper.., arm_L_upper/lower/hand, wing_L_1.., tentacle_1_1… blobs : [{center:[x,y,z], radii:[rx,ry,rz], bone, subtract:false, smoothness}] pour muscles, ventre, crâne, museau, joues ; subtract:true creuse (bouche, orbites, narines). eyes : [{position:[x,y,z], radius, color, glow, look:[0,0,1], bone, mirror_x:true}]. parts : pièces rigides ou souples (mêmes champs que add_part + bone, follow_bones:true pour suivre la déformation : cornes, griffes, pointes, ailes membranes, armures). material (scales, skin, organic, fur, leather, stone, metal, lava…), color, color2, veins, pattern_scale, smoothness (fusion, ex. 0.05 à 0.3), resolution (60-200, 120+ pour du détail), height (m, met à l'échelle). Si un objet du même nom existe, il est remplacé (itérez ainsi).", inputSchema: S({ name: { type: "string" }, bones: { type: "array", items: { type: "object" } }, blobs: { type: "array", items: { type: "object" } }, eyes: { type: "array", items: { type: "object" } }, parts: { type: "array", items: { type: "object" } }, material: { type: "string" }, color: { type: "string" }, color2: { type: "string" }, veins: { type: "string" }, smoothness: { type: "number" }, resolution: { type: "number" }, height: { type: "number" }, pattern_scale: { type: "number" }, quality: { type: "string" } }, ["name"]) },
  { name: "add_part", description: "Ajoute une pièce à un modèle (créé s'il n'existe pas) : assemblez ainsi bâtiments, véhicules, armes, objets, décors ou détails. shape : box (size [x,y,z], bevel), sphere (radius), ellipsoid (radii), cylinder (radius, height, radius_top), cone, torus (radius, tube), capsule (radius, height), tube (points [[x,y,z]…] courbe lissée, radii [..], tip:true pointe, flatten), lathe (profile [[rayon, hauteur]…] tourné autour de Y), extrude (outline [[x,y]…] contour 2D, depth, bevel, holes), loft (sections [[z, rx, ry, cy]…]), membrane (origin, rows), plane, icosphere. position [x,y,z], rotation [°x,°y,°z], scale, mirror_x:true (copie symétrique), noise (relief), material (preset), color, emissive (couleur ou true), roughness, metallic, texture (brick|planks|stone|tiles|metal), bone (attacher à un os du modèle).", inputSchema: S({ model: { type: "string" }, name: { type: "string" }, shape: { type: "string" }, size: {}, radius: { type: "number" }, radii: { type: "array", items: { type: "number" } }, height: { type: "number" }, radius_top: { type: "number" }, tube: { type: "number" }, points: { type: "array" }, profile: { type: "array" }, outline: { type: "array" }, holes: { type: "array" }, sections: { type: "array" }, origin: { type: "array" }, rows: { type: "array" }, depth: { type: "number" }, bevel: { type: "number" }, tip: { type: "boolean" }, position: { type: "array" }, rotation: { type: "array" }, scale: {}, mirror_x: { type: "boolean" }, noise: { type: "number" }, material: { type: "string" }, color: { type: "string" }, emissive: {}, roughness: { type: "number" }, metallic: { type: "number" }, texture: { type: "string" }, bone: { type: "string" } }, ["shape"]) },
  { name: "remove_part", description: "Retire une pièce d'un modèle.", inputSchema: S({ name: { type: "string" }, part: { type: "string" } }, ["name", "part"]) },
  { name: "transform_part", description: "Repositionne / tourne (degrés) / met à l'échelle une pièce d'un modèle.", inputSchema: S({ name: { type: "string" }, part: { type: "string" }, position: { type: "array" }, rotation: { type: "array" }, scale: {} }, ["name", "part"]) },
  { name: "get_object_details", description: "Détails d'un modèle : pièces (boîtes englobantes monde), os (positions monde), définition utilisée par build_organic (pour la modifier et la renvoyer).", inputSchema: S({ name: { type: "string" } }, ["name"]) },
  { name: "sculpt", description: "Sculpte le maillage : points [[x,y,z]] en coordonnées monde (normale facultative en 4e-6e valeurs), brush draw|inflate|smooth|flatten|pinch|crease, radius (m), strength, invert (creuser), symmetry (miroir X), repeat. part facultatif.", inputSchema: S({ name: { type: "string" }, part: { type: "string" }, points: { type: "array" }, brush: { type: "string" }, radius: { type: "number" }, strength: { type: "number" }, invert: { type: "boolean" }, symmetry: { type: "boolean" }, repeat: { type: "number" } }, ["name", "points"]) },
  { name: "paint", description: "Peint les couleurs : points [[x,y,z]] + radius, ou fill:true (pièce entière). color en nom ou #rrggbb.", inputSchema: S({ name: { type: "string" }, part: { type: "string" }, points: { type: "array" }, color: { type: "string" }, radius: { type: "number" }, strength: { type: "number" }, fill: { type: "boolean" } }, ["name", "color"]) },
  { name: "deform", description: "Déforme un modèle ou une pièce : smooth (lisser), noise (relief irrégulier), inflate (gonfler), taper (effiler, amount = facteur au sommet), twist (degrés), bend (degrés), stretch (facteur) ; axis x|y|z.", inputSchema: S({ name: { type: "string" }, part: { type: "string" }, operation: { type: "string" }, amount: { type: "number" }, axis: { type: "string" } }, ["name", "operation"]) },
];

const INSTRUCTIONS = `SUPERESSENCE 3D Mobile : studio 3D sur le téléphone de l'utilisateur (application ouverte, « Connecté à Claude » activé). Mètres, Y = haut, Z = avant, sol à y=0.
Pour des modèles de qualité, MODÉLISEZ VOUS-MÊME plutôt que de vous limiter à generate_model :
1. Planifiez l'anatomie / la structure (proportions réelles en mètres, silhouette lisible).
2. Créatures et personnages : build_organic — os de la colonne, du cou, de la tête, des membres (rayons effilés : épais au corps, fins aux extrémités), puis blobs pour les masses (thorax, ventre, cuisses, crâne, mâchoire) et creux (bouche, orbites) ; yeux ; parts pour cornes, griffes, crocs, pointes, ailes (membrane + tubes), armures ; rôles d'os pour l'animation. resolution 120-160 pour du détail.
3. Objets, bâtiments, véhicules, armes : add_part pièce par pièce (lathe, extrude, loft, tube, box biseautée), mirror_x pour la symétrie, matériaux et couleurs par pièce, emissive pour les lumières.
4. Vérifiez TOUJOURS avec view_scene (4 vues) puis corrigez : renvoyez build_organic avec le même nom (remplacement), transform_part, sculpt, deform, paint, set_material.
5. Terminez par animate si pertinent, et proposez export_model.
Demandez l'accord avant delete_object. Génération procédurale/géométrique, pas un réseau neuronal.`;

async function callPhone(secret: string, tool: string, args: unknown) {
  await db(`se_commands?created_at=lt.${new Date(Date.now() - 86400000).toISOString()}`, { method: "DELETE", headers: { Prefer: "return=minimal" } }).catch(() => {});
  const [cmd] = await db("se_commands", { method: "POST", body: JSON.stringify({ secret, tool, args }) });
  const deadline = Date.now() + 55000;
  while (Date.now() < deadline) {
    await sleep(500);
    const [row] = await db(`se_commands?id=eq.${cmd.id}&select=status,result`);
    if (row && (row.status === "done" || row.status === "error")) return row;
  }
  await db(`se_commands?id=eq.${cmd.id}`, { method: "PATCH", body: JSON.stringify({ status: "error", result: { error: "timeout" } }) }).catch(() => {});
  return { status: "error", result: { error: "Le téléphone n'a pas répondu. Ouvrez SUPERESSENCE 3D, activez « Connecté à Claude » et gardez l'application au premier plan." } };
}

async function handleMcp(req: Request, secret: string) {
  if (req.method === "GET") return new Response("Utilisez POST (MCP Streamable HTTP).", { status: 405, headers: CORS });
  if (!(await deviceExists(secret))) return json({ jsonrpc: "2.0", id: null, error: { code: -32001, message: "Appareil inconnu : régénérez l'adresse dans l'application." } }, 404);
  const msg = await req.json();
  const batch = Array.isArray(msg) ? msg : [msg];
  const out = [];
  for (const m of batch) {
    if (m.id === undefined || m.id === null) continue; // notifications
    const reply = (result: unknown) => out.push({ jsonrpc: "2.0", id: m.id, result });
    const fail = (code: number, message: string) => out.push({ jsonrpc: "2.0", id: m.id, error: { code, message } });
    if (m.method === "initialize") reply({ protocolVersion: m.params?.protocolVersion || "2025-06-18", capabilities: { tools: { listChanged: false } }, serverInfo: { name: "superessence-mobile", version: "0.3.0" }, instructions: INSTRUCTIONS });
    else if (m.method === "ping") reply({});
    else if (m.method === "tools/list") reply({ tools: TOOLS });
    else if (m.method === "resources/list") reply({ resources: [] });
    else if (m.method === "prompts/list") reply({ prompts: [] });
    else if (m.method === "tools/call") {
      const name = m.params?.name;
      if (!TOOLS.some((t) => t.name === name)) { fail(-32602, `Outil inconnu : ${name}`); continue; }
      const row = await callPhone(secret, name, m.params?.arguments || {});
      const r = row.result || {};
      if (row.status === "error") { reply({ content: [{ type: "text", text: String(r.error || "Erreur") }], isError: true }); continue; }
      const content: unknown[] = [];
      if (r.image) content.push({ type: "image", data: r.image, mimeType: "image/jpeg" });
      const { image: _i, ...rest } = r;
      content.push({ type: "text", text: JSON.stringify(rest) });
      reply({ content, isError: false });
    } else fail(-32601, `Méthode non prise en charge : ${m.method}`);
  }
  if (!out.length) return new Response(null, { status: 202, headers: CORS });
  return json(Array.isArray(msg) ? out : out[0]);
}

Deno.serve(async (req) => {
  if (req.method === "OPTIONS") return new Response(null, { headers: CORS });
  const url = new URL(req.url);
  const parts = url.pathname.split("/").filter(Boolean);
  const i = parts.indexOf("superessence");
  const p = parts.slice(i + 1);
  try {
    if (p[0] === "mcp" && p[1]) return await handleMcp(req, p[1]);
    if (p[0] === "device" && p[1] === "register" && req.method === "POST") {
      const bytes = crypto.getRandomValues(new Uint8Array(24));
      const secret = btoa(String.fromCharCode(...bytes)).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/, "");
      await db("se_devices", { method: "POST", body: JSON.stringify({ secret }) });
      // derrière la passerelle Supabase, req.url est interne (http, sans /functions/v1) : adresse publique reconstruite
      const base = `${URL_.replace(/^http:/, "https:").replace(/\/$/, "")}/functions/v1/superessence`;
      return json({ secret, mcp_url: `${base}/mcp/${secret}` });
    }
    if (p[0] === "device" && p[1] && p[2] === "wait") {
      if (!(await deviceExists(p[1]))) return json({ error: "appareil inconnu" }, 404);
      await db(`se_devices?secret=eq.${p[1]}`, { method: "PATCH", body: JSON.stringify({ last_seen: new Date().toISOString() }), headers: { Prefer: "return=minimal" } });
      const deadline = Date.now() + 25000;
      while (Date.now() < deadline) {
        const rows = await db(`se_commands?secret=eq.${p[1]}&status=eq.pending&order=id.asc&limit=1&select=id,tool,args`);
        if (rows.length) {
          await db(`se_commands?id=eq.${rows[0].id}&status=eq.pending`, { method: "PATCH", body: JSON.stringify({ status: "running" }), headers: { Prefer: "return=minimal" } });
          return json(rows[0]);
        }
        await sleep(400);
      }
      return new Response(null, { status: 204, headers: CORS });
    }
    if (p[0] === "device" && p[1] && p[2] === "result" && req.method === "POST") {
      if (!(await deviceExists(p[1]))) return json({ error: "appareil inconnu" }, 404);
      const b = await req.json();
      await db(`se_commands?id=eq.${Number(b.id)}&secret=eq.${p[1]}`, { method: "PATCH", body: JSON.stringify({ status: b.ok ? "done" : "error", result: b.ok ? b.result : { error: b.error }, done_at: new Date().toISOString() }), headers: { Prefer: "return=minimal" } });
      return json({ ok: true });
    }
    if (p.length === 0) return json({ app: "superessence3d-relay", version: "0.3.1" });
    return json({ error: "introuvable" }, 404);
  } catch (e) {
    return json({ error: String((e as Error).message || e) }, 500);
  }
});
