UI API
======

The UI API is the unified replacement for the older formspec and player HUD
systems, exposing a new system that is simpler, more robust and powerful, while
additionally being less buggy and quirky than its predecessors. It is not yet
stable, and feedback is encouraged.

**Warning**: The UI API is entirely experimental and may only be used for
testing purposes, not for stable mods. The API can and will change without
warning between versions until it is feature complete and stabilized, including
the network protocol.

To use the UI API, Luanti must be compiled with the `BUILD_UI` CMake option
turned on. The UI additionally requires SDL2 support, so `USE_SDL2` must also
be set. If Luanti is built without `BUILD_UI`, the `ui` namespace will still
exist in Lua, but the client will not have any C++ UI functionality and all UI
network packets will be silently ignored.

> This documentation will sometimes refer to features that are not implemented
> in the current version of the UI API, such as scrollbars and edit fields.
> These are included in the documentation since they are particularly useful
> for explaining some of the features of the UI API that can't be explained as
> precisely otherwise. When this occurs, a block like this precedes the example
> to note the discrepancy between the documentation and current implementation.

API design
----------

The UI API is exposed to Lua through the global `ui` namespace of functions and
classes. Most of these classes are opaque and effectively immutable, meaning
they have no user-visible properties or methods. Users must not access or
modify undocumented properties or inherit from any UI class.

All tables passed into UI API functions are defensively copied by the API.
Modifying a table after passing it in to a function will not change the
constructed object in any way.

### Example

Here is a simple example of a working UI made with the UI API:

> Since the UI API has no core theme bundled by default at this point (aside
> from the prelude theme), a few extra style properties are required to make
> anything visible.

```lua
local function builder(context, player, state, param)
    return ui.Window "gui" {
        root = ui.Root {
            size = {108, 76},
            padding = {4},

            box_fill = "black#8C",

            ui.Label {
                pos = {0, 0}, span = {1, 1/2},

                label = "Hello, world!",
            },

            ui.Button "close" {
                pos = {0, 1/2}, span = {1, 1/2},
                box_fill = "maroon",

                label = "Close",

                on_press = function(ev)
                    context:close()
                    minetest.chat_send_player(player, "The window has been closed")
                end,
            },
        },
    }
end

core.register_on_joinplayer(function(player)
    ui.Context(builder, player:get_player_name()):open()
end)
```

### ID strings

Elements require unique IDs, which are represented as strings. ID strings may
only contain letters, numbers, dashes, underscores, and colons, and may not be
the empty string.

All IDs starting with a dash or underscore are reserved for use by the engine,
and should not be used unless otherwise specified. IDs should not use a colon
except to include a `mod_name:` prefix.

Element and group IDs are local to a single window, so the `mod_name:` prefix
used elsewhere in Luanti is generally unnecessary for them. However, if a
library mod creates themes, elements, or styles, then using a mod prefix for
the library's element and group IDs is highly encouraged to avoid ID conflicts.

Derived element type names are placed in a global namespace, so mods should
always use a `mod_name:` prefix for mod-created derived elements. Only the
engine is allowed to make elements with unprefixed type names like `switch`.

### Constructors

The UI API makes heavy use of tables, currying, and Lua's syntactic sugar for
function calls to provide a convenient DSL-like syntax for creating UIs. For
instance, the curried function signature for elements is `ui.Elem(id)(props)`
or `ui.Elem(props)`, depending on whether the element is given an ID. To
illustrate how this is used, here are some examples for creating a label:

```lua
-- For elements, the first curried argument is the element ID and the second is
-- a property table defining the element:
ui.Label("my_label")({
    label = "My label text",
})

-- Using Lua's syntactic sugar, we can drop the function call parentheses for
-- string and table literals, which is the preferred convention for the UI API:
ui.Label "my_label" {
    label = "My label text",
}

-- If the ID string or property table is a variable or expression, parentheses
-- are still required around one or both arguments:
local id = "my_label"
ui.Label(id) {
    label = "My label text",
}

-- If the ID is not necessary, it can be omitted altogether:
ui.Label {
    label = "My label text",
}
```

The constructors for `ui.Window` and `ui.Style` use a similar curried function
signature.

To further increase the convenience of element and style constructors, certain
properties may be "inlined" into the constructor table rather than specified as
an explicit table. For example, the list of children for an element can be
specified explicitly in the `children` property, or it can be put directly in
the constructor table if the `children` property is omitted:

```lua
-- The `children` property explicitly specifies the list of children.
ui.Group {
    children = {
        ui.Label {label="Child 1"},
        ui.Label {label="Child 2"},
    },
}

-- The `children` property is omitted, so the list of children is taken from
-- constructor table instead.
ui.Group {
    ui.Label {label="Child 1"},
    ui.Label {label="Child 2"},
}
```

If the `children` property is explicitly specified, then elements placed
directly in the constructor table will be ignored. Other properties that may be
inlined follow similar rules.

Unless otherwise documented, it should be assumed that all fields in
constructor tables are optional.

Windows
-------

_Windows_ represent discrete self-contained UIs formed by a tree of elements
and other parameters that affecting the entire window. Windows are represented
by the `ui.Window` class.

### Root element

The window contains a single element, the _root element_, in which the entire
element tree is contained. Because the root element has no parent, it is
positioned in the entire screen. The root element must be of type `ui.Root`.

```lua
-- This creates a HUD-type window with the root element as its only element.
ui.Window "hud" {
    root = ui.Root {
        span = {1, 1},
        label = "Example HUD text",
    },
}
```

### Window types

Windows require a _window type_, which determines whether they can receive user
input, what type of scaling to apply to the pixels, and the Z order of how they
are drawn in relation to other things on the screen. These are the following:

* `filter`: Used for things that need to be drawn before everything else, such
  as vignettes or filters covering the outside world.
* `mask`: Used for visual effects in between the camera and wieldhand, such as
  masks or other such objects.
* `hud`: Used for normal HUD purposes. Hidden when the HUD is hidden.
* `chat`: Used to display GUI-like popup boxes or chat messages that can't be
  interacted with. Hidden when the chat is hidden.
* `gui`: Used for GUIs that the user can interact with.

If there are no formspecs open, then the topmost window (that is, the one that
was opened last) on the `gui` layer will receive user input from the keyboard,
mouse, and touchscreen. No other layer can receive user input. See the [Events
and input] section for more information on how `gui` windows handle user input.

The `gui` and `chat` window types scale all dimensions by `real_gui_scaling`
pixels in size from `core.get_player_window_information()`, whereas every other
window type scales them by `real_hud_scaling`.

The Z order for window types and other things displayed on the screen is:

* Luanti world
* `filter` window types
* Wieldhand and crosshairs
* `mask` window types
* Player HUD API elements
* `hud` window types
* Nametags
* `chat` window types
* `gui` window types
* Formspecs

If two or more windows of the same type are displayed at the same time, then
the windows that were opened more recently will be displayed on top of the
less recent ones.

### Styling

There are two properties in the window relevant to styling: `theme` and
`styles`. Both properties use `ui.Style` objects to select elements from the
entire element tree and apply styles to them.

The `theme` property is meant for using an external theme provided by a game or
mod that gives default styling to different elements. If one is not specified
explicitly, the current default theme from `ui.get_default_theme()` will be
used instead. See [Theming] for more information.

The `styles` property is intended for styling that is globally applied to a
single window. It takes higher precedence than the `theme` property, meaning
that properties set by any style within `styles` will override properties set
by `theme`. Any element styling that is specific to this particular window
should either reside in the `styles` property or in local element styles.

```lua
ui.Window "gui" {
    -- Set the theme for this window to a theme provided by the mod `my_mod`.
    -- If this line is removed, the default theme set by the game will be used.
    theme = my_mod.get_cool_theme(),

    -- This window also has its own particular styles, such as changing the
    -- text color for some labels. These override properties set by the theme.
    styles = {
        ui.Style "label%warning" {
            text_color = "yellow",
        },
        ui.Style "label%error" {
            text_color = "red",
        },
    },

    root = ui.Root {
        size = {100, 40},

        -- Aside from having any styling from the theme, this label will also
        -- have red text due to the window style. Local styles could also be
        -- added to the element itself to override any of these styles.
        ui.Label {
            groups = {"error"},
            label = "A problem has occurred",
        },
    },
}

ui.Window "gui" {
    theme = my_mod.get_cool_theme(),

    -- If the `styles` property is omitted, global styles can be inlined
    -- directly into the window for convenience.
    ui.Style "label%warning" {
        text_color = "yellow",
    },
    ui.Style "label%error" {
        text_color = "red",
    },

    root = ui.Root {},
}
```

Elements
--------

_Elements_ are the basic units of interface in the UI API and include such
things as sizers, buttons, and edit fields. Elements are represented by the
`ui.Elem` class and its subclasses.

### Element IDs

Each element in a window is required to have a unique _element ID_ that is
different from every other ID in that window. This ID uniquely identifies the
element for both network communication and styling. Elements that have user
interaction require an ID to be provided whereas static elements will
automatically generate an ID if none is provided.

```lua
-- Buttons are an example of an element that require an ID, since they are
-- dynamic and have state on the client.
ui.Button "my_button" {
    label = "My button",
}

-- Labels are fully static, so they don't require an ID.
ui.Label {
    label = "My label",
}
```

Each element's [Type info] section lists whether IDs must be provided. Elements
that are not given an ID will automatically generate one with `ui.new_id()`.

```lua
-- Both of these elements will throw an error because buttons need unique IDs
-- that have not been automatically generated.
ui.Button {
    label = "Missing ID",
}

ui.Button(ui.new_id()) {
    label = "Auto-generated ID",
}
```

If the ID for a dynamic element changes when the UI is updated, this will
result in the loss of the element's persistent state, as detailed below.

### Styling

Each element has a specific _type name_ that is used when referring to the
element in a `SelectorSpec`. The type name of each element is listed in the
element's [Type info] section.

Elements can be styled according to their unique ID. Additionally, elements
also have a list of _group IDs_ that allow selectors to style multiple elements
at once. Group IDs are only used for styling.

```lua
ui.Window "gui" {
    style = ui.Style {
        -- Style all `ui.Button` elements to have a maroon background by
        -- styling the type name `button`.
        ui.Style "button" {
            box_fill = "maroon",
        },

        -- Style all elements with the group `yellow` to have yellow text.
        ui.Style "%yellow" {
            text_color = "yellow",
        },
    },

    root = ui.Root {
        size = {212, 76},

        scale = 1,
        padding = {4},

        box_fill = "black#8C",

        ui.Label {
            pos = {0, 0}, span = {100, 32},

            label = "No style",
        },
        ui.Label {
            pos = {104, 0}, span = {100, 32},

            groups = {"yellow"},
            label = "Yellow text",
        },
        ui.Button "a" {
            pos = {0, 36}, span = {100, 32},

            label = "Maroon background",
        },
        ui.Button "b" {
            pos = {104, 36}, span = {100, 32},

            groups = {"yellow"},
            label = "Yellow text on maroon background",
        },
    },
}
```

Aside from the global styles found in the window, each element may have local
styles of its own that only apply to itself. Effectively, these styles are the
same as appending nested styles to the window's list of global styles with a
selector that only selects the element's ID; however, a local style is often
more convenient. Local styles have higher precedence than styles specified in
the window.

```lua
-- This button uses local styles to set the button's background color to red
-- and also to make the text yellow when the box is hovered.
ui.Button "local" {
    label = "Hovered yellow",

    styles = {
        ui.Style {
            box_fill = "red",
        },
        ui.Style "$hovered" {
            text_color = "yellow",
        },
    },
}

-- This button inlines the styles into the element itself. Since the first
-- style has no selector, its properties may be inlined into the constructor
-- directly without being wrapped in a `ui.Style` with no selector.
ui.Button "inline" {
    label = "Hovered yellow",

    box_fill = "red",

    ui.Style "$hovered" {
        text_color = "yellow",
    },
}
```

### Child elements

Each element has a list of _child elements_. Child elements are positioned
inside their parent element, and are thus subject to any special positioning
rules that a specific element has, such as in the case of scrolled elements.

```lua
-- This group element has two buttons as children, positioned side-by-side.
ui.Group {
    ui.Button "left" {
        pos = {0, 0}, span = {1/2, 1},
        label = "Left",
    },
    ui.Button "right" {
        pos = {1/2, 0}, span = {1/2, 1},
        label = "Right",
    },
}

-- Alternatively, we can use the `children` property to include an explicit
-- list of children rather than inlining them into the constructor.
ui.Group {
    children = {
        ui.Button "left" {
            pos = {0, 0}, span = {1/2, 1},
            label = "Left",
        },
        ui.Button "right" {
            pos = {1/2, 0}, span = {1/2, 1},
            label = "Right",
        },
    },
}

-- Both child elements and local styles may be inlined into the constructor
-- table at the same time and mixed freely.
ui.Group {
    ui.Style {
        box_fill = "red",
    },

    ui.Button "fill" {
        label = "Button",
    },
}
```

The order in which elements are drawn is the parent element first, followed by
the first child and its descendants, then the second child, and so on, i.e.
drawing takes a pre-order search path.

Each `ui.Elem` object can only be used once. After being set as the child
element of some other element or set as the root of a window, it cannot be
reused, either in the same window or in another window.

### Persistent fields

Certain elements and the window have properties that can be modified by the
user, such as checkboxes or edit fields. However, it must also be possible for
the server to modify them. Since there may be substantial latency between
client and server, it is undesirable for the server to update every
user-modifiable field every time the window is updated, as is the case with
formspecs, since that may overwrite the user's input.

These fields that contain user input are called _persistent fields_. Normal
fields use a default value if the server omits the field. Persistent fields, on
the other hand, keep their previous value if the server omits the field.

For example, suppose there is a window with a checkbox labelled `Check me`
which the user has checked. Then, the server updates the window, omitting both
the `label` and `selected` properties. Since `label` is a normal field, the
checkbox's label will become empty. However, `selected` is a persistent field,
so the checkbox will remain checked. If the server had explicitly set the
`selected` property to false, the checkbox would become unchecked.

Changing persistent fields often has side effects. For instance, the UI API
doesn't support edit fields yet, but setting the `text` property on an edit
field would cause the caret to move to the end of the text. Similarly, the user
may have changed the state of a checkbox before a `ui.Context:update()` reached
the client, so always setting the `selected` property on that checkbox could
overwrite the user's input. As such, it is highly recommended to leave the
value for persistent fields at `nil` unless the server explicitly needs to
change the value.

Note that omitted persistent fields are set to a default value when the element
is first created, such as when the window is opened or reopened. The `selected`
property, for instance, will be false when the window is first opened unless
the server gives it a value.

### Derived elements

Often, there are different types of elements that work the same way, but are
styled in vastly different ways that make them look like entirely different
controls to the user. For instance, `ui.Check` and `ui.Switch` are simply
toggle buttons like `ui.Toggle`, but have their own conventional appearances.

Specialized appearances for these different controls can be made by using group
IDs, but it is often more convenient to have them act as different element
types entirely. Such elements are called _derived elements_, and can be created
using the `ui.derive_elem()` function.

Derived elements are a purely server-side construct for styling, and act
exactly like their normal counterpart on the client. Moreover, all the fields
that can be provided to the constructor of the original type can also be
provided to the derived type.

As an example, if a mod wanted to create a new special kind of toggle switch,
it could create a `MyToggle`, which acts exactly like a `ui.Toggle` except for
the lack of default styling:

```lua
local MyToggle = ui.derive_elem(ui.Toggle, "my_mod:toggle")

local function builder(context, player, state, param)
    return ui.Window "gui" {
        style = ui.Style {
            -- We style our specific toggle with a basic blue color.
            ui.Style "my_mod:toggle" {
                box_fill = "navy",
            },
            ui.Style "my_mod:toggle$selected" {
                box_fill = "blue",
            },

            -- Standard toggles are styled entirely independently of elements
            -- derived from them.
            ui.Style "toggle" {
                box_fill = "olive",
            },
            ui.Style "toggle$selected" {
                box_fill = "yellow",
            },
        },

        root = ui.Root {
            size = {108, 76},
            padding = {4},

            box_fill = "black#8C",

            MyToggle "my" {
                pos = {0, 0}, span = {1, 1/2},
                label = "My toggle",
                selected = true,
            },
            ui.Toggle "ui" {
                pos = {0, 1/2}, span = {1, 1/2},
                label = "Standard toggle",
            },
        },
    }
end

core.register_on_joinplayer(function(player)
    ui.Context(builder, player:get_player_name()):open()
end)
```

All standard derived elements can be found in the [Derived elements] section of
their respective element's documentation.

Boxes
-----

Elements handle state and behavior, but the elements themselves are invisible
and can't be styled on their own. Instead, elements contain _boxes_, which
are rectangular regions inside each element that can be styled and denote the
visible bounds of each part of the element.

Boxes can be styled with any of the style properties listed in `StyleSpec`,
such as box images or padding. Boxes also contain certain types of state
information relevant to styling, such as whether the mouse was pressed down
within the box's boundaries.

### Box positioning

Much like elements, boxes are arranged in a tree where each box can have one or
more child boxes. Every element has a `main` box which serves as the ancestor
of every other box in the element. The only exception to this rule is
`ui.Root`, which has a `backdrop` box as the parent of the `main` box.

Each element type has a predefined set of boxes in a fixed hierarchy. For
instance, the box hierarchy for scrollbars looks like the following:

> **Note**: Scrollbars are not implemented yet, so this documentation only
> serves as a representative example of box hierarchies.

```
+-----+--------------+-------------+-------------------------------+-----+
| /__ |              |      =      |                               | __\ |
| \   |              |      =      |                               |   / |
+-----+--------------+-------------+-------------------------------+-----+
^^^^^^^.            .^^^^^^^^^^^^^^^.                             .^^^^^^^
decrease            .     thumb     .                             increase
.      .            .               .                             .      .
.      ^^^^^^^^^^^^^^               ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^      .
.      .   before                                after            .      .
.      .                                                          .      .
.      ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^      .
.                                  track                                 .
.                                                                        .
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
                                   main
```

Specifically, the `main` box has the scrollbar `track` box and the `decrease`
and `increase` boxes as children. The `track` box, in turn, has the `thumb` box
as a child to indicate the value of the scrollbar, plus `before` and `after`
boxes on either side of the thumb that can be used to give a separate style to
each half of the scrolled region.

Boxes can have special positioning rules for their children. For scrollbars,
the `decrease`, `increase`, and `track` boxes have a bounding rectangle that
spans the entire `main` box. As such, they can be positioned freely, e.g. the
buttons could be moved to the left side of the scrollbar or even hidden
altogether. The bounding rectangle for the `thumb` box, however, depends on the
value of the scrollbar since the thumb moves as the value changes. Therefore,
`thumb` can only be positioned in a limited fashion within its bounding
rectangle. The situation is similar for `before` and `after`.

Boxes are also in charge of where the content of the element is placed. For
simple elements like `ui.Button`, the children of the element and the label are
both positioned within the `main` box. More complex elements often handle
things differently. For instance, the `ui.Caption` element uses separate boxes
for these, `caption` for the label and `content` for the child elements.

The list of boxes for each element is described in the element's [Type info]
section. The children and content of each box, plus any special behavior they
might have, is documented there.

### Input and interaction

Elements do not react to mouse or touch input directly. Instead, boxes within
the element handle these types of input, reacting to various situations such as
when the box is pressed or the cursor is hovering over the box.

In the scrollbar example above, the `decrease` and `increase` boxes act like
buttons that can change the value of the scrollbar. The `thumb` is a box that
can be dragged by the pointer within the bounds of the track. These are known
as _dynamic_ boxes since they respond to mouse and touch input. On the other
hand, the `main` and `track` boxes are inert and ignore mouse and touch input
altogether. These are called _static_ boxes.

Static boxes are invisible to mouse or touch events and let them pass through
to the box directly underneath them, and hence styling the `hovered` or
`pressed` states never has any effect. Dynamic boxes, on the other hand,
respond to events, so they always respond to styling the `pressed`, `hovered`,
and `focused` states. Moreover, dynamic boxes generally do not let mouse or
touch events pass through them. In particular, only the topmost dynamic box
under the cursor will have the `hovered` state set.

To illustrate how events pass through different boxes, consider a button with
two children, a checkbox and a static label. If the mouse hovers over the
checkbox, which has a dynamic `main` box, the checkbox will have the `hovered`
state whereas the parent button will not. However, if the mouse hovers over the
label, which has a static `main` box, the event will pass through the label and
the parent button will have the `hovered` state instead.

Events and input
----------------

_Events_ are the means through which user input is communicated from the client
to the server. They range from detecting when a checkbox was pressed to
notifying when focus changed from one element to another.

Events come in two variants: _window events_ are events that are global to the
entire window, such as when the window has been closed or focus has changed.
_Element events_, on the other hand, fire when an event happens to a specific
element, such as a scrollbar being moved or a checkbox being toggled.

When a server receives an event from the client, it calls the appropriate
_event handler_. Event handlers are callback functions that can be provided to
the window or to elements as properties. The signature of every event handler
is `function(ev)`, where `ev` is an `EventSpec` containing information about
the event, such as the new value of the scrollbar that was changed. See the
`EventSpec` documentation for the generic fields supported by all events.

### Network latency

Beware of the effects of network latency on event handlers, since an event
coming from the previous state of the element may surface after the element has
been updated by the server. For instance, the user might click a button, and
the server disables a checkbox in response. However, the user clicked the
checkbox before the client received the server's update, causing the server to
receive a checkbox event after it disabled the checkbox.

The server could filter out these events, e.g. dropping all events for disabled
checkboxes, but this might lead to inconsistent state between the client and
server. In the previous example, the client would see the checkbox as checked,
but the server would still believe the checkbox was unchecked. This is a bad
situation, so the server will never drop events outright (except if the event
was for an element that was since removed). However, it will ensure that the
data contained in the event is valid, e.g. by clamping a scrollbar's value
to the current range of the scrollbar.

Additionally, note that fake events may come from malicious clients, such as a
button press for a button that was never enabled. The server will filter out
events that are obviously incorrect, such as when they come from the wrong
player or from a window that is no longer open. Validation of the data
contained within each event is another line of defense. However, any stronger
validation, such as checking whether the user could have clicked that button at
all (given that it was always disabled) is impractical for the UI API to check
automatically, and hence the responsibility lies on the event handler if
absolute security is necessary.

### Input dispatching

Since windows usually contain many elements that may be nested or overlapping,
user input is dispatched to elements in a specific order. This also has
important impacts on if and when event handlers are called.

First, the window contains a _focused element_, which is the element that has
keyboard focus. Elements that contain dynamic boxes, such as buttons and
scrollbars, can be focused. Static elements can never be focused.

When a keyboard key is pressed or released, the window allows the focused
element to use the event first. If it ignores the input (such as if the element
has no dynamic boxes), the window allows its parent to use the input, and so
forth. If none of them used the keyboard input, or if there is no focused
element, then the window itself gets to use the input, possibly sending it to
the server.

The window also contains a _hovered element_, which is the element that
primarily receives mouse and touch input. Just like the focused element, only
dynamic boxes may be the hovered element.

When mouse or touch input is received by the window, it first allows the
focused element and its parents to peek at the event, such as to let buttons
become unpressed. Then, it sends the input to the hovered element. If it
ignores the input, the input passes to the element directly underneath it at
the mouse/touch position (which is not necessarily its parent element), and so
on. If none of them used the input, the window again gets to use the input,
possibly sending it to the server.

### Window events

The window has a few predefined ways of using user input besides passing the
input along to the server.

The mouse is used for a number of features. When the mouse moves, the window
updates the currently hovered element. Similarly, when the left mouse button is
pressed, the window sets the focused element to the topmost focusable element
under the cursor.

If certain keys are not handled by the focused element, then the window uses a
few of them for special purposes. The Escape key will cause the window to be
closed if `allow_close` is not set to false for the window, which will
naturally cause the `on_close` event to be fired. The Enter key will cause the
`on_submit` event to be sent to the server, which the server may use for
whatever purpose it desires, such as by closing the window and using the form's
data somewhere.

The Tab key is used for changing which element has user focus via the keyboard.
Pressing Tab will cause the next focusable element in a preorder traversal of
the element tree to become focused, whereas pressing Shift + Tab will transfer
focus to the previous focusable element.

Styles
------

_Styles_ are the interface through which the display and layout characteristics
of boxes are changed. They offer a wide variety of style properties that are
supported by every box. Styles can vary based on _states_, which indicate how
the user is interacting with a box. Styles are represented with the `ui.Style`
class.

There are three components of a style:

1. A `SelectorSpec` that decides which boxes and states to apply the style to.
2. A `StyleSpec` that contains the actual styling properties.
3. A list of nested styles with their own selectors and properties that are
   cascaded with the ones in the parent style.

One simple example of a style is the following:

```lua
-- Select all buttons with the group ID `important` and give them yellow text
-- and a blue background.
ui.Style "button%important" {
    text_color = "yellow",
    box_fill = "blue",
}

-- Alternatively, a `StyleSpec` with style properties can be included
-- explicitly with the `props` property rather than being inlined:
ui.Style "button%important" {
    props = {
        text_color = "yellow",
        box_fill = "blue",
    },
}
```

### Selectors and properties

See the respective sections on `SelectorSpec` and `StyleSpec` for advanced
details on selectors and the list of supported style properties. Additionally,
a comprehensive description of how style properties affect the layout and
display of boxes is also discussed in the [Layout and visuals] section.

### States

The list of states is as follows, from highest precedence to lowest:

* `disabled`: The box is disabled, which means that user interaction with it
  does nothing.
* `pressed`: The left mouse button was pressed down inside the boundaries of
  the box, and has not yet been released.
* `hovered`: The mouse cursor is currently inside the boundaries of the box.
* `selected`: The box is currently selected in some way, e.g. a checkbox is
  checked or a list item is selected.
* `focused`: The box currently has keyboard focus.

The `pressed` and `hovered` states are properties of the box itself, and hence
only apply to dynamic boxes. Any dynamic box can be hovered and pressed,
although different boxes may have different requirements for what constitutes
being pressed.

The `disabled`, `selected`, and `focused` states are often shared among
multiple boxes, and hence may apply to both static and dynamic boxes. For
example, when an individual scrollbar is disabled, every box within that
scrollbar has the `disabled` state.

By default, it should be assumed that the `focused` state applies to every box
within the element if the element has focus. Moreover, if the element has a
`disabled` property, then the `disabled` state will also apply to every box
when that property is set. Boxes that have different behavior document how they
behave instead.

### State cascading

States fully cascade over each other. For instance, if there are styles that
give hovered buttons yellow text and pressed buttons blue backgrounds, then a
hovered and pressed button will have yellow text on a blue background.

```lua
-- Creating two styles with separate states like so...
ui.Style "button$hovered" {
    text_color = "yellow",
}
ui.Style "button$pressed" {
    box_fill = "blue",
}

-- ...automatically implies the following cascaded style as well:
ui.Style "button$hovered$pressed" {
    text_color = "yellow",
    box_fill = "blue",
}
```

However, if an element is currently in multiple states, then states with higher
precedence will override states with lower precedences. For instance, if one
style makes hovered buttons red and another makes pressed buttons blue, then a
button that is simultaneously hovered and pressed will be blue.

```lua
-- One style makes hovered buttons red, and another makes pressed buttons blue.
ui.Style "button$hovered" {
    box_fill = "red",
}
ui.Style "button$pressed" {
    box_fill = "blue",
}

-- Then, the implicit cascaded style will make buttons blue:
ui.Style "button$hovered$pressed" {
    box_fill = "blue",
}
```

Lastly, state precedences can combine to form a higher precedence state. A
style for pressed and hovered buttons will override styles for only pressed or
only hovered buttons. Similarly, a style for disabled buttons will override the
style for pressed and hovered buttons since it has higher precedence than
either one.

```lua
-- The first style makes hovered buttons red.
ui.Style "button$hovered" {
    box_fill = "red",
}

-- The second style makes buttons that are both hovered and pressed blue.
-- Therefore, hovered buttons will only be red if they are not pressed.
ui.Style "button$hovered$pressed" {
    box_fill = "blue",
}

-- If a button is disabled, this style makes it gray regardless of whether it
-- is hovered or pressed.
ui.Style "button$disabled" {
    box_fill = "gray",
}
```

### Nested styles

_Nested styles_ are a way of adding styles to a base style that apply extra
properties to a subset of the boxes. For instance, here is an example that
shows how nested styles work:

```lua
-- Here is a style with nested styles contained within it.
ui.Style "button" {
    box_fill = "yellow",

    ui.Style "$hovered" {
        box_fill = "red",
    },
    ui.Style "$pressed" {
        box_fill = "blue",
    },
}

-- That style is equivalent to having the following three styles:
ui.Style "button" {
    box_fill = "gray",
}
ui.Style "button$hovered" {
    box_fill = "red",
}
ui.Style "button$pressed" {
    box_fill = "blue",
}

-- The nested styles can also be explicitly included with the `nested` property
-- rather than being inlined into the style table:
ui.Style "button" {
    box_fill = "yellow",

    nested = {
        ui.Style "$hovered" {
            box_fill = "red",
        },
        ui.Style "$pressed" {
            box_fill = "blue",
        },
    },
}
```

Nested styles are always evaluated top to bottom, so the parent style is
applied to the box first, and then each nested style is applied to the box in
order. This order-dependent styling is in direct contrast to CSS, which
calculates precedence based on weights associated with each part of the
selector. Order-dependence was deliberately chosen because it gives mods more
control over the style of their own windows without external themes causing
problems. The only weighting done by the UI API is for state selectors, as
described above, due to states being calculated on the client.

There is no reason why a parent style containing nested styles must have a
selector or properties; nested styles can just be used for organizational
purposes by placing related styles in an empty parent style. Omitting the
selector causes it to be automatically set to the universal selector `*`. For
example, this style might be used as a theme for `ui.set_default_theme()`:

```lua
local theme = ui.Style {
    ui.Style "button, toggle, option" {
        -- Style properties for all the different types of buttons.
    },
    ui.Style "check, switch" {
        -- Style properties for checkboxes and switches.
    },
    ui.Style "radio" {
        -- Style properties for radio buttons.
    },
}
```

### Style resets

Sometimes, it is desireable to remove all existing style from a certain element
type. This can be done via _style resets_. The `reset` boolean, when set,
resets all style properties for the selected elements to their default values.

For instance, if one style gives buttons a red background and nonzero padding,
then setting the `reset` property on a later style for buttons will reset that
to the defaults of a transparent background and no padding. Note that this will
also reset any properties set by the prelude theme, described in [Theming].

```lua
-- One style somewhere adds a bunch of properties to buttons.
ui.Style "button" {
    box_fill = "red",
    padding = {2, 2, 2, 2},
}

-- The button with the ID "special" needs unique styling, and hence uses the
-- `reset` property to get a clean slate for styling.
ui.Style "#special" {
    reset = true,
    text_color = "yellow",
}
```

Style resets cascade with style states as well. Resetting buttons with the
`hovered` state will also reset the properties for buttons that are both
hovered and pressed, for instance. Resetting `button` directly will reset every
state for the button as well.

Theming
-------

It is usually the case that different windows share the same basic element
styles. This concept is supported natively by the UI API through the use of
_themes_, which are large groups of styles that can be used in the `theme`
property of windows or set as the globally default theme.

If a window doesn't have the `theme` property set, it will automatically use
the _default theme_, which is retrieved by `ui.get_default_theme()`. The engine
initially sets the default theme to the prelude theme, but games and mods can
change the default theme with the `ui.set_default_theme()` function. In the
future, the UI API will include a default theme called the "core theme", which
games can optionally use if they don't want to make their own theme or are in
early stages of development.

### Prelude theme

Nearly all themes should be based on the _prelude theme_, a theme that serves
no other purpose than being a base for other themes. The prelude theme sets
style properties that are essential to the functionality of the element, or are
at least a basic part of the element's intended design. It can be accessed with
the `ui.get_prelude_theme()` function.

For instance, it is part of the basic functionality of `ui.Accordion` to hide
and show its contents, so the prelude hides the `content` box if the `selected`
state is not active. As another example, `ui.Image` almost always wants its
image to fill as much of the element as possible, so the prelude sets their
`icon_scale` property to zero.

It is important to stress that the prelude theme does _not_ change the default
visuals of any elements. Box and icon images, tints, fills, paddings, margins,
and so on are never changed by the prelude theme, meaning all elements are
totally invisible by default and free to be themed.

The prelude theme is designed to be highly stable, and should rarely change in
any substantial way. Moreover, each element documents what default styling the
prelude theme gives it in its [Theming] section, making it easy for new themes
to override prelude styling where necessary.

When creating a new theme, the prelude theme should be included like so:

```lua
local new_theme = ui.Style {
    -- Include the prelude theme into this theme.
    ui.get_prelude_theme(),

    -- Add any number of new styles to the theme, possibly overriding
    -- properties set by the prelude theme.
    ui.Style "root" {
        box_fill = "navy#8C",
    },
}
```

To make a window with no theme, it is recommended to set the `theme` property
to `ui.get_prelude_theme()`. On rare occasions, it may be useful to make a
window that contains no styles, including those set by the prelude. In that
case, the `theme` property can be set to a blank `ui.Style {}`.

Layout and visuals
------------------

The UI API has powerful support for positioning, sizing, and styling the
visuals and display of boxes. Rather than just supporting the strange
coordinate system of formspecs or the normalized coordinates of the player HUD
API, the UI API supports both pixel positioning and normalized coordinates.
Future versions will also support more advanced flex and grid layouts.
Moreover, every box supports a universal set of style properties for visuals,
rather than the limited and element-dependent styling of formspecs.

See `StyleSpec` for a full list of supported style properties.

### Box layout

Boxes have a number of paddings and margins can be applied to add space inside
and around themselves. This leads to a number of conceptual rectangular areas
inside each box that serve various purposes. The following diagram illustrates
each of these rectangles:

```
+---------------------------------------------------------+
|  Layout rect                                            |
|   +-------------------------------------------------+   |
|   |* Display rect  * * * * * * * * * * * * * * * * *|<->|
|   | * +-----------------------------------------+ * | Margin
|   |* *|  Border rect                            |* *|   |
|   | * |                +--------------------+   |<->|   |
|   |* *|   +--------+   |  Content rect      |   | Image border
|   | * |   |* Icon *|   |      |   |  .      |   | * |   |
|   |* *|   | * rect |<->|      |---|  |      |<->|* *|   |
|   | * |   |* * * * | Gutter   |   |  |      | Padding   |
|   |* *|   +--------+   |                    |   |* *|   |
|   | * |                +--------------------+   | * |   |
|   |* *|                                         |* *|   |
|   | * +-----------------------------------------+ * |   |
|   |* * * * * * * * * * * * * * * * * * * * * * * * *|   |
|   +-------------------------------------------------+   |
|                                                         |
+---------------------------------------------------------+
```

After the box is positioned within its bounding rectangle (as described below),
the resulting positioned rectangle is called the _layout rectangle_. The
contents of the layout rectangle are inset by the `margin` property, making it
possible to add space between adjacent boxes.

Inside the layout rectangle is the _display rectangle_, which as the name
implies is where the box is displayed. This is where the `box_*` style
properties draw their contents, e.g. `box_image` draws an image that is
stretched to the boundaries of the bounding rectangle. Additionally, the
display rectangle is where mouse and touch input are detected.

If the `box_middle` property is set, then the `box_image` is drawn as a
nine-slice image where the image borders are scaled by the `box_scale`
property. The contents of the display rectangle are automatically inset by the
size of this border. This results in the _middle rectangle_, which in turn
insets its contents by the `padding` property to make the _padding rectangle_.

Placed inside the padding rectangle is the _content rectangle_, which is where
the content of the box is placed. This might include text, child boxes, and/or
other special types of content contained in the element, or it might contain
nothing at all, such as in the case of scrollbar buttons and thumbs.

Finally, if the `icon_image` property is set, then an _icon rectangle_ is
allocated to make space for the icon. The size of this rectangle is based on
the size of the icon image scaled by the `icon_scale` property. By default, the
icon rectangle is placed in the center of the content rectangle, but it can be
moved to any side of the content by using the `icon_place` property. The
`icon_overlap` property controls whether the content rectangle should overlap
the icon rectangle, which will cause any content to be displayed on top of the
icon. If they are not set to overlap, the `icon_gutter` property can be used to
control how much space will be placed between them.

### Content layout

TODO

#### Place layout

#### Flex and grid layout

### Visual properties

Some style properties that control the visual aspects of boxes have been
described above. However, there are other properties that can modify the
appearance of the box.

The `clip` property controls how the box and its contents are clipped without
affecting the layout of the box. The default is `normal`, which causes the box
and its contents to be clipped to the bounding rectangle of the box.
Alternatively, the `overflow` value can be used to clip them to the display
rectangle instead, ignoring the bounding rectangle entirely. Lastly, the
`complete` value can be used to clip the box and its contents away entirely.
They will still exist and take up space, but the mouse will be unable to
interact with them (although the keyboard still can). Descendants of a
`clipped` box can be shown by using the `overflow` value.

The `box_*` and `icon_*` properties have some overlap in terms of style
properties. The `*_fill` properties fill the respective rectangle with a solid
color. As mentioned before, the `*_image` properties choose the image to draw
inside the rectangle. To draw only part of the image, a source rectangle can be
specified in normalized coordinates with `*_source`. The image can also be
colorized by using `*_tint`. Finally, for animated images, `*_frames` can be
used to specify the number of frames in the image, along with `*_frame_time` to
set the length of each frame in milliseconds.

There are also some properties unique to both. As mentioned, the icon can use
`icon_scale` to set a scale factor for the icon. If this scale is set to 0,
then the icon rectangle fills up as much of the box as possible without
changing the aspect ratio of the image.

For the display rectangle, the aforementioned `box_middle` property sets the
image to be a nine-slice image. Like `box_source`, it is specified in
normalized coordinates based on the image size. The `box_tile` property can be
used to have the image be tiled rather than streched, which tiles each slice
individually for a nine-slice image. Finally, the `box_scale` property scales
the size of each tile in a tiled image and the borders of a nine-slice image.

Contexts
--------

In order to show a window, the UI API provides _contexts_, represented by the
`ui.Context` class. Contexts encapsulate the state of a single window and the
player it is shown to, and provide a means of opening, closing, rebuilding, and
updating the window. Unlike most classes in the UI API, `ui.Context` is not
immutable and contains public methods that can modify its state.

### Builder functions

In order to create a context, a _builder function_ must be provided. When
called, this function should return a window containing the desired element
tree. Every time the window is shown to a player or updated, the builder
function is called again. This allows the function to change the number of
elements or the properties they have. Since window and element objects are
immutable to the user, rebuilding everything using the builder function is the
only way to modify the UI.

Each `ui.Window` object can only be used once. After being returned from a
builder function once, the same object cannot be returned from a builder
function again. The same applies to elements inside the window.

Besides the builder function, the `ui.Context` constructor requires the name of
the player that the window is shown to. The builder function and player can be
extracted from a context using the `get_builder()` and `get_player()` methods
respectively.

### Opening, updating, and closing windows

After a context has been constructed, it can be shown to the player using the
`open()` method. If the state has changed and the UI needs to be rebuilt and
shown to the player, the `update()` method can be called. To close the window
programmatically, use the `close()` method. Open windows will be automatically
closed when the player leaves the game.

When a window is closed, the `update()` and `close()` methods do nothing.
Similarly, when the window is open, the `open()` method does nothing. If
multiple windows need to be shown to a player, then multiple contexts must be
created since a context represents a single window.

To query whether a context is open, use the `is_open()` method. To get a list of
all currently open contexts, use the `ui.get_open_contexts()` function.

### Non-updatable properties

Some properties cannot be updated via the `update()` method since that could
lead to race conditions where the server changed a property, but the client
sent an event that relied on the old property before it received the server's
changes. For most situations, this is not problematic, but it could cause weird
behavior for some changes, such as when changing the window type from `gui` to
another window type that doesn't use events.

To change these non-updatable properties, use the `reopen()` method, which
is effectively the same as calling `context:close():open()`, but will do so in
a single atomic step (i.e. the player won't see the window disappear and
reappear). Additionally, `reopen()` will do nothing if the window is already
closed, just like `update()`.

The `reopen()` method is not an exact replacement for `update()`. For one, it
will change the window's Z order by moving it in front of all other windows
with the same window type. Additionally, all persistent properties will be
changed by this operation, just like `open()`.

### State and parameter tables

Since UIs generally have some state associated with each open window, contexts
provide a means of holding state across calls to the builder function via a
_state table_. When a `ui.Context` is constructed, a state table can be
provided that holds state that will be passed to the builder function every
time it is called for this UI. This table can be modified by the builder
function or by event handlers in the UI. The UI API will never modify a state
table by itself. The state table can be obtained from a context via the
`get_state()` method or replaced entirely via `set_state()`.

The following is a basic example that uses a state table:

```lua
local function builder(context, player, state, param)
    return ui.Window "gui" {
        root = ui.Root {
            size = {108, 40},
            padding = {4},

            box_fill = "black#8C",

            ui.Button "counter" {
                box_fill = "maroon",
                label = "Clicks: " .. state.num_clicks,

                on_press = function(ev)
                    state.num_clicks = state.num_clicks + 1
                    context:update()
                end,
            },
        },
    }
end

core.register_on_joinplayer(function(player)
    local state = {num_clicks = 0}
    ui.Context(builder, player:get_player_name(), state):open()
end)
```

The state table is primarily for persistent data. However, it is often useful
to send temporary data that only applies to a single call of the builder
function, such as when setting persistent fields. This can be provided in the
form of a _parameter table_. Parameter tables can be provided to the methods
`open()`, `update()`, and `reopen()` and will be passed directly to the builder
function. After the builder function returns, the parameter table is discarded.

The following is a basic example that sets a persistent field when first
opening the window by using parameter tables (note that it uses a slider, which
is not implemented yet):

```lua
local function builder(context, player, state, param)
    return ui.Window "gui" {
        root = ui.Root {
            size = {500, 40},
            padding = {4},

            box_fill = "black#8C",

            ui.Slider "percent" {
                label = state.scroll .. "%",

                min = 0,
                max = 100,

                -- `param.set_scroll` is only set in `open()`, not in the
                -- `update()` in `on_scroll`, so `value` is only set to
                -- `state.scroll` when the window is first opened.
                value = param.set_scroll and state.scroll,

                on_scroll = function(ev)
                    state.scroll = ev.value
                    context:update()
                end,
            },
        },
    }
end

core.register_on_joinplayer(function(player)
    local state = {scroll = 50}
    local param = {set_scroll = true}

    ui.Context(builder, player:get_player_name(), state):open(param)
end)
```

Utility functions
-----------------

* `ui.new_id()`: Returns a new unique ID string.
    * **This function may not be used for elements that require an ID to be
      provided, such as buttons or edit field elements!** These elements
      require an ID that will stay constant across window updates!
    * It is usually not necessary to use this function directly since the API
      will automatically generate IDs when none is required.
    * The format of this ID is not precisely specified, but it will have the
      format of an engine reserved ID and will not conflict with any other IDs
      generated during this session.
* `ui.is_id(str)`: Checks whether the argument is a string that follows the
  format of an ID string.
* `ui.get_coord_size()`: Returns the size of a single coordinate in a
  fixed-size formspec, i.e. a formspec with a size of `size[<x>,<y>,true]`. Can
  be used when transitioning from formspecs to the UI API.
* `ui.derive_elem(elem, name)`: Creates and returns a new derived element type.
    * `elem`: The element class to derive from, e.g. `ui.Toggle`.
    * `name`: The type name for the derived element to use. The name should use
      a `mod_name:` prefix.
    * Returns the constructor for the new type, which can be used to create new
      elements of the new derived type.
* `ui.get_prelude_theme()`: Returns the style defining the prelude theme.
* `ui.get_default_theme()`: Returns the style used as the default theme for
  windows without an explicit theme. Defaults to the prelude theme for now.
* `ui.set_default_theme(theme)`: Sets the default theme to a new style.
* `ui.get_open_contexts()`: Returns a table containing the context objects for
  all currently open windows.

`ui.Context`
------------

Contexts encapsulate the state of a single window shown to a specific player,
as described in the [Contexts] section.

### Constructor

* `ui.Context(builder, player[, state])`: Creates a new context with a player
  and an initial state table. The window is initially not open.
    * `builder` (function): The builder function for the context. This function
      takes four parameters, `function(context, player, state, param)`:
        * `context` (`ui.Context`): The context itself.
        * `player` (string): The name of the player that the window will be
          shown to, equivalent to `context:get_player()`.
        * `state` (table): The state table associated with this context,
          equivalent to `context:get_state()`.
        * `param` (table): The parameter table for this call to the builder
          function.
        * The function should return a freshly created `ui.Window` object.
    * `player` (string): The player the context is associated with.
    * `state` (table, optional): The initial state table for the window. If not
      provided, defaults to an empty table.

### Methods

* `open([param])`: Builds a window and shows it to the player. Does nothing if
  the window is already open.
    * `param` (table, optional): The parameter table for this call to the
      builder function. If not provided, defaults to an empty table.
    * Returns `self` for method chaining.
* `update([param])`: Updates a window by rebuilding it and propagating the
  changes to the player. Does nothing if the window is not open.
    * `param` (table, optional): The parameter table for this call to the
      builder function. If not provided, defaults to an empty table.
    * Returns `self` for method chaining.
* `reopen([param])`: Reopens a window by rebuilding it, closing the player's
  old window, and showing the new window to the player atomically. Does nothing
  if the window is not open.
    * `param` (table, optional): The parameter table for this call to the
      builder function. If not provided, defaults to an empty table.
    * Returns `self` for method chaining.
* `close()`: Closes a window that is currently shown to the player. Does
  nothing if the window is not currently open.
    * Returns `self` for method chaining.
* `is_open()`: Returns true if the window is currently open, otherwise false.
* `get_builder()`: Returns the builder function associated with the context.
* `get_player()`: Returns the player associated with the context.
* `get_state()`: Returns the state table associated with the context.
* `set_state(state)`: Sets a new state table for the context, replacing the
  existing one.
    * Returns `self` for method chaining.

`ui.Window`
-----------

Windows represent discrete self-contained UIs as described in the [Windows]
section.

### Constructor

* `ui.Window(type)(props)`: Creates a new window object.
    * `type` (string): The window type for this window. This parameter cannot
      be changed by `ui.Context:update()`.
    * `props` (table): A table containing various fields for configuring the
      window. See the [Fields] section for a list of all accepted fields.

### Fields

The following fields can be provided to the `ui.Window` constructor:

* `root` (`ui.Root`, required): The root element for the element tree.
* `theme` (`ui.Style`): Specifies a style to use as the window's theme.
  Defaults to the theme provided by `ui.get_default_theme()`.
* `styles` (table of `ui.Style`s): A table of global styles that apply across
  the entire element tree. If this property is omitted, global styles may be
  inlined into the constructor table.
* `allow_close` (boolean): Indicates whether the user is able to close the
  window via the Escape key or similar. Defaults to true. This field cannot be
  changed by `ui.Context:update()`.

The following persistent fields can also be provided:

* `focused` (ID string): If present, specifies the ID of an element to set as
  the focused element. If set to the empty string, no element will have focus.
  Newly created windows default to having no focused element.

The following event handlers can also be provided:

* `on_close`: Fired if the window was closed by the user. This event will never
  be fired for windows with `allow_close` set to false. Additionally, the
  player leaving the game and `ui.Context:close()` will never fire this event.
* `on_submit`: Fired if the user pressed the Enter key and that keypress was
  not used by the focused element.
* `on_focus_change`: Fired when the user changed the currently focused element.
  Additional `EventSpec` fields:
    * `unfocused`: Contains the ID of the element that just lost focus, or the
      empty string if no element was previously focused.
    * `focused`: Contains the ID of the element that just gained focus, or the
      empty string if the user unfocused the current element.

`ui.Elem`
---------

Elements are the basic units of interface in the UI API, as described in the
[Elements] section. All elements inherit from the `ui.Elem` class.

In general, plain `ui.Elem`s should not be used when there is a more
appropriate derived element, such as `ui.Image` for an element that has the
sole purpose of displaying an image. Moreover, plain `ui.Elem`s should never be
given default styling by any theme.

### Type info

* Type name: `elem`
* ID required: No
* Boxes:
    * `main` (static): The main box of the element. Unless otherwise stated in
      the documentation for other boxes, text content and child elements are
      placed within this box.

### Derived elements

* `ui.Group`
    * Type name: `group`
    * A static element meant for holding generic grouping of elements for
      layout purposes, similar to the HTML `<div>` element.
* `ui.Label`
    * Type name: `label`
    * A static element that is meant to be used for static textual labels in a
      window. The regular `label` property should be used to display the text
      for the label.
* `ui.Image`
    * Type name: `image`
    * A static element that only meant for displaying an image, either static
      or animated. The `icon_image` style property should be used to display
      the image, not the `box_image` property.

### Theming

For `ui.Image`, the prelude sets `icon_image` to zero to make the image fill as
much space as possible. There is no prelude theming for either `ui.Elem` or
`ui.Label`.

### Constructor

All elements have the same function signatures as `ui.Elem` for their
constructors and only differ in the properties accepted for `props`.

* `ui.Elem(id)(props)`: Creates a new element object.
    * `id` (ID string): The unique ID for this element. It is only required if
      stated as such in the element's [Type info] section.
    * `props` (table): A table containing various fields for configuring the
      element. See the [Fields] section for a list of all accepted fields.
* `ui.Elem(props)`: Same as the above constructor, but generates an ID with
  `ui.new_id()` automatically.

### Fields

The following fields can be provided to the `ui.Elem` constructor:

* `label` (string): The text label to display for the element.
* `groups` (table of ID strings): The list of group IDs for this element.
* `props` (`StyleSpec`): A table of style properties that only apply to this
  element's `main` box without any state selectors applied. If this property is
  omitted, these style properties may be inlined into the constructor table.
* `styles` (table of `ui.Style`s): A table of local styles that only apply to
  this element. If this property is omitted, local styles may be inlined into
  the constructor table.
* `children` (table of `ui.Elem`s): The list of elements that are children of
  this element. If this property is omitted, children may be inlined into the
  constructor table.

`ui.Root`
---------

The root element is a special type of element that is used for the sole purpose
of being the root of the element tree. Root elements may not be used anywhere
else in the element tree.

Aside from its `main` box, the root element also has a `backdrop` box as the
the parent of its `main` box. The backdrop takes up the entire screen behind
the window and is intended to be used for dimming or hiding things behind a
window with a translucent or opaque background.

The root element is fully static, but the `backdrop` box will have the
`focused` state set if the window is a GUI window that has user focus. In
general, the `backdrop` box should be invisible unless it is focused. This
prevents backdrops from different windows from stacking on top of each other,
which generally leads to an undesirable appearance.

The `backdrop` box does not count as part of the element for mouse clicks, so
clicking on the backdrop counts as clicking outside the window.

### Type info

* Type name: `root`
* ID required: No
* Boxes:
    * `backdrop` (static): The backdrop box, which has the entire screen as its
      layout rectangle. It is the parent of the `main` box.
    * `main` (static): The main box. See `ui.Elem` for more details.

### Theming

The prelude centers the main box on the screen with `pos` and `anchor`, but
gives it no size, so users must explicitly choose a size with `size` and/or
`min`. The `clip` property is set to `overflow` so the main box can visually
extend past the backdrop.

Additionally, the prelude sets `hidden` to true for the backdrop if it doesn't
have the `focused` state set. The backdrop also sets `truncate` to `both` to
ensure that the backdrop never expands past the screen size.

### Fields

The fields that can be provided to the `ui.Root` constructor are the same as
those in `ui.Elem`.

`ui.Button`
-----------

The button is a very simple interactive element that can do nothing except be
clicked. When the button is clicked with the mouse or pressed with the
spacebar, then the `on_press` event is fired unless the button is disabled.

### Type info

* Type name: `button`
* ID required: Yes
* Boxes:
    * `main` (dynamic): The main box, which constitutes the pressable portion
      of the button. Also see `ui.Elem` for more details.

### Theming

There is no prelude theming for `ui.Button`.

### Fields

In additional to all fields in `ui.Elem`, the following fields can be provided
to the `ui.Button` constructor:

* `disabled` (boolean): Indicates whether the button is disabled, meaning the
  user cannot interact with it. Default false.

The following event handlers can also be provided:

* `on_press`: Fired if the button was just pressed.

`ui.Toggle`
-----------

The toggle button is a type of button that has two states: selected and
deselected. In its simplest form, the state of the toggle button flips between
selected and deselected whenever it is pressed. The state of the toggle button
can be controlled programmatically by the persistent `selected` property.

Toggle buttons have two events, `on_press` and `on_change`, which fire
simultaneously. Although `on_change` is a strict superset of the functionality
of `on_press`, both are provided for parity with `ui.Option`.

### Type info

* Type name: `toggle`
* ID required: Yes
* Boxes:
    * `main` (dynamic): The main box, which constitutes the pressable portion
      of the toggle button. The `selected` state is active when the toggle
      button is selected. Also see `ui.Elem` for more details.

### Derived elements

* `ui.Check`
    * Type name: `check`
    * A variant of the toggle button that is meant to be styled like a
      traditional checkbox rather than a pushable button.
* `ui.Switch`
    * Type name: `switch`
    * A variant of the toggle button that is meant to be styled like a switch
      control; that is, a horizontal switch where left corresponds to
      deselected and right corresponds to selected.

### Theming

The prelude horizontally aligns the icon image for `ui.Check` and `ui.Switch`
to the left. There is no prelude theming for `ui.Toggle`.

### Fields

In additional to all fields in `ui.Elem`, the following fields can be provided
to the `ui.Toggle` constructor:

* `disabled` (boolean): Indicates whether the toggle button is disabled,
  meaning the user cannot interact with it. Default false.

The following persistent fields can also be provided:

* `selected` (boolean): If present, changes the state of the toggle button to a
  new value. Newly created toggle buttons default to false.

The following event handlers can also be provided:

* `on_press`: Fired if the toggle button was just pressed.
* `on_change`: Fired if the value of the toggle button changed. Additional
  `EventSpec` fields:
    * `selected` (boolean): The state of the toggle button.

`ui.Option`
-----------

The option button is similar to a toggle button in the sense that it can be
either selected or deselected. However, option buttons are grouped into
_families_ such that exactly one option button in each family can be selected
at a time by the user.

The family of an option button is controlled by the `family` property, which is
set to a ID string that is shared by each option button in the family. If no
family is provided, the option button acts as if it were alone in a family with
one member.

When the user presses a non-disabled option button, that button is selected and
all the other buttons in the family (including disabled ones) are deselected.
Although the user can only select one option button, zero or more option
buttons may be set programmatically via the persistent `selected` property.

Option buttons have two events: `on_press` and `on_change`. The `on_press`
event occurs whenever a user presses an option button, even if that button was
already selected. The `on_change` event occurs whenever the value of an option
button changes, whether that be the button the user selected or the other
buttons in the family that were deselected. The `on_change` event will fire for
the selected button first, followed by the deselected buttons.

### Type info

* Type name: `option`
* ID required: Yes
* Boxes:
    * `main` (dynamic): The main box, which constitutes the pressable portion
      of the option button. The `selected` state is active when the option
      button is selected. Also see `ui.Elem` for more details.

### Derived elements

* `ui.Radio`
    * Type name: `radio`
    * A variant of the option button that is meant to be styled like a
      traditional radio button rather than a pushable button.

### Theming

The prelude horizontally aligns the icon image for `ui.Radio` to the left.
There is no prelude theming for `ui.Option`.

### Fields

In additional to all fields in `ui.Elem`, the following fields can be provided
to the `ui.Option` constructor:

* `disabled` (boolean): Indicates whether the option button is disabled,
  meaning the user cannot interact with it. Default false.
* `family` (ID string): Sets the family of the option button. If none is
  provided, the option button works independently of any others. Default none.

The following persistent fields can also be provided:

* `selected` (boolean): If present, changes the state of the option button to a
  new value. Newly created option buttons default to false.

The following event handlers can also be provided:

* `on_press`: Fired if the option button was just pressed.
* `on_change`: Fired if the value of the option button changed. Additional
  `EventSpec` fields:
    * `selected` (boolean): The state of the option button.

`ui.Style`
----------

Styles are the interface through which the display and layout characteristics
of element boxes are changed, as described in [Styles].

### Constructor

* `ui.Style(sel)(props)`: Creates a new style object.
    * `sel` (`SelectorSpec`): The primary selector that this style applies to.
    * `props` (table): A table containing various fields for configuring the
      style. See the [Fields] section for a list of all accepted fields.
* `ui.Style(props)`: Same as the above constructor, but the selector defaults
  to `*` instead.

### Fields

The following fields can be provided to the `ui.Style` constructor:

* `props` (`StyleSpec`): The table of properties applied by this style. If
  this property is omitted, style properties may be inlined into the
  constructor table.
* `nested` (table of `ui.Style`s): A list of `ui.Style`s that should be used as
  the cascading nested styles of this style. If this property is omitted,
  nested styles may be inlined into the constructor table.
* `reset` (boolean): If true, resets all style properties for the selected
  elements to their default values before applying the new properties.

`SelectorSpec`
--------------

A _selector_ is a string similar to a CSS selector that matches elements by
various attributes, such as their ID, group, and state, what children they
have, etc. Many of the same concepts apply from CSS, and there are a few
similarities in the syntax, but there is no compatibility between the two.

### Usage and syntax

A selector is composed of one or more _terms_. For instance, the `button` term
selects all button elements, whereas the `$hovered` term selects all boxes in
the hovered state. Terms can be combined with each other by concatenation to
form a longer and more specific selector. For instance, the selector
`button$hovered` selects elements that are both buttons and are hovered.

Using a comma between terms forms the union of both terms. So, `button,
$hovered` selects terms that are either buttons or are hovered. These can be
combined freely, e.g. `button$pressed, scrollbar$hovered` selects elements that
are pressed buttons or hovered scrollbars.

The order of operations for these operations can be controlled by parentheses,
so `(button, check)($pressed, $hovered)` is the same as the much longer
selector `button$pressed, button$hovered, check$pressed, check$hovered`.

Note that it is not invalid for a selector to have contradictory terms. The
selector `#abc#xyz` is valid, but will never select anything since an element
can only have a single ID.

Whitespace between terms is ignored, so both `button$hovered` and `button
$hovered` are valid and equivalent, although it is customary to only put
whitespace after commas. The order that selectors are written in is also
irrelevant, so `button$hovered` and `$hovered button` are equivalent, although
the former order is preferred by convention.

### Basic terms

The full list of basic terms is as follows, listed in the conventional order
that they should be written in:

* `/window/` matches any element inside a window with window type `window`.
* `*` matches every element. This is necessary since empty selectors and terms
  are invalid. It is redundant when combined with other terms.
* `type` matches any element with the type name `type`. Inheritance is ignored,
  so `elem` matches `ui.Elem` but not `ui.Label` or `ui.Button`.
* `#id` matches any element with the ID `id`.
* `%group` matches any element that contains the group `group`.
* `@*` matches any box within any element, not just the `main` box.
* `@box` matches any box with the name `box` within any element.
* `$state` matches any box currently in the state `state` within any element.

### Box selectors

The `@box` selector selects a specific box to style. For instance, a selector
of `scrollbar%overflow@thumb` can be used to style the thumb of any scrollbar
with the group `overflow`. On the other hand, `button@thumb` will not match
anything since buttons don't have a `thumb` box. Similarly, `@main@thumb` will
not match anything since a box cannot be two separate boxes at once.

By default, a selector that contains no box selector will only match the `main`
box. Alternatively, the special `@*` box selector can be used to select every
box in the element. For instance, `accordion@*` would be equivalent to
`accordion(@main, @caption, @content)`. This selector is especially useful when
performing style resets for an entire element and all the boxes within it.

### Predicates

Terms can be combined with more complicated operators called _predicates_. The
simplest predicate is the `!` predicate, which inverts a selector. So `!%tall`
selects all elements without the `tall` group. Note that `!` only applies to
the term directly after it, so `!button%tall` means `(!button)%tall`. To invert
both terms, use `!(button%tall)`.

Predicates can only work with certain types of selectors, called _predicate
selectors_. Predicate selectors cannot use box or state selectors, since the
server has no knowledge of which boxes are in which states at any given time.
For example, the selector `button!$pressed` is invalid. On the other hand,
the selector that `ui.Style` uses is called a _primary selector_, which is a
selector that is allowed to use boxes and states outside of predicates.

All predicates other than `!` start with the `?` symbol. _Simple predicates_
are one such predicate type, which match an element based on some intrinsic
property of that element. The `?first_child` predicate, for instance, checks
if an element is the first child of its parent.

There are also _function predicates_ which take extra parameters, usually a
selector, to select the element. For instance, the `?<()` predicate tries to
match a selector `sel` against a parent element. So, `?<(button, check)`
matches all elements that have a button or a checkbox as their parent. Not all
function predicates take selectors as parameters, such as `?nth_child()`, which
takes a positive integer instead.

### List of predicates

The complete list of predicates aside from `!` is as follows:

* `?no_children` matches all elements with no children.
* `?first_child` matches all elements that are the first child of their parent
  element.
* `?last_child` matches all elements that are the last child of their parent
  element.
* `?only_child` matches all elements that are the only child of their parent
  element.
* `?nth_child(index)` matches all elements that are at the `index`th child
  from the start of their parent.
* `?nth_last_child(index)` matches all elements that are the `index`th
  child from the end of their parent.
* `?first_match(sel)` matches all elements that are the first child of their
  parent to be matched by the selector `sel`.
* `?last_match(sel)` matches all elements that are the last child of their
  parent to be matched by the selector `sel`.
* `?only_match(sel)` matches all elements that are the only child of their
  parent to be matched by the selector `sel`.
* `?nth_match(sel; index)` matches all elements that are the `index`th child
  from the start of their parent to be matched by the selector `sel`.
* `?nth_last_match(sel; index)` matches all elements that are the `index`th
  child from the end of their parent to be matched by the selector `sel`.
* `?<(sel)` matches all elements whose parent is matched by the selector `sel`.
* `?>(sel)` matches all elements that have a child that is matched by the
  selector `sel`.
* `?<<(sel)` matches all elements that have an ancestor that is matched by the
  selector `sel`.
* `?>>(sel)` matches all elements that have a descendant that is matched by the
  selector `sel`.
* `?<>(sel)` matches all elements that have a sibling that is matched by the
  selector `sel`.
* `?family(name)` matches all elements that have a family of `name`. If `name`
  is `*`, then it matches elements that have a family, regardless of which one.

Note that the root element is considered to be the only "child" of the window,
so it is matched by e.g. `?first_child` and `?only_match(root)`.

Unlike CSS, there are no child or descendant combinators since the more
powerful `?<()` and `?<<()` predicates can be used instead. Note that
predicates like `?>>()` should be used with care since they may have to check a
large number of elements in order to see if there is a match, which may cause
loss of performance for particularly large UIs.

`StyleSpec`
------------

A `StyleSpec` is a plain table of properties for use in `ui.Style` or as
inlined properties in `ui.Elem`.

### Field formats

`StyleSpec` has specific field formats for positions and rectangles:

* 2D vector: A table `{x, y}` representing a position, size, or offset. The
  shorthand `{num}` can be used instead of `{num, num}`.
* Rectangle: A table `{left, top, right, bottom}` representing a rectangle or
  set of borders. The shorthand `{x, y}` can be used instead of `{x, y, x, y}`,
  and `{num}` can be used instead of `{num, num, num, num}`.

### Fields

All properties are optional. Invalid properties are ignored.

#### Layout fields

* `layout` (string): Chooses what layout scheme this box will use when laying
  its children out. Currently, the only valid option is `place`.
* `truncate` (string): Controls whether the minimum size of a box is expanded
  or truncated when the contents of the box do not fit within the box's alloted
  space. One of `none`, `x`, `y`, or `both`. Default `none`.
* `scale` (number): The scale factor by which positions and sizes will be
  multiplied by in `place` layouts. Default 0.
    * A scale of zero will scale coordinates by the width and height of the
      parent box, effectively creating normalized coordinates.

#### Sizing fields

* `min` (2D vector): The minimum size of the box in pixels. May not be
  negative. Default `{0, 0}`.
* `margin` (rectangle): Margin in pixels of blank space between the layout
  rectangle and the display rectangle. It is valid for margins to be negative.
  Default `{0, 0, 0, 0}`.
* `padding` (rectangle): Padding in pixels of blank space between the border
  rectangle and the padding rectangle. It is valid for padding to be negative.
  Default `{0, 0, 0, 0}`.
* `pos` (2D vector): The position that `place` layouts will put the box at,
  scaled by `scale`. Default `{0, 0}`.
* `size` (2D vector): The size that `place` layouts will allocate for the box,
  scaled by `scale`. Default `{1, 1}`.
* `anchor` (2D vector): The point which should be considered the origin for
  positioning the box for `place` layouts. Must have components in the range
  [0, 1]. Default `{0, 0}`.
    * Specified in normalized coordinates relative to the size of the box
      itself, e.g. `{1, 1/2}` means to position the box from the right center.

#### Visual fields

* `clip` (string): Specifies how the box and its contents should be clipped.
  One of `normal`, `overflow`, or `complete`. Default `normal`.
* `hidden` (boolean): Controls whether the box's fill color, image, and object
  should be shown or hidden without affecting the appearance of the box's
  children. Default false.
* `fill` (`ColorSpec`): Color to fill the display rectangle of the box with,
  drawn behind the image and object. Default transparent.

#### Image fields

* `img_pane` (string): Name of the texture to be used for the pane layer of the
  image, or `""` for no texture. The pane layer is used for defining the border
  rectangle and tiling of the box. Default `""`.
* `img_overlay` (string): Name of the texture to be used for the overlay layer
  of the image, or `""` for no texture. The overlay layer is positioned within
  the border rectangle defined by the pane layer. Default `""`.
* `img_tint` (`ColorSpec`): Color to multiply both layers of the image by.
  Default white.
* `img_slice` (rectangle): Allows a sub-rectangle of both layers of the image
  to be drawn instead of the whole texture for each. Default `{0, 0, 1, 1}`.
    * The slice rectangle uses normalized coordinates relative to the size of
      each layer texture. E.g. `{0, 1/2, 1/2, 1}` draws the lower left quadrant
      of each texture. This makes slice rectangles friendly to texture packs
      with varying base texture sizes as well as allowing differing resolutions
      for each layer texture.
    * The top left coordinates may be greater than the bottom right
      coordinates, which causes each texture be mirrored. E.g. `{1, 0, 0, 1}`
      mirrors each texture horizontally.
    * Coordinates may extend past the texture boundaries in order to make each
      texture repeat, including via negative coordinates. E.g. `{-1, 0, 2, 1}`
      displays three copies of each layer texture side by side.
* `img_frames` (integer): If the image should be animated, the slice rectangle
  for each texture is vertically split into this many frames that will be
  animated, starting with the top. Must be positive. If set to one, the image
  will be static. Default 1.
* `img_frame_time` (integer): If the image is animated, this defines the time
  in milliseconds to display each frame in the image. Must be positive.
  Default 1000.
* `img_border` (rectangle): Configures the sizes of the nine-slice borders of
  the pane layer. May not be negative. Default `{0, 0, 0, 0}`.
    * Uses normalized coordinates relative to the size of the texture. E.g.
      using `{2/16, 1/16, 2/16, 1/16}` on a 16 by 16 texture will make the
      horizontal borders two pixels and the vertical borders one pixel.
    * If the edges of the borders overlap with each other and/or extend past
      the image boundaries, the pane texture will repeat and be mirrored in the
      same manner as `img_slice`.
    * In conjunction with `img_scale`, this also defines the space between the
      display rectangle and the border rectangle.
* `img_tile` (string): Specifies whether the pane should be tiled rather than
  stretched in the horizontal or vertical direction. One of `none`, `x`, `y`,
  or `both`. Default `none`.
    * If used in conjunction with `img_border`, then each slice of the pane
      will be tiled individually.
* `img_align` (2d vector): The alignment of the overlay within the border
  rectangle. Must have components in the range [0, 1]. Default `{1/2, 1/2}`.
    * The position uses normalized coordinates relative to the size of the
      border rectangle, e.g. `{1/2, 1}` will place the overlay at the bottom
      center of the rectangle.
* `img_scale` (number): Defines the scaling factor for the borders and tiles of
  the pane as well as the scale of the overlay. May not be negative. Default 1.

`EventSpec`
-----------

An `EventSpec` is a plain table that is passed to event handler functions to
give detailed information about what changed during the event and where the
event was targeted.

An `EventSpec` always contains the following fields:

* `context` (`ui.Context`): The context that the event originates from.
* `player` (string): The name of the player that the window is shown to,
  equivalent to `context:get_player()`.
* `state` (table): The state table associated with the context, equivalent to
  `context:get_state()`.

Additionally, `EventSpec`s that are sent to elements have an additional field:

* `target` (ID string): The ID of the element that the event originates from.

`EventSpec`s may have other fields, depending on the specific event. See each
event handler's documentation for more detail.
