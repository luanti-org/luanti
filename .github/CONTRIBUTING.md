# Contributing

Contributions are welcome! Here's how you can help:

- [Contributing code](#code)
- [Reporting issues](#issues)
- [Requesting features](#feature-requests)
- [Translating](#translations)
- [Donating](#donations)

## Code

1. [Fork](https://help.github.com/articles/fork-a-repo/) the repository and
   [clone](https://help.github.com/articles/cloning-a-repository/) your fork.

2. Before you start coding, consider opening an
   [issue on Github](https://github.com/luanti-org/luanti/issues) to discuss the
   suitability and implementation of your intended contribution with the core
   developers.

   Any Pull Request that isn't a bug fix and isn't covered by
   [the roadmap](../doc/direction.md) will be closed within a month unless it
   receives a concept approval from a Core Developer. For this reason, it is
   recommended that you open an issue for any such pull requests before doing
   the work, to avoid disappointment.

   You may also benefit from discussing on our IRC development channel
   [#luanti-dev](https://docs.luanti.org/about/irc/). Note that a proper IRC client
   is required to speak on this channel.

3. Start coding!
    - Refer to the
      [Lua API](https://github.com/luanti-org/luanti/blob/master/doc/lua_api.md),
      [Luanti Documentation](https://docs.luanti.org/) and other
      [documentation](https://github.com/luanti-org/luanti/tree/master/doc).
    - Follow the [C/C++](https://docs.luanti.org/for-engine-devs/code-style-guidelines/) and
      [Lua](https://docs.luanti.org/for-engine-devs/lua-code-style-guidelines/) code style guidelines.
    - Check your code works as expected and document any changes to the Lua API.
    - To avoid conflicting changes between contributions, do not do the following manually. They will be done before each release.
      - Run `updatepo.sh` or update `luanti.po{,t}` even if your code adds new translatable strings.
      - Update `minetest.conf.example` and `settings_translation_file.cpp` even if your code adds new core settings.

4. Commit & [push](https://help.github.com/articles/pushing-to-a-remote/) your changes to a new branch (not `master`, one change per branch)
    - Commit messages should:
        - Use the present tense.
        - Be descriptive. See the commit messages by core developers for examples.
    - The first line should:
        - Start with a capital letter.
        - Be a compact summary of the commit.
        - Preferably have less than 70 characters.
        - Have no full stop at the end.
    - The second line should be empty.
    - The following lines should describe the commit, starting a new line for each point.

5. Once you are happy with your changes, submit a pull request.
     - Open the [pull-request form](https://github.com/luanti-org/luanti/pull/new/master).
     - Add a description explaining what you've done (or if it's a
       work-in-progress - what you need to do).
     - Make sure to fill out the pull request template.

### A pull-request is considered merge-able when:

1. It follows [the roadmap](../doc/direction.md) in some way and fits the whole
   picture of the project.
2. It works.
3. It follows the code style for
   [C/C++](https://docs.luanti.org/for-engine-devs/code-style-guidelines/) or
   [Lua](https://docs.luanti.org/for-engine-devs/lua-code-style-guidelines/).
4. The code's interfaces are well designed, regardless of other aspects that
   might need more work in the future.
5. It uses protocols and formats which include the required compatibility.

## Issues

If you experience an issue, we would like to know the details - especially when
a stable release is on the way.

1. Do a quick search on GitHub to check if the issue has already been reported.
2. Is it an issue with the Luanti *engine*? If not, report it
   [elsewhere](http://www.luanti.org/development/#reporting-issues).
3. [Open an issue](https://github.com/luanti-org/luanti/issues/new) and describe
   the issue you are having - you could include:
     - Error logs (check the bottom of the `debug.txt` file).
     - Screenshots.
     - Ways you have tried to solve the issue, and whether they worked or not.
     - Your Luanti version and the content (games, mods or texture packs) you have installed.
     - Your platform (e.g. Windows 10 or Ubuntu 15.04 x64).

After reporting you should aim to answer questions or clarifications as this
helps pinpoint the cause of the issue (if you don't do this your issue may be
closed after 1 month).

## Feature requests

Feature requests are welcome but take a moment to see if your idea follows
[the roadmap](../doc/direction.md) in some way and fits the whole picture of
the project. You should provide a clear explanation with as much detail as
possible.

## Translations

The core translations of Luanti are performed using Weblate. You can access
the project page with a list of current languages
[here](https://hosted.weblate.org/projects/minetest/minetest/).

Builtin (the component which contains things like server messages, chat command
descriptions, privilege descriptions) is translated separately; it needs to be
translated by editing a `.tr` text file. See
[Translation](https://docs.luanti.org/for-creators/translation/) for more information.

## Donations

If you'd like to monetarily support Luanti development, you can find donation
methods on [our website](http://www.luanti.org/development/#donate).

# Maintaining

* This is a concise version of the
  [Rules & Guidelines](https://docs.luanti.org/for-engine-devs/) on the Luanti Documentation.*

These notes are for those who have push access Luanti (core developers / maintainers).

- See the [project organisation](https://docs.luanti.org/for-engine-devs/organization/) for the people involved.

## Concept approvals and roadmaps

If a Pull Request is not a bug fix:

* If it matches a goal in [the roadmap](../doc/direction.md), then the PR should
  be labeled as "Roadmap" and the goal stated by number in the description.
* If it doesn't match a goal, then it needs to receive a concept approval within
  a week of being opened to remain open. This 1 week deadline does not apply to
  PRs opened before the roadmap was adopted; instead, they may remain open or be
  closed as needed. Use the "Concept Approved" label. Issues can be marked as
  "Concept Approved" to give preapproval to future PRs.

## Reviewing pull requests

Pull requests should be reviewed and, if appropriate, checked if they achieve
their intended purpose. You can show that you are in the process of, or will
review the pull request by commenting *"Looks good"* or something similar.

**If the pull-request is not [merge-able](#a-pull-request-is-considered-merge-able-when):**

Submit a comment explaining to the author what they need to change to make the
pull-request merge-able.

- If the author comments or makes changes to the pull-request, it can be
  reviewed again.
- If no response is made from the author within 1 month (when improvements are
  suggested or a question is asked), it can be closed.

**If the pull-request is [merge-able](#a-pull-request-is-considered-merge-able-when):**

Submit a :+1: (+1) or "Looks good" comment to show you believe the pull-request should be merged. "Looks good" comments often signify that the patch might require (more) testing.

- Two core developers must agree to the merge before it is carried out and both should +1 the pull request.
- Who intends to merge the pull-request should follow the commit rules:
    - The title should follow the commit guidelines (title starts with a capital letter, present tense, descriptive).
    - Don't modify history older than 10 minutes.
    - Use rebase, not merge to get linear history:
    - `curl -Ls https://github.com/luanti-org/luanti/pull/1.patch | git am`

## Reviewing issues and feature requests

- If an issue does not get a response from its author within 1 month (when requiring more details), it can be closed.
- When an issue is a duplicate, refer to the first ones and close the later ones.
- Tag issues with the appropriate [labels](https://github.com/luanti-org/luanti/labels) for devices, platforms etc.

## Releasing a new version

*Refer to [docs.luanti.org/for-engine-devs/releasing-luanti](https://docs.luanti.org/for-engine-devs/releasing-luanti/)*
