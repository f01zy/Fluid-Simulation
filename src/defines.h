#ifndef DEFINES_H
#define DEFINES_H

#define G                   (9.80665f)
#define FPS                 (60.0f)
#define CUSHION             (1.0e-4f)
#define EPSILON             (1.0e-6f)
#define WIDTH               (600)
#define HEIGHT              (500)
#define INVALID             ((uint32_t)-1)
#define TITLE               ("Fluid Simulation")
#define PRESSURE_ITERS      (100)
#define MAX_CAMERA_RADIUS   (100.0f)
#define PI                  (3.141592653589f)
#define IX(i, j, k, ny, nz) ((i) * (ny) * (nz) + (j) * (nz) + (k))

#endif
