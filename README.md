# Introduction

A recreation of the 'Macrodata Refinement' terminal from Apple TV's *Severance*, written in C++17 with Dear ImGui, GLFW and OpenGL 3. It runs on Windows and Linux.

Based on the original project by Andrew Chilicki (andrewchilx@gmail.com).

---

# I. The Session

1. **Boot.** Press Enter or click to start. The terminal types out its boot text, then draws the Lumon logo. Enter or a click skips ahead a stage.
2. **Pick a file.** Type a file name at the `> ` prompt and press Enter. The file wheel spins to it. Enter or a click skips the spin.
3. **Refine.** Find the bad numbers and click them to send them to the bins.
4. **Complete.** At 100% the screen cracks open to reveal a video, followed by "I love you!". It then fades back to the logo and the file picker.

Each file keeps its own grid and progress until the app closes. Some files start partly or fully refined. Reopening a completed file shows "File complete".

There may be something waiting for whoever finishes Labrador.

## Controls

| Key | Action |
|---|---|
| Enter / click | Start the boot, skip ahead, choose a file |
| Arrow keys, or the mouse at the grid edges | Scroll the grid |
| Mouse wheel, `,` / `.` | Zoom |
| Left click | Refine the hovered group |
| Esc | Back to the logo and file picker |
| Tab | Toggle the settings panel |
| I | Toggle idle mode (Lumon logo screensaver); click to exit |
| Ctrl+1 | Complete the open file instantly (for testing) |
| Alt+F4 | Quit |

---

# II. The Software

## a. The Number Grid

![generation](https://github.com/user-attachments/assets/ba004291-8ea9-4b84-995f-f6229e6b8b41)

> **GENERATE**
  - Each file has its own 30×30 grid, built from a fixed seed so its layout is the same every run.
  - Each cell is filled with a random integer between 0-9.

> **MAP**
  - A Perlin noise map assigns smooth gradient values to the grid.
  - This ensures numbers are naturally grouped rather than randomly scattered.

> **THRESHOLD**
  - Each cell's value is compared to a set threshold.
  - Values above the threshold are marked as 'bad'/'scary' numbers.

> **GROUP**
  - Neighbouring 'bad' numbers form a group.
  - Every so often, up to three groups become active. Each one grows, holds, then shrinks back.
    - Hovering over an active group agitates it and keeps it active for longer.
    - Clicking an active group **refines** it: its numbers fly into a bin, and the group is no longer 'bad'.
  - When only two groups are left, they are highlighted, with arrows pointing to any that are off screen.

## b. The Interface

<p align="center">
  <img src="https://github.com/user-attachments/assets/14310434-81b7-4f62-8833-948708edee78" width="30%">&nbsp;
  <img src="https://github.com/user-attachments/assets/6e2145ac-08f5-44f9-bdad-ebf272cbc0ce" width="30%">&nbsp;
  <img src="https://github.com/user-attachments/assets/5d105682-2eb9-4c84-9806-cfafa7e4dd91" width="30%">
</p>

- A moving Perlin noise map offsets each number (vertically or horizontally).
- Numbers swell near the cursor, more so when zoomed in.
- The grid scrolls and zooms smoothly, and fades out at its borders.
- Bins open their hinged lids as numbers fly in, fill their bars as each group lands, and update the header percentage.
- The boot sequence, the Lumon logo and the crack reveal on completion are all drawn procedurally.
- The UI uses the Rajdhani font (SIL Open Font License; see `assets/Rajdhani-OFL.txt`).

#### Additional Features:
- The settings panel (Tab) exposes the grid and animation settings, which can be saved / loaded from disk as JSON (`settings.json`).
- An *'idle mode'* shows the Lumon logo screensaver.

---

# III. Building

You need CMake 3.10+, a C++17 compiler, GLFW, GLEW, OpenGL, pkg-config and FFmpeg (libavformat, libavcodec, libswscale, libavutil). FFmpeg plays the completion video. Dear ImGui, nlohmann/json and stb are bundled in `external/`.

#### Windows (MSYS2 MinGW64)

```sh
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja \
          mingw-w64-x86_64-pkgconf mingw-w64-x86_64-glfw mingw-w64-x86_64-glew \
          mingw-w64-x86_64-ffmpeg
mkdir build && cd build
cmake -G Ninja ..
cmake --build .
```

A post-build step copies the MinGW runtime DLLs the exe needs next to `LumonMDR.exe`, so it runs without MSYS2 on the `PATH`.

#### Linux (Debian/Ubuntu)

```sh
sudo apt install build-essential cmake pkg-config libglfw3-dev libglew-dev \
                 libavformat-dev libavcodec-dev libswscale-dev libavutil-dev
mkdir build && cd build
cmake ..
cmake --build .
```

#### Running

Run from the repository root, since assets and settings are loaded from relative paths:

```sh
./build/LumonMDR                       # 1920×1080 window
./build/LumonMDR --full-screen         # full screen on the primary monitor
./build/LumonMDR --full-screen --monitor=2
```
