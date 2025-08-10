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
[roadmap brainstorm](https://github.com/luanti-org/luanti/issues/10461).
This should be reviewed approximately yearly, or when goals are achieved.

Pull requests that address one of these goals will be labeled as "Roadmap".
PRs that are not on the roadmap will be closed unless they receive a concept
approval within a month, issues can be used for preapproval.
Bug fixes are exempt for this, and are always accepted and prioritized.
See [CONTRIBUTING.md](../.github/CONTRIBUTING.md) for more info.

### 2.1 Rendering/Graphics improvements

The priority is fixing the issues, performance, and general correctness.
Once that is done, fancier features can be worked on, such as water shaders,
shadows, and improved lighting.

Examples include
[transparency sorting](https://github.com/luanti-org/luanti/issues/95),
[particle performance](https://github.com/luanti-org/luanti/issues/1414),
[general view distance](https://github.com/luanti-org/luanti/issues/7222).

This includes work on maintaining
[our Irrlicht fork](https://github.com/luanti-org/luanti/tree/master/irr), and switching to
alternative libraries to replace Irrlicht functionality as needed

### 2.2 Internal code refactoring

To ensure sustainable development, Luanti's code needs to be
[refactored and improved](https://github.com/luanti-org/luanti/pulls?q=is%3Aopen+sort%3Aupdated-desc+label%3A%22Code+quality%22).
This will remove code rot and allow for more efficient development.

### 2.3 UI Improvements

A [formspec replacement](https://github.com/luanti-org/luanti/issues/6527) is
needed to make GUIs better and easier to create. This replacement could also
be a replacement for HUDs, allowing for a unified API.

A [new mainmenu](https://github.com/luanti-org/luanti/issues/6733) is needed to
improve user experience. First impressions matter, and the current main menu
doesn't do a very good job at selling Luanti or explaining what it is.
A new main menu should promote games to users, allowing Minetest Game to no
longer be bundled by default.

The UI code is undergoing rapid changes, so it is especially important to make
an issue for any large changes before spending lots of time.

### 2.4 Object and entity improvements

There are still a significant number of issues with objects.
Collisions,
[performance](https://github.com/luanti-org/luanti/issues/6453),
API convenience, and discrepancies between players and entities.
