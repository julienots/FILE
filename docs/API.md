# Référence de l'API (générée)

Produite par `agr_editor --api-reference docs/API.md` à partir du moteur lui-même : la liste est toujours celle du code. Guides : [SCRIPTING.md](SCRIPTING.md), [ARCHITECTURE.md](ARCHITECTURE.md).

## Composants (76, 747 champs)

**2D** : [Bone2D](#Bone2D), [ChainIK2D](#ChainIK2D), [Light2D](#Light2D), [LightOccluder2D](#LightOccluder2D), [Parallax](#Parallax), [ParticleEmitter2D](#ParticleEmitter2D), [Skeleton2D](#Skeleton2D), [Skin2D](#Skin2D), [SpriteAnimator](#SpriteAnimator), [Tilemap](#Tilemap), [TwoBoneIK2D](#TwoBoneIK2D)

**Animation** : [Animator](#Animator), [ChainIK3D](#ChainIK3D), [LookAtIK3D](#LookAtIK3D), [TwoBoneIK3D](#TwoBoneIK3D)

**Audio** : [AudioListener](#AudioListener), [AudioSource](#AudioSource), [AudioZone](#AudioZone)

**Core** : [Name](#Name), [State](#State), [Transform](#Transform)

**Effects** : [ParticleAttractor](#ParticleAttractor), [ParticleCollider](#ParticleCollider), [Trail](#Trail)

**Navigation** : [NavAgent](#NavAgent), [NavArea](#NavArea), [NavLink](#NavLink), [NavObstacle](#NavObstacle), [NavigationSettings](#NavigationSettings)

**Network** : [NetSync](#NetSync)

**Physics 2D** : [CharacterController2D](#CharacterController2D), [Collider2D](#Collider2D), [Joint2D](#Joint2D), [RigidBody2D](#RigidBody2D), [TilemapCollider2D](#TilemapCollider2D)

**Physics 3D** : [CharacterController3D](#CharacterController3D), [Collider](#Collider), [Joint3D](#Joint3D), [RigidBody](#RigidBody), [SoftBody3D](#SoftBody3D), [Vehicle3D](#Vehicle3D)

**Rendering** : [BlobShadow](#BlobShadow), [Camera](#Camera), [Decal](#Decal), [DirectionalLight](#DirectionalLight), [Environment](#Environment), [MeshRenderer](#MeshRenderer), [Occluder](#Occluder), [ParticleEmitter3D](#ParticleEmitter3D), [PointLight](#PointLight), [PostProcess](#PostProcess), [ReflectionProbe](#ReflectionProbe), [SpotLight](#SpotLight), [SpriteRenderer](#SpriteRenderer), [Terrain](#Terrain), [Water](#Water)

**Scripting** : [Groups](#Groups), [Script](#Script)

**UI** : [UIButton](#UIButton), [UICanvas](#UICanvas), [UIColorPicker](#UIColorPicker), [UIDialog](#UIDialog), [UIDropdown](#UIDropdown), [UIImage](#UIImage), [UIInputField](#UIInputField), [UIItemList](#UIItemList), [UILayout](#UILayout), [UIProgressBar](#UIProgressBar), [UIRect](#UIRect), [UIScrollView](#UIScrollView), [UISlider](#UISlider), [UITabs](#UITabs), [UIText](#UIText), [UIToggle](#UIToggle), [UITooltip](#UITooltip)

### Bone2D

Bone of a 2D skeleton, pointing along +X. Its pose in edit mode is the rest pose of the skins.

*2D*

| Champ | Type | Détails |
|---|---|---|
| `length` | float | plage 0.001 .. 1000 |
| `restSet` | bool |  |
| `restPosition` | vec3 |  |
| `restRotation` | rotation |  |
| `restScale` | vec3 |  |

### ChainIK2D

On the root Bone2D of a chain (tail, rope, tentacle): FABRIK, the last bone's tip reaches the target.

*2D*

| Champ | Type | Détails |
|---|---|---|
| `target` | entity |  |
| `boneCount` | int | plage 1 .. 64 |
| `iterations` | int | plage 1 .. 64 |
| `maxAngle` | angle | Bend limit at each joint ; plage 0 .. 3.1416 |
| `jointLimits` | string | Per joint limits in degrees, from the first inner joint: "30, 90, 45" |
| `weight` | float | plage 0 .. 1 |
| `enabled` | bool |  |

### Light2D

Point light for sprites and tilemaps marked 'lit'.

*2D*

| Champ | Type | Détails |
|---|---|---|
| `color` | color |  |
| `intensity` | float | plage 0 .. 20 |
| `radius` | float | plage 0.01 .. 1000 |
| `falloff` | float | plage 0.1 .. 8 |
| `castShadows` | bool | Shadows of LightOccluder2D shapes (8 shadowed lights at most) |
| `shadowSoftness` | float | plage 0 .. 4 |
| `shadowStrength` | float | plage 0 .. 1 |
| `shadowPenetration` | float | How far the light enters occluders before the shadow starts (lit wall faces) ; plage 0 .. 100 |
| `height` | float | Height above the sprites for normal maps: low = grazing light, strong relief ; plage 0 .. 100 |
| `directional` | bool | Sun-like light along the entity's +X axis: no radius, no falloff |

### LightOccluder2D

Blocks 2D light: a box, or every non-empty cell of the entity's Tilemap.

*2D*

| Champ | Type | Détails |
|---|---|---|
| `size` | vec2 |  |
| `offset` | vec2 |  |

### Parallax

Scrolls this entity and its children slower or faster than the world (backgrounds).

*2D*

| Champ | Type | Détails |
|---|---|---|
| `factor` | vec2 | 1 = moves with the world, 0 = stays with the camera |

### ParticleEmitter2D

Emits and simulates 2D particles (fire, smoke, sparks, magic...).

*2D*

| Champ | Type | Détails |
|---|---|---|
| `emitting` | bool |  |
| `loop` | bool |  |
| `duration` | float | plage 0.05 .. 600 |
| `rate` | float | plage 0 .. 10000 |
| `burst` | uint | plage 0 .. 100000 |
| `maxParticles` | uint | plage 1 .. 100000 |
| `lifetimeMin` | float | plage 0.01 .. 60 |
| `lifetimeMax` | float | plage 0.01 .. 60 |
| `speedMin` | float |  |
| `speedMax` | float |  |
| `direction` | angle |  |
| `spread` | angle | plage 0 .. 3.14159 |
| `spawnShape` | int | valeurs : `Disc`, `Circle`, `Rectangle`, `Line` |
| `spawnSize` | vec2 | Rectangle size / line length (x) |
| `spawnRadius` | float | plage 0 .. 1000 |
| `gravity` | vec2 |  |
| `drag` | float | plage 0 .. 100 |
| `startColor` | color |  |
| `endColor` | color |  |
| `startSize` | float | plage 0 .. 100 |
| `endSize` | float | plage 0 .. 100 |
| `angularVelocity` | angle |  |
| `texture` | asset | Empty = soft round particle |
| `curves` | asset | Size curve and color gradient over the lifetime (.agrparticle) |
| `flipbookColumns` | int | Frames of the texture across ; plage 1 .. 16 |
| `flipbookRows` | int | plage 1 .. 16 |
| `flipbookCycles` | float | Times the frames play during a life ; plage 0 .. 32 |
| `velocityStretch` | float | Stretch along the motion (seconds of speed): sparks, rain ; plage 0 .. 2 |
| `collisions` | bool | Bounce on the ground plane and ParticleCollider shapes (world space only) |
| `groundPlane` | bool |  |
| `groundHeight` | float |  |
| `bounce` | float | plage 0 .. 1 |
| `collisionFriction` | float | plage 0 .. 1 |
| `dieOnCollision` | bool |  |
| `affectedByAttractors` | bool |  |
| `turbulence` | float | Swirling noise force (units / s^2), 0 = off ; plage -100 .. 100 |
| `turbulenceScale` | float | Size of the swirls (world units) ; plage 0.01 .. 100 |
| `turbulenceSpeed` | float | How fast the swirls change ; plage 0 .. 20 |
| `inheritVelocity` | float | Fraction of the emitter's velocity given to new particles (world space) ; plage -2 .. 2 |
| `subEmitter` | entity | Emitter that bursts where a particle dies or hits something |
| `subEmitOn` | enum | valeurs : `Death`, `Collision` |
| `subEmitCount` | int | plage 0 .. 1000 |
| `lit` | bool | Lit by Light2D like lit sprites (not with additive) |
| `additive` | bool | Additive blending (fire, light, magic) |
| `worldSpace` | bool |  |
| `sortingLayer` | int |  |
| `orderInLayer` | int |  |
| `seed` | uint | 0 = random, otherwise reproducible |

### Skeleton2D

Root of a 2D skeleton: its Bone2D children deform the Skin2D parts below it.

*2D*

| Champ | Type | Détails |
|---|---|---|
| `showBones` | bool |  |

### Skin2D

Picture deformed by the bones of the enclosing Skeleton2D (smooth bending).

*2D*

| Champ | Type | Détails |
|---|---|---|
| `texture` | asset |  |
| `normalMap` | asset | Normal map (lit skins) |
| `color` | color |  |
| `size` | vec2 |  |
| `pivot` | vec2 | plage 0 .. 1 |
| `uvRect` | vec4 |  |
| `columns` | int | Grid cells across: more = smoother bends ; plage 1 .. 64 |
| `rows` | int | plage 1 .. 64 |
| `stiffness` | float | Higher = rigid parts and sharper joints, lower = soft bends ; plage 0.5 .. 16 |
| `sortingLayer` | int |  |
| `orderInLayer` | int |  |
| `pixelArt` | bool |  |
| `lit` | bool |  |
| `visible` | bool |  |

### SpriteAnimator

Frame-by-frame animation: plays a clip of the sprite sheet of the SpriteRenderer.

*2D*

| Champ | Type | Détails |
|---|---|---|
| `clip` | string | Clip name defined in the sprite sheet |
| `speed` | float | plage 0 .. 20 |
| `playing` | bool |  |
| `time` | float |  (non enregistré) |

### Tilemap

Grid of tiles from a tileset sprite sheet. Paint it with the Tilemap tool of the editor.

*2D*

| Champ | Type | Détails |
|---|---|---|
| `tileset` | asset | Sprite sheet whose frames are the tiles |
| `cellSize` | vec2 |  |
| `width` | uint | plage 1 .. 4096 |
| `height` | uint | plage 1 .. 4096 |
| `color` | color |  |
| `sortingLayer` | int |  |
| `orderInLayer` | int |  |
| `lit` | bool |  |
| `normalMap` | asset | Normal map laid out like the tileset texture (lit tilemaps) |
| `animatedTiles` | string | Animated tiles: "f0,f1,f2@fps;..." (cells holding f0 cycle) |
| `autotile` | string | Autotile groups: "F;G" — 16 frames from F chosen by the neighbours (+y=1, +x=2, -y=4, -x=8) |
| `dataWidth` | uint |  |
| `tiles` | int[] |  |

### TwoBoneIK2D

On an upper bone (arm, thigh): turns it and its child bone so the child's tip reaches the target.

*2D*

| Champ | Type | Détails |
|---|---|---|
| `target` | entity | Entity the chain reaches for (hand or foot target) |
| `flipBend` | bool | Bend the other way (elbow / knee) |
| `weight` | float | plage 0 .. 1 |
| `enabled` | bool |  |

### Animator

Animates fields of the entity and its children with clips (.agranim) and a state machine (.agrctrl).

*Animation*

| Champ | Type | Détails |
|---|---|---|
| `controller` | asset | State machine (.agrctrl) |
| `clip` | asset | Clip played alone when there is no state machine (.agranim) |
| `speed` | float | plage 0 .. 10 |
| `playing` | bool |  |

### ChainIK3D

On the root bone of a chain (spine, tail, tentacle, crane): FABRIK, the chain's end reaches the target.

*Animation*

| Champ | Type | Détails |
|---|---|---|
| `target` | entity |  |
| `pole` | entity | Optional: the joints bend toward it |
| `boneCount` | int | Bones in the chain (following the first child) ; plage 1 .. 64 |
| `iterations` | int | plage 1 .. 64 |
| `maxAngle` | angle | Bend limit at each joint ; plage 0 .. 3.1416 |
| `jointLimits` | string | Per joint limits in degrees, from the first inner joint: "30, 90, 45" |
| `weight` | float | plage 0 .. 1 |
| `enabled` | bool |  |

### LookAtIK3D

Turns the bone (head, eyes, turret) toward the target, within an angle of its animated direction.

*Animation*

| Champ | Type | Détails |
|---|---|---|
| `target` | entity |  |
| `forwardAxis` | vec3 | Local axis that points at the target |
| `maxAngle` | angle | plage 0 .. 3.1416 |
| `weight` | float | plage 0 .. 1 |
| `enabled` | bool |  |

### TwoBoneIK3D

On an upper bone (arm, thigh): bends it and the next bone so the tip (hand, foot) reaches the target.

*Animation*

| Champ | Type | Détails |
|---|---|---|
| `target` | entity | Entity the tip reaches for |
| `pole` | entity | Optional: the elbow / knee points toward it |
| `mid` | entity | Middle bone (default: first child) |
| `tip` | entity | End bone (default: first child of the middle bone) |
| `weight` | float | plage 0 .. 1 |
| `enabled` | bool |  |

### AudioListener

Position and orientation from which spatial sounds are heard (default: the primary camera).

*Audio*

| Champ | Type | Détails |
|---|---|---|
| `active` | bool |  |

### AudioSource

Plays a sound or music (.wav, .ogg, .mp3, .flac), optionally positioned in space.

*Audio*

| Champ | Type | Détails |
|---|---|---|
| `clip` | asset | Audio file, e.g. assets://audio/jump.wav |
| `bus` | enum | Mixer bus (volume groups) ; valeurs : `Master`, `Music`, `Effects`, `Ambient`, `Interface`, `Voice` |
| `volume` | float | plage 0 .. 2 |
| `pitch` | float | Playback speed ; plage 0.1 .. 4 |
| `loop` | bool |  |
| `playOnStart` | bool | Starts with the game |
| `spatial` | bool | Attenuated and panned from its position |
| `minDistance` | float | plage 0.01 .. 1000 |
| `maxDistance` | float | plage 0.01 .. 10000 |
| `stream` | bool | Decode while playing (long music): less memory |
| `attenuation` | enum | How the volume falls with distance (spatial) ; valeurs : `None`, `Inverse`, `Linear`, `Exponential` |
| `rolloff` | float | plage 0 .. 20 |
| `doppler` | float | Pitch shift of moving sources (0 = off, 1 = real) ; plage 0 .. 10 |
| `coneInnerAngle` | angle | Full volume inside (forward = -Z) ; plage 0 .. 6.2832 |
| `coneOuterAngle` | angle | plage 0 .. 6.2832 |
| `coneOuterVolume` | float | Volume outside the outer angle ; plage 0 .. 1 |
| `alternateClips` | string | Other clips picked at random (';'-separated) |
| `randomPitch` | float | Pitch varies by up to this (0.1 = 0.9x .. 1.1x) ; plage 0 .. 1 |
| `occlusion` | bool | Spatial: muffled and quieter behind 3D colliders (walls) |
| `occlusionStrength` | float | How much a wall removes (0..1) ; plage 0 .. 1 |
| `randomVolume` | float | Volume lowered by up to this (0.2 = 80% .. 100%) ; plage 0 .. 1 |

### AudioZone

Re-routes spatial sounds to another bus (reverb, underwater...) inside a box or sphere, or everywhere while the listener is inside.

*Audio*

| Champ | Type | Détails |
|---|---|---|
| `shape` | enum | valeurs : `Box`, `Sphere` |
| `size` | vec3 | Box size (before scale) |
| `radius` | float | Sphere radius (before scale) ; plage 0 .. 10000 |
| `bus` | enum | Bus the sounds play on (give it a Reverb in the mixer) ; valeurs : `Master`, `Music`, `Effects`, `Ambient`, `Interface`, `Voice` |
| `mode` | enum | Sources: sounds inside the zone. Listener: every sound while the listener is inside ; valeurs : `Sources`, `Listener` |
| `priority` | int | Highest wins where zones overlap |

### Name

*Core* — ajouté par le moteur

| Champ | Type | Détails |
|---|---|---|
| `name` | string |  |

### State

*Core* — ajouté par le moteur

| Champ | Type | Détails |
|---|---|---|
| `active` | bool | Inactive entities and their children are skipped by systems. |
| `layer` | uint | plage 0 .. 31 |
| `hiddenInEditor` | bool |  |
| `lockedInEditor` | bool |  |

### Transform

Position, rotation and scale relative to the parent entity.

*Core* — ajouté par le moteur

| Champ | Type | Détails |
|---|---|---|
| `position` | vec3 |  |
| `rotation` | rotation |  |
| `scale` | vec3 |  |

### ParticleAttractor

Pulls (strength > 0) or pushes (< 0) nearby particles.

*Effects*

| Champ | Type | Détails |
|---|---|---|
| `strength` | float |  |
| `radius` | float | plage 0 .. 1000 |
| `enabled` | bool |  |

### ParticleCollider

Obstacle the particles of emitters with collisions bounce on (sphere or box).

*Effects*

| Champ | Type | Détails |
|---|---|---|
| `shape` | enum | valeurs : `Sphere`, `Box` |
| `size` | vec3 | Box size, or sphere diameter (x) |

### Trail

Ribbon left behind the entity as it moves (projectiles, swords, dashes).

*Effects*

| Champ | Type | Détails |
|---|---|---|
| `emitting` | bool |  |
| `lifetime` | float | plage 0.01 .. 30 |
| `minDistance` | float | plage 0.001 .. 10 |
| `width` | float | plage 0 .. 100 |
| `color` | color |  |
| `curves` | asset | Width curve and color gradient over the age (.agrparticle) |
| `texture` | asset |  |
| `additive` | bool |  |
| `sortingLayer` | int |  |
| `orderInLayer` | int |  |
| `maxPoints` | uint | plage 2 .. 4096 |

### NavAgent

Follows paths toward a destination given by a script (entity:navigateTo).

*Navigation*

| Champ | Type | Détails |
|---|---|---|
| `speed` | float | plage 0 .. 100 |
| `stoppingDistance` | float | plage 0 .. 10 |
| `faceMovement` | bool | 3D: turns the entity (its +Z axis) toward where it goes |
| `plane` | enum | Auto: 3D navigation mesh if the scene has one, else the 2D grid ; valeurs : `Auto`, `Grid2D`, `Mesh3D` |
| `avoidance` | bool | Steers around other agents and obstacles (crowds) |
| `radius` | float | plage 0.01 .. 20 |

### NavArea

Zone where walking costs more (mud, water) or is forbidden: a box centered on the entity.

*Navigation*

| Champ | Type | Détails |
|---|---|---|
| `size` | vec3 |  |
| `cost` | float | Path cost multiplier (1 = normal, 3 = avoided unless much shorter) ; plage 1 .. 100 |
| `walkable` | bool | Off: a forbidden zone (a hole in the navigation) |

### NavLink

Shortcut for paths (jump, ladder, teleporter) from this entity to `end` (relative).

*Navigation*

| Champ | Type | Détails |
|---|---|---|
| `end` | vec3 |  |
| `bidirectional` | bool |  |
| `cost` | float | Multiplies its length in path costs ; plage 0.01 .. 100 |

### NavObstacle

Agents steer around it; with carve, it is also cut out of the navigation (paths go around it).

*Navigation*

| Champ | Type | Détails |
|---|---|---|
| `radius` | float | plage 0.01 .. 100 |
| `height` | float | plage 0.01 .. 100 |
| `carve` | bool | Cut out of the navigation (rebuilt when it moves) |

### NavigationSettings

Build parameters of the 2D grid and the 3D navigation mesh (static colliders and tilemaps).

*Navigation*

| Champ | Type | Détails |
|---|---|---|
| `gridCellSize` | float | plage 0.05 .. 10 |
| `agentRadius2D` | float | plage 0 .. 10 |
| `cellSize` | float | plage 0.02 .. 5 |
| `agentRadius` | float | plage 0 .. 10 |
| `backgroundBuild` | bool | Rebuild the 3D mesh on a worker thread (the old one serves meanwhile) |
| `agentSizes` | string | Extra agent radii, ';'-separated ("0.8;1.5"): a grid and a mesh per size |
| `agentHeight` | float | plage 0.1 .. 20 |
| `agentMaxClimb` | float | plage 0 .. 5 |
| `agentMaxSlope` | float | plage 0 .. 89 |
| `meshRenderersAsSources` | bool | Static imported models are walkable geometry |

### NetSync

Kept in step over the network: Transform (interpolated) and listed fields, sent by its owner.

*Network*

| Champ | Type | Détails |
|---|---|---|
| `owner` | int | Peer that simulates it (1 = host); set by the host at run time |
| `syncTransform` | bool |  |
| `interpolate` | bool | Smooth between received states |
| `rate` | float | States per second ; plage 1 .. 120 |
| `fields` | string | Component.field separated by ';' (e.g. Health.value;SpriteRenderer.color) |
| `visibility` | enum | Who receives it: everyone, its owner only, players owning an entity within visibleRange ; valeurs : `All`, `OwnerOnly`, `Range` |
| `visibleRange` | float | plage 0 .. 100000 |
| `onlyChanges` | bool | Send only when the state changed (and once per second) |
| `spawnWhenVisible` | bool | Network-spawned: exists only for the players that can see it |

### CharacterController2D

Character moved by its velocity: slides on walls and slopes, stands on floors, follows moving platforms (move and slide).

*Physics 2D*

| Champ | Type | Détails |
|---|---|---|
| `size` | vec2 | Capsule width x height |
| `offset` | vec2 |  |
| `velocity` | vec2 | m/s, set by scripts; walls and floors remove the part going into them |
| `gravityScale` | float | plage -10 .. 10 |
| `maxSlope` | angle | Steepest floor ; plage 0 .. 1.55 |
| `snapDistance` | float | Stays on the floor going down slopes ; plage 0 .. 2 |
| `pushForce` | float | > 0: pushes dynamic bodies; 0: they block ; plage 0 .. 100 |
| `layer` | int | plage 0 .. 31 |
| `collidesWith` | uint |  |
| `onFloor` | bool |  (non enregistré) (lecture seule) |
| `onWall` | bool |  (non enregistré) (lecture seule) |
| `onCeiling` | bool |  (non enregistré) (lecture seule) |
| `floorNormal` | vec2 |  (non enregistré) (lecture seule) |

### Collider2D

Collision shape (box, circle, capsule, polygon, chain). Static if no RigidBody2D on the entity or its parents.

*Physics 2D*

| Champ | Type | Détails |
|---|---|---|
| `shape` | enum | valeurs : `Box`, `Circle`, `Capsule`, `Polygon`, `Chain` |
| `size` | vec2 | Box / capsule size (world units, before scale) |
| `radius` | float | Circle radius ; plage 0.001 .. 1000 |
| `offset` | vec2 |  |
| `density` | float | plage 0 .. 10000 |
| `friction` | float | plage 0 .. 10 |
| `restitution` | float | plage 0 .. 1 |
| `isTrigger` | bool | Detects overlaps without colliding |
| `layer` | int | Collision layer (0..31) ; plage 0 .. 31 |
| `collidesWith` | uint | Layers this collides with |
| `points` | vec2[] | Polygon / Chain vertices (local units) |
| `loop` | bool | Chain: closes the outline |
| `oneWay` | bool | Solid only from above (local +Y): jump through it from below |

### Joint2D

Joint between this body and another one (or the world): fixed, hinge, distance / spring, slider.

*Physics 2D*

| Champ | Type | Détails |
|---|---|---|
| `type` | enum | valeurs : `Fixed`, `Hinge`, `Distance`, `Slider` |
| `connectedBody` | entity | Other body (none = a fixed point of the world) |
| `anchor` | vec2 | Joint point, local to this entity |
| `autoConnectedAnchor` | bool | The connected anchor is the same world point as the anchor when the joint is created |
| `connectedAnchor` | vec2 | Local to the connected body (world point without one) |
| `axis` | vec2 | Slider direction, local to this entity |
| `length` | float | Distance: rest length (0 = distance when created) ; plage 0 .. 10000 |
| `springFrequency` | float | Hz, 0 = rigid ; plage 0 .. 120 |
| `springDamping` | float | Damping ratio (1 = no bounce) ; plage 0 .. 10 |
| `enableLimits` | bool |  |
| `lowerAngle` | angle | Hinge ; plage -6.28319 .. 6.28319 |
| `upperAngle` | angle | Hinge ; plage -6.28319 .. 6.28319 |
| `lowerLimit` | float | Slider translation / Distance min length (m) |
| `upperLimit` | float | Slider translation / Distance max length (m) |
| `enableMotor` | bool |  |
| `motorSpeed` | float | Hinge rad/s, slider m/s |
| `maxMotorForce` | float | N (N·m for a hinge) ; plage 0 .. 1e+09 |
| `breakForce` | float | N; 0 = unbreakable ; plage 0 .. 1e+09 |
| `collideConnected` | bool | The two bodies still collide with each other |
| `broken` | bool | The joint broke (untick to rebuild it) (non enregistré) |

### RigidBody2D

2D physics body (Box2D): moved by gravity, forces and collisions.

*Physics 2D*

| Champ | Type | Détails |
|---|---|---|
| `type` | enum | valeurs : `Static`, `Kinematic`, `Dynamic` |
| `gravityScale` | float | plage -10 .. 10 |
| `linearDamping` | float | plage 0 .. 100 |
| `angularDamping` | float | plage 0 .. 100 |
| `fixedRotation` | bool | Never rotates (characters) |
| `bullet` | bool | Continuous collision for fast bodies |
| `initialVelocity` | vec2 |  |

### TilemapCollider2D

Static collision for the painted cells of the Tilemap.

*Physics 2D*

| Champ | Type | Détails |
|---|---|---|
| `oneWayTiles` | string | Tiles that are one-way platforms (frame indices: "12,13") |
| `slopeTiles` | string | Slope tiles: "20:/,21:\" (/ rises to the right) |
| `friction` | float | plage 0 .. 10 |
| `restitution` | float | plage 0 .. 1 |
| `layer` | int | Collision layer (0..31) ; plage 0 .. 31 |
| `collidesWith` | uint | Layers this collides with |

### CharacterController3D

Character capsule moved by its velocity: slides along walls, climbs steps and slopes, pushes bodies. Feet at the entity origin.

*Physics 3D*

| Champ | Type | Détails |
|---|---|---|
| `radius` | float | plage 0.05 .. 50 |
| `height` | float | Feet to head ; plage 0.1 .. 100 |
| `maxSlope` | angle | Steepest walkable slope ; plage 0 .. 1.55334 |
| `stepHeight` | float | Steps climbed without jumping ; plage 0 .. 10 |
| `gravityScale` | float | plage -10 .. 10 |
| `mass` | float | kg, for pushing bodies ; plage 0.1 .. 100000 |
| `pushForce` | float | Strongest push on dynamic bodies (N) ; plage 0 .. 1e+07 |
| `layer` | int | Collision layer (0..31) ; plage 0 .. 31 |
| `collidesWith` | uint | Layers this collides with |

### Collider

3D collision shape (box, sphere, capsule, model mesh, convex hull). Static if no RigidBody on the entity or its parents.

*Physics 3D*

| Champ | Type | Détails |
|---|---|---|
| `shape` | enum | Mesh: exact triangles of a model (static / kinematic). Convex: hull of the model (any body) ; valeurs : `Box`, `Sphere`, `Capsule`, `Mesh`, `Convex` |
| `size` | vec3 | Box size (before scale) |
| `radius` | float | plage 0.001 .. 1000 |
| `height` | float | Capsule total height ; plage 0.001 .. 1000 |
| `offset` | vec3 |  |
| `density` | float | plage 0.001 .. 100000 |
| `friction` | float | plage 0 .. 10 |
| `restitution` | float | plage 0 .. 1 |
| `isTrigger` | bool | Detects overlaps without colliding |
| `layer` | int | Collision layer (0..31) ; plage 0 .. 31 |
| `collidesWith` | uint | Layers this collides with |
| `model` | asset | Mesh / Convex: model file; empty = the MeshRenderer's model |
| `meshIndex` | int | Mesh / Convex: mesh of the model (-1 = the MeshRenderer's) ; plage -1 .. 100000 |

### Joint3D

Joint between this body and another one (or the world): fixed, hinge, distance / spring, slider, ball.

*Physics 3D*

| Champ | Type | Détails |
|---|---|---|
| `type` | enum | valeurs : `Fixed`, `Hinge`, `Distance`, `Slider`, `Ball`, `Pulley`, `SwingTwist` |
| `connectedBody` | entity | Other body (none = a fixed point of the world) |
| `anchor` | vec3 | Joint point, local to this entity |
| `autoConnectedAnchor` | bool | The connected anchor is the same world point as the anchor when the joint is created |
| `connectedAnchor` | vec3 | Local to the connected body (world point without one) |
| `axis` | vec3 | Hinge / slider axis, local to this entity |
| `length` | float | Distance: rest length (0 = distance when created) ; plage 0 .. 10000 |
| `springFrequency` | float | Hz, 0 = rigid ; plage 0 .. 120 |
| `springDamping` | float | Damping ratio (1 = no bounce) ; plage 0 .. 10 |
| `enableLimits` | bool |  |
| `lowerAngle` | angle | Hinge ; plage -6.28319 .. 6.28319 |
| `upperAngle` | angle | Hinge ; plage -6.28319 .. 6.28319 |
| `lowerLimit` | float | Slider translation / Distance min length (m) |
| `upperLimit` | float | Slider translation / Distance max length (m) |
| `enableMotor` | bool |  |
| `motorSpeed` | float | Hinge rad/s, slider m/s |
| `maxMotorForce` | float | N (N·m for a hinge) ; plage 0 .. 1e+09 |
| `breakForce` | float | N; 0 = unbreakable ; plage 0 .. 1e+09 |
| `collideConnected` | bool | The two bodies still collide with each other |
| `broken` | bool | The joint broke (untick to rebuild it) (non enregistré) |
| `pulleyPoint` | vec3 | Pulley: fixed world point above this body |
| `connectedPulleyPoint` | vec3 | Pulley: fixed world point above the connected body |
| `ratio` | float | Pulley ratio ; plage 0.01 .. 100 |
| `swingLimit` | angle | SwingTwist: cone half angle ; plage 0 .. 3.14159 |

### RigidBody

3D physics body (Jolt): moved by gravity, forces and collisions.

*Physics 3D*

| Champ | Type | Détails |
|---|---|---|
| `type` | enum | valeurs : `Static`, `Kinematic`, `Dynamic` |
| `mass` | float | kg; 0 = from the colliders' density ; plage 0 .. 1e+06 |
| `linearDamping` | float | plage 0 .. 100 |
| `angularDamping` | float | plage 0 .. 100 |
| `gravityScale` | float | plage -10 .. 10 |
| `freezeRotation` | bool | Translation only (characters) |
| `bullet` | bool | Continuous collision for fast bodies |
| `initialVelocity` | vec3 |  |

### SoftBody3D

Deformable body (cloth, jelly, balloon) simulated by Jolt, drawn with the entity's MeshRenderer material.

*Physics 3D*

| Champ | Type | Détails |
|---|---|---|
| `shape` | enum | valeurs : `Cloth`, `Box`, `Sphere` |
| `size` | vec3 | Cloth: x by z; box: xyz; sphere: x = diameter |
| `resolution` | uint | Vertices per side (rings for a sphere) ; plage 2 .. 64 |
| `mass` | float | plage 0.001 .. 10000 |
| `stiffness` | float | Stretch resistance ; plage 0 .. 1 |
| `bendStiffness` | float | Fold resistance ; plage 0 .. 1 |
| `pressure` | float | Closed shapes: > 0 inflates (balloon) ; plage 0 .. 100000 |
| `pin` | enum | Cloth: vertices held by the entity ; valeurs : `None`, `Edge`, `Corners` |
| `iterations` | uint | plage 1 .. 30 |
| `damping` | float | plage 0 .. 10 |
| `gravityScale` | float | plage -10 .. 10 |
| `enabled` | bool |  |

### Vehicle3D

Wheeled vehicle on the dynamic RigidBody: suspension, engine, steering, brakes. Children WheelFL/FR/RL/RR follow the wheels.

*Physics 3D*

| Champ | Type | Détails |
|---|---|---|
| `throttle` | float | -1 reverse .. 1 full gas (scripts) ; plage -1 .. 1 (non enregistré) |
| `steering` | float | -1 left .. 1 right (scripts) ; plage -1 .. 1 (non enregistré) |
| `brake` | float | plage 0 .. 1 (non enregistré) |
| `handBrake` | bool |  (non enregistré) |
| `wheelBase` | float | Distance between the axles ; plage 0.2 .. 50 |
| `trackWidth` | float | Distance between left and right wheels ; plage 0.2 .. 50 |
| `wheelHeight` | float | Local height of the suspension attachment ; plage -10 .. 10 |
| `wheelRadius` | float | plage 0.05 .. 5 |
| `wheelWidth` | float | plage 0.02 .. 5 |
| `suspensionMin` | float | plage 0 .. 5 |
| `suspensionMax` | float | plage 0.01 .. 5 |
| `suspensionFrequency` | float | Spring stiffness (Hz) ; plage 0.1 .. 20 |
| `suspensionDamping` | float | plage 0 .. 5 |
| `maxSteerAngle` | angle | plage 0 .. 1.4 |
| `engineTorque` | float | N·m ; plage 0 .. 100000 |
| `maxRpm` | float | plage 500 .. 30000 |
| `brakeTorque` | float | plage 0 .. 100000 |
| `handBrakeTorque` | float | plage 0 .. 100000 |
| `frontDrive` | bool |  |
| `rearDrive` | bool |  |
| `tireFriction` | float | plage 0 .. 5 |
| `speed` | float | m/s forward (simulation) (non enregistré) (lecture seule) |
| `rpm` | float |  (non enregistré) (lecture seule) |
| `gear` | int |  (non enregistré) (lecture seule) |

### BlobShadow

Soft round shadow on the ground under the entity (cheap, readable, mobile-friendly).

*Rendering*

| Champ | Type | Détails |
|---|---|---|
| `radius` | float | plage 0.01 .. 100 |
| `opacity` | float | plage 0 .. 1 |
| `softness` | float | plage 0 .. 1 |
| `groundHeight` | float | World height of the ground |
| `fadeHeight` | float | Height above the ground where the shadow has faded out ; plage 0.01 .. 1000 |
| `color` | color |  |

### Camera

Renders the scene from this entity, looking down its -Z axis.

*Rendering*

| Champ | Type | Détails |
|---|---|---|
| `projection` | enum | valeurs : `Perspective`, `Orthographic` |
| `fieldOfView` | angle | plage 0.0174533 .. 2.96706 |
| `orthographicSize` | float | plage 0.01 .. 10000 |
| `nearPlane` | float | plage 0.001 .. 1000 |
| `farPlane` | float | plage 0.01 .. 100000 |
| `clearColor` | color |  |
| `primary` | bool |  |
| `scaleMode` | enum | Orthographic: how the view adapts to screen shapes (phones, tablets) ; valeurs : `FixedHeight`, `FixedWidth`, `FitInside`, `Crop` |
| `referenceAspect` | float | Width / height of the design area (16:9 = 1.778) ; plage 0.2 .. 5 |

### Decal

Projects an image onto the surfaces inside its box (impacts, stains, road marks). Projects along local -Y.

*Rendering*

| Champ | Type | Détails |
|---|---|---|
| `texture` | asset | Color + alpha image (empty: a plain square) |
| `color` | color | Tint; alpha = opacity |
| `size` | vec3 | Box size: x, z = image, y = projection depth |
| `normalFade` | float | 0: every slope; 1: only surfaces facing the decal ; plage 0 .. 1 |
| `upperFade` | float | plage 0 .. 1 |
| `lowerFade` | float | plage 0 .. 1 |
| `emission` | float | Glow strength (adds light) ; plage 0 .. 16 |
| `enabled` | bool |  |

### DirectionalLight

Light shining along the entity's forward (-Z) axis, like the sun. Casts shadows.

*Rendering*

| Champ | Type | Détails |
|---|---|---|
| `color` | color |  |
| `intensity` | float | plage 0 .. 20 |
| `castShadows` | bool |  |
| `shadowDistance` | float | Shadows are drawn up to this distance from the camera (smaller = sharper) ; plage 1 .. 1000 |
| `shadowStrength` | float | plage 0 .. 1 |

### Environment

Ambient light, sky, fog and exposure of the scene.

*Rendering*

| Champ | Type | Détails |
|---|---|---|
| `ambientColor` | color | Flat ambient light (sky = CameraColor) |
| `ambientIntensity` | float | plage 0 .. 4 |
| `sky` | enum | Gradient: procedural sky and hemisphere ambient light from its colors; Image: an equirectangular image lights the scene (diffuse + reflections) ; valeurs : `CameraColor`, `Gradient`, `Image` |
| `skyTexture` | asset | Image sky: equirectangular 2:1 image (.hdr for real light levels, or .png / .jpg) |
| `skyIntensity` | float | plage 0 .. 16 |
| `skyRotation` | angle | Image sky: turn around the vertical axis |
| `skyColor` | color |  |
| `horizonColor` | color |  |
| `groundColor` | color |  |
| `sunDisc` | bool | Draws the sun of the directional light in the gradient sky |
| `fog` | bool |  |
| `fogColor` | color |  |
| `fogStart` | float | plage 0 .. 100000 |
| `fogEnd` | float | plage 0 .. 100000 |
| `exposure` | float | plage 0.01 .. 16 |

### MeshRenderer

Draws a built-in mesh or a mesh of an imported model (glTF, OBJ, FBX) with PBR materials.

*Rendering*

| Champ | Type | Détails |
|---|---|---|
| `mesh` | enum | Built-in mesh (used when no model is set) ; valeurs : `Cube`, `Sphere`, `Plane`, `Quad` |
| `model` | asset | Imported model: assets://models/robot.glb |
| `meshIndex` | int | Mesh of the model ; plage 0 .. 100000 |
| `skin` | int | Skin of the model (-1 = none): the mesh follows the joints of its skeleton |
| `material` | asset | Optional .agrmat material replacing the mesh materials |
| `texture` | asset | Built-in material: base color texture |
| `color` | color | Tint applied to any material |
| `uvScale` | vec2 |  |
| `metallic` | float | Built-in material ; plage 0 .. 1 |
| `roughness` | float | Built-in material ; plage 0 .. 1 |
| `morph` | vec4 | Morph target (blend shape) weights 0..3 of an imported mesh ; plage -1 .. 2 |
| `morph2` | vec4 | Morph target weights 4..7 ; plage -1 .. 2 |
| `castShadows` | bool |  |
| `visible` | bool |  |
| `lodBias` | float | Automatic LOD of imported models: > 1 keeps detail longer, 0 = always full detail ; plage 0 .. 8 |
| `visibilityRange` | float | Hidden beyond this distance from the camera, shadows included (0 = always drawn) ; plage 0 .. 100000 |
| `toon` | bool | Cartoon lighting for every material of the mesh: light bands, tinted shadows, rim light |
| `outlineWidth` | float | Outline in pixels (0: the material's). Best on smooth meshes ; plage 0 .. 16 |
| `outlineColor` | color |  |

### Occluder

Hides the objects behind it from the renderer (occlusion culling). Keep it inside the visible geometry.

*Rendering*

| Champ | Type | Détails |
|---|---|---|
| `shape` | enum | valeurs : `Box`, `Quad` |
| `size` | vec3 | Full size in local units (Quad: x and y) |
| `enabled` | bool |  |

### ParticleEmitter3D

3D particles (camera-facing): fire, smoke, sparks, magic, rain... (presets in the inspector).

*Rendering*

| Champ | Type | Détails |
|---|---|---|
| `emitting` | bool |  |
| `loop` | bool |  |
| `duration` | float | plage 0.01 .. 600 |
| `rate` | float | Particles per second ; plage 0 .. 10000 |
| `burst` | uint | Particles at the start of each cycle ; plage 0 .. 10000 |
| `maxParticles` | uint | plage 1 .. 100000 |
| `lifetimeMin` | float | plage 0.01 .. 60 |
| `lifetimeMax` | float | plage 0.01 .. 60 |
| `speedMin` | float | plage 0 .. 200 |
| `speedMax` | float | plage 0 .. 200 |
| `emitFromSurface` | bool | Sphere / box: start on the surface |
| `alignToVelocity` | bool | Mesh particles point along their motion |
| `shape` | enum | valeurs : `Point`, `Sphere`, `Box`, `Cone`, `Ring` |
| `shapeRadius` | float | plage 0 .. 100 |
| `boxSize` | vec3 |  |
| `coneAngle` | angle | plage 0 .. 3.14159 |
| `gravity` | vec3 |  |
| `drag` | float | plage 0 .. 20 |
| `startColor` | color |  |
| `endColor` | color |  |
| `startSize` | float | plage 0 .. 100 |
| `endSize` | float | plage 0 .. 100 |
| `angularVelocity` | angle |  |
| `texture` | asset | Empty = soft round particle |
| `curves` | asset | Size curve and color gradient over the lifetime (.agrparticle) |
| `flipbookColumns` | int | Frames of the texture across ; plage 1 .. 16 |
| `flipbookRows` | int | plage 1 .. 16 |
| `flipbookCycles` | float | Times the frames play during a life ; plage 0 .. 32 |
| `velocityStretch` | float | Stretch along the motion (seconds of speed): sparks, rain ; plage 0 .. 2 |
| `collisions` | bool | Bounce on the ground plane and ParticleCollider shapes (world space only) |
| `groundPlane` | bool |  |
| `groundHeight` | float |  |
| `bounce` | float | plage 0 .. 1 |
| `collisionFriction` | float | plage 0 .. 1 |
| `dieOnCollision` | bool |  |
| `affectedByAttractors` | bool |  |
| `turbulence` | float | Swirling noise force (units / s^2), 0 = off ; plage -100 .. 100 |
| `turbulenceScale` | float | Size of the swirls (world units) ; plage 0.01 .. 100 |
| `turbulenceSpeed` | float | How fast the swirls change ; plage 0 .. 20 |
| `inheritVelocity` | float | Fraction of the emitter's velocity given to new particles (world space) ; plage -2 .. 2 |
| `lit` | bool | Lit by ambient, the directional light and point / spot lights in range |
| `meshParticles` | bool | Each particle is a small lit 3D mesh (debris, rocks, coins) |
| `particleMesh` | enum | valeurs : `Cube`, `Sphere`, `Plane`, `Quad` |
| `particleModel` | asset | Imported model whose first mesh is used (overrides particleMesh) |
| `meshMetallic` | float | plage 0 .. 1 |
| `meshRoughness` | float | plage 0 .. 1 |
| `meshCastShadows` | bool |  |
| `subEmitter` | entity | Emitter that bursts where a particle dies or hits something |
| `particleMaterial` | asset | Mesh particles: .agrmat material (textures, shader, transparency); the particle color tints it |
| `subEmitOn` | enum | valeurs : `Death`, `Collision` |
| `subEmitCount` | int | plage 0 .. 1000 |
| `additive` | bool | Adds light (fire, sparks, magic) |
| `worldSpace` | bool | Particles stay where emitted when the emitter moves |
| `seed` | uint | 0 = random; otherwise the same particles every time |

### PointLight

Light radiating in every direction, up to its range (bulbs, torches).

*Rendering*

| Champ | Type | Détails |
|---|---|---|
| `color` | color |  |
| `intensity` | float | plage 0 .. 1000 |
| `range` | float | plage 0.01 .. 1000 |
| `castShadows` | bool | Shadows in every direction (the nearest shadowed point light; 6 shadow views) |

### PostProcess

Image effects: bloom, anti-aliasing (FXAA), tone mapping, color grading, vignette.

*Rendering*

| Champ | Type | Détails |
|---|---|---|
| `bloom` | bool | Glow around bright areas (needs a graphics profile that allows it) |
| `bloomIntensity` | float | plage 0 .. 5 |
| `bloomThreshold` | float | Brightness above which pixels glow ; plage 0 .. 10 |
| `bloomRadius` | float | plage 0.25 .. 4 |
| `fxaa` | bool | Smooths jagged edges (cheap anti-aliasing) |
| `filmicToneMapping` | bool | ACES filmic curve; off for flat 2D colors |
| `contrast` | float | plage 0 .. 3 |
| `saturation` | float | plage 0 .. 3 |
| `colorFilter` | color |  |
| `vignette` | float | plage 0 .. 1 |
| `ambientOcclusion` | bool | Contact shadows in corners and under objects (SSAO, half resolution; MSAA off, FXAA instead) |
| `aoRadius` | float | plage 0.05 .. 5 |
| `aoIntensity` | float | plage 0 .. 2 |
| `heightFog` | bool | Fog denser low down, thinning with height, lit around the sun |
| `fogDensity` | float | plage 0 .. 2 |
| `fogHeight` | float | World height of the given density |
| `fogFalloff` | float | Thinning per world unit of height ; plage 0 .. 5 |
| `fogColor` | color |  |
| `fogLightScatter` | float | Halos of point / spot lights in the height fog ; plage 0 .. 10 |
| `fogSunScatter` | float | plage 0 .. 5 |
| `lightShafts` | bool | Rays of sunlight through gaps (god rays) |
| `shaftIntensity` | float | plage 0 .. 5 |
| `shaftDecay` | float | 0.9 = short rays, 0.99 = long rays ; plage 0.5 .. 0.999 |
| `depthOfField` | bool | Lens blur in front of and behind the focus zone (bokeh) |
| `focusDistance` | float | World units from the camera ; plage 0.01 .. 10000 |
| `focusRange` | float | Depth of the sharp zone ; plage 0.01 .. 10000 |
| `dofBlur` | float | Largest blur radius (fraction of the image height) ; plage 0 .. 0.05 |
| `dofNear` | bool |  |
| `dofFar` | bool |  |
| `motionBlur` | bool | Blur along the camera's motion |
| `motionBlurIntensity` | float | plage 0 .. 1 |
| `temporalAA` | bool | Temporal anti-aliasing: smoother edges, accumulated over frames |
| `taaBlend` | float | Weight of the new frame ; plage 0.02 .. 1 |
| `screenSpaceReflections` | bool | Smooth / metallic surfaces reflect what is on screen (wet floors, metal) |
| `ssrIntensity` | float | plage 0 .. 2 |
| `ssrMaxDistance` | float | World units a reflected ray travels at most ; plage 0.1 .. 1000 |
| `ssrThickness` | float | Depth of the surfaces a ray can hit ; plage 0.01 .. 10 |
| `autoExposure` | bool | Brightness adapts to the light seen (HDR) |
| `exposureTarget` | float | plage 0.01 .. 4 |
| `exposureMin` | float | plage 0.01 .. 64 |
| `exposureMax` | float | plage 0.01 .. 64 |
| `exposureSpeed` | float | Adaptation speed (per second) ; plage 0.01 .. 100 |

### ReflectionProbe

Captures the scene around it: shiny surfaces inside its box reflect the surroundings (rooms, metal, water).

*Rendering*

| Champ | Type | Détails |
|---|---|---|
| `size` | vec3 | Box of influence (full size) |
| `intensity` | float | plage 0 .. 4 |
| `blendDistance` | float | Fade-in distance from the box faces ; plage 0 .. 50 |
| `boxProjection` | bool | Line reflections up with the walls of a room the size of the box |
| `update` | enum | Once: when added or moved. Always: refreshed continuously (6 frames each) |
| `maxDistance` | float | plage 1 .. 5000 |
| `enabled` | bool |  |
| `diffuseGI` | float | Indirect diffuse light from the capture (global illumination), 0 = off ; plage 0 .. 4 |

### SpotLight

Cone of light along the entity's forward (-Z) axis (flashlights, street lamps).

*Rendering*

| Champ | Type | Détails |
|---|---|---|
| `color` | color |  |
| `intensity` | float | plage 0 .. 1000 |
| `range` | float | plage 0.01 .. 1000 |
| `innerAngle` | angle | plage 0 .. 1.55334 |
| `outerAngle` | angle | plage 0.0174533 .. 1.55334 |
| `castShadows` | bool | Shadows in the cone (the 2 nearest shadowed spot lights; needs a profile with shadows) |

### SpriteRenderer

Textured quad for 2D games, batched automatically.

*Rendering*

| Champ | Type | Détails |
|---|---|---|
| `texture` | asset | Asset URI, e.g. assets://sprites/hero.png |
| `color` | color |  |
| `size` | vec2 |  |
| `pivot` | vec2 | plage 0 .. 1 |
| `uvRect` | vec4 | u0, v0, u1, v1 — region of a sprite sheet |
| `flipX` | bool |  |
| `flipY` | bool |  |
| `sortingLayer` | int |  |
| `orderInLayer` | int |  |
| `pixelArt` | bool |  |
| `visible` | bool |  |
| `sheet` | asset | Sprite sheet (.agrsheet): texture and frames |
| `frame` | int | Frame index in the sheet (-1 = none) ; plage -1 .. 100000 |
| `lit` | bool | Lit by Light2D components |
| `shader` | asset | Sprite shader (.agrshader with void sprite(inout Sprite s)) |
| `normalMap` | asset | Normal map of the texture (lit sprites): the lights shade its relief |

### Terrain

Heightmap terrain painted with up to 4 texture layers; sculpt and paint it with the Terrain tools.

*Rendering*

| Champ | Type | Détails |
|---|---|---|
| `size` | float | Width and depth (meters) ; plage 1 .. 10000 |
| `resolution` | uint | Vertices per side (2 to 1025); changing it keeps the existing heights where they fit ; plage 2 .. 1025 |
| `heights` | int[] |  |
| `splat` | int[] |  |
| `layer0` | asset | Base layer texture |
| `layer1` | asset |  |
| `layer2` | asset |  |
| `layer3` | asset |  |
| `tiling` | vec4 | Texture repeats across the terrain, per layer |
| `color` | color |  |
| `roughness` | float | plage 0 .. 1 |
| `castShadows` | bool |  |
| `collision` | bool | Static collision (3D physics) and navigation mesh source |

### Water

Animated water surface: waves, sky reflection, sun highlight, transparency.

*Rendering*

| Champ | Type | Détails |
|---|---|---|
| `size` | vec2 |  |
| `resolution` | uint | Grid cells per side ; plage 2 .. 512 |
| `shallowColor` | color |  |
| `deepColor` | color |  |
| `opacity` | float | plage 0 .. 1 |
| `waveHeight` | float | plage 0 .. 10 |
| `waveLength` | float | plage 0.1 .. 200 |
| `waveSpeed` | float | plage 0 .. 20 |
| `waveDirection` | angle |  |
| `reflectivity` | float | plage 0 .. 2 |

### Groups

Groups of the entity (comma-separated): agr.group("enemies") lists their members.

*Scripting*

| Champ | Type | Détails |
|---|---|---|
| `groups` | string | Comma-separated names, e.g. enemies,flying |

### Script

Lua script: onStart, onUpdate, onFixedUpdate, onCollision, onTrigger, onDestroy.

*Scripting*

| Champ | Type | Détails |
|---|---|---|
| `script` | asset | Lua file, e.g. assets://scripts/player.lua |
| `properties` | string | Values of the script's properties (JSON) |
| `enabled` | bool |  |

### UIButton

Clickable / touchable element: calls onClick() on its script and a function of the target's script.

*UI*

| Champ | Type | Détails |
|---|---|---|
| `interactable` | bool |  |
| `normalColor` | color |  |
| `hoverColor` | color |  |
| `pressedColor` | color |  |
| `disabledColor` | color |  |
| `target` | entity | Entity whose script receives the message |
| `message` | string | Function of the target's script, called as target:message(button) |

### UICanvas

Root of a game UI drawn over the scene; adapts to every screen size.

*UI*

| Champ | Type | Détails |
|---|---|---|
| `scaleMode` | enum | valeurs : `ConstantPixelSize`, `ScaleWithScreen` |
| `referenceResolution` | vec2 | Design resolution (ScaleWithScreen) |
| `match` | float | 0 = follow the width, 1 = follow the height ; plage 0 .. 1 |
| `pixelScale` | float | Pixels per unit (ConstantPixelSize) ; plage 0.1 .. 10 |
| `sortOrder` | int | Higher canvases are drawn on top |
| `respectSafeArea` | bool | Keep the UI away from notches and system bars (phones) |
| `keyboardNavigation` | bool | Arrows / D-pad move between controls, Enter / A activates |
| `theme` | asset | Look of the controls (.agrtheme) |

### UIColorPicker

Color picker: saturation / brightness square, hue bar, alpha bar (onValueChanged).

*UI*

| Champ | Type | Détails |
|---|---|---|
| `color` | color |  |
| `alpha` | bool |  |
| `interactable` | bool |  |
| `target` | entity |  |
| `message` | string |  |
| `hsv` | vec3 |  (non enregistré) |
| `hsvOf` | vec4 |  (non enregistré) |

### UIDialog

Modal dialog box (title, text, buttons): blocks the rest of the UI while open; a button closes it (onValueChanged(index)).

*UI*

| Champ | Type | Détails |
|---|---|---|
| `open` | bool |  |
| `title` | string |  |
| `text` | string |  |
| `buttons` | string | Buttons separated by ';' |
| `size` | vec2 |  |
| `result` | int |  (non enregistré) (lecture seule) |
| `font` | asset |  |
| `fontSize` | float | plage 1 .. 300 |
| `panelColor` | color |  |
| `dimColor` | color |  |
| `buttonColor` | color |  |
| `textColor` | color |  |
| `target` | entity |  |
| `message` | string |  |

### UIDropdown

Drop-down list: calls onValueChanged(index) on its script and a function of the target's script.

*UI*

| Champ | Type | Détails |
|---|---|---|
| `options` | string | Options separated by ';' |
| `selected` | int |  |
| `interactable` | bool |  |
| `font` | asset |  |
| `fontSize` | float | plage 1 .. 300 |
| `boxColor` | color |  |
| `listColor` | color |  |
| `highlightColor` | color |  |
| `textColor` | color |  |
| `target` | entity |  |
| `message` | string | Called as target:message(dropdown, index) |

### UIImage

Colored or textured rectangle, with optional 9-slice borders.

*UI*

| Champ | Type | Détails |
|---|---|---|
| `texture` | asset | Empty = plain color |
| `color` | color |  |
| `border` | vec4 | 9-slice borders in texture pixels: left, top, right, bottom |
| `borderScale` | float | plage 0.01 .. 20 |
| `preserveAspect` | bool |  |
| `pixelArt` | bool | Sharp pixels (nearest filtering) |
| `blocksPointer` | bool | Off: clicks and touches go through the image |
| `cornerRadius` | float | Rounded corners (no texture), canvas units ; plage 0 .. 1000 |
| `cornerRadii` | vec4 | Per corner: top-left, top-right, bottom-right, bottom-left (negative = cornerRadius) ; plage -1 .. 1000 |
| `outlineWidth` | float | Outline inside the box (no texture), canvas units ; plage 0 .. 200 |
| `outlineColor` | color |  |
| `shadowSize` | float | Soft drop shadow around the box (no texture), canvas units ; plage 0 .. 200 |
| `shadowColor` | color |  |
| `shadowOffset` | vec2 |  |

### UIInputField

Text field (with a UIText): onTextChanged(text), onSubmit(text) on Enter; on-screen keyboard on phones.

*UI*

| Champ | Type | Détails |
|---|---|---|
| `text` | string |  |
| `placeholder` | string |  |
| `maxLength` | uint |  |
| `numeric` | bool |  |
| `password` | bool |  |
| `multiline` | bool | Enter starts a new line |
| `interactable` | bool |  |
| `textColor` | color |  |
| `placeholderColor` | color |  |
| `target` | entity |  |
| `message` | string | Called as target:message(field, text) on Enter |

### UIItemList

List or tree of items: click selects (onValueChanged), double click activates (onSubmit), wheel / drag scrolls.

*UI*

| Champ | Type | Détails |
|---|---|---|
| `items` | string | Items separated by ';' (tree: leading '>' per level) |
| `selected` | int |  |
| `reorderable` | bool | Drag an item onto another row to move it |
| `multiSelect` | bool | Ctrl+click: add / remove an item, Shift+click: a range |
| `selection` | string | Selected indices (multiSelect), comma-separated |
| `tree` | bool | Folding rows: '>' at the start of an item = one level deeper |
| `collapsed` | string | Folded items (indices, comma-separated) |
| `itemHeight` | float | plage 4 .. 500 |
| `scroll` | float |  (non enregistré) |
| `interactable` | bool |  |
| `font` | asset |  |
| `fontSize` | float | plage 1 .. 300 |
| `backgroundColor` | color |  |
| `selectedColor` | color |  |
| `textColor` | color |  |
| `target` | entity |  |
| `message` | string |  |

### UILayout

Arranges the child elements in a column, a row or a grid.

*UI*

| Champ | Type | Détails |
|---|---|---|
| `type` | enum | valeurs : `Vertical`, `Horizontal`, `Grid` |
| `spacing` | float | plage 0 .. 1000 |
| `padding` | vec4 | left, top, right, bottom |
| `childAlignment` | enum | valeurs : `Start`, `Center`, `End` |
| `expandChildren` | bool | Children fill the width (column) or height (row) |
| `cellSize` | vec2 | Grid cell size |

### UIProgressBar

Progress, health or loading bar.

*UI*

| Champ | Type | Détails |
|---|---|---|
| `value` | float |  |
| `minValue` | float |  |
| `maxValue` | float |  |
| `direction` | enum | valeurs : `LeftToRight`, `RightToLeft`, `BottomToTop`, `TopToBottom` |
| `backgroundColor` | color |  |
| `fillColor` | color |  |
| `inset` | float | plage 0 .. 100 |

### UIRect

Position and size of a UI element relative to its parent (anchors, pivot).

*UI*

| Champ | Type | Détails |
|---|---|---|
| `anchorMin` | vec2 | 0,0 = top-left of the parent; 1,1 = bottom-right ; plage 0 .. 1 |
| `anchorMax` | vec2 | plage 0 .. 1 |
| `pivot` | vec2 | plage 0 .. 1 |
| `position` | vec2 |  |
| `size` | vec2 | Size (or extra size when the anchors are apart) |
| `scale` | vec2 | Around the pivot, children included |
| `rotation` | angle | Around the pivot |
| `screenRect` | vec4 |  (non enregistré) (lecture seule) |

### UIScrollView

Scrolling area: children are the content, clipped; wheel or drag to scroll.

*UI*

| Champ | Type | Détails |
|---|---|---|
| `vertical` | bool |  |
| `horizontal` | bool |  |
| `scroll` | vec2 |  |
| `wheelSpeed` | float | plage 1 .. 1000 |
| `showScrollbar` | bool |  |
| `scrollbarColor` | color |  |

### UISlider

Horizontal slider: calls onValueChanged(value) on its script and a function of the target's script.

*UI*

| Champ | Type | Détails |
|---|---|---|
| `value` | float |  |
| `minValue` | float |  |
| `maxValue` | float |  |
| `wholeNumbers` | bool |  |
| `interactable` | bool |  |
| `vertical` | bool | Bottom = min, top = max |
| `trackColor` | color |  |
| `fillColor` | color |  |
| `handleColor` | color |  |
| `target` | entity |  |
| `message` | string | Called as target:message(slider, value) |

### UITabs

Tabs over pages: the i-th child is the page of the i-th tab.

*UI*

| Champ | Type | Détails |
|---|---|---|
| `tabs` | string | Tab names separated by ';' |
| `current` | int |  |
| `headerHeight` | float | plage 1 .. 500 |
| `font` | asset |  |
| `fontSize` | float | plage 1 .. 300 |
| `headerColor` | color |  |
| `activeColor` | color |  |
| `textColor` | color |  |
| `target` | entity |  |
| `message` | string | Called as target:message(tabs, index) |

### UIText

Text with a TrueType font, aligned in its rectangle.

*UI*

| Champ | Type | Détails |
|---|---|---|
| `text` | string |  |
| `localizationKey` | string | Clé de traduction (Affichage > Localisation) : remplace le texte selon la langue |
| `font` | asset | TrueType font (.ttf) |
| `fontSize` | float | Line height (canvas units) ; plage 1 .. 500 |
| `color` | color |  |
| `hAlign` | enum | valeurs : `Left`, `Center`, `Right` |
| `vAlign` | enum | valeurs : `Top`, `Middle`, `Bottom` |
| `wrap` | bool | Break lines at the rectangle's width |
| `lineSpacing` | float | plage 0.5 .. 3 |
| `richText` | bool | [color=#ff8800]...[/color], [b]...[/b], [u]...[/u] |

### UIToggle

Check box / radio button: onValueChanged(1 or 0); toggles of the same group are exclusive.

*UI*

| Champ | Type | Détails |
|---|---|---|
| `isOn` | bool |  |
| `interactable` | bool |  |
| `group` | string | Same group = radio buttons (one on at a time) |
| `boxColor` | color |  |
| `checkColor` | color |  |
| `target` | entity |  |
| `message` | string | Called as target:message(toggle, value) |

### UITooltip

Help text shown near the pointer when it rests on the element.

*UI*

| Champ | Type | Détails |
|---|---|---|
| `text` | string |  |
| `delay` | float | plage 0 .. 10 |

## API Lua

Modules de `agr` (et leurs fonctions), puis les méthodes d'entité (`e:nom(...)`). Détails et exemples : [SCRIPTING.md](SCRIPTING.md).

231 fonctions.

- `agr.after`
- **`agr.audio`** : `addBusEffect`, `busEffectCount`, `busMuted`, `busParent`, `busVolume`, `clearBusEffects`, `createBus`, `generator`, `isPlaying`, `isRecording`, `play`, `push`, `queued`, `recordingLevel`, `setBusEffect`, `setBusMuted`, `setBusParent`, `setBusVolume`, `setPitch`, `setVolume`, `spectrum`, `startRecording`, `stop`, `stopRecording`
- **`agr.base64`** : `decode`, `encode`
- **`agr.camera`** : `ray`, `screenToWorld`, `worldToScreen`
- `agr.clone`
- **`agr.device`** : `canVibrate`, `isMobile`, `platform`, `safeArea`, `vibrate`
- `agr.error`
- **`agr.events`** : `emit`, `off`, `on`
- `agr.every`
- **`agr.file`** : `exists`, `read`, `write`
- `agr.find`
- **`agr.fx`** : `burst`, `fade`, `flash`, `hitStop`, `popup`, `shake`
- `agr.globals`
- `agr.group`
- **`agr.http`** : `cancel`, `get`, `pending`, `post`, `progress`, `request`
- **`agr.input`** : `accelerometer`, `axis`, `down`, `drag`, `gamepadAxis`, `gamepadCount`, `gamepadDown`, `gamepadPressed`, `gestures`, `gyroscope`, `keyDown`, `keyPressed`, `keyReleased`, `mouseDown`, `mousePosition`, `mousePressed`, `pinch`, `pressed`, `released`, `tapped`, `touch`, `touchCount`, `value`
- `agr.instantiate`
- `agr.isTweening`
- **`agr.json`** : `decode`, `encode`
- `agr.loadPack`
- `agr.loadScene`
- **`agr.locale`** : `get`, `list`, `set`
- `agr.log`
- **`agr.nav`** : `path`, `rebuild`
- **`agr.net`** : `advertise`, `broadcast`, `despawn`, `discover`, `games`, `host`, `id`, `join`, `joinError`, `leave`, `peers`, `ping`, `poll`, `port`, `role`, `send`, `sender`, `setOwner`, `setPassword`, `setVisible`, `spawn`, `stats`, `stopDiscovery`, `time`
- **`agr.physics2d`** : `castBox`, `castCircle`, `gravity`, `layer`, `mask`, `overlapBox`, `overlapCircle`, `raycast`, `setGravity`
- **`agr.physics3d`** : `castSphere`, `layer`, `mask`, `overlapBox`, `overlapSphere`, `raycast`, `setGravity`
- **`agr.profile`** : `begin`, `finish`
- **`agr.save`** : `clear`, `flush`, `get`, `has`, `remove`, `set`
- **`agr.screen`** : `size`
- `agr.sha256`
- `agr.spawn`
- `agr.start`
- `agr.stopTween`
- `agr.stopTweens`
- **`agr.time`** : `delta`, `fixedDelta`, `frame`, `now`, `scale`, `setScale`, `total`
- `agr.tr`
- `agr.tween`
- **`agr.ui`** : `focus`, `isPressed`, `isTyping`, `pointerOverUI`
- `agr.wait`
- `agr.waitFrames`
- `agr.waitSignal`
- `agr.waitUntil`
- `agr.warn`
- **`agr.ws`** : `close`, `connect`, `send`, `state`

**Méthodes d'entité** (61) : `add`, `addToGroup`, `applyForce`, `applyImpulse`, `children`, `connect`, `currentState`, `destroy`, `disconnect`, `emit`, `find`, `forward`, `get`, `getParameter`, `groundNormal`, `groups`, `has`, `inGroup`, `isActive`, `isGrounded`, `isOwner`, `isSoundPlaying`, `isValid`, `jointValue`, `key`, `layerWeight`, `name`, `navigateTo`, `navigationPath`, `navigationStatus`, `owner`, `parent`, `playSound`, `playState`, `position`, `remove`, `removeFromGroup`, `rotate`, `rotation`, `rpc`, `rpcTo`, `scale`, `script`, `set`, `setActive`, `setAngularVelocity`, `setLayerWeight`, `setName`, `setParameter`, `setPosition`, `setRotation`, `setScale`, `setText`, `setTrigger`, `setVelocity`, `stopNavigation`, `stopSound`, `text`, `translate`, `velocity`, `worldPosition`
