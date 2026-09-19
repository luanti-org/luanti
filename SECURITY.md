# Security Policy

## Supported Versions

We only support the latest stable version for security issues.
Refer to the [releases page](https://github.com/luanti-org/luanti/releases).

When a security issue is discovered, the Luanti team may choose to include the
fix in the next planned stable release *or* release a patch version as soon as possible.
Security advisories will be made public as soon as the first release with a fix is out.

## Scope

Our primary concerns for the security of Luanti are:
* the Lua sandbox must prevent any mods or games from accessing the host system
  in unintended ways (arbitrary code execution, full filesystem access)
* in a multiplayer scenario any involved parties (client, server, other clients)
  must not be able to violate the Confidentiality, Integrity or Availability of others
  * e.g. if a client can crash the server or the server can take over the client (RCE)

Typical issues that would *only* be considered regular bugs:
* the user can configure Luanti so that it crashes/stops functioning
* the network protocol is unencrypted
* the client can fake data to attain in-game advantages (movement cheat)

If in doubt, please report a potential issue privately so we can triage it.

## Reporting a Vulnerability

We ask that you report vulnerabilities privately first:

* [Report a vulnerability on GitHub](https://github.com/luanti-org/luanti/security/advisories/new)
* or email [security@luanti.org](mailto:security@luanti.org)

Depending on severity, we will either create a private issue for the vulnerability
or give you permission to file the issue publicly.
For more information on the justification of this policy, see
[Responsible Disclosure](https://en.wikipedia.org/wiki/Responsible_disclosure).

Please be considerate of our volunteer maintainers and make sure any security reports
are verifiably real and reproducible (especially if created with the assistance of AI).

Reporters will be credited in the advisory, but we are unable to provide any kind
of compensation (bounty).
