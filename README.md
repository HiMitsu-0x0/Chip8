# CHIP-8 Emulator

A CHIP-8 emulator written in C++ using SDL2 for graphics and audio.

## Features
- Full CHIP-8 instruction set (35 opcodes)
- 64x32 monochrome display with configurable scale
- Sound timer with square wave audio output
- Keyboard input mapping to the CHIP-8 hex keypad
- Pause / resume, reset, and quit controls
- Configurable instructions per second (default: 700 IPS)

## Usage
```bash
./chip8 <path-to-rom>
```

You can find free CHIP-8 ROMs at [davidwinter/CHIP-8 ROMs](https://github.com/davidwinter/CHIP-8) or [kripod/chip8-roms](https://github.com/kripod/chip8-roms).

## Controls
CHIP-8 uses a hexadecimal keypad. The default mapping is:

| CHIP-8 Key | Keyboard |
|------------|----------|
| 1 2 3 C    | 1 2 3 4  |
| 4 5 6 D    | Q W E R  |
| 7 8 9 E    | A S D F  |
| A 0 B F    | Z X C V  |

**System keys**

| Key          | Action         |
|--------------|----------------|
| `Space`      | Pause / Resume |
| `Backspace`  | Reset ROM      |
| `Esc`        | Quit           |

## Project Structure

```
chip8.cpp    # Emulator source code
roms/        # (optional) Place your .ch8 ROM files here
```

## How It Works

The emulator implements the classic CHIP-8 fetch–decode–execute cycle:

1. Fetch a 2-byte opcode from memory at `PC`
2. Decode fields (`NNN`, `NN`, `N`, `X`, `Y`)
3. Execute the corresponding instruction
4. Update timers at 60 Hz
5. Render the display when a draw instruction is executed

The memory layout follows the CHIP-8 spec:

- `0x000–0x1FF` — interpreter and font data
- `0x200–0xFFF` — ROM and program data

## References
- [Cowgod's CHIP-8 Technical Reference](http://devernay.free.fr/hacks/chip8/C8TECH10.HTM)
- [ Tobias V. Langhoff — Guide to making a CHIP-8 emulator](https://tobiasvl.github.io/blog/write-a-chip-8-emulator/)
- [SDL2 Documentation](https://wiki.libsdl.org/SDL2/FrontPage)
