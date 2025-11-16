# Profiling

## Using builtin's mod profiler

Builtin has a profiler for callbacks registered by mods. It measures how much
time each callback took per server-step (on average / min / max).

To start, enable the `profiler.load` setting.
(There are more settings, see `profiler.*` and `instrument.*`.)

Use the `profiler` chatcommand to look at the results.


## Profiling Luanti on Linux with perf

We will be using a tool called "perf", which you can get by installing `perf` or `linux-perf` or `linux-tools-common`.

To get usable results you need to build Luanti with debug symbols
(`-DCMAKE_BUILD_TYPE=RelWithDebInfo` or `-DCMAKE_BUILD_TYPE=Debug`).

Run the client (or server) like this and do whatever you wanted to test:
```bash
perf record -z --call-graph dwarf -- ./bin/luanti
```

This will leave a file called "perf.data".

You can open this file with perf built-in tools but much more interesting
is the visualization using a GUI tool: **[Hotspot](https://github.com/KDAB/hotspot)**.
It will give you flamegraphs, per-thread, per-function views and much more.

### Remote Profiling

Attach perf to your running server, press *^C* to stop:
```bash
perf record -z --call-graph dwarf -F 400 -p "$(pidof luantiserver)"
```

Collect a copy of the required libraries/executables:
```bash
perf buildid-list | grep -Eo '/[^ ]+(luantiserver|\.so)[^ ]*$' | \
	tar -cvahf debug.tgz --files-from=- --ignore-failed-read
```

Give both files to the developer and also provide:
* Linux distribution and version
* commit the source was built from and/or modified source code (if applicable)

Hotspot will resolve symbols correctly when pointing the sysroot option at the collected libs.


## Profiling with Tracy

[Tracy](https://github.com/wolfpld/tracy) is
> A real time, nanosecond resolution, remote telemetry, hybrid frame and sampling
> profiler for games and other applications.

It allows one to annotate important functions and generate traces, where one can
see when each individual function call happened, and how long it took.

Tracy can also record when frames, e.g. server step, start and end, and inspect
frames that took longer than usual. Luanti already contains annotations for
its frames.

See also [Tracy's official documentation](https://github.com/wolfpld/tracy/releases/latest/download/tracy.pdf).

### Installing

Tracy consists of a client (Luanti) and a server (the gui).

Install the server, e.g. using your package manager.

### Building

Build Luanti with `-DDBUILD_WITH_TRACY=1`, this will fetch Tracy for building
the Tracy client. And use `FETCH_TRACY_GIT_TAG` to get a version matching your
Tracy server, e.g. `-DFETCH_TRACY_GIT_TAG=v0.11.0` if it's `0.11.0`.

To actually use Tracy, you also have to enable it with Tracy's build options
(`-DTRACY_ENABLE=1`).

See Tracy's documentation for more build options.

TL;DR:
```
-DDBUILD_WITH_TRACY=1 -DFETCH_TRACY_GIT_TAG=<your_version> -DTRACY_ENABLE=1 -DTRACY_ONLY_LOCALHOST=1
```

### Using in C++

Start the Tracy server and Luanti. You should see Luanti in the menu.

To actually get useful traces, you have to annotate functions with `ZoneScoped`
macros and recompile. Please refer to Tracy's official documentation.

### Using in Lua

Tracy also supports Lua.
If built with Tracy, Luanti loads its API in the global `tracy` table.
See Tracy's official documentation for more information.

Note: The whole Tracy Lua API is accessible to all mods. And we don't check if it
is or becomes insecure. Run untrusted mods at your own risk.

Enable `profiler.load` and `profiler.tracy` to automatically instrument mod
callback functions.
