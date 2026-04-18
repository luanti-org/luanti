# MariaDB Database Backend Setup

Luanti supports MariaDB as a database backend for map blocks, player data,
authentication, and mod storage. This document explains how to set up
MariaDB for use with Luanti.

## Requirements

- MariaDB server 10.3 or later
- MariaDB C connector development headers (for compiling Luanti)

## Installing MariaDB

### Fedora

    sudo dnf install mariadb-server mariadb-connector-c-devel

### Debian/Ubuntu

    sudo apt install mariadb-server libmariadb-dev

### Arch Linux

    sudo pacman -S mariadb mariadb-libs

### openSUSE

    sudo zypper install mariadb mariadb-connector-c-devel

After installing, start and enable the MariaDB service:

    sudo systemctl enable --now mariadb

## Database Setup

### Connecting to MariaDB as administrator

The setup steps require running SQL statements as a MariaDB administrator.
On most Linux distributions, the default setup uses unix socket
authentication for the root user:

    sudo mysql

If your server uses password authentication instead:

    mysql -u root -p

### SQL setup

Once connected to the MariaDB prompt, run:

    CREATE USER 'luanti'@'127.0.0.1' IDENTIFIED BY 'changeme';
    CREATE DATABASE luanti CHARACTER SET = 'utf8mb4' COLLATE = 'utf8mb4_unicode_ci';
    GRANT ALL ON luanti.* TO 'luanti'@'127.0.0.1';

Replace `changeme` with a strong password. If Luanti connects from a
different host than the database server, replace `'localhost'` with the
appropriate host or `'%'` for any host.

## Compiling Luanti with MariaDB Support

MariaDB support is enabled by default when the development headers are
installed. The build system uses pkg-config to find `libmariadb`.

    cmake . -DRUN_IN_PLACE=TRUE
    make -j$(nproc)

To explicitly disable MariaDB support:

    cmake . -DENABLE_MARIADB=OFF

## World Configuration

Edit your world's `world.mt` file to use MariaDB for one or more backends.
Each backend can use a different connection or they can all share the same one.

    backend = mariadb
    player_backend = mariadb
    auth_backend = mariadb
    mod_storage_backend = mariadb

    mariadb_connection = host=127.0.0.1 port=3306 user=luanti password=changeme dbname=luanti
    mariadb_player_connection = host=127.0.0.1 port=3306 user=luanti password=changeme dbname=luanti
    mariadb_auth_connection = host=127.0.0.1 port=3306 user=luanti password=changeme dbname=luanti
    mariadb_mod_storage_connection = host=127.0.0.1 port=3306 user=luanti password=changeme dbname=luanti

The connection string parameters are:

| Parameter  | Required | Description                  | Example                          |
|------------|----------|------------------------------|----------------------------------|
| `host`     | yes      | Server address               | `127.0.0.1`                      |
| `port`     | yes      | Server port (0 for socket)   | `3306`                           |
| `user`     | yes      | Database user                | `luanti`                         |
| `password` | yes      | User password                | `changeme`                       |
| `dbname`   | yes      | Database name                | `luanti`                         |
| `socket`   | no       | Path to unix socket          | `/var/lib/mysql/mysql.sock`      |

Use `127.0.0.1` instead of `localhost` to avoid unnecessary DNS lookups.

Tables are created automatically on first use.

### Unix socket connections

To connect via a unix socket instead of TCP, add the `socket` parameter
and set `port` to `0`:

    mariadb_connection = host=localhost port=0 user=luanti password=changeme dbname=luanti socket=/var/lib/mysql/mysql.sock

The socket path varies by distribution:

| Distribution       | Default socket path                  |
|--------------------|--------------------------------------|
| Fedora / RHEL      | `/var/lib/mysql/mysql.sock`          |
| Debian / Ubuntu    | `/run/mysqld/mysqld.sock`            |
| Arch Linux         | `/run/mysqld/mysqld.sock`            |
| openSUSE           | `/var/run/mysql/mysql.sock`          |

You can find the socket path with:

    mariadb --print-defaults 2>/dev/null | grep -o 'socket=[^ ]*'

When using unix sockets, the MariaDB user should be created with
`'localhost'` as the host (not `'127.0.0.1'`):

    CREATE USER 'luanti'@'localhost' IDENTIFIED BY 'changeme';
    GRANT ALL ON luanti.* TO 'luanti'@'localhost';

## Migrating from Another Backend

First add the MariaDB connection strings to your world's `world.mt` (see
above), then run the migration commands. The `--server` flag is required:

    ./bin/luanti --server --migrate mariadb --world /path/to/world
    ./bin/luanti --server --migrate-players mariadb --world /path/to/world
    ./bin/luanti --server --migrate-auth mariadb --world /path/to/world
    ./bin/luanti --server --migrate-mod-storage mariadb --world /path/to/world

Each command migrates one backend and automatically updates `world.mt`.

## Multiple Worlds

Each world can use its own database, or multiple worlds can share one
MariaDB server by using separate databases:

    mysql -sse "CREATE DATABASE luanti_survival CHARACTER SET = 'utf8mb4' COLLATE = 'utf8mb4_unicode_ci';"
    mysql -sse "CREATE DATABASE luanti_creative CHARACTER SET = 'utf8mb4' COLLATE = 'utf8mb4_unicode_ci';"
    mysql -sse "GRANT ALL ON luanti_survival.* TO 'luanti'@'%';"
    mysql -sse "GRANT ALL ON luanti_creative.* TO 'luanti'@'%';"

Then configure each world's `world.mt` with the appropriate `dbname`.

## Running Unit Tests

To run the MariaDB-specific unit tests, set the connection string in an
environment variable:

    LUANTI_MARIADB_CONNECT_STRING="host=127.0.0.1 port=3306 user=luanti password=changeme dbname=luanti_test" ./bin/luanti --run-unittests

Use a dedicated test database to avoid interfering with world data.
