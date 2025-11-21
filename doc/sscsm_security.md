# SSCSM security


## Threat model

* SSCSM scripts come from the server (potential malicious actor). We are the client.
* Authenticity of server is not given (Luanti's networking is not secure). So we have
  to expect anyone who can send us UDP packets to the appropriate IP address to be
  able to act on behalf of the server.
* The server may not tamper with, or get access to information of, anything besides
  the stuff explicitly made accessible via the modding API (i.e. gameplay relevant
  stuff, like map, node definitions, ...).
  In particular, this excludes for (non-exhaustive) example files, file paths,
  and settings.
* DOS is not an issue (as it is already easily possible to DOS a client).
  (It's also low risk (uninteresting target, and no catastrophic impact).)
* There already is an API via network packets (see `networkprotocol.h`).
  This acts as upper bound: Every SSCSM API function could instead be a network
  packet endpoint. There are no efforts to make SSCSM more secure than this.


## Non-binary `enable_sscsm` setting

The `enable_sscsm` setting does not just allow en-/disabling SSCSM, it also allows
limiting on what sort of servers to enable SSCSM. Options are `nowhere`, `singleplayer`,
`localhost` (or singleplayer), `lan` (or lower), and everywhere.
On options `localhost` and lower, we know that (anyone who acts on the behalf of)
the server runs on the same machine, and the risk of it being malicious is pretty
much zero.

Until sufficient security measures are in place, users are disallowed to set this
setting to anything higher than `localhost`.


## Lua sandbox

* We execute only Lua scripts, in a Lua sandbox.
* See also `initializeSecuritySSCSM()`.
* We do not trust the Lua implementation to not have bugs. => Additional process
  isolation layer as fallback.


## Process isolation

* Not yet implemented.
* Separate SSCSM process.
* Sandboxing:
  * Linux: Uses SECCOMP.
  * ... (FIXME: write down stuff when you implement)


## Limit where we call into SSCSM

* Even if the Lua sandbox and/or the process isolation are bug-free, the main
  process client code can still be vulnerable. Consider this example:
  * Client has an inventorylist A.
  * User moves an item.
  * SSCSM gets called (callback when item is moved).
  * SSCSM can do anything now. It decides to delete A, then returns.
  * Client still has reference to A on stack, tries to access it.
  * => Use-after-free.
* To avoid these sort of issues, we only give control-flow to SSCSM in few special
  places.
  In particular, this includes packet handlers, and the client's `step()` function.
* In these places, the client already does not assume anything about the current
  state (e.g. that an inventory exists).
* This makes sure that SSCSM API calls can also just happen in these places.
  In packet handlers, the server can already cause arbitrary network API "calls"
  to happen. Hence, new SSCSM API calls here do not lead to new vulnerabilities
  that a network API would not cause as well.


## No precise clocks

To mitigate time-based side-channel attacks, all available clock API functions
(`os.clock()` and `core.get_us_time()`) only have a precision of
`SSCSM_CLOCK_RESOLUTION_US` (20) us.
