-- Minetest: builtin/game/default_physics.lua

-- DEFAULT_PHYSICS variable of a player
core.DEFAULT_PHYSICS = {
    speed = 1,
    speed_walk = 1,
    speed_climb = 1,
    speed_crouch = 1,
    speed_fast = 1,

    jump = 1,
    gravity = 1,

    liquid_fluidity = 1,
    liquid_fluidity_smooth = 1,
    liquid_sink = 1,

    acceleration_default = 1,
    acceleration_air = 1,
    acceleration_fast = 1,

    sneak = true,
    sneak_glitch = false,
    new_move = true,
}

core.register_on_joinplayer(function(player)
    player:set_physics_override(core.DEFAULT_PHYSICS)
end)
