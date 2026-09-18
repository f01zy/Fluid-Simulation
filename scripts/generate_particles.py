import random

def generate_particles(
    filename="particles.in",
    grid_origin=None,
    grid_size=None,
    origin=(0.0, 0.0, 0.0),
    size=(10, 10, 10),
    ppc=8,
    cell_size=1.0,
    initial_vel=(0.0, 0.0, 0.0)
):
    particles = []
    nx, ny, nz = size
    vx, vy, vz = initial_vel

    if grid_origin is not None and grid_size is not None:
        gx, gy, gz = grid_origin
        gnx, gny, gnz = grid_size
        ox = gx + ((gnx - nx) * cell_size) / 2.0
        oy = gy + ((gny - ny) * cell_size) / 2.0
        oz = gz + ((gnz - nz) * cell_size) / 2.0
    else:
        ox, oy, oz = origin

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
        grid_origin=(-7.5, -10.0, -15.0),
        grid_size=(15, 15, 15),
        size=(10, 10, 10),
        ppc=8,
        cell_size=1.0,
        initial_vel=(0.0, 0.0, 0.0)
    )
