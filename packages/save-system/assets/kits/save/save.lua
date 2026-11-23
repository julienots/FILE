-- Sauvegardes (paquet officiel save-system).
--   local Save = require("assets://kits/save/save.lua")
--   local saves = Save.new({ version = 2, migrations = { [2] = function(d) d.gold = d.coins or 0; d.coins = nil end } })
--   saves:save("slot1", { level = 3, coins = 10 })
--   local data = saves:load("slot1")     -- migrée vers la version 2 si elle était plus ancienne
local Save = {}
Save.__index = Save

function Save.new(options)
    options = options or {}
    return setmetatable({
        version = options.version or 1,
        migrations = options.migrations or {},
        folder = options.folder or "user://saves/",
    }, Save)
end

function Save:path(slot) return self.folder .. slot .. ".json" end

-- Table -> texte (avec l'en-tête de version et l'heure).
function Save:serialize(data)
    return agr.json.encode({ version = self.version, savedAt = agr.time.now(), data = data }, true)
end

-- Texte -> table migrée vers la version courante ; nil, erreur si illisible ou trop récente.
function Save:deserialize(text)
    local doc, err = agr.json.decode(text)
    if type(doc) ~= "table" or type(doc.data) ~= "table" then return nil, err or "sauvegarde illisible" end
    local v = doc.version or 1
    if v > self.version then return nil, "sauvegarde d'une version plus récente du jeu (" .. v .. ")" end
    for target = v + 1, self.version do
        local migrate = self.migrations[target]
        if migrate then
            local replaced = migrate(doc.data)
            if type(replaced) == "table" then doc.data = replaced end
        end
    end
    return doc.data, nil, doc.savedAt
end

function Save:save(slot, data)
    return agr.file.write(self:path(slot), self:serialize(data))
end

function Save:load(slot)
    if not agr.file.exists(self:path(slot)) then return nil, "aucune sauvegarde '" .. slot .. "'" end
    return self:deserialize(agr.file.read(self:path(slot)))
end

function Save:exists(slot) return agr.file.exists(self:path(slot)) end

-- Efface un emplacement (écrit une sauvegarde vide, reconnue comme absente).
function Save:clear(slot) return agr.file.write(self:path(slot), "") end

return Save
