-- Planning des vagues (paquet officiel wave-spawner), sans dépendance à la scène : testable.
--   Waves.schedule({ { prefab = "assets://enemy.agrprefab", count = 5, interval = 0.5 }, ... }, pause)
--   -> liste triée de { time, wave, prefab, index } (la vague n commence `pause` secondes après la fin
--      de l'apparition de la vague n - 1).
local Waves = {}

function Waves.schedule(waves, pause)
    local out, t = {}, 0
    for w, wave in ipairs(waves) do
        local interval = wave.interval or 0.5
        for i = 1, wave.count or 1 do
            out[#out + 1] = { time = t + (i - 1) * interval, wave = w, prefab = wave.prefab, index = i }
        end
        t = t + ((wave.count or 1) - 1) * interval + (wave.pause or pause or 3)
    end
    return out
end

-- Position d'apparition n sur un cercle de rayon r autour de (x, z), régulièrement réparties.
function Waves.ring(n, count, r, x, z)
    local a = (n - 1) / math.max(count, 1) * math.pi * 2
    return x + math.cos(a) * r, z + math.sin(a) * r
end

return Waves
