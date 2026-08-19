<!-- markdownlint-disable-file MD033 -->

# SpaceCadetPinball

## Summary

Reverse engineering of `3D Pinball for Windows - Space Cadet`, a game bundled with Windows.

## How to play

Place compiled executable into a folder containing original game resources (not included).\
Supports data files from Windows and Full Tilt versions of the game.

## Cabinet mode

Splits the game over the screens of a virtual pinball cabinet. Start the game with
`-cabinet` to enable it; there is no setting for it, because the screen layout cannot be
rearranged sensibly while the game is running. Everything else is configured in
`Options -> Graphics -> Cabinet Settings...`, is visible whether or not cabinet mode is
active, and persists to the settings file.

Without `-cabinet` the game runs in a single window whose display, general position
(centered, top/bottom left/right, or custom X/Y) and size are configurable.

In cabinet mode, up to three screens are used:

* **Playfield** - the main window. The score sidebar is cropped away, and the table can be
  rotated by 0, 90, 180 or 270 degrees for a portrait mounted monitor. Mouse and overlay
  coordinates follow the rotation.
* **Backglass** - a static image supplied by you, scaled to fit without distortion. Point
  `Backglass Image` at a file in the media folder (`Cabinet Media Path`, default `cabinet`,
  relative to the game data folder) or at an absolute path. PNG and JPG need `SDL2_image` at
  build time, otherwise use BMP. Disable this screen if a frontend already draws a backglass.
* **DMD** - a simulated dot matrix display carrying player, ball, score and mission messages.
  The dot grid size (default 128x48), dot color and unlit dot visibility are configurable.
  The layout reserves the status and mission lines first, so they are never squeezed out by
  the score, and it adapts to the grid: a shorter grid drops message lines, a taller one
  grows the score digits.

Each screen has its own display index, position, size and fullscreen flag. A size of 0 means
"fill the chosen display". Cabinet mode also hides the menu bar and the
mouse cursor, which is optional.

Cabinet hardware and frontends are supported through:

* **VSync** - on by default, since tearing is obvious on a large playfield screen.
* **Pause On Focus Loss** - turn it off so a cabinet keeps playing when a frontend or an
  overlay takes focus. Otherwise losing focus mutes the audio and stops the game.
* **Command line** - `-cabinet` enables cabinet mode for the run, and `-playfield-display N`,
  `-backglass-display N` and `-dmd-display N` let a frontend drive the layout without
  touching the settings file. Display overrides are not saved back over the stored options.
* **High score entry without a keyboard** - in cabinet mode a new high score asks for three
  initials picked with the cabinet buttons: the flippers cycle the character, the plunger
  accepts it, `<` rubs out the previous one and the bottom bump clears the whole entry. It is
  mirrored on the DMD.

## Known source ports

| Platform           | Author          | URL                                                                                                        |
| ------------------ | --------------- | ---------------------------------------------------------------------------------------------------------- |
| PS Vita            | Axiom           | <https://github.com/suicvne/SpaceCadetPinball_Vita>                                                        |
| Emscripten         | alula           | <https://github.com/alula/SpaceCadetPinball> <br> Play online: <https://alula.github.io/SpaceCadetPinball> |
| Nintendo Switch    | averne          | <https://github.com/averne/SpaceCadetPinball-NX>                                                           |
| webOS TV           | mariotaku       | <https://github.com/webosbrew/SpaceCadetPinball>                                                           |
| Android (WIP)      | Iscle           | https://github.com/Iscle/SpaceCadetPinball                                                                 |
| Nintendo Wii       | MaikelChan      | https://github.com/MaikelChan/SpaceCadetPinball                                                            |
| Nintendo 3DS       | MaikelChan      | https://github.com/MaikelChan/SpaceCadetPinball/tree/3ds                                                   |
| Nintendo DS        | Headshotnoby    | https://github.com/headshot2017/3dpinball-nds                                                              |
| Nintendo Wii U     | IntriguingTiles | https://github.com/IntriguingTiles/SpaceCadetPinball-WiiU                                                  |
| PlayStation 2      | Headshotnoby    | https://github.com/headshot2017/3dpinball-ps2                                                              |
| Sega Dreamcast     | Headshotnoby    | https://github.com/headshot2017/3dpinball-dc                                                               |
| MorphOS            | BeWorld         | https://www.morphos-storage.net/?id=1688897                                                                |
| AmigaOS 4          | rjd324          | http://aminet.net/package/game/actio/spacecadetpinball-aos4                                                |
| Android (WIP)      | fexed           | https://github.com/fexed/Pinball-on-Android                                                                |

Platforms covered by this project: desktop Windows, Linux and macOS.

<br>
<br>
<br>
<br>
<br>
<br>

## Source

* `pinball.exe` from `Windows XP` (SHA-1 `2A5B525E0F631BB6107639E2A69DF15986FB0D05`) and its public PDB
* `CADET.EXE` 32bit version from `Full Tilt! Pinball` (SHA-1 `3F7B5699074B83FD713657CD94671F2156DBEDC4`)

## Tools used

`Ghidra`, `Ida`, `Visual Studio`

## What was done

* All structures were populated, globals and locals named.
* All subs were decompiled, C pseudo code was converted to compilable C++. Loose (namespace?) subs were assigned to classes.

## Compiling

Project uses `C++11` and depends on `SDL2` libs.\
`SDL2_image` is optional; it is only used to load PNG/JPG cabinet backglass art, and can be
disabled with `-DUSE_SDL_IMAGE=OFF`. Without it, backglass art has to be a BMP file.

### On Windows

Download and unpack devel packages for `SDL2` and `SDL2_mixer` (and optionally `SDL2_image`).\
Set paths to them in `CMakeLists.txt`, see suggested placement in `/Libs`.\
Compile with Visual Studio; tested with 2019.

### On Linux

Install devel packages for `SDL2` and `SDL2_mixer` (and optionally `SDL2_image`).\
Compile with CMake; tested with GCC 10, Clang 11.\
To cross-compile for Windows, install a 64-bit version of mingw and its `SDL2` and `SDL2_mixer` distributions, then use the `mingwcc.cmake` toolchain.

[![Packaging status](https://repology.org/badge/tiny-repos/spacecadetpinball.svg)](https://repology.org/project/spacecadetpinball/versions) 

Some distributions provide a package in their repository. You can use those for easier dependency management and updates.

This project is available as Flatpak on [Flathub](https://flathub.org/apps/details/com.github.k4zmu2a.spacecadetpinball).

### On macOS

Install XCode (or at least Xcode Command Line Tools with `xcode-select --install`) and CMake.

**HomeBrew**

You can easily install the build artifact by using `brew`.

```sh
brew tap draftbrew/tap
brew install --no-quarantine space-cadet-pinball
```

Be aware that the flag `--no-quarantime` will disable macOS's Gatekeeper during installation.

**Manual compilation:**

* **Homebrew**: Install the `SDL2`, `SDL2_mixer` homebrew packages.
* **MacPorts**: Install the `libSDL2`, `libSDL2_mixer` macports packages.

Compile with CMake. Ensure that `CMAKE_OSX_ARCHITECTURES` variable is set for either `x86_64` Apple Intel or `arm64` for Apple Silicon.

Tested with: macOS Big Sur (Intel) with Xcode 13 & macOS Montery Beta (Apple Silicon) with Xcode 13.

**Automated compilation:**

Run the `build-mac-app.sh` script from the root of the repository. The app will be available in a DMG file named `SpaceCadetPinball-<version>-mac.dmg`.

Tested with: macOS Ventura (Apple Silicon) with Xcode Command Line Tools 14 & macOS Big Sur on GitHub Runner (Intel) with XCode 13.

## Plans

* ~~Decompile original game~~
* ~~Resizable window, scaled graphics~~
* ~~Loader for high-res sprites from CADET.DAT~~
* ~~Cross-platform port using SDL2, SDL2_mixer, ImGui~~
* Full Tilt Cadet features
* Localization support
* Maybe: Support for the other two tables - Dragon and Pirate
* Maybe: Game data editor

## On 64-bit bug that killed the game

I did not find it, decompiled game worked in x64 mode on the first try.\
It was either lost in decompilation or introduced in x64 port/not present in x86 build.\
Based on public description of the bug (no ball collision), I guess that the bug was in `TEdgeManager::TestGridBox`
