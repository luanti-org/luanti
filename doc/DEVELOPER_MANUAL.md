# Luanti Developer Manual

## Table of Contents

1.  [Introduction](#introduction)
2.  [Architecture](#architecture)
    *   [Client-Server Model](#client-server-model)
    *   [Core Engine (C++)](#core-engine-c)
    *   [Lua Scripting API](#lua-scripting-api-overview)
    *   [Graphics Rendering (Irrlicht)](#graphics-rendering-irrlicht)
3.  [Codebase Structure](#codebase-structure)
4.  [Building and Installation](#building-and-installation)
    *   [General Process](#general-process)
    *   [Key Dependencies](#key-dependencies)
    *   [Running Luanti](#running-luanti)
5.  [Network Protocol](#network-protocol)
    *   [Overview](#overview)
    *   [Initialization Handshake](#initialization-handshake)
    *   [Message Types (Opcodes)](#message-types-opcodes)
        *   [Server-to-Client (ToClient) Messages](#server-to-client-toclient-messages)
        *   [Client-to-Server (ToServer) Messages](#client-to-server-toserver-messages)
    *   [Data Serialization](#data-serialization)
6.  [Game Engine](#game-engine)
    *   [Core Game Loop (Client-Side)](#core-game-loop-client-side)
    *   [Server-Side Game State Management (Conceptual)](#server-side-game-state-management-conceptual)
    *   [Client-Side Prediction and Server Reconciliation](#client-side-prediction-and-server-reconciliation)
    *   [Rendering Pipeline and Irrlicht Engine](#rendering-pipeline-and-irrlicht-engine)
    *   [Input Processing](#input-processing)
7.  [Lua Scripting API](#lua-scripting-api)
    *   [Mod Loading and Execution](#mod-loading-and-execution)
    *   [Core Lua API (`core.*`)](#core-lua-api-core)
    *   [Key Lua Objects](#key-lua-objects)
    *   [Node Definitions (`nodedef`)](#node-definitions-nodedef)
    *   [Entity Definitions (Lua Entities)](#entity-definitions-lua-entities)
8.  [Data Flow and Storage](#data-flow-and-storage)
    *   [Data Flow Overview](#data-flow-overview)
    *   [Storage Mechanisms](#storage-mechanisms)
    *   [Database Interfaces (`src/database/database.h`)](#database-interfaces-srcdatadatabaseh)
    *   [World Data Organization: MapBlocks](#world-data-organization-mapblocks)
    *   [World Format on Disk (`doc/world_format.md`)](#world-format-on-disk-docworld_formatmd)
    *   [MapBlock Serialization](#mapblock-serialization)
9.  [Function Invocation Flow: Player Movement](#function-invocation-flow-player-movement)
10. [Utilities and Libraries](#utilities-and-libraries)
11. [Mods](#mods)
    *   [Creating Mods](#creating-mods)
    *   [Installing and Enabling Mods](#installing-and-enabling-mods)
    *   [Modding API Overview (Summary)](#modding-api-overview-summary)
    *   [Naming Conventions](#naming-conventions)
    *   [Dependency Management](#dependency-management)
    *   [Modpacks](#modpacks)
12. [Contributing](#contributing)

## Introduction

Luanti is a free open-source voxel game engine that provides users with a platform for easy modding and game creation. It is a fork of the Minetest engine, building upon its foundation to offer an enhanced experience for both players and developers. Luanti retains the core strengths of Minetest, such as its lightweight nature and extensive modding capabilities, while also aiming to address some of its limitations.

The primary goals of the Luanti project revolve around several key areas of improvement:
-   Significantly enhance rendering and graphics capabilities for a more visually appealing and immersive experience.
-   Focus on internal code refactoring to improve maintainability, performance, and stability, including modernizing the codebase and streamlining existing systems.
-   Deliver substantial UI/UX improvements, making the engine more intuitive and user-friendly.
-   Enhance the object and entity systems, providing more flexibility and power for creating complex and interactive game worlds.

Through these efforts, Luanti strives to be a leading choice for voxel game development, offering a robust and versatile platform for creative expression.

## Architecture

Luanti employs a client-server architecture, which allows for both single-player and multiplayer experiences.

[Diagram: High-level client-server architecture. Shows a central server with multiple clients connecting to it. Indicate that the server handles game logic and world state, while clients handle rendering and input.]

### Client-Server Model

In a multiplayer setup, the server hosts the game world, manages game logic, and synchronizes player actions. Clients connect to the server to interact with the game world and other players. Even in single-player mode, Luanti runs an internal server that the client connects to, ensuring a consistent experience across both modes.

### Core Engine (C++)

The engine's core is built in C++, providing high performance and stability for demanding tasks such as world generation, physics simulation, and network communication. This C++ core forms the backbone of Luanti, handling the low-level operations and ensuring efficient resource management.

### Lua Scripting API Overview

Complementing the C++ core is a powerful Lua scripting API. This API allows developers to extend and customize the game in numerous ways, from creating new game mechanics and items to designing complex quests and interactive NPCs. The Lua API provides a flexible and accessible way to modify the game without needing to delve into the C++ codebase, fostering a vibrant modding community. (See the [Lua Scripting API](#lua-scripting-api) section for more details).

### Graphics Rendering (Irrlicht)

For graphics rendering, Luanti integrates the Irrlicht Engine, a versatile open-source 3D graphics engine. Irrlicht handles the rendering of the game world, including terrain, objects, and visual effects. This integration allows Luanti to leverage Irrlicht's features for creating visually rich and engaging environments. Luanti aims to modernize and improve upon the existing Irrlicht integration to unlock more advanced graphical capabilities.

## Codebase Structure

The Luanti codebase is organized into several key directories:

-   **`src/`**: Contains the core C++ engine code, including systems for physics, networking, world management, and more. It forms the heart of Luanti's functionality.
-   **`builtin/`**: Houses Lua code that implements built-in game features, such as basic player interactions, inventory management, and the main menu interface.
-   **`games/`**: Intended for game-specific code and assets. Different games built on Luanti can have their own subdirectories here, allowing for modular game development.
-   **`mods/`**: Mod code and assets reside in this directory. Luanti's modding API allows users to create and install mods that extend or modify the game's behavior and content.
-   **`doc/`**: Contains documentation files, including this manual, guides, API references, and tutorials.
-   **`lib/`**: Third-party libraries used by Luanti are stored here (e.g., LuaJIT, SQLite3, zlib). These libraries provide additional functionality.
-   **`irr/`**: Contains the source code for the Irrlicht Engine, which Luanti uses for 3D graphics rendering.
-   **`android/`**: Android-specific build files and code, enabling the compilation and deployment of Luanti on Android devices.
-   **`po/`**: Translation files (typically `.po` format) for internationalization and localization, allowing Luanti to be translated into multiple languages.
-   **`textures/`**: Default textures used by the engine and built-in game features.
-   **`client/`**: Contains client-specific assets, such as shaders used for rendering visual effects, and other resources unique to the client application.
-   **`fonts/`**: Font files used for displaying text in the game and UI.
-   **`misc/`**: A collection of miscellaneous utility scripts and files that support development, building, or other aspects of the Luanti project.
-   **`cmake/`**: Contains CMake build scripts, which are used to configure and build the Luanti project on various platforms.

## Building and Installation

### General Process

Building Luanti from source is managed using CMake, a cross-platform build system generator. The general process involves:
1.  Ensuring all dependencies are installed.
2.  Generating build files specific to your platform and compiler (e.g., Makefiles for Linux, Visual Studio projects for Windows) using CMake.
3.  Compiling the source code using the generated build files.

### Key Dependencies

-   A C++ compiler (e.g., GCC, Clang, MSVC)
-   CMake (version 3.10 or higher is recommended)
-   SDL2 (Simple DirectMedia Layer) for windowing and input
-   SQLite3 for database storage
-   LuaJIT for the Lua scripting backend
-   Other libraries like zlib, libcurl, and OpenAL are also required.

### Running Luanti

After successfully compiling, you can typically run Luanti from the build directory or install it to a system-wide location. For detailed, platform-specific compilation instructions (Linux, macOS, Windows), please refer to the guides located in the `doc/compiling/` directory. These guides provide step-by-step instructions for installing dependencies and building Luanti.

## Network Protocol

### Overview

Luanti utilizes a custom network protocol built on top of UDP (User Datagram Protocol) for communication between the client and server. This protocol is designed to be relatively lightweight and efficient for real-time game interactions. Integers in the protocol are generally transmitted in big-endian format.

[Diagram: Simple network data flow. Client sends player actions (TOSERVER_PLAYERPOS, TOSERVER_INTERACT) to Server. Server processes and sends world updates (TOCLIENT_BLOCKDATA, TOCLIENT_ACTIVE_OBJECT_REMOVE_ADD) back to Client.]

### Initialization Handshake

The connection process begins with an initialization handshake:

1.  **Client to Server (Initial Packet)**: The client sends a packet to the server. This packet includes a protocol ID (`0x4f457403`), an initial sender peer ID (`PEER_ID_INEXISTENT = 0`), a channel number (usually 0), and a reliable packet header with a sequence number (`SEQNUM_INITIAL = 65500`) and an original packet type (`TYPE_ORIGINAL = 1`). This packet essentially signals the client's intent to connect.
2.  **Server to Client (Peer ID Assignment)**: The server responds with a similar packet structure. This response contains a control message (`TYPE_CONTROL = 0`) with a control type `CONTROLTYPE_SET_PEER_ID = 1`. The payload of this message includes the `peer_id_new` that the server assigns to the client for the duration of the session.
3.  **Disconnection**: To disconnect, a packet is sent with the assigned `sender_peer_id` and a control message of type `CONTROLTYPE_DISCO = 3`.

Further details can be found in `doc/protocol.txt`.

### Message Types (Opcodes)

Communication is managed through various message types, or opcodes, each serving a specific purpose. These are defined in `src/network/networkprotocol.h`, `src/network/clientopcodes.h`, and `src/network/serveropcodes.h`.

#### Server-to-Client (ToClient) Messages:

A brief overview of some key messages sent from the server to the client:
-   **`TOCLIENT_HELLO (0x02)`**: Sent after the client's initial connection request. Contains server information like serialization version, protocol version, and supported authentication methods.
-   **`TOCLIENT_AUTH_ACCEPT (0x03)`**: Signals that the server has accepted the client's authentication. Includes map seed, recommended send interval, and sudo mode authentication methods.
-   **`TOCLIENT_BLOCKDATA (0x20)`**: Transmits map block data (a 16x16x16 chunk of nodes) to the client. Includes the block's position and the serialized map block data.
-   **`TOCLIENT_ADDNODE (0x21)`**: Instructs the client to add or update a single node (block) at a specific position. Includes the position, serialized node data, and a flag to keep metadata.
-   **`TOCLIENT_REMOVENODE (0x22)`**: Instructs the client to remove a node at a specific position.
-   **`TOCLIENT_INVENTORY (0x27)`**: Sends the player's inventory data to the client.
-   **`TOCLIENT_TIME_OF_DAY (0x29)`**: Updates the client with the current game time and time speed.
-   **`TOCLIENT_CHAT_MESSAGE (0x2F)`**: Transmits chat messages from the server or other players to the client.
-   **`TOCLIENT_ACTIVE_OBJECT_REMOVE_ADD (0x31)`**: Informs the client about active objects (entities) being removed or added to the game world.
-   **`TOCLIENT_ACTIVE_OBJECT_MESSAGES (0x32)`**: Sends messages or updates related to specific active objects (e.g., position, animation).
-   **`TOCLIENT_HP (0x33)`**: Updates the player's health points.
-   **`TOCLIENT_MOVE_PLAYER (0x34)`**: Instructs the client to move its player character to a new position with a specific pitch and yaw (often used for corrections or initial spawn).
-   **`TOCLIENT_NODEDEF (0x3a)`**: Sends node definitions (how blocks look and behave) to the client.
-   **`TOCLIENT_ITEMDEF (0x3d)`**: Sends item definitions to the client.
-   **`TOCLIENT_PLAY_SOUND (0x3f)`**: Tells the client to play a sound effect.
-   **`TOCLIENT_PRIVILEGES (0x41)`**: Sends the player's privileges (permissions) to the client.
-   **`TOCLIENT_INVENTORY_FORMSPEC (0x42)`**: Sends the layout definition (formspec) for an inventory screen.
-   **`TOCLIENT_SHOW_FORMSPEC (0x44)`**: Tells the client to display a specific UI form (formspec).
-   **`TOCLIENT_SET_SKY (0x4f)`**: Configures the client's sky rendering parameters.
-   **`TOCLIENT_MODCHANNEL_MSG (0x57)`**: Transmits messages over custom mod channels.

#### Client-to-Server (ToServer) Messages:

A brief overview of some key messages sent from the client to the server:
-   **`TOSERVER_INIT (0x02)`**: The initial packet sent by the client to initiate connection, containing client version information and player name.
-   **`TOSERVER_INIT2 (0x11)`**: An acknowledgment sent by the client after receiving `TOCLIENT_AUTH_ACCEPT`, signaling readiness to receive game data.
-   **`TOSERVER_PLAYERPOS (0x23)`**: Periodically sends the player's current position, orientation (pitch/yaw), speed, and pressed keys to the server. This is the primary way player movement and actions are communicated.
-   **`TOSERVER_GOTBLOCKS (0x24)`**: Sent by the client to acknowledge receipt of map block data, helping the server manage data transmission.
-   **`TOSERVER_INVENTORY_ACTION (0x31)`**: Informs the server about actions the player takes within their inventory (e.g., moving items between slots, dropping items).
-   **`TOSERVER_CHAT_MESSAGE (0x32)`**: Sends a chat message from the client to the server for broadcasting.
-   **`TOSERVER_INTERACT (0x39)`**: Sent when the player interacts with the game world (e.g., digging a node, placing a node, using an item on a node or entity). Includes the type of action, item involved, and the target.
-   **`TOSERVER_NODEMETA_FIELDS (0x3b)`**: Sent by the client to update metadata associated with a node (e.g., contents of a chest, text on a sign) after interacting with a formspec.
-   **`TOSERVER_INVENTORY_FIELDS (0x3c)`**: Sent when the player interacts with fields in a general formspec (not necessarily node-specific).
-   **`TOSERVER_REQUEST_MEDIA (0x40)`**: Sent by the client to request media files (textures, sounds) from the server that it doesn't have in its cache.
-   **`TOSERVER_CLIENT_READY (0x43)`**: Sent by the client after it has received initial definitions (nodes, items) and media, signaling it's ready to fully enter the game.
-   **`TOSERVER_SRP_BYTES_A (0x51)` & `TOSERVER_SRP_BYTES_M (0x52)`**: Used for Secure Remote Password (SRP) authentication.

### Data Serialization

Luanti uses a custom serialization format for transmitting data over the network. Complex data structures like map blocks, node definitions, and inventories are serialized into a byte stream before being sent and then deserialized by the receiving end.

-   **Basic Data Types**: Integers are typically sent in big-endian byte order. Floating-point numbers, strings, and boolean values have their specific serialization methods.
-   **Map Blocks**: `TOCLIENT_BLOCKDATA` messages contain serialized `MapBlock` objects. This includes node IDs, parameters (`param1`, `param2`), and metadata for all nodes within that block. (See [MapBlock Serialization](#mapblock-serialization)).
-   **Node and Item Definitions**: `TOCLIENT_NODEDEF` and `TOCLIENT_ITEMDEF` messages transmit compressed (using zstd) serialized data for `NodeDefManager` and `ItemDefManager` respectively. These managers contain all the definitions for nodes and items available in the game.
-   **Inventories**: `TOCLIENT_INVENTORY` and `TOSERVER_INVENTORY_ACTION` messages involve serialized inventory data.
-   **Formspecs**: Formspecs, which define UI layouts, are transmitted as strings.

The specific serialization and deserialization logic can be found within the C++ source code, particularly in files dealing with network communication and data structures (e.g., `src/mapblock.cpp`, `src/nodedef.cpp`, `src/itemdef.cpp`, `src/inventorymanager.cpp`, and various files in `src/network/`). The `serialize.h` and `serialize.cpp` files contain core serialization helper functions.

For more in-depth details on specific packet structures and serialization methods, consulting the source code, especially the files mentioned in `src/network/` and the definitions in `src/network/networkprotocol.h`, is recommended. The `doc/protocol.txt` file also provides a foundational (though potentially outdated) overview of the initial connection sequence.

## Game Engine

The Luanti game engine orchestrates the entire player experience, from rendering the world to handling player input and managing game state.

[Diagram: Simplified Game Engine Loop. Input -> Process Input -> Update Game State (Client Prediction/Server Reconciliation) -> Update Scene -> Render -> Output. Show client-server interaction for state updates.]

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

While `src/server/server.cpp` handles server operations, the server is the ultimate authority on game state. It is responsible for:
-   **World State**: Maintaining the definitive state of all map blocks, nodes, and their metadata.
-   **Player State**: Tracking each connected player's position, inventory, health, privileges, and other relevant attributes.
-   **Entity (Active Object) State**: Managing the state and behavior of all active objects in the game world.
-   **Game Logic**: Executing core game rules, processing player actions validated from client inputs, running AI for entities, and managing game events (e.g., day/night cycle, weather).
-   **Persistence**: Saving and loading the game world and player data to/from storage (e.g., using a database like SQLite3).
-   **Network Synchronization**: Broadcasting relevant game state updates to all connected clients to keep their views consistent.

### Client-Side Prediction and Server Reconciliation

Luanti implements client-side prediction for actions like node digging and placement to make the game feel responsive despite network latency:
1.  **Client Predicts**: The client immediately simulates the result of an action (e.g., a node appears).
2.  **Client Sends Action to Server**: The client sends a message like `TOSERVER_INTERACT`.
3.  **Server Validates and Processes**: The server validates the action and updates its authoritative game state.
4.  **Server Broadcasts Update**: The server sends messages (e.g., `TOCLIENT_ADDNODE`) about the official state change.
5.  **Client Reconciles**: If the client's prediction differs from the server's update, the client corrects its local state.

### Rendering Pipeline and Irrlicht Engine

Luanti uses the Irrlicht Engine (source in `irr/`) for 3D graphics. The `RenderingEngine` class (`src/client/renderingengine.h/cpp`) manages Irrlicht:
1.  **Initialization**: `RenderingEngine::RenderingEngine()` sets up Irrlicht with parameters like resolution and VSync.
2.  **Scene Setup**: The `Client` and `Game` classes populate the Irrlicht scene with terrain, objects, sky, etc. `ClientMap::updateDrawList()` manages visible map block meshes.
3.  **Shader Management**: Custom shaders for effects are managed by `ShaderSource`. Global shader parameters (uniforms) like lighting and fog are set each frame by classes like `GameGlobalShaderUniformSetter`.
4.  **Drawing Loop**: `RenderingEngine::draw_scene()` orchestrates rendering, calling `driver->beginScene()`, drawing the 3D scene and 2D GUI, and `driver->endScene()`.
5.  **Irrlicht Integration**: Luanti uses Irrlicht's scene manager, video driver, mesh system, and GUI environment.

### Input Processing

Player input is handled by `InputHandler` and `MyEventReceiver` (`src/client/inputhandler.h/cpp`):
1.  **Event Reception**: `MyEventReceiver::OnEvent()` receives system events from Irrlicht (keyboard, mouse, touch, joystick).
2.  **State Tracking**: `MyEventReceiver` tracks key states (`keyIsDown`, `keyWasPressed`, etc.) using bitsets.
3.  **Input Abstraction**: `InputHandler` provides an abstraction for the `Game` class to query input states.
4.  **Game Logic Access**: `Game::processUserInput()` uses the `InputHandler` to update player movement, camera, and trigger interactions.

## Lua Scripting API

Luanti features an extensive server-side Lua API for modding, primarily through the `core` global table (aliased as `minetest` for backward compatibility).

[Diagram: Lua API interaction. Mod (Lua) <-> Luanti Core API (Lua bindings) <-> C++ Engine Core.]

### Mod Loading and Execution
-   **`init.lua`**: The main script for each mod, executed at server startup. Responsible for registering nodes, items, entities, crafts, and callbacks.
-   **Mod Load Paths**: Mods are loaded from `games/<gameid>/mods/`, `mods/` (user's mod directory), and `worlds/<worldname>/worldmods/`.
-   **`mod.conf`**: Metadata file for each mod (name, title, description, dependencies).
    ```
    name = my_mod
    title = My Mod
    description = This is an example mod.
    depends = default
    ```
-   **Directory Structure**: Includes `init.lua`, `mod.conf`, and subdirectories like `textures/`, `sounds/`, `models/`, `locale/`.

### Core Lua API (`core.*`)
A brief overview of common API functionalities:

-   **Registration**:
    -   `core.register_node(name, nodedef_table)`: Defines a new block type.
    -   `core.register_craftitem(name, itemdef_table)`: Defines a new basic item.
    -   `core.register_tool(name, tooldef_table)`: Defines a new tool.
    -   `core.register_entity(name, entitydef_table)`: Defines a new Lua entity.
    -   `core.register_craft(recipe_table)`: Defines a crafting recipe.
    -   `core.register_chatcommand(name, command_def_table)`: Defines a new chat command.
    -   `core.register_alias(alias_name, original_name)`: Aliases an item.
-   **World Interaction**:
    -   `core.get_node(pos)`: Returns the node at `pos`.
    -   `core.set_node(pos, node_table)`: Sets a node at `pos`.
    -   `core.remove_node(pos)`: Removes a node.
    -   `core.add_item(pos, itemstring_or_stack)`: Spawns a dropped item.
    -   `core.add_entity(pos, entity_name)`: Spawns an entity.
    -   `core.get_meta(pos)`: Returns a `NodeMetaRef` for node metadata.
-   **Player Interaction**:
    -   `core.get_player_by_name(name)`: Returns an `ObjectRef` for the player.
    -   `core.chat_send_player(name, message)`: Sends a chat message.
    -   `core.show_formspec(playername, formname, formspec_string)`: Displays a UI form.
-   **Callbacks**:
    -   `core.register_on_joinplayer(function(player_ref, last_login))`
    -   `core.register_on_dignode(function(pos, oldnode, digger_ref))`
    -   `core.register_abm(abm_definition)`: Active Block Modifier for periodic node actions.
    -   `core.register_lbm(lbm_definition)`: Loading Block Modifier for actions on map block activation.
    -   `core.register_globalstep(function(dtime))`: Called every server step.

### Key Lua Objects
-   **`ObjectRef`**: Represents players, Lua entities, and dropped items. Methods include `get_pos()`, `set_pos()`, `get_hp()`, `set_hp()`, `get_inventory()`.
-   **`ItemStack`**: Represents a stack of items. Methods include `get_name()`, `get_count()`, `get_wear()`, `set_count()`, `get_meta()`.
-   **`InvRef`**: Represents inventories. Methods include `get_size()`, `set_size()`, `get_stack()`, `set_stack()`, `add_item()`.
-   **`NodeMetaRef` / `ItemStackMetaRef`**: For key-value metadata storage. Methods include `get_string()`, `set_string()`, `get_int()`, `set_int()`.

### Node Definitions (`nodedef`)
Example:
```lua
core.register_node("mymod:super_stone", {
    description = "Super Stone",
    tiles = {"mymod_super_stone.png"},
    groups = {cracky = 1, stone = 1},
    drop = "mymod:super_stone_cobble",
    sounds = {
        footstep = {name="default_stone_footstep", gain=0.5},
        dug = {name="default_break_stone", gain=0.8},
    },
})
```

### Entity Definitions (Lua Entities)
Example:
```lua
core.register_entity("mymod:bouncy_ball", {
    initial_properties = {
        physical = true,
        collisionbox = {-0.3, -0.3, -0.3, 0.3, 0.3, 0.3},
        visual = "sprite",
        textures = {"mymod_bouncy_ball.png"},
    },
    on_step = function(self, dtime, moveresult)
        if moveresult.touching_ground then
            local vel = self.object:get_velocity()
            vel.y = 5 -- Bounce
            self.object:set_velocity(vel)
        end
    end,
})
```
For comprehensive API details, see `doc/lua_api.md`.

## Data Flow and Storage

Luanti manages various types of data, including the game world itself, player information, and data specific to mods. This data flows between the client and server and is persisted using several storage mechanisms.

[Diagram: Data Storage. Shows Server connected to Database (Map, Player, Auth, Mod Storage) and File System (world.mt, map_meta.txt, auth.txt, player files).]

### Data Flow Overview

1.  **Client Actions**: Player interactions (movement, digging, etc.) generate events sent to the server.
2.  **Server Processing**: The server validates actions, updates its authoritative game state, and executes Lua callbacks.
3.  **Data Retrieval/Storage**: The server interacts with storage backends:
    *   **World Data**: Map blocks are loaded/saved from/to the database.
    *   **Player Data**: Loaded on join, saved periodically/on leave.
    *   **Authentication & Mod Data**: Accessed as needed.
4.  **Server Updates to Clients**: Changes are sent to clients to synchronize their views.
5.  **Client Updates**: Clients update their local game world and UI.

### Storage Mechanisms

Configured in `world.mt`:
-   **SQLite3**: Default for map, player, and mod storage (`map.sqlite`).
-   **LevelDB, PostgreSQL, Redis**: Alternative database backends.
-   **Raw Files**: `auth.txt` (auth), player files (player data), `world.mt` (world settings), `map_meta.txt` (map seed), `env_meta.txt` (game time).

### Database Interfaces (`src/database/database.h`)
Abstract C++ classes for database operations:
-   `Database`: Base class (e.g., `beginSave()`, `endSave()`).
-   `MapDatabase`: For map blocks (`saveBlock()`, `loadBlock()`).
-   `PlayerDatabase`: For player data (`savePlayer()`, `loadPlayer()`).
-   `AuthDatabase`: For authentication (`getAuth()`, `saveAuth()`).
-   `ModStorageDatabase`: For mod-specific data (`getModEntry()`, `setModEntry()`).

### World Data Organization: MapBlocks
The world is divided into `MapBlock`s (16x16x16 nodes), the fundamental unit for storage and networking. The `MapBlock` class (`src/mapblock.h`) contains:
-   Node Data: Array of `MapNode` (type, `param1`, `param2`).
-   Position: 3D coordinates of the block.
-   Flags: `m_generated`, `is_underground`, `m_lighting_complete`.
-   Metadata: `NodeMetadataList` for inventories, sign text, etc.
-   Timers, Static Objects, Timestamps.

### World Format on Disk (`doc/world_format.md`)
A world directory contains:
-   `world.mt`: World settings, enabled mods, backend choices.
-   Map Data: e.g., `map.sqlite` for map blocks.
-   Player Data: e.g., `players/<name>` or in the database.
-   Authentication Data: e.g., `auth.txt` or in the database.
-   `map_meta.txt` (map seed), `env_meta.txt` (game time).

### MapBlock Serialization
`MapBlock`s are serialized for disk storage and network transfer. Key components:
-   Version byte, flags byte, lighting completion mask.
-   Timestamp and Name-ID mappings (for newer formats).
-   Node data arrays (`param0`, `param1`, `param2`), compressed (zlib or zstd).
-   Serialized node metadata list, node timers, and static objects.
Refer to `doc/world_format.md` for the full binary structure.

## Function Invocation Flow: Player Movement

Player movement involves client-side input capture, server-side processing and validation, and synchronization with other clients.

[Diagram: Player Movement Flow. Client Input -> Network (TOSERVER_PLAYERPOS) -> Server (ProcessInput, Physics, Collision) -> Network (TOCLIENT_ACTIVE_OBJECT_MESSAGES) -> Other Clients (Update Remote Player).]

1.  **Client: Input Detection (`MyEventReceiver::OnEvent` in `src/client/inputhandler.cpp`)**
    *   Keyboard/mouse/joystick events are captured.
    *   Mapped to game actions (e.g., `KeyType::FORWARD`).
    *   Input state is updated (e.g., `keyIsDown`).
2.  **Client: Processing Input (`Game::updatePlayerControl` in `src/client/game.cpp`)**
    *   A `PlayerControl` object is created with current movement keys and look direction.
    *   This is passed to `Client::setPlayerControl()`.
3.  **Client: Sending to Server (`TOSERVER_PLAYERPOS`)**
    *   Client periodically sends its position, speed, look direction, and pressed keys via `TOSERVER_PLAYERPOS`.
4.  **Server: Receiving and Processing (`Server::handleCommand_PlayerPos` in `src/server.cpp`)**
    *   Server deserializes the packet.
    *   Updates the player's `PlayerSAO` (Server Active Object) with new position, look direction, and key states.
    *   Server performs physics simulation and collision detection in `ServerActiveObject::step()`.
5.  **Server: Updating Other Clients (`TOCLIENT_ACTIVE_OBJECT_MESSAGES`)**
    *   Server sends updates for the moved player to other relevant clients. This includes new position, orientation, and animation state.
6.  **Client: Receiving Updates**
    *   Other clients receive these messages and update their local representation of the moving player.
7.  **Lua Callbacks**
    *   `on_step(self, dtime, moveresult)`: Called on the server for entities (including players) each server tick. `moveresult` provides collision info.
    *   `ObjectRef:set_pos(pos)`, `ObjectRef:set_physics_override(overrides)`: Lua API functions mods can use to directly influence player position and physics on the server.

## Utilities and Libraries

Luanti integrates several third-party open-source libraries (typically in `lib/`):

-   **Irrlicht Engine**: Core 3D graphics rendering.
-   **Lua/LuaJIT**: Scripting language for mods and game logic.
-   **SQLite3**: Default database backend.
-   **LevelDB, PostgreSQL, Redis**: Alternative database backends.
-   **zlib & zstd**: Data compression.
-   **libjpeg & libpng**: Image format handling.
-   **OpenAL Soft**: 3D audio.
-   **libogg & libvorbis**: Ogg Vorbis audio format.
-   **GMP (mini-gmp)**: Large number arithmetic.
-   **JsonCpp**: JSON parsing/generation.
-   **libcurl**: HTTP/HTTPS requests (ContentDB, server list).
-   **Gettext**: Internationalization.
-   **OpenSSL (or compatible)**: Cryptographic hashing.
-   **SDL2**: Low-level hardware access (windowing, input).
-   **Freetype**: Font rendering.
-   **catch2**: C++ unit testing framework (internal).
-   **bitop (Lua BitOp)**: Bitwise operations for Lua (if not using LuaJIT).
-   **tiniergltf**: glTF 2.0 model loading.

## Mods

Mods are Lua scripts and assets that extend Luanti's functionality.

### Creating Mods
-   **Directory Structure**: `modname/` (root) with `init.lua`, `mod.conf`, and subfolders like `textures/`, `sounds/`, `models/`, `locale/`.
-   **`mod.conf`**: Metadata file (name, title, description, dependencies).
    ```
    name = my_mod
    description = An example mod.
    depends = default
    ```
-   **`init.lua`**: Main script for registering nodes, items, entities, crafts, and callbacks using the `core.*` API.
-   **Media**: Assets like textures (`.png`), sounds (`.ogg`), models (`.gltf`, `.obj`, etc.) go into respective folders, usually prefixed with `modname_`.

### Installing and Enabling Mods
-   **Load Paths**: `games/<gameid>/mods/`, `mods/` (user dir), `worlds/<worldname>/worldmods/`.
-   **Enabling**: Via the main menu's "Content" tab (ContentDB integration) or by adding `load_mod_my_mod_name = true` to a world's `world.mt` file.

### Modding API Overview (Summary)
The server-side Lua API (`core.*`) allows:
-   Registering game elements (nodes, items, entities, crafts).
-   Handling game events via callbacks (player actions, server ticks).
-   Interacting with the world and game objects.
-   Creating custom UIs (formspecs).
-   Managing inventories and metadata.
(Refer to the [Lua Scripting API](#lua-scripting-api) section or `doc/lua_api.md` for full details).

### Naming Conventions
-   Prefix registered items with `modname:` (e.g., `my_mod:my_item`).

### Dependency Management
-   Use `depends` and `optional_depends` in `mod.conf`.

### Modpacks
-   Group related mods into a directory containing a `modpack.conf` file.

## Contributing

Luanti is an open-source project and welcomes community contributions.

-   **Ways to Contribute**: Code (C++ engine, Lua mods/games), issue reporting, feature requests, translations, documentation, testing, donations.
-   **Getting Started with Code**:
    1.  Discuss significant changes with core developers first (e.g., via GitHub issues).
    2.  Familiarize yourself with coding style guidelines (see `.github/CONTRIBUTING.md`).
    3.  Review the project roadmap (`doc/direction.md`).
-   **Detailed Guidelines**: For comprehensive information on coding standards, commit message formats, and the pull request process, please refer to the **`.github/CONTRIBUTING.md`** file in the Luanti repository.

Community involvement is key to Luanti's development.
