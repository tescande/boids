# Boids

This repository contains an implementation of the [boids artificial life program](https://en.wikipedia.org/wiki/Boids) developed by [Craig Reynolds](https://en.wikipedia.org/wiki/Craig_Reynolds_(computer_graphics)) in 1986.

![Screeshot](screenshot.png)

## Building

The application depends on **GLib-2.0** and the **GTK+-3.0** toolkit. Once the development packages of these libraries are installed, just type **make**.

## Boids Behavior

### Rules

The boids obey to 3 main rules:
- **Avoidance**: A boid avoids its very close neighbors
- **Alignment**: A boid aligns with its neighbors
- **Cohesion**: Boids tend to aggregate together

You can enable / disable rules by with their corresponding check-boxes.

There is also a rule that defines the boid **field of view** dead-angle. It's the angle in the back of a boid in which it cannot see its neighbors.

### Obstacles

You can add **obstacles** by clicking in the field. Use Ctrl+Click on an **obstacle** to remove it.

You can also add **walls** with the corresponding checkbox.
