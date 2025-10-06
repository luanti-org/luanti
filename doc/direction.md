# Luanti Direction Document

## 1. Long-term Roadmap

The long-term roadmaps, aims, and guiding philosophies are set out using the
following documents:

* [What is Minetest? (archived)](https://web.archive.org/web/20160328054721/http://c55.me/blog/?p=1491)
* [celeron55's roadmap](https://forum.luanti.org/viewtopic.php?t=9177)
* [celeron55's comment in "A clear mission statement for Minetest is missing"](https://github.com/luanti-org/luanti/issues/3476#issuecomment-167399287)
* [Core developer to-do/wish lists](https://forum.luanti.org/viewforum.php?f=7)

## 2. Medium-term Roadmap

These are the current medium-term goals for Luanti development, in no
particular order.

These goals were created from the top points in a
[roadmap brainstorm](https://github.com/luanti-org/luanti/issues/16162).
This is reviewed approximately every two years.

Pull requests that address one of these goals will be labeled as "Roadmap".
PRs that are not on the roadmap will be closed unless they receive a concept
approval within a month. Issues can be used for preapproval.
Bug fixes are exempt from this, and are always accepted and prioritized.
See [CONTRIBUTING.md](../.github/CONTRIBUTING.md) for more info.

### 2.1 SSCSM

Server-Sent Client-Side Modding has been a long requested feature, as it
allows game developers to highly expand the tools for their creations.
Implementing SSCSM also means to feature more dehardcoding, furthermore
allowing us to improve the overall performances of the engine.

Instead of focusing on short-term solutions that will inevitably lead to more
technical debt to deal with, SSCSM paves the way for a cleaner architecture
designed to stay.

### 2.2 Input Handling

Luanti keys are currently limited to a small subset, not allowing game developers
to map the majority of the keys a device usually offers. This limits the possibilities
of game creators, forcing them to either implement a workaround or, worse, forget
about it.

Using a gamepad also represents a known issue in Luanti, as some devices might not
work at all or result in an uncomfortable user experience.

### 2.3 UI Improvements

A [formspec replacement](https://github.com/luanti-org/luanti/issues/6527) is
needed to make GUIs better and easier to create. This replacement could also
be a replacement for HUDs, allowing for a unified API.

A [new mainmenu](https://github.com/luanti-org/luanti/issues/6733) is needed to
improve user experience. First impressions matter, and the current main menu
doesn't do a very good job at selling Luanti or explaining what it is.

The UI code is undergoing rapid changes, so it is especially important to make
an issue for any large changes before spending lots of time.
