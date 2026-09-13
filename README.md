## Building
To get started, clone this repository with all the submodules:

```bash
git clone --recurse-submodules https://github.com/f01zy/Fluid-Simulation fluid-simulation && cd fluid-simulation 
```

If you have NixOS then run the nix-shell to load wayland, X11 and OpenGL dependencies:

```bash
nix-shell
```

And now build the project:

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

## Usage

You can change the simulation settings in `settings.json`. Also you can generate particles cube by `scripts/generate_particles.py` script. then just run the simulation:

```bash
./fluid-simulation settings.json
```

## Acknowledgments / References

- Chand T. John, ["Physics-Based Simulation & Animation of Fluids"](https://unusualinsights.github.io/fluid_tutorial/).

