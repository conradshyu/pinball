# Space Fleet Pinball

A self-contained Windows pinball game written in C++17 with the Win32 API and GDI. It uses no third-party libraries or game assets. The table and its animated lights are drawn in code, and a compatible-memory-DC back buffer is blitted to the window to prevent flicker when the table is repainted or resized.

## Requirements

- Windows 10 or Windows 11
- Visual Studio 2026 (Visual Studio 18) with the **Desktop development with C++** workload
- MSVC v145 x64 build tools and a Windows 10/11 SDK

The project is configured for the v145 toolset installed with Visual Studio 18. If using another Visual Studio release, open **Pinball Properties > General** and select an installed Platform Toolset and Windows SDK.

## Build in Visual Studio

1. Extract `pinball.zip` to a writable folder.
2. Open `Pinball.sln` in Visual Studio.
3. In the toolbar, select **Debug** or **Release** and **x64**.
4. Choose **Build > Build Solution** (or press **Ctrl+Shift+B**).
5. Run with **Debug > Start Without Debugging** (or press **Ctrl+F5**).

The executable is written to `x64\Debug\Pinball.exe` or `x64\Release\Pinball.exe` inside the project folder.

## Build from a Visual Studio Developer Command Prompt

Open **x64 Native Tools Command Prompt for VS 2026**, change to the extracted project directory, and run:

```bat
msbuild Pinball.sln /m /p:Configuration=Release /p:Platform=x64
```

The command-line build also requires the Desktop development with C++ workload and the v145 toolset.

## Controls

| Key | Action |
|---|---|
| Enter | Start or restart a game |
| Hold Space, then release | Charge and fire the plunger |
| Left Arrow / A | Left flipper |
| Right Arrow / D | Right flipper |
| Z / X | Nudge left or right; repeated nudges cause a temporary tilt |
| C | Activate the temporary center post |
| Esc | Quit |

## Gameplay

Start with three balls and score by hitting the attack, engine, and satellite bumpers. Launch timing and the lit skill lane award launch bonuses. Complete mission objectives to advance through the nine fleet ranks; successful promotions award replay balls, and the wormhole can also award an extra ball. The fuel meter drains during play and refills on bumper hits. A tilt temporarily disables both flippers. Score, high score, ball, rank, mission, fuel, and replay counts appear in the right-side display.

This is an original, lightweight GDI pinball implementation inspired by classic space-themed tables; it does not include Microsoft's original Space Cadet artwork, soundtrack, or hidden developer codes.

## Project files

- `Pinball.sln` — Visual Studio solution
- `Pinball.vcxproj` — x64 Debug and Release application project
- `Pinball.cpp` — Win32 window, double-buffered rendering, table, input, and gameplay
