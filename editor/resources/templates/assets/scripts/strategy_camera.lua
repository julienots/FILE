-- Caméra de stratégie (AGR ENGINE), à mettre sur la caméra d'un jeu de village, de carte ou de
-- tactique vu de dessus :
--   * glisser un doigt (ou la souris) déplace la vue, le point touché reste sous le doigt ;
--   * pincer à deux doigts (ou la molette) zoome vers le centre du pincement ; deux doigts qui
--     glissent ensemble déplacent aussi la vue ;
--   * au lâcher, la vue continue un peu sur sa lancée (inertie) ;
--   * la vue reste dans les limites (minX..maxX, minY..maxY : plan du sol x, z en 3D ; x, y en 2D).
-- Caméra 3D en perspective : elle garde son orientation et se rapproche / s'éloigne de son point
-- de visée. Caméra orthographique (2D) : le zoom change orthographicSize.
-- Les gestes qui commencent sur l'interface (boutons, cartes) sont ignorés.
-- Depuis un autre script : cam = agr.find("Main Camera"):script(); cam:focusOn(x, y); cam.zoom
local StrategyCamera = {
    properties = {
        minX = -20, maxX = 20, minY = -20, maxY = 20,
        minZoom = 0.5, maxZoom = 2.5,
        inertia = 0.9,     -- part de la vitesse gardée par 1/60 s après le lâcher (0 = aucune)
        enabled = true,
    },
}

local function isOrtho(e)
    local p = e:get("Camera", "projection")
    return p == 1 or p == "Orthographic" or p == "orthographic"
end

function StrategyCamera:onStart()
    self.ortho = isOrtho(self.entity)
    local x, y, z = self.entity:position()
    if self.ortho then
        self.fx, self.fy = x, y
        self.depth = z
        self.baseSize = self.entity:get("Camera", "orthographicSize")
    else
        -- Point de visée : là où l'axe de la caméra coupe le sol (y = 0), sinon 10 m devant elle.
        local dx, dy, dz = self.entity:forward()
        local t = dy < -1e-3 and -y / dy or 10
        self.fx, self.fy = x + dx * t, z + dz * t
        self.offset = { x - self.fx, y, z - self.fy }
    end
    self.zoom = 1
    self.vx, self.vy = 0, 0
    self:apply()
end

-- Point du monde sous un point de l'écran, dans le plan de la caméra (x, z en 3D ; x, y en 2D).
function StrategyCamera:ground(sx, sy)
    local x, y, z = agr.camera.screenToWorld(sx, sy, 0)
    if not x then return nil end
    if self.ortho then return x, y end
    return x, z
end

function StrategyCamera:clamp()
    self.fx = math.max(self.minX, math.min(self.maxX, self.fx))
    self.fy = math.max(self.minY, math.min(self.maxY, self.fy))
    self.zoom = math.max(self.minZoom, math.min(self.maxZoom, self.zoom))
end

function StrategyCamera:apply()
    self:clamp()
    if self.ortho then
        self.entity:setPosition(self.fx, self.fy, self.depth)
        self.entity:set("Camera", "orthographicSize", self.baseSize / self.zoom)
    else
        local o, k = self.offset, 1 / self.zoom
        self.entity:setPosition(self.fx + o[1] * k, o[2] * k, self.fy + o[3] * k)
    end
end

function StrategyCamera:focusOn(x, y)
    self.fx, self.fy = x, y
    self.vx, self.vy = 0, 0
    self:apply()
end

-- Déplace la vue pour que le point du monde sous (sx - dx, sy - dy) passe sous (sx, sy).
function StrategyCamera:pan(sx, sy, dx, dy, dt)
    local ax, ay = self:ground(sx - dx, sy - dy)
    local bx, by = self:ground(sx, sy)
    if not (ax and bx) then return end
    self.fx, self.fy = self.fx + (ax - bx), self.fy + (ay - by)
    if dt > 0 then self.vx, self.vy = (ax - bx) / dt, (ay - by) / dt end
end

function StrategyCamera:onUpdate(dt)
    if not self.enabled then return end
    local moved = false
    for _, g in ipairs(agr.input.gestures()) do
        if g.type == "dragBegin" then
            self.blocked = agr.ui.pointerOverUI()
            self.holding = true
            self.vx, self.vy = 0, 0
        end
        -- dragBegin porte le mouvement depuis l'appui (seuil du toucher) : à appliquer aussi.
        if (g.type == "dragBegin" or g.type == "drag") and not self.blocked then
            self:pan(g.x, g.y, g.dx, g.dy, dt)
            moved = true
        elseif g.type == "dragEnd" then
            self.blocked, self.holding = false, false
        elseif g.type == "pinch" and not agr.ui.pointerOverUI() then
            -- Zoom vers le centre du pincement : ce point du sol reste sous les doigts.
            local px, py = self:ground(g.x, g.y)
            local before = self.zoom
            self.zoom = math.max(self.minZoom, math.min(self.maxZoom, self.zoom * g.scale))
            if px and self.zoom ~= before then
                local k = before / self.zoom
                self.fx, self.fy = px + (self.fx - px) * k, py + (self.fy - py) * k
            end
            if g.dx ~= 0 or g.dy ~= 0 then self:pan(g.x, g.y, g.dx, g.dy, dt) end
            moved = true
            self.vx, self.vy = 0, 0
        end
    end
    -- Doigt posé mais immobile : pas d'élan au lâcher.
    if self.holding and not moved then self.vx, self.vy = 0, 0 end
    if not self.holding and (self.vx ~= 0 or self.vy ~= 0) then
        self.fx, self.fy = self.fx + self.vx * dt, self.fy + self.vy * dt
        local keep = self.inertia ^ (dt * 60)
        self.vx, self.vy = self.vx * keep, self.vy * keep
        if math.abs(self.vx) + math.abs(self.vy) < 0.01 then self.vx, self.vy = 0, 0 end
    end
    self:apply()
end

return StrategyCamera
