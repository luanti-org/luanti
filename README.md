Luanti is a free open-source voxel game engine that provides users with a platform for easy modding and game creation. It is a fork of the Minetest engine, building upon its foundation to offer an enhanced experience for both players and developers. Luanti retains the core strengths of Minetest, such as its lightweight nature and extensive modding capabilities, while also aiming to address some of its limitations.

The primary goals of the Luanti project revolve around several key areas of improvement. Firstly, Luanti seeks to significantly enhance rendering and graphics capabilities, aiming for a more visually appealing and immersive experience. Secondly, there is a strong focus on internal code refactoring to improve maintainability, performance, and stability. This includes modernizing the codebase and streamlining existing systems.

Furthermore, Luanti aims to deliver substantial UI/UX improvements, making the engine more intuitive and user-friendly. Finally, the project is dedicated to enhancing the object and entity systems, providing more flexibility and power for creating complex and interactive game worlds. Through these efforts, Luanti strives to be a leading choice for voxel game development, offering a robust and versatile platform for creative expression.

## Architecture

Luanti employs a client-server architecture, which allows for both single-player and multiplayer experiences. In a multiplayer setup, the server hosts the game world, manages game logic, and synchronizes player actions. Clients connect to the server to interact with the game world and other players. Even in single-player mode, Luanti runs an internal server that the client connects to, ensuring a consistent experience across both modes.

The engine's core is built in C++, providing high performance and stability for demanding tasks such as world generation, physics simulation, and network communication. This C++ core forms the backbone of Luanti, handling the low-level operations and ensuring efficient resource management.

Complementing the C++ core is a powerful Lua scripting API. This API allows developers to extend and customize the game in numerous ways, from creating new game mechanics and items to designing complex quests and interactive NPCs. The Lua API provides a flexible and accessible way to modify the game without needing to delve into the C++ codebase, fostering a vibrant modding community.

For graphics rendering, Luanti integrates the Irrlicht Engine, a versatile open-source 3D graphics engine. Irrlicht handles the rendering of the game world, including terrain, objects, and visual effects. This integration allows Luanti to leverage Irrlicht's features for creating visually rich and engaging environments. Luanti aims to modernize and improve upon the existing Irrlicht integration to unlock more advanced graphical capabilities.

## Codebase Structure

The Luanti codebase is organized into several key directories:

- **`src`**: This directory contains the core C++ engine code, including systems for physics, networking, world management, and more. It forms the heart of Luanti's functionality.
- **`builtin`**: Here you'll find Lua code that implements built-in game features, such as basic player interactions, inventory management, and the main menu interface.
- **`games`**: This directory is intended for game-specific code and assets. Different games built on Luanti can have their own subdirectories here, allowing for modular game development.
- **`mods`**: Mod code and assets reside in this directory. Luanti's modding API allows users to create and install mods that extend or modify the game's behavior and content.
- **`doc`**: This directory houses documentation files, including guides, API references, and tutorials to help developers and users understand and work with Luanti.
- **`lib`**: Third-party libraries used by Luanti are stored here. These libraries provide additional functionality, such as audio processing or specific data format support.
- **`irr`**: This directory contains the source code for the Irrlicht Engine, which Luanti uses for 3D graphics rendering.
- **`android`**: Android-specific build files and code are located here, enabling the compilation and deployment of Luanti on Android devices.
- **`po`**: Translation files for internationalization and localization are stored in this directory. These files allow Luanti to be translated into multiple languages.
- **`textures`**: Default textures used by the engine and built-in game features can be found here.
- **`client`**: This directory contains client-specific assets, such as shaders used for rendering visual effects, and other resources unique to the client application.
- **`fonts`**: Font files used for displaying text in the game and UI are stored in this directory.
- **`misc`**: A collection of miscellaneous utility scripts and files that support development, building, or other aspects of the Luanti project.
- **`cmake`**: This directory contains CMake build scripts, which are used to configure and build the Luanti project on various platforms.

## Building and Installation

Building Luanti from source is managed using CMake, a cross-platform build system generator. The general process involves generating build files specific to your platform and compiler (e.g., Makefiles for Linux, Visual Studio projects for Windows), and then compiling the source code.

Key dependencies required to build Luanti include:
- A C++ compiler (e.g., GCC, Clang, MSVC)
- CMake (version 3.10 or higher is recommended)
- SDL2 (Simple DirectMedia Layer) for windowing and input
- SQLite3 for database storage
- LuaJIT for the Lua scripting backend
- Other libraries like zlib, libcurl, and OpenAL are also required.

After successfully compiling, you can typically run Luanti from the build directory or install it to a system-wide location. For detailed, platform-specific compilation instructions (Linux, macOS, Windows), please refer to the guides located in the `doc/compiling/` directory. These guides provide step-by-step instructions for installing dependencies and building Luanti.

## Network Protocol

Luanti utilizes a custom network protocol built on top of UDP (User Datagram Protocol) for communication between the client and server. This protocol is designed to be relatively lightweight and efficient for real-time game interactions. Integers in the protocol are generally transmitted in big-endian format.

### Initialization Handshake

The connection process begins with an initialization handshake:

1.  **Client to Server (Initial Packet)**: The client sends a packet to the server. This packet includes a protocol ID (`0x4f457403`), an initial sender peer ID (`PEER_ID_INEXISTENT = 0`), a channel number (usually 0), and a reliable packet header with a sequence number (`SEQNUM_INITIAL = 65500`) and an original packet type (`TYPE_ORIGINAL = 1`). This packet essentially signals the client's intent to connect.

2.  **Server to Client (Peer ID Assignment)**: The server responds with a similar packet structure. This response contains a control message (`TYPE_CONTROL = 0`) with a control type `CONTROLTYPE_SET_PEER_ID = 1`. The payload of this message includes the `peer_id_new` that the server assigns to the client for the duration of the session.

3.  **Disconnection**: To disconnect, a packet is sent with the assigned `sender_peer_id` and a control message of type `CONTROLTYPE_DISCO = 3`.

### Message Types (Opcodes)

Communication is managed through various message types, or opcodes, each serving a specific purpose. These are defined in `src/network/networkprotocol.h`, `src/network/clientopcodes.h`, and `src/network/serveropcodes.h`.

#### Server-to-Client (ToClient) Messages:

-   **`TOCLIENT_HELLO (0x02)`**: Sent after the client's initial connection request. Contains server information like serialization version, protocol version, and supported authentication methods.
-   **`TOCLIENT_AUTH_ACCEPT (0x03)`**: Signals that the server has accepted the client's authentication. Includes map seed, recommended send interval, and sudo mode authentication methods.
-   **`TOCLIENT_BLOCKDATA (0x20)`**: Transmits map block data to the client. Includes the block's position and the serialized map block data.
-   **`TOCLIENT_ADDNODE (0x21)`**: Instructs the client to add or update a single node (block) at a specific position. Includes the position, serialized node data, and a flag to keep metadata.
-   **`TOCLIENT_REMOVENODE (0x22)`**: Instructs the client to remove a node at a specific position.
-   **`TOCLIENT_INVENTORY (0x27)`**: Sends the player's inventory data to the client.
-   **`TOCLIENT_TIME_OF_DAY (0x29)`**: Updates the client with the current game time and time speed.
-   **`TOCLIENT_CHAT_MESSAGE (0x2F)`**: Transmits chat messages from the server or other players to the client.
-   **`TOCLIENT_ACTIVE_OBJECT_REMOVE_ADD (0x31)`**: Informs the client about active objects (entities) being removed or added to the game world.
-   **`TOCLIENT_ACTIVE_OBJECT_MESSAGES (0x32)`**: Sends messages or updates related to specific active objects.
-   **`TOCLIENT_HP (0x33)`**: Updates the player's health points.
-   **`TOCLIENT_MOVE_PLAYER (0x34)`**: Instructs the client to move its player character to a new position with a specific pitch and yaw.
-   **`TOCLIENT_NODEDEF (0x3a)`**: Sends node definitions (how blocks look and behave) to the client.
-   **`TOCLIENT_ITEMDEF (0x3d)`**: Sends item definitions to the client.
-   **`TOCLIENT_PLAY_SOUND (0x3f)`**: Tells the client to play a sound effect.
-   **`TOCLIENT_PRIVILEGES (0x41)`**: Sends the player's privileges (permissions) to the client.
-   **`TOCLIENT_INVENTORY_FORMSPEC (0x42)`**: Sends the layout definition (formspec) for an inventory screen.
-   **`TOCLIENT_SHOW_FORMSPEC (0x44)`**: Tells the client to display a specific UI form (formspec).
-   **`TOCLIENT_SET_SKY (0x4f)`**: Configures the client's sky rendering parameters.
-   **`TOCLIENT_MODCHANNEL_MSG (0x57)`**: Transmits messages over custom mod channels.

#### Client-to-Server (ToServer) Messages:

-   **`TOSERVER_INIT (0x02)`**: The initial packet sent by the client to initiate connection, containing client version information and player name.
-   **`TOSERVER_INIT2 (0x11)`**: An acknowledgment sent by the client after receiving `TOCLIENT_AUTH_ACCEPT`.
-   **`TOSERVER_PLAYERPOS (0x23)`**: Periodically sends the player's current position, orientation, speed, and pressed keys to the server.
-   **`TOSERVER_GOTBLOCKS (0x24)`**: Sent by the client to acknowledge receipt of map block data.
-   **`TOSERVER_INVENTORY_ACTION (0x31)`**: Informs the server about actions the player takes within their inventory (e.g., moving items).
-   **`TOSERVER_CHAT_MESSAGE (0x32)`**: Sends a chat message from the client to the server.
-   **`TOSERVER_INTERACT (0x39)`**: Sent when the player interacts with the game world (e.g., digging, placing blocks, using items). Includes the type of action, item involved, and the target.
-   **`TOSERVER_NODEMETA_FIELDS (0x3b)`**: Sent by the client to update metadata associated with a node (e.g., contents of a chest).
-   **`TOSERVER_INVENTORY_FIELDS (0x3c)`**: Sent when the player interacts with fields in a formspec (e.g., typing text into an input field).
-   **`TOSERVER_REQUEST_MEDIA (0x40)`**: Sent by the client to request media files (textures, sounds) from the server.
-   **`TOSERVER_CLIENT_READY (0x43)`**: Sent by the client to signal it has loaded initial assets and is ready to enter the game.
-   **`TOSERVER_SRP_BYTES_A (0x51)` & `TOSERVER_SRP_BYTES_M (0x52)`**: Used for Secure Remote Password (SRP) authentication.

### Data Serialization

Luanti uses a custom serialization format for transmitting data over the network. Complex data structures like map blocks, node definitions, and inventories are serialized into a byte stream before being sent and then deserialized by the receiving end.

-   **Basic Data Types**: Integers are typically sent in big-endian byte order. Floating-point numbers, strings, and boolean values have their specific serialization methods.
-   **Map Blocks**: `TOCLIENT_BLOCKDATA` messages contain serialized `MapBlock` objects. This includes node IDs, parameters (param1, param2), and metadata for all nodes within that block.
-   **Node and Item Definitions**: `TOCLIENT_NODEDEF` and `TOCLIENT_ITEMDEF` messages transmit compressed (using zstd) serialized data for `NodeDefManager` and `ItemDefManager` respectively. These managers contain all the definitions for nodes and items available in the game.
-   **Inventories**: `TOCLIENT_INVENTORY` and `TOSERVER_INVENTORY_ACTION` messages involve serialized inventory data.
-   **Formspecs**: Formspecs, which define UI layouts, are transmitted as strings.

The specific serialization and deserialization logic can be found within the C++ source code, particularly in files dealing with network communication and data structures (e.g., `src/mapblock.cpp`, `src/nodedef.cpp`, `src/itemdef.cpp`, `src/inventorymanager.cpp`, and various files in `src/network/`). The `serialize.h` and `serialize.cpp` files likely contain core serialization helper functions.

For more in-depth details on specific packet structures and serialization methods, consulting the source code, especially the files mentioned in `src/network/` and the definitions in `src/network/networkprotocol.h`, is recommended. The `doc/protocol.txt` file also provides a foundational (though potentially outdated) overview of the initial connection sequence.

## Game Engine

The Luanti game engine orchestrates the entire player experience, from rendering the world to handling player input and managing game state.

### Core Game Loop (Client-Side)

The client-side game loop is primarily managed within `src/client/game.cpp`. The `Game::run()` method forms the heart of this loop. Here's a simplified breakdown of its operations:

1.  **Time Management**: Calculates `dtime` (delta time), the time elapsed since the last frame. This is crucial for smooth, frame-rate-independent animations and physics. An `FpsControl` class helps manage and limit frame rates.
2.  **Input Processing**: `Game::processUserInput()` is called to gather and handle all player input (keyboard, mouse, joystick, touch). This includes actions like movement, interaction, opening menus, and chat.
3.  **Network Communication**: Checks the connection status (`Game::checkConnection()`) and processes incoming network packets from the server (`Game::processQueues()`).
4.  **Client Event Handling**: Processes a queue of client-side events (`Game::processClientEvents()`). These events can be triggered by server messages (e.g., player damage, formspec display) or internal client actions. Each event type has a dedicated handler function (e.g., `Game::handleClientEvent_PlayerDamage()`).
5.  **Player and Camera Updates**:
    *   `Game::updatePlayerControl()`: Translates processed input into player control actions (movement direction, speed, sneak, jump, etc.) and sends them to the server.
    *   `Game::updateCameraDirection()` and `Game::updateCamera()`: Update the camera's orientation and position based on player input and game events. This also involves camera smoothing and handling different camera modes (first-person, third-person).
6.  **Game State Simulation (Client-Side Prediction)**:
    *   The client performs some local simulation, such as predicting node placement (`Game::nodePlacement()`) or digging effects (`Game::handleDigging()`). This provides immediate feedback to the player before the server confirms the action. For example, when a player places a node, the client might predict its appearance locally while the server validates and broadcasts the change.
    *   This is a common technique in networked games to reduce perceived latency. The server remains the authority, and if its state differs from the client's prediction, the client will eventually receive a correction (reconciliation).
7.  **Sound Updates**: `Game::updateSound()` updates the sound listener's position and orientation and plays sounds triggered by game events.
8.  **World Interaction**: `Game::processPlayerInteraction()` determines what the player is pointing at (node, object, or nothing) using raycasting (`ClientEnvironment::continueRaycast()`) and handles the corresponding interactions (digging, placing, punching objects).
9.  **Scene Updates**:
    *   `Game::updateFrame()`: Updates various game elements like the sky, clouds, particles, HUD elements, and chat. It also triggers updates to the map's visual representation if needed.
    *   `ClientMap::updateDrawList()`: Updates the list of map blocks that need to be rendered.
10. **Rendering**: `Game::drawScene()` calls into the `RenderingEngine` to draw the 3D world and 2D GUI elements.
11. **Loop Continuation**: The loop continues as long as the rendering engine is running and no shutdown conditions are met.

### Server-Side Game State Management (Conceptual)

While `src/server/game.cpp` was not directly provided for this analysis, in a typical client-server architecture like Luanti's, the server is the ultimate authority on game state. It would be responsible for:

-   **World State**: Maintaining the definitive state of all map blocks, nodes, and their metadata.
-   **Player State**: Tracking each connected player's position, inventory, health, privileges, and other relevant attributes.
-   **Entity (Active Object) State**: Managing the state and behavior of all active objects in the game world.
-   **Game Logic**: Executing core game rules, processing player actions validated from client inputs, running AI for entities, and managing game events (e.g., day/night cycle, weather).
-   **Persistence**: Saving and loading the game world and player data to/from storage (e.g., using a database like SQLite3).
-   **Network Synchronization**: Broadcasting relevant game state updates to all connected clients to keep their views consistent.

### Client-Side Prediction and Server Reconciliation

Luanti appears to implement client-side prediction for actions like node digging and placement. When a player performs such an action:

1.  **Client Predicts**: The client immediately simulates the result of the action (e.g., a node appears or disappears, a crack animation starts). This gives the player instant feedback.
2.  **Client Sends Action to Server**: The client sends a message to the server (`TOSERVER_INTERACT`) informing it of the player's action.
3.  **Server Validates and Processes**: The server receives the action, validates it against its authoritative game state and rules (e.g., does the player have the item, are they allowed to build here?). If valid, the server updates its game state.
4.  **Server Broadcasts Update**: The server sends messages (e.g., `TOCLIENT_ADDNODE`, `TOCLIENT_REMOVENODE`) to all relevant clients, including the one that initiated the action, to inform them of the official state change.
5.  **Client Reconciles**: If the client's predicted state differs from the server's authoritative update, the client adjusts its local state to match the server. This is server reconciliation. For example, if a predicted node placement was invalid, the server's update would cause the client to remove the predicted node.

This pattern helps make the game feel responsive despite network latency.

### Rendering Pipeline and Irrlicht Engine

Luanti uses the Irrlicht Engine (source in `irr/`) for its 3D graphics rendering. The `RenderingEngine` class (in `src/client/renderingengine.h/cpp`) acts as a bridge and manager for Irrlicht.

1.  **Initialization**:
    *   `RenderingEngine::RenderingEngine()`: Initializes Irrlicht, creating an `IrrlichtDevice` with specified parameters (screen resolution, fullscreen, VSync, anti-aliasing). It tries to use the video driver specified in settings or falls back to other supported OpenGL drivers.
    *   `RenderingEngine::initialize()`: Sets up the `RenderingCore` which handles different 3D rendering modes (e.g., side-by-side for VR).
2.  **Scene Setup**:
    *   The `Client` class and `Game` class work together to populate the Irrlicht scene graph with nodes representing the game world (terrain, objects, sky, clouds).
    *   `Sky` and `Clouds` classes manage the visual representation of the skybox and cloud layers.
    *   `ClientMap::updateDrawList()` determines which map blocks are visible and need their meshes updated or created. Map block meshes (`MapBlockMesh`) are generated and added to the scene.
3.  **Shader Management**:
    *   Luanti uses custom shaders for various effects. The `ShaderSource` and related classes manage loading and compiling these shaders.
    *   `GameGlobalShaderUniformSetter` and `FogShaderUniformSetter` are examples of classes that set global shader parameters (uniforms) each frame, such as lighting conditions, fog parameters, and animation timers. These are applied by the `RenderingEngine`.
4.  **Drawing Loop**:
    *   `RenderingEngine::draw_scene()`: This is the main drawing method called from `Game::drawScene()`.
    *   It typically involves:
        *   `driver->beginScene()`: Clears the screen and depth buffer.
        *   The `RenderingCore` then draws the 3D scene (world, entities, sky).
        *   The HUD and other 2D elements are drawn on top.
        *   `driver->endScene()`: Presents the rendered frame to the screen.
5.  **Irrlicht Integration**:
    *   Luanti uses Irrlicht's scene manager (`ISceneManager`) to manage 3D objects, cameras, and lights.
    *   It uses Irrlicht's video driver (`IVideoDriver`) for low-level rendering operations, texture management, and material setup.
    *   Meshes for map blocks and entities are created as `IMesh` or `IAnimatedMesh` objects within Irrlicht.
    *   The GUI system (formspecs, chat, HUD) is also built using Irrlicht's GUI environment (`IGUIEnvironment`).

### Input Processing

Player input is handled by the `InputHandler` class and its derived classes (`RealInputHandler`, `RandomInputHandler`), along with the `MyEventReceiver` class (all in `src/client/inputhandler.h/cpp`).

1.  **Event Reception**:
    *   `MyEventReceiver::OnEvent()`: This is the callback method registered with Irrlicht. It receives all system events (keyboard, mouse, joystick, touch, application events).
    *   It translates these low-level events into game-specific actions or updates internal state.
    *   For key presses, it checks against a map of `keybindings` (loaded from settings via `MyEventReceiver::reloadKeybindings()`) to determine the `GameKeyType` (e.g., `KeyType::FORWARD`, `KeyType::JUMP`).
    *   Mouse movements, button presses, and wheel events are also processed.
    *   Touch events can be handled by `TouchControls` if active.
    *   Joystick events are passed to a `JoystickController`.
2.  **State Tracking**:
    *   `MyEventReceiver` maintains several bitsets to track key states:
        *   `keyIsDown`: True if a key is currently held down.
        *   `keyWasDown`: True if a key was held down in the previous frame (used for continuous actions, reset after checking).
        *   `keyWasPressed`: True if a key was just pressed down in this frame (cleared after processing in `Game::processPlayerInteraction()`).
        *   `keyWasReleased`: True if a key was just released in this frame.
3.  **Input Abstraction**:
    *   The `InputHandler` class provides an abstraction layer. The `Game` class interacts with an `InputHandler` object.
    *   `RealInputHandler` uses `MyEventReceiver` to get actual player input.
    *   `RandomInputHandler` can generate random input for testing/debugging.
4.  **Game Logic Access**:
    *   In `Game::processUserInput()`, the game queries the `InputHandler` for the state of various game actions (e.g., `input->isKeyDown(KeyType::FORWARD)`).
    *   These states are then used to update player movement, camera orientation, and trigger game interactions (digging, placing, etc.) in `Game::updatePlayerControl()` and `Game::processPlayerInteraction()`.
    *   Mouse position and wheel state are used for camera control and hotbar selection.
    *   Input focus is managed; if a menu or the chat console is active, game input is generally ignored.

This system allows for configurable keybindings and supports multiple input devices, translating raw hardware events into meaningful game actions.

## Lua Scripting API

Luanti provides an extensive Lua scripting API that allows developers to create mods and customize nearly every aspect of the game. This API is server-side, meaning mods run on the server, and the results (like new node types or game logic changes) are communicated to clients.

**Note**: The global Lua table `minetest` is an alias for `core` for backward compatibility. New code should use `core`.

### Mod Loading and Execution

-   **`init.lua`**: This is the main Lua script for a mod. When Luanti starts, it runs the `init.lua` file for each enabled mod. This script is responsible for registering all of the mod's nodes, items, entities, crafts, and callbacks.
-   **Mod Load Paths**: Mods are loaded from specific directories:
    -   `games/<gameid>/mods/`
    -   `mods/` (user's mod directory)
    -   `worlds/<worldname>/worldmods/` (world-specific mods)
    The engine searches these paths in order.
-   **`mod.conf`**: Each mod must have a `mod.conf` file. This file contains metadata about the mod:
    -   `name`: The unique name of the mod (should match the folder name).
    -   `title`: A human-readable title for display purposes.
    -   `description`: A short description of the mod.
    -   `depends`: A comma-separated list of mod names that this mod depends on. These mods will be loaded before this one.
    -   `optional_depends`: Similar to `depends`, but no error is raised if the mod is missing.
    -   `author`: The author's name or ContentDB username.
    -   `textdomain`: The text domain used for localization (defaults to `name`).

    Example `mod.conf`:
    ```
    name = my_cool_mod
    title = My Cool Mod
    description = Adds cool things to the game.
    depends = default, another_mod
    ```
-   **Mod Directory Structure**: A typical mod directory includes:
    -   `init.lua` (main script)
    -   `mod.conf` (metadata)
    -   `textures/`: For image files (.png, .jpg).
    -   `sounds/`: For sound files (.ogg).
    -   `models/`: For 3D models (.x, .b3d, .obj, .gltf, .glb).
    -   `locale/`: For translation files (.tr, .po).
    -   `settingtypes.txt`: To define mod-specific settings accessible in the main menu.
    -   `screenshot.png`: A preview image for the mod manager.

### Core Lua API (`core.*`)

The `core` namespace provides a vast array of functions to interact with the game engine.

#### Node and Item Registration:

-   `core.register_node(name, nodedef)`: Registers a new type of node (block). `nodedef` is a table defining its properties (textures, drawtype, walkability, light emission, sounds, groups, on_construct/on_destruct callbacks, etc.).
    ```lua
    core.register_node("my_mod:magic_block", {
        description = "Magic Block",
        tiles = {"my_mod_magic_block.png"},
        groups = {cracky = 3, level = 2},
        light_source = 5,
        on_construct = function(pos)
            core.log("action", "A magic block was placed at " .. core.pos_to_string(pos))
        end,
    })
    ```
-   `core.register_craftitem(name, itemdef)`: Registers a basic item that is not a node or a tool.
-   `core.register_tool(name, tooldef)`: Registers a tool with properties like durability (`uses`) and digging capabilities.
-   `core.override_item(name, redefinition)`: Modifies the definition of an existing registered item or node.
-   `core.register_alias(alias, original_name)`: Creates an alias for an existing item name.

#### Crafting:

-   `core.register_craft(recipedef)`: Registers a crafting recipe.
    -   **Shaped**: `recipe` is a table of rows, e.g., `{{"wood", "wood"}, {"wood", "wood"}}`. `output` is the item string.
    -   **Shapeless**: `type = "shapeless"`, `recipe` is a list of input items.
    -   **Cooking**: `type = "cooking"`, `recipe` is the input item, `cooktime` is the time in seconds.
    -   **Fuel**: `type = "fuel"`, `recipe` is the fuel item, `burntime` is the duration.
    ```lua
    core.register_craft({
        output = "my_mod:magic_ingot 4",
        recipe = {
            {"my_mod:magic_block", "default:diamondblock", "my_mod:magic_block"},
            {"default:diamondblock", "default:mese", "default:diamondblock"},
            {"my_mod:magic_block", "default:diamondblock", "my_mod:magic_block"},
        }
    })
    ```

#### Interacting with the World:

-   `core.set_node(pos, node)`: Sets a node at a specific position. `node` is a table like `{name="default:stone", param2=5}`.
-   `core.get_node(pos)`: Returns the node table at `pos`.
-   `core.add_entity(pos, name, [staticdata])`: Spawns a Lua-defined entity.
-   `core.add_item(pos, itemstring_or_stack)`: Spawns a dropped item entity.
-   `core.get_meta(pos)`: Returns a `NodeMetaRef` for storing/retrieving metadata associated with a node (e.g., chest inventory).
-   `core.get_node_timer(pos)`: Returns a `NodeTimerRef` for per-node timers.

#### Player Interaction:

-   `core.get_player_by_name(name)`: Returns an `ObjectRef` for the named player.
-   `core.chat_send_player(name, message)`: Sends a chat message to a specific player.
-   `core.chat_send_all(message)`: Sends a chat message to all players.
-   `core.show_formspec(playername, formname, formspec_string)`: Shows a custom UI form to a player.
-   `core.get_player_privs(name)` and `core.set_player_privs(name, privs_table)`: Manage player privileges.

#### Callbacks and Event Handling:

Luanti uses a callback system for many game events. Mods can register functions to be called when these events occur.
-   `core.register_on_joinplayer(function(player_ref, last_login_timestamp))`
-   `core.register_on_leaveplayer(function(player_ref, timed_out))`
-   `core.register_on_placenode(function(pos, newnode, placer, oldnode, itemstack, pointed_thing))`
-   `core.register_on_dignode(function(pos, oldnode, digger))`
-   `core.register_on_chat_message(function(name, message))`
-   `core.register_on_player_receive_fields(function(player, formname, fields))` (for formspec interaction)
-   `core.register_globalstep(function(dtime))` (called every server tick)
-   `core.register_abm(abm_definition)` (Active Block Modifier, for periodic actions on nodes)
-   `core.register_lbm(lbm_definition)` (Loading Block Modifier, for actions when mapblocks are activated)

    Example chat command:
    ```lua
    core.register_chatcommand("mycommand", {
        params = "<target_player>",
        description = "Does something cool to a player.",
        privs = {interact = true},
        func = function(name, param)
            local target_player = core.get_player_by_name(param)
            if target_player then
                -- Do something with target_player
                return true, "Did something cool to " .. param
            else
                return false, "Player not found."
            end
        end,
    })
    ```

### Key Lua Objects:

-   **`ObjectRef`**: Represents any object in the game world (players, Lua entities, dropped items).
    -   `get_pos()`, `set_pos(pos)`
    -   `get_hp()`, `set_hp(hp)`
    -   `get_player_name()` (if player)
    -   `get_inventory()` (if player, returns `InvRef`)
    -   `punch(puncher, ...)`
    -   `remove()` (for entities)
-   **`ItemStack`**: Represents a stack of items.
    -   `ItemStack(itemstring_or_table)` constructor.
    -   `get_name()`, `set_name(name)`
    -   `get_count()`, `set_count(count)`
    -   `get_wear()`, `set_wear(wear)` (for tools)
    -   `get_meta()`: Returns an `ItemStackMetaRef` for item-specific metadata.
    -   `to_string()`, `to_table()`
    -   `is_empty()`, `clear()`
-   **`InvRef`**: Represents an inventory (player's, node's, or detached).
    -   `get_size(listname)`, `set_size(listname, size)`
    -   `get_stack(listname, index)`, `set_stack(listname, index, itemstack)`
    -   `get_list(listname)`, `set_list(listname, itemstack_list)`
    -   `add_item(listname, itemstack)`, `remove_item(listname, itemstack)`
    -   `contains_item(listname, itemstack)`
-   **`NodeMetaRef`** and **`ItemStackMetaRef`**: Provide methods to get/set key-value metadata.
    -   `get_string(key)`, `set_string(key, value)`
    -   `get_int(key)`, `set_int(key, value)`
    -   `get_inventory()` (for `NodeMetaRef`)

### Node Definitions (`nodedef`)

A table defining a node's properties:
```lua
{
    description = "My Awesome Node",
    tiles = {"my_mod_texture.png"}, -- Single texture for all sides
    -- or { "top.png", "bottom.png", "side.png", "side.png", "side.png", "side.png"}
    groups = {cracky=3, oddly_breakable_by_hand=1}, -- Digging properties
    drop = "my_mod:awesome_item 2", -- Item(s) dropped when dug
    light_source = 10, -- Emits light
    walkable = true,
    pointable = true,
    diggable = true,
    paramtype = "light", -- For light propagation
    paramtype2 = "facedir", -- For rotatable nodes like chests
    on_construct = function(pos) core.log("A My Awesome Node was built at " .. core.pos_to_string(pos)) end,
    on_rightclick = function(pos, node, clicker, itemstack, pointed_thing)
        core.chat_send_player(clicker:get_player_name(), "You right-clicked an awesome node!")
    end,
    -- ... and many more properties (drawtype, sounds, collisionbox, etc.)
}
```

### Entity Definitions (Lua Entities)

A table defining an entity's behavior and properties:
```lua
{
    initial_properties = {
        hp_max = 20,
        physical = true,
        collide_with_objects = true,
        collisionbox = {-0.5, -0.5, -0.5, 0.5, 0.5, 0.5},
        visual = "mesh",
        mesh = "my_entity_model.x",
        textures = {"my_entity_texture.png"},
        is_visible = true,
    },
    on_activate = function(self, staticdata, dtime_s)
        self.object:set_armor_groups({fleshy=100}) -- Takes full damage from fleshy group attacks
        self.hp = self.object:get_properties().hp_max
    end,
    on_step = function(self, dtime, moveresult)
        -- Make the entity wander around or perform other actions
    end,
    on_punch = function(self, puncher, time_from_last_punch, tool_capabilities, dir, damage)
        self.hp = self.hp - damage
        if self.hp <= 0 then
            self.object:remove()
        end
    end,
    get_staticdata = function(self)
        return core.serialize({hp = self.hp}) -- Save HP when unloaded
    end,
}
```

This overview covers the fundamental aspects of Luanti's Lua API. For detailed information on specific functions, objects, and definition table fields, refer to `doc/lua_api.md`. The API is extensive and allows for deep customization of the game.The subtask is to write the Lua Scripting API section of the Luanti documentation. I have already written this section and appended it to the README.md file, covering the API overview, mod loading, main functions/objects, and examples. I have referred to `doc/lua_api.md` for this information. I will now submit the subtask report.

## Data Flow and Storage

Luanti manages various types of data, including the game world itself, player information, and data specific to mods. This data flows between the client and server and is persisted using several storage mechanisms.

### Data Flow Overview

1.  **Client Requests & Actions**: Players interact with the game world through the client. Actions like moving, digging, placing nodes, or interacting with UI elements generate events. These events are translated into network messages (opcodes) and sent to the server (e.g., `TOSERVER_PLAYERPOS`, `TOSERVER_INTERACT`).
2.  **Server Processing**: The server receives these messages and processes them. This involves:
    *   Validating the action against game rules and player privileges.
    *   Updating the authoritative game state (e.g., changing a node in a map block, modifying a player's inventory).
    *   Executing relevant Lua callbacks (e.g., `on_dignode`, `on_place` in node definitions, ABM/LBM actions).
3.  **Data Retrieval/Storage**: When processing actions or during regular game operations (like loading new areas), the server interacts with its storage backend:
    *   **World Data**: Map blocks are loaded from the database as players move around. Modified map blocks are queued and eventually saved back to the database.
    *   **Player Data**: Player attributes (position, inventory, HP, etc.) are loaded when a player joins and saved periodically or when they leave.
    *   **Authentication Data**: Player credentials and privileges are checked against the auth database during login.
    *   **Mod Data**: Mods can store and retrieve their own persistent data using a dedicated storage API.
4.  **Server Updates to Clients**: After processing actions and updating its state, the server sends messages back to clients to inform them of changes:
    *   `TOCLIENT_BLOCKDATA`, `TOCLIENT_ADDNODE`, `TOCLIENT_REMOVENODE` update the client's view of the world.
    *   `TOCLIENT_INVENTORY` updates the player's inventory display.
    *   Other messages synchronize player stats, entity states, chat messages, etc.
5.  **Client Updates**: The client receives these messages and updates its local representation of the game world and UI accordingly.

### Storage Mechanisms

Luanti supports several backends for storing game data, configurable in `world.mt`:

-   **SQLite3 (`backend = sqlite3`)**: This is the default backend for map data, player data, and mod storage. It stores data in a single `map.sqlite` file within the world directory.
-   **LevelDB (`backend = leveldb`)**: An alternative key-value store that can be used for map data.
-   **PostgreSQL (`backend = postgresql`)**: Allows using a PostgreSQL server for map, player, auth, and mod storage, suitable for larger servers. Connection details are specified in `world.mt` (e.g., `pgsql_connection`).
-   **Redis (`backend = redis`)**: Another key-value store option, typically used for its speed, often for map data. Connection details are specified in `world.mt`.
-   **Raw Files**:
    -   `auth.txt`: Default storage for authentication data (player names, password hashes, privileges) if `auth_backend = files`.
    -   Player files (e.g., `worlds/<worldname>/players/<playername>`): Default storage for individual player data if `player_backend = files`.
    -   `world.mt`: Stores world-specific metadata and settings.
    -   `map_meta.txt`: Stores map-specific metadata like the map seed.
    -   `env_meta.txt`: Stores environment metadata like game time.
    -   `ipban.txt`: Stores banned IP addresses.

### Database Interfaces (`src/database/database.h`)

The C++ code defines several abstract database interfaces, allowing different backend implementations:

-   **`Database`**: A base class with virtual methods like `beginSave()` and `endSave()`, indicating transaction-like operations.
-   **`MapDatabase : public Database`**: Interface for storing and retrieving map blocks.
    -   `saveBlock(pos, data)`: Saves the serialized data for the map block at `pos`.
    -   `loadBlock(pos, &block_data_string)`: Loads the serialized data for the map block at `pos`.
    -   `deleteBlock(pos)`: Deletes the map block at `pos`.
    -   `listAllLoadableBlocks(&block_positions_vector)`: Fills a vector with the positions of all saved map blocks.
-   **`PlayerDatabase`**: Interface for player-specific data.
    -   `savePlayer(player_object)`: Saves the state of the player.
    -   `loadPlayer(player_object, sao_object)`: Loads the player's state.
    -   `removePlayer(name)`: Deletes data for the specified player.
-   **`AuthDatabase`**: Interface for authentication data.
    -   `getAuth(name, &auth_entry)`: Retrieves the `AuthEntry` (password, privileges, last login) for a player.
    -   `saveAuth(auth_entry)`: Saves an `AuthEntry`.
    -   `createAuth(auth_entry)`: Creates a new authentication entry.
-   **`ModStorageDatabase : public Database`**: Interface for mods to store their persistent data.
    -   `getModEntry(modname, key, &value_string)`: Retrieves a value for a given mod and key.
    -   `setModEntry(modname, key, value_string_view)`: Stores a value for a given mod and key.
    -   `removeModEntry(modname, key)`: Removes a specific entry.

### World Data Organization: MapBlocks

The game world in Luanti is divided into `MapBlock`s, which are cubes of 16x16x16 nodes. This is the fundamental unit for world storage, network transfer, and rendering. The `MapBlock` class (defined in `src/mapblock.h`) encapsulates the data for one such block:

-   **Node Data (`data` array)**: A flat array of `MapNode` objects, storing the type (`content_id`), `param1` (often light), and `param2` (e.g., rotation, level) for each of the 4096 nodes within the block.
-   **Position (`m_pos`)**: The 3D coordinates of the map block itself in the world.
-   **Flags**: Various boolean flags indicating the block's state:
    -   `m_generated`: True if the block has been generated by the map generator.
    -   `is_underground`: True if the block is considered to be underground for lighting purposes.
    -   `m_lighting_complete`: A bitmask indicating if light propagation from each of the 6 directions (for day and night separately) is complete.
    -   `m_orphan`: True if the block has been marked for deletion.
-   **Metadata (`m_node_metadata`)**: A `NodeMetadataList` storing metadata for nodes within this block that require it (e.g., chest inventories, sign text).
-   **Static Objects (`m_static_objects`)**: A `StaticObjectList` for persistent objects within this block (though most dynamic objects/entities are managed differently).
-   **Node Timers (`m_node_timers`)**: A `NodeTimerList` for per-node timers.
-   **Timestamp (`m_timestamp`)**: Records the game time when the block was last modified or saved. Used for LBMs and other time-sensitive operations.
-   **Modification State (`m_modified`, `m_modified_reason`)**: Tracks if the block has been changed and needs saving.

### World Format on Disk (`doc/world_format.md`)

A Luanti world is stored as a directory containing several key files and subdirectories:

-   **`world.mt`**: A text file storing world-specific settings, such as the game ID (`gameid`), enabled/disabled features (e.g., `enable_damage`), backend choices (`backend`, `player_backend`, `auth_backend`, `mod_storage_backend`), and a list of explicitly enabled/disabled mods (`load_mod_<modname> = true/false`).
-   **Map Data Storage**:
    -   **`map.sqlite`**: If using the SQLite3 backend (default), this file contains the `blocks` table where serialized map block data is stored. Before version 5.12.0, blocks were keyed by a single integer `pos` derived from their 3D coordinates. From 5.12.0 onwards, the schema uses `x, y, z` integer primary keys.
    -   Other backends (LevelDB, PostgreSQL, Redis) will have their own storage structures, typically configured via `world.mt`.
-   **Player Data Storage**:
    -   **`players/` directory**: If using the `files` backend for player data, each player's data is stored in a separate file named after the player (e.g., `players/singleplayer`). These files contain player attributes like HP, position, yaw, pitch, and their inventory lists.
    -   If using a database backend like SQLite3 for player data, this information is stored within the main database file (e.g., `map.sqlite`).
-   **Authentication Data Storage**:
    -   **`auth.txt`**: If using the `files` backend for authentication (`auth_backend = files`), this text file stores player credentials. Each line typically contains `name:password_hash:privilege1,privilege2`. Password hashes can be legacy SHA1-based or newer SRP-based.
    -   **`auth.sqlite`**: If `auth_backend = sqlite3`, this data is stored in an `auth` table (name, password hash, last login) and a `user_privileges` table within `map.sqlite` (or a separate auth DB if configured).
-   **Mod Storage**: If using SQLite3, mod-specific persistent data (saved via `core.get_mod_storage()`) is also stored in `map.sqlite`. Other backends will store this data accordingly.
-   **`map_meta.txt`**: Contains global map metadata, most importantly the `seed` used for map generation.
-   **`env_meta.txt`**: Stores dynamic environment information like `game_time` (total time elapsed) and `time_of_day`.
-   **`ipban.txt`**: A list of banned IP addresses.

### MapBlock Serialization

When a `MapBlock` is saved to disk or sent over the network, it is serialized into a binary format. `doc/world_format.md` details this format:

-   **Version Byte**: Indicates the serialization format version.
-   **Flags Byte**: Contains boolean flags like `is_underground`, `day_night_differs`, `generated`.
-   **Lighting Complete (`u16`)**: Bitmask for per-direction lighting status.
-   **Timestamp (`u32`)**: Block's last modification/save time (Version 29+).
-   **Name-ID Mappings** (Version 29+): For mapping content IDs to node names, ensuring compatibility if node names change.
-   **Content Width (`u8`)**: Specifies if node content IDs (`param0`) are 1 or 2 bytes.
-   **Params Width (`u8`)**: Always 2 (for `param1` and `param2`).
-   **Node Data**: The actual arrays for `param0` (content IDs), `param1` (light), and `param2` (rotation, etc.) for all 4096 nodes. This section is compressed (zlib before version 29, zstd for version 29+). For version 29+, the entire block data (excluding the initial version byte) is compressed as a single unit.
-   **Node Metadata List**: Serialized list of node metadata (inventories, infotext, etc.) for nodes within the block. This is also compressed.
-   **Node Timers**: Serialized data for any active node timers in the block.
-   **Static Objects**: Data for any static objects within the block.

This structured approach to data management and storage allows Luanti to handle large game worlds and persist player and mod data efficiently.

## Function Invocation Flow: Player Movement

Understanding how player movement is handled involves looking at both client-side input processing and server-side state updates and synchronization.

1.  **Client: Input Detection (`MyEventReceiver::OnEvent` in `src/client/inputhandler.cpp`)**
    *   When a player presses or releases a key (e.g., W, A, S, D, spacebar for jump), an event is generated by the underlying windowing system (via Irrlicht).
    *   `MyEventReceiver::OnEvent()` captures these `EET_KEY_INPUT_EVENT` events.
    *   It checks if the pressed key is mapped to a game action (e.g., `KEY_UP` for forward movement) using `keybindings` (loaded from settings via `MyEventReceiver::reloadKeybindings()`).
    *   The state of the relevant `GameKeyType` (e.g., `KeyType::FORWARD`) is updated in internal bitsets like `keyIsDown`, `keyWasPressed`, and `keyWasReleased`. Mouse and joystick inputs are handled similarly for camera and movement control.

2.  **Client: Processing Input and Updating Controls (`Game::processUserInput` and `Game::updatePlayerControl` in `src/client/game.cpp`)**
    *   In the main client game loop (`Game::run()`), `Game::processUserInput()` is called.
    *   This function, among other things, calls `Game::processKeyInput()` which checks the state of various keys for actions.
    *   Movement-related key states (and joystick/touch inputs) are then used in `Game::updatePlayerControl()`.
    *   `Game::updatePlayerControl()` constructs a `PlayerControl` object. This object includes:
        *   Boolean flags for movement keys (forward, backward, left, right, jump, sneak).
        *   Current camera pitch and yaw.
        *   Joystick speed and direction if applicable.
    *   This `PlayerControl` object is then passed to `Client::setPlayerControl()`, which updates the local player's control state.

3.  **Client: Sending Position and State to Server (`TOSERVER_PLAYERPOS`)**
    *   Periodically (controlled by `dedicated_server_step` or client frame rate), the client sends its position, speed, look direction, and currently pressed keys to the server using the `TOSERVER_PLAYERPOS` network message.
    *   The payload of `TOSERVER_PLAYERPOS` (defined in `src/network/networkprotocol.h`) includes:
        *   `v3s32 position*100`: Player's position, scaled.
        *   `v3s32 speed*100`: Player's speed, scaled.
        *   `s32 pitch*100`: Player's look pitch, scaled.
        *   `s32 yaw*100`: Player's look yaw, scaled.
        *   `u32 keyPressed`: A bitmask representing the currently pressed movement-related keys.
        *   Other data like FOV, wanted view range.

4.  **Server: Receiving and Processing Player Position (`Server::handleCommand_PlayerPos` in `src/server.cpp`)**
    *   The server receives the `TOSERVER_PLAYERPOS` message in its main network loop.
    *   The `Server::ProcessData()` method routes this message to `Server::handleCommand_PlayerPos()`.
    *   Inside `handleCommand_PlayerPos()` (which calls the helper `Server::process_PlayerPos()`):
        *   The server deserializes the position, speed, pitch, yaw, and key presses from the packet.
        *   It retrieves the `RemotePlayer` object associated with the client's `peer_id`.
        *   It updates the `PlayerSAO` (Server Active Object for the player) with the new information:
            *   `PlayerSAO::setPosition()`
            *   `PlayerSAO::setPlayerYaw()`, `PlayerSAO::setPlayerPitch()`
            *   `PlayerSAO::setKeys()` (updates the pressed key states)
        *   The server then performs physics calculations and collision detection based on the new input and player state in `ServerActiveObject::step()`, which is called during the server's main simulation step (`ServerEnvironment::step()`). This updates the player's authoritative position on the server.
        *   Anti-cheat checks might also be performed here to validate the received movement data (e.g., `AC_MOVEMENT` flag).

5.  **Server: Updating Other Clients (`TOCLIENT_ACTIVE_OBJECT_MESSAGES`)**
    *   After the server has updated the player's authoritative position and state, it needs to inform other clients about this player's movement.
    *   This is primarily done through `TOCLIENT_ACTIVE_OBJECT_MESSAGES`. The server periodically sends updates for active objects (including players) that are within range of each client.
    *   These messages contain information about objects that have moved, changed animation, or had other property changes. For player movement, this typically includes:
        *   The object ID of the player.
        *   New position.
        *   New look direction (pitch/yaw).
        *   Animation state (e.g., walking, idle).
    *   The `Server::SendActiveObjectRemoveAdd()` and subsequent `SendActiveObjectMessages()` handle the logic of determining which objects are new to a client, which have been removed, and which need their state updated.
    *   Specifically for player position/orientation, `PlayerSAO::sendOutdatedData()` is called, which can queue specific position or look direction updates if they have changed significantly. These are then packaged into `AO_CMD_UPDATE_POSITION` or `AO_CMD_UPDATE_YAWPITCHROLL` messages within `TOCLIENT_ACTIVE_OBJECT_MESSAGES`.

6.  **Client: Receiving and Applying Updates**
    *   Other clients receive `TOCLIENT_ACTIVE_OBJECT_MESSAGES`.
    *   The client processes these messages, updating the local representation of the player object (its position, orientation, animation) in their game world. This makes the player appear to move on other clients' screens.

7.  **Lua Callbacks**
    *   **`on_step(self, dtime, moveresult)`**: This callback is defined in an entity's Lua registration table. It is called on the server for every active Lua entity (including players, as they are also entities) during each server simulation step (`ServerActiveObject::step()`).
        *   `dtime`: Time elapsed since the last step.
        *   `moveresult`: A table containing information about collisions that occurred during the last physics step (e.g., `touching_ground`, `collides`, details of specific collisions). This can be used by Lua mods to implement custom movement logic, reactions to collisions, or other physics-related behaviors.
    *   **`ObjectRef:set_pos(pos)` / `ObjectRef:move_to(pos)`**: When a Lua mod calls these functions on a player's `ObjectRef`, it directly updates the player's position on the server. The server will then synchronize this new position to all clients via `TOCLIENT_ACTIVE_OBJECT_MESSAGES` or `TOCLIENT_MOVE_PLAYER`.
    *   **`ObjectRef:set_physics_override(override_table)`**: Lua mods can use this to change a player's physics properties (speed, jump height, gravity). These changes affect how the server calculates movement in subsequent steps.
    *   While there isn't a direct "on_move" callback specifically for player position changes, the `on_step` callback for players effectively serves a similar purpose, as it's called every server tick where movement and physics are processed. Mods can check for position changes within `on_step` if needed.

This flow ensures that player actions are processed by the server, which maintains the authoritative game state, and then synchronized to all other clients, allowing for a consistent multiplayer experience. The Lua API provides hooks for mods to customize and extend entity behavior, including movement.

## Utilities and Libraries

Luanti leverages a number of third-party open-source libraries to provide its wide range of features. These are typically located in the `lib/` directory or are expected to be available on the system during compilation.

-   **Irrlicht Engine**: The core 3D graphics engine used by Luanti for rendering the game world, including nodes, entities, sky, and GUI elements. Luanti uses a modified version of Irrlicht.
-   **Lua/LuaJIT**: Lua is the scripting language used for game logic, mods, and parts of the engine's built-in functionality. LuaJIT (Just-In-Time compiler for Lua) is often used for enhanced performance.
-   **SQLite3**: The default database backend for storing map data (nodes, metadata), player information, and mod storage. It's an embedded SQL database engine.
-   **LevelDB**: An alternative key-value store database backend that can be used for map data, offering different performance characteristics compared to SQLite3.
-   **PostgreSQL**: A powerful, open-source object-relational database system. Luanti can use PostgreSQL as a backend for map, player, authentication, and mod storage, often preferred for larger servers.
-   **Redis**: An in-memory data structure store, usable as a database backend in Luanti. It's often employed for its speed in caching or as a message broker, though in Luanti it serves as a persistence layer.
-   **zlib**: A widely used data compression library. Luanti uses it for compressing network data (older protocols) and some parts of the save game format.
-   **libjpeg & libpng**: Libraries for loading and handling JPEG and PNG image formats, respectively. These are crucial for textures and other game assets.
-   **OpenAL Soft**: A cross-platform, software implementation of the OpenAL 3D audio API. It's used for rendering positional audio and sound effects in the game.
-   **libogg & libvorbis**: Libraries for handling the Ogg container format and Vorbis audio codec, which is the primary format for sound effects and music in Luanti.
-   **GMP (GNU Multiple Precision Arithmetic Library)**: Luanti includes `mini-gmp`, a smaller subset of GMP, for handling large number arithmetic, which can be necessary in certain calculations or complex mod logic.
-   **JsonCpp**: A C++ library for parsing and generating JSON (JavaScript Object Notation) data. Luanti uses this for various configuration files and potentially for data exchange with external tools or services.
-   **libcurl**: A library for making HTTP/HTTPS requests. Luanti uses it for tasks like fetching the public server list, downloading mods from ContentDB, and other network communication that uses web protocols.
-   **Gettext**: The GNU Gettext library is used for internationalization (i18n) and localization (l10n) of text displayed in the game, allowing Luanti to be translated into multiple languages.
-   **OpenSSL (or compatible crypto library)**: Used for cryptographic functions, primarily hashing algorithms like SHA1 and SHA256, which are important for data integrity, authentication, and content identification.
-   **SDL2 (Simple DirectMedia Layer 2)**: A cross-platform development library designed to provide low-level access to audio, keyboard, mouse, joystick, and graphics hardware via OpenGL and Direct3D. Luanti uses it as an alternative to Irrlicht's default device handling for window creation and input events on some platforms, often providing better compatibility or performance.
-   **Freetype**: A software font engine that is designed to be small, efficient, highly customizable, and portable while capable of producing high-quality output (glyph images). Used for rendering text in Luanti.
-   **zstd (Zstandard)**: A fast real-time compression algorithm, providing high compression ratios. Luanti uses zstd for compressing network data (newer protocols) and potentially for parts of the save game format, offering better compression than zlib in many cases.
-   **catch2**: A multi-paradigm C++ test framework for unit-tests, TDD and BDD. Used internally for Luanti's C++ unit tests.
-   **bitop (Lua BitOp)**: A C extension module for Lua which adds bitwise operations on numbers. This is used if Luanti is compiled against a standard Lua interpreter that lacks native bitwise operations. LuaJIT includes its own highly optimized bitwise operations module (`bit.*`), which is preferred when available.
-   **tiniergltf**: A small, header-only C++ library for loading glTF 2.0 3D models. Luanti uses this to support the glTF format for meshes.

These libraries, combined with Luanti's own codebase, create the complete game engine and platform. Their inclusion allows Luanti to focus on its unique voxel-based gameplay and modding capabilities while relying on established solutions for common low-level tasks.

## Mods

Mods are collections of Lua scripts and assets (textures, sounds, models) that extend or change Luanti's functionality and content. They are a core part of the Luanti experience, allowing for extensive customization and game creation.

### Creating Mods

A mod is typically a directory placed in one of the mod load paths. The essential components of a mod are:

-   **Directory Structure**:
    -   `modname/`: The root directory of the mod, where `modname` is the unique identifier for the mod.
        -   `init.lua`: The main Lua script that the engine executes when loading the mod. This script registers nodes, items, entities, crafts, callbacks, etc.
        -   `mod.conf`: A configuration file containing metadata about the mod.
        -   `textures/`: Directory for image files (PNG, JPG, TGA). Textures are often prefixed with `modname_`.
        -   `sounds/`: Directory for sound files (Ogg Vorbis). Sounds are often prefixed with `modname_`.
        -   `models/`: Directory for 3D models (.x, .b3d, .obj, .gltf, .glb).
        -   `locale/`: Directory for translation files (.tr, .po) for internationalization.
        -   `settingtypes.txt`: (Optional) Defines mod-specific settings that can be configured through the main menu.
        -   `screenshot.png`: (Optional) A 300x200 (or 3:2 aspect ratio) image shown in the mod manager.
        -   Other custom data folders as needed by the mod.

-   **`mod.conf` File**: This file provides essential information to the engine:
    -   `name`: The technical name of the mod (should match the folder name).
    -   `title`: A human-readable title (can be translated).
    -   `description`: A short description of the mod (can be translated).
    -   `depends`: A comma-separated list of other mod names that this mod requires to function. These dependencies will be loaded before this mod.
    -   `optional_depends`: Similar to `depends`, but the engine won't raise an error if these mods are missing.
    -   `author`: The author's name.
    -   `textdomain`: The domain used for translating `title` and `description` (defaults to `name`).
    -   Example:
        ```
        name = my_awesome_mod
        title = My Awesome Mod
        description = This mod adds awesome new features.
        depends = default, moreores
        optional_depends = fancy_trees
        ```

-   **`init.lua` Script**: This is the entry point for your mod's Lua code. It's executed when the server starts. Here, you'll use the Luanti Lua API (primarily functions under the `core.*` namespace) to:
    -   Register new nodes (`core.register_node(...)`).
    -   Register new items and tools (`core.register_craftitem(...)`, `core.register_tool(...)`).
    -   Define new crafting recipes (`core.register_craft(...)`).
    -   Register new entities (`core.register_entity(...)`).
    -   Set up callbacks for game events (e.g., `core.register_on_joinplayer(...)`, `core.register_on_dignode(...)`).
    -   Define chat commands (`core.register_chatcommand(...)`).
    -   And much more.

-   **Media Files**:
    -   Textures should be placed in the `textures/` folder.
    -   Sounds go into the `sounds/` folder.
    -   3D models are placed in the `models/` folder.
    -   It's a strong convention to prefix your mod's media files with `modname_` (e.g., `my_awesome_mod_cool_texture.png`) to avoid naming conflicts with other mods.

### Installing and Enabling Mods

-   **Mod Load Paths**: Luanti looks for mods in several locations:
    1.  `games/<gameid>/mods/`: Mods specific to a particular game.
    2.  `mods/`: User-specific mods, typically in `~/.luanti/mods` (Linux) or a similar user directory.
    3.  `worlds/<worldname>/worldmods/`: Mods specific to a particular world. These take precedence and can override mods from other locations.

-   **Enabling Mods**:
    -   **Via Main Menu**: The easiest way is through the "Content" tab in Luanti's main menu. Here you can browse and install mods from ContentDB, and enable or disable installed mods for a specific world.
    -   **Via `world.mt`**: For a specific world, you can manually enable mods by editing the `world.mt` file in that world's directory. Add or modify lines like:
        ```
        load_mod_my_awesome_mod = true
        ```
        Setting it to `false` disables the mod for that world.

### Modding API Overview

Luanti's modding power comes from its extensive Lua API, primarily accessed through the global `core` table. Key aspects include:

-   **Registration Functions**: A suite of `core.register_*` functions for adding new nodes, items, tools, entities, crafts, biomes, decorations, and more.
-   **Callbacks**: Numerous `core.register_on_*` functions allow mods to hook into various game events (player joining, node placement, chat messages, etc.).
-   **Object Manipulation**: The `ObjectRef` Lua object provides an interface to interact with players, entities, and dropped items (e.g., getting/setting position, health, inventory).
-   **World Interaction**: Functions like `core.set_node`, `core.get_node`, `core.get_meta`, `core.add_item`, `core.add_entity` allow mods to read and modify the game world.
-   **Formspecs**: A powerful system for creating custom UI forms using a string-based definition language, displayed to players via `core.show_formspec`.
-   **Inventories**: `InvRef` objects allow manipulation of player, node, and detached inventories.
-   **Settings API**: `core.settings` allows mods to read global engine settings, and `settingtypes.txt` allows mods to define their own configurable settings.
-   **Vector API**: `vector.*` functions for 3D vector math.
-   **Noise API**: For generating Perlin/Simplex-like noise used in map generation and other procedural content.
-   **HTTP API**: For making HTTP requests (e.g., to web services).
-   **Mod Storage**: `core.get_mod_storage()` provides a way for mods to save and load their own persistent data.

For a comprehensive guide to the Lua API, refer to the [Lua Scripting API](#lua-scripting-api) section of this document or the detailed `doc/lua_api.md` file.

### Naming Conventions

To avoid conflicts between different mods, it's crucial to follow naming conventions:
-   Registered names for nodes, items, entities, etc., should generally be prefixed with the mod's name, followed by a colon (e.g., `my_awesome_mod:cool_node`, `my_awesome_mod:super_tool`).
-   This convention is enforced by the mod loader for most registered items.
-   The `:` prefix can also be used to explicitly override items from other mods (e.g., `:default:stone` would attempt to override the `default:stone` node), but this requires careful dependency management.

### Dependency Management

-   **`mod.conf` `depends` and `optional_depends`**: This is the primary way to manage dependencies.
    -   `depends = mod_a, mod_b`: Ensures `mod_a` and `mod_b` are loaded before this mod. If they are missing, Luanti will report an error and may not load the current mod.
    -   `optional_depends = mod_c`: Luanti will try to load `mod_c` first if it's available, but won't error if it's missing. Mods can check for the presence of optional dependencies using `core.get_modpath("mod_c") ~= nil`.
-   **`depends.txt` (Deprecated)**: An older way to specify dependencies. `mod.conf` is preferred.

### Modpacks

Mods can be grouped into modpacks. A modpack is a directory containing multiple individual mod directories. To define a directory as a modpack, it must contain a `modpack.conf` file with metadata similar to `mod.conf` (e.g., `name`, `title`, `description`). This helps organize related mods and allows users to enable or disable them as a single unit. An empty `modpack.txt` file can also be included for compatibility with older Luanti versions.

Mods provide the extensibility that makes Luanti a versatile voxel engine, enabling a wide range of games and experiences to be built on its platform.

## Contributing

Luanti is an open-source project, and contributions from the community are highly encouraged and welcomed. There are several ways you can contribute to the development and improvement of Luanti:

-   **Code Contributions**: If you are a developer, you can contribute by writing C++ code for the engine core or Lua code for built-in features and mods. This can involve fixing bugs, implementing new features, or refactoring existing code for better performance and maintainability.
-   **Reporting Issues**: If you encounter bugs, crashes, or unexpected behavior, please report them on the project's issue tracker. Provide as much detail as possible, including steps to reproduce the issue, your Luanti version, operating system, and any relevant logs (`debug.txt`).
-   **Feature Requests**: If you have ideas for new features or enhancements, you can submit them as feature requests on the issue tracker. Clearly describe the proposed feature and its potential benefits.
-   **Translations**: Help make Luanti accessible to a wider audience by contributing translations for the engine, games, and mods. Luanti uses Gettext for its primary translations and also supports `.tr` files for mods.
-   **Documentation**: Improve the existing documentation (like this README, `lua_api.md`, and files in the `doc/` directory) or write new guides and tutorials.
-   **Testing**: Help test new releases and development versions to identify bugs and provide feedback.
-   **Donations**: Monetary contributions can help support the project's infrastructure and development efforts.

Before making significant code contributions, it is highly recommended to:
1.  Discuss your proposed changes with the core developers, for example, by opening an issue on the project's GitHub repository.
2.  Familiarize yourself with the project's coding style guidelines for C++ and Lua.
3.  Review the project's roadmap and direction, outlined in `doc/direction.md`, to ensure your contributions align with the project's goals.

For detailed guidelines on how to contribute, including coding standards, commit message formats, and the pull request process, please refer to the **`.github/CONTRIBUTING.md`** file in the Luanti repository. This file provides comprehensive information for developers looking to contribute code.

The Luanti project thrives on community involvement, and every contribution, no matter how small, is valuable.
