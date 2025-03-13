# Builtin Entities
Luanti registers two entities by default: Falling nodes and dropped items.
This document describes how they behave and what you can do with them.

## Falling node (`__builtin:falling_node`)

This entity is created by `core.check_for_falling` in place of a node
with the special group `falling_node=1`. Falling nodes can also be created
artificially with `core.spawn_falling_node`.

Needs manual initialization when spawned using `/spawnentity`.

Default behavior:

* Falls down in a straight line (gravity = `movement_gravity` setting)
* Collides with `walkable` node
* Collides with all physical objects except players
* If the node group `float=1` is set, it also collides with liquid nodes
  (nodes with `liquidtype ~= "none"`)
* When it hits a solid (=`walkable`) node, it will try to place itself as a
  node, replacing the node above.
    * If the falling node cannot replace the destination node, it is dropped
      as an item.
    * If the destination node is a leveled node (`paramtype2="leveled"`) of the
      same node name, the levels of both are summed.

### Entity fields

* `set_node(self, node[, meta])`
    * Function to initialize the falling node
    * `node` and `meta` are explained below.
    * The `meta` argument is optional.
* `node`: Node table of the node (`name`, `param1`, `param2`) that this
  entity represents. Read-only.
* `meta`: Node metadata of the falling node. Will be used when the falling
  nodes tries to place itself as a node. Read-only.

### Rendering / supported nodes

Falling nodes have visuals to look as close as possible to the original node.
This works for most drawtypes, but there are limitations.

Supported drawtypes:

* `normal`
* `signlike`
* `torchlike`
* `nodebox`
* `raillike`
* `glasslike`
* `glasslike_framed`
* `glasslike_framed_optional`
* `allfaces`
* `allfaces_optional`
* `firelike`
* `mesh`
* `fencelike`
* `liquid`
* `airlike` (not pointable)

Other drawtypes still kinda work, but they might look weird.
If the node uses a world-aligned texture with a `scale` greater
than 1, the falling node will display the top-most, left-most
portion of that texture.

Supported `paramtype2` values:

* `wallmounted`
* `facedir`
* `4dir`
* `colorwallmounted`
* `colorfacedir`
* `color4dir`
* `color`

## Dropped item stack (`__builtin:item`)

This is an item stack in a collectable form.

Common cases that spawn a dropped item:

* Item dropped by player
* The root node of a node with the group `attached_node=1` is removed
* `core.add_item` is called

Needs manual initialization when spawned using `/spawnentity`.

### Behavior

* Players can collect it by punching
* Lifespan is defined by the setting `item_entity_ttl`
* Slides on `slippery` nodes
* Subject to gravity (uses `movement_gravity` setting)
* Collides with `walkable` nodes
* Does not collide physical objects
* When it's inside a solid (`walkable=true`) node, it tries to escape to a
  neighboring non-solid (`walkable=false`) node

### Entity fields

* `set_item(self, item)`:
    * Function to initialize the dropped item
    * `item` (type `ItemStack`) specifies the item to represent
* `age`: Age in seconds. Behavior according to the setting `item_entity_ttl`
* `itemstring`: Itemstring of the item that this item entity represents.
  Read-only.

Other fields are for internal use only.
