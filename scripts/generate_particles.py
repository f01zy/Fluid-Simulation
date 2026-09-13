import random

def generate_particles(
    filename="particles.in",
    origin=(0.0, 0.0, 0.0),
    size=(20, 20, 20),
    ppc=8,
    cell_size=1.0,
    initial_vel=(0.0, 0.0, 0.0)
):
    particles = []
    ox, oy, oz = origin
    nx, ny, nz = size
    vx, vy, vz = initial_vel

    for x in range(nx):
        for y in range(ny):
            for z in range(nz):
                for _ in range(ppc):
                    px = ox + (x + random.uniform(0.1, 0.9)) * cell_size
                    py = oy + (y + random.uniform(0.1, 0.9)) * cell_size
                    pz = oz + (z + random.uniform(0.1, 0.9)) * cell_size
                    particles.append(f"{px:.4f} {py:.4f} {pz:.4f} {vx:.4f} {vy:.4f} {vz:.4f}")

    with open(filename, "w") as f:
        f.write(f"{len(particles)}\n")
        f.write("\n".join(particles) + "\n")

    print(f"Generated {len(particles)} particles in {filename}")

if __name__ == "__main__":
    generate_particles(
        filename="particles.in",
        origin=(-2.0, -4.5, -5.5),
        size=(4, 4, 4),
        ppc=8,
        cell_size=1.0,
        initial_vel=(0.0, 0.0, 0.0)
    )
