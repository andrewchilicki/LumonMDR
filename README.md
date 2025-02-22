# Introduction

The goal of this project was to create a working miniature desktop display inspired by the 'Macrodata Refinement' computers featured in Apple TV's *Severance*. This build consists of two key aspects: the **software** and the **hardware design**, both outlined below.

Feel free to email comments, suggestions, or inquiries directly to andrewchilx@gmail.com.

<p align="center">
  <img src="https://github.com/user-attachments/assets/aaed01c5-b200-4d43-9d2c-5a4399ba4960" width="98%">
</p>
<p align="center">
  <img src="https://github.com/user-attachments/assets/dc987a56-2a41-4780-ac42-345e78c8344e" width="49%">
  <img src="https://github.com/user-attachments/assets/f46a447d-42ce-47af-b2ae-17409240cb9f" width="49%">
</p>
<p align="center">
  <img src="https://github.com/user-attachments/assets/15eabc19-85e8-4757-908c-e7565946b3b8" width="49%">
  <img src="https://github.com/user-attachments/assets/54204491-7dae-41e7-aed4-271cc2c316bf" width="49%">
</p>

---
---

# I. The Software

#### Objectives

  - Generate a randomized **number grid** with grouped 'bad numbers'.  
  - Animate and style the **interface** to match the aesthetics of the show. 

## a. The Number Grid  

![generation](https://github.com/user-attachments/assets/ba004291-8ea9-4b84-995f-f6229e6b8b41)

> **GENERATE**  
  - A *N×N* grid is created.  
  - Each cell is filled with a random integer between 0-9.  

> **MAP**  
  - A Perlin noise map assigns smooth gradient values to the grid.  
  - This ensures numbers are naturally grouped rather than randomly scattered.  

> **THRESHOLD**  
  - Each cell's value is compared to a set threshold.  
  - Values above the threshold are marked as 'bad'/'scary' numbers.  

> **GROUP**  
  - 'Bad' numbers are grouped based on proximity to one another.  
  - At random intervals, a group visible in the viewport will be activated:  
    - If hovered over, they become 'super active', extending their active time and appearing agitated.  
    - If clicked, the group is **'refined'**, animating into a pre-determined bin and resetting as no longer 'bad'.  

## b. The Interface

<p align="center">
  <img src="https://github.com/user-attachments/assets/14310434-81b7-4f62-8833-948708edee78" width="30%">&nbsp;
  <img src="https://github.com/user-attachments/assets/6e2145ac-08f5-44f9-bdad-ebf272cbc0ce" width="30%">&nbsp;
  <img src="https://github.com/user-attachments/assets/5d105682-2eb9-4c84-9806-cfafa7e4dd91" width="30%">
</p>

- A moving Perlin noise map offsets each number (vertically or horizontally).  
- Activated bad groups expand / contract / jitter.
- Numbers scale-up based on cursor position.
- Refined groups animate into bins, with a percent bar keeping track of your progress.  

#### Additional Features:
- The application is cross-compatible, allowing compilation for both Linux and Raspberry Pi (ARM).
- A *'debug mode'* reveals various settings (which can be saved / loaded from disk as JSON).
- An *'idle mode'* can be enabled to display the Lumon logo screensaver.
- The user can navigate the full grid using arrow and zoom keys.

## Install
```
# Clone Repository
git clone https://github.com/andrewchilicki/LumonMDR

# Install CMake
sudo apt install g++ cmake

# Create build directory and navigate to it
cd LumonMDR && \
   mkdir build && cd build

# Generate build files and compile
cmake .. && make

# Copy LumonMDR and navigate back
cp LumonMDR .. && cd ..
```
> [!NOTE]  
> See below, ff using a headless Pi with Raspbian LiteOS, you may require additional dependecies.

<details closed>
<summary>Installation Instructions (Debia/Ubuntu)</summary>

To install the required dependencies, run the following:

```bash
sudo apt-get install libglfw3-dev libglew-dev libxinerama-dev libxxf86vm-dev libxcursor-dev
```

Additionally, set the DISPLAY variable
```bash
export DISPLAY=:0
```
</details>

---
---

# II. Hardware Design

#### Objectives

  - Reconstruct the computer housing from the show as a printable 3D model.
  - Design the interior to support a RaspberryPi and LCD screen 

## Final Design

<p align="center">
  <img src="https://github.com/user-attachments/assets/874d9d46-c7f7-44cd-a9ec-21d55afb4973" width="30%">&nbsp;
  <img src="https://github.com/user-attachments/assets/67b06a72-48b5-4889-83c1-97fb0fe5fab0" width="30%">&nbsp;
  <img src="https://github.com/user-attachments/assets/2189913c-6f3b-453f-a94a-37ee4acf72d1" width="30%">
</p>
<p align="center">
  <img src="https://github.com/user-attachments/assets/2b0c8d53-02ca-4402-87ce-a6af0ef87a9b" width="60%">&nbsp;
  <img src="https://github.com/user-attachments/assets/3c73aa3b-753d-4f1e-9f5e-b81ce0db6a6c" width="30%">
</p>

- The computer’s outer shell was modeled in Blender from the ground up, closely following reference images from the show to capture the original look.
- The design was then optimized for 3D printing.
  - The structure was made modular, allowing for easy assembly and disassembly.
  - Each part includes a flat printing surface, enabling clean prints without the need for supports.
  - Tolerances were incorporated to ensure a snug fit for all components.
- Finally, the interior was equipped with an adjustable Raspberry Pi mount.
  - The mounting bracket can move laterally, and the RPi forward, allowing for fine-tuned alignment of the LCD screen with the front panel.

## Final Product
<p align="center">
  <img src="https://github.com/user-attachments/assets/b8fa5a15-ae32-4bd2-a9c3-7a4a64b0ad40" width="90%">
</p>

## Build Photos

<p align="center">
  <img src="https://github.com/user-attachments/assets/661fbc69-a9db-4aee-9691-d9dad5d3d9fc" width="30%">&nbsp;
  <img src="https://github.com/user-attachments/assets/be88fc36-a475-4b3f-a740-7af5ddd39ef7" width="30%">&nbsp;
  <img src="https://github.com/user-attachments/assets/8f4e8245-6b6e-4912-96fd-88ff38a1b87e" width="30%">
</p>
<p align="center">
  <img src="https://github.com/user-attachments/assets/c5e0b223-2800-4896-ac11-5bf557b85046" width="30%">&nbsp;
  <img src="https://github.com/user-attachments/assets/190788f9-335e-4f94-a391-0c0d07545d1f" width="30%">&nbsp;
  <img src="https://github.com/user-attachments/assets/9e747680-1f53-4fea-b93f-71308c976430" width="30%">
</p>
