# Docker Server

We provide Luanti server Docker images using the GitHub container registry.

Images are built on each commit and available using the following tag scheme:

* `ghcr.io/luanti-org/luanti:master` (latest build)
* `ghcr.io/luanti-org/luanti:<tag>` (specific Git tag)
* `ghcr.io/luanti-org/luanti:latest` (latest Git tag, which is the stable release)

See [here](https://github.com/luanti-org/luanti/pkgs/container/luanti) for all available tags.

Versions released before the project was renamed are available with the same tag scheme at `ghcr.io/minetest/minetest`.
See [here](https://github.com/orgs/minetest/packages/container/package/minetest) for all available tags.

## Quick Start - Complete Working Example

```shell
# Create directories
mkdir -p ~/luanti/{data,conf}

# Download proper configuration file (REQUIRED)
wget https://raw.githubusercontent.com/luanti-org/luanti/master/minetest.conf.example -O ~/luanti/conf/minetest.conf

# Download and extract a game (if you don't have one already)
mkdir -p ~/luanti/games
wget https://content.luanti.org/packages/Wuzzy/mineclone2/releases/30536/download/ -O ~/luanti/mineclone2.zip
unzip ~/luanti/mineclone2.zip -d ~/luanti/games/

sudo chown -R 30000:30000 ~/luanti/{data,conf,games}

# Run the server
docker run -d \
  -v ~/luanti/data:/var/lib/minetest \
  -v ~/luanti/conf:/etc/minetest \
  -v ~/luanti/games/mineclone2:/usr/local/share/luanti/games/mineclone2 \
  -p 30000:30000/udp \
  -p 30000:30000/tcp \
  --restart always \
  --name luanti_server \
  ghcr.io/luanti-org/luanti:master \
  --gameid mineclone2 --port 30000
```

## Important Directory Paths

The Docker image expects specific paths:

- `/var/lib/minetest/` - Data directory for worlds and player info
- `/etc/minetest/` - Configuration directory for `minetest.conf`
- `/usr/local/share/luanti/games/` - **Game directory** (not included in image)

## Game Installation

⚠️ The container does NOT include any games by default. You must:

1. Download a game (e.g., from [ContentDB](https://content.luanti.org/))
2. Mount the game to `/usr/local/share/luanti/games/<game_name>`
3. Use `--gameid <game_name>` parameter to select the game

The game directory must contain a valid `game.conf` file at its root.

## Docker Compose Setup

```yaml
version: "2"
services:
  luanti_server:
    image: ghcr.io/luanti-org/luanti:master
    restart: always
    volumes:
      - ~/luanti/data:/var/lib/minetest
      - ~/luanti/conf:/etc/minetest
      - ~/luanti/games/mineclone2:/usr/local/share/luanti/games/mineclone2
    ports:
      - "30000:30000/udp"
      - "30000:30000/tcp"
    command: --gameid mineclone2 --port 30000
```

## Server Configuration

⚠️ **CRITICAL**: The server requires a valid configuration file to run properly.

Download the example configuration file and optionally customize it:

```shell
# Download the example config file
wget https://raw.githubusercontent.com/luanti-org/luanti/master/minetest.conf.example -O ~/luanti/conf/minetest.conf
```

If no config file is present, the server will crash on startup.

## Permissions and Technical Details

- The container runs as user `minetest` with UID 30000
- Fix permission issues with: `sudo chown -R 30000:30000 ~/luanti/{data,conf,games}`
- Entrypoint is `/usr/local/bin/luantiserver`
- Default command arguments: `--config /etc/minetest/minetest.conf`

#### "Game not found" Error

If you get an error like `ERROR[Main]: Game "mineclone2" not found`, check:

1. The game directory is mounted to `/usr/local/share/luanti/games/<game_name>`
2. The `--gameid` parameter matches the game directory name exactly
3. The game directory contains a valid `game.conf` file

#### Server Crashes on Startup

If the server crashes immediately, it's often due to a missing or invalid configuration file. Always ensure:

1. You've downloaded the example config file: `minetest.conf.example`
2. You've mounted it correctly to `/etc/minetest/minetest.conf`
3. The file has appropriate permissions

**Note:** If you don't understand the previous commands please read the [official Docker documentation](https://docs.docker.com) before use.
