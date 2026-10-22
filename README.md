# Whisk 3D

<p align="center">
    <img src="logo_outlined.svg" width="400" alt="Whisk3D logo">
</p>

## Retro-style 2D and 3D cross-platform engine

Whisk3D is a retro-style 2D/3D creation tool aimed at bringing modeling, rendering, animation, and game development to any device, including "obsolete" ones.

It was born as a lightweight Blender clone for Nokia, but today it is much more: a creative environment centered on low-poly aesthetics and the visual identity of PlayStation, Dreamcast, and retro PCs. Whisk3D prioritizes speed, accessibility, and fun over excessive complexity.

## Free Software!

The MIT license allows anyone to freely use it to create games, commercial, educational, or artistic projects, and even fork and expand the tool without restrictions.

**It is currently in pre-alpha**, a first look at something that is still growing, but with a clear vision: enabling anyone to model, animate, design, render, and develop 3D games without needing a modern PC or facing intimidating learning curves.

Whisk3D is built on a simple philosophy: **hardware shouldn't die because of software**. I aim to show that planned obsolescence can be reversed through code—that a machine considered "old" can continue creating, imagining, and generating new content.

The project, and the games developed with it, plan to sustain themselves through voluntary donations, **not with microtransactions, annoying ads, loot boxes, or mechanisms designed to create addiction or manipulate the player.**

Whisk3D **DOES NOT** install hidden services, spy on the user, sell user data, or require accounts or a permanent internet connection. **We respect creators and players**.

We want to **give you back control of your software**, democratize creation, and allow new generations to experiment with 3D **without technical or financial barriers**.

![](https://i.ibb.co/hp1w0DH/Captura-desde-2025-11-26-15-19-37.png)

YouTube: [Youtube Video](https://youtu.be/dMe-Vit5OT0)

## Build and Install

Whisk3D currently runs on Linux, Windows, Mac, Android, Symbian, and Web. Instructions for each system can be found in:

- [Windows](platform/windows/README.md) · [GNU/Linux](platform/linux/README.md) · [macOS](platform/mac/README.md)
- [Web (WebGL)](platform/web/README.md) · [Android](platform/android/README.md) · [Symbian s60v3/s60v5/Symbian^3 (N95/N8)](platform/symbian/README.md)

On all systems, run first: `git submodule update --init --recursive` (the `platform/<system>/build_*` scripts already do this for you).

## Clarifications:

1. No is a port: This program **does not share** code with Blender.
2. Planned obsolescence: The goal of the project **is to highlight the planned obsolescence** of both old and current phones. I aim to drive consumer demand for more sustainable and durable phones, reducing the massive amount of electronic waste. This would also help more people access technology through the recycling of these legacy devices.
3. Computer science literacy: We want to highlight the general lack of computer science knowledge. It shouldn't be surprising that Whisk 3D runs on a Nokia N95—a phone that was high-end in its time, comparable to an iPhone 17 Pro Max today. I want to make it clear that with 300 MHz and 128 MB of RAM, you can perform tasks that nowadays require phones with 8 GB of RAM and multi-core processors.
4. Rendering and ray tracing: Whisk 3D can already perform rendering. In computer science terms, rendering is the process of generating an image from a 2D or 3D model. Although I haven't implemented ray tracing yet, the Nokia N95 is capable of doing it via its CPU and FPU (Floating-Point Unit). Movies like Terminator 2 and Jurassic Park used SGI Onyx computers from 1993. The Nokia N95, released in 2007, came 14 years later and is comparable in power. It is concerning that so many people are surprised that an older phone can perform rendering.
5. Graphics performance: Whisk 3D runs smoothly thanks to the hardware **graphics acceleration** of the N95 (PowerVR MBX Lite) and its FPU, allowing it to move complex 3D models like Arthur Morgan from Red Dead Redemption 2.

![](https://pbs.twimg.com/media/GPawZAKWsAA7Rw9?format=png) ![](https://pbs.twimg.com/media/GPawalTWUAAe-J1?format=png)

## Thank you!

Thanks to everyone who shared the project, and to the people who are starting to contribute to the code.
Thanks to Marcig for the new name!

# Get involved!

I plan to add more features.
Any code contribution you would like to make is welcome.
