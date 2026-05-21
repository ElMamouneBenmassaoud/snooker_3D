# 3D Snooker

Real-time 3D snooker game, INFO-H502 Virtual Reality and 3D Graphics, ULB 2025-2026.

**Authors:** Imad El Harrouti, El Mamoune Benmassaoud


## Build instructions

Clone the repository and initialise the submodules (this downloads all dependencies automatically):

```bash
git clone https://github.com/ElMamouneBenmassaoud/snooker_3D.git
cd snooker_3D
git submodule update --init --recursive
```

Then build:

```bash
cmake -S . -B build
cmake --build build
```

On **Linux**, you may need to install the following packages first:

```bash
sudo apt install cmake build-essential libgl-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
```

On **macOS**, no extra packages are needed.


## Run

The executable must be launched from the project root directory:

```bash
./build/snooker
```


## Controls

| Action | Mouse | Keyboard |
|--------|-------|----------|
| Aim | Left-click drag | Left / Right arrows |
| Power | Left-click pull back | Up / Down arrows |
| Shoot | Release left-click | Space |
| Spin | Right-click drag | Q / E / W / S |
| Camera | Scroll | Tab |
| Menu | | ESC |
