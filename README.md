# Soil Respiration Project
Repository of the soil respiration project

# Building
To build the image, run the following commands:
```
west build
west sign -t imgtool
```
# Flashing
To flash, run:
```
west flash
```
# OTA
To update the firmware over-the-air, run the following commands in the ./build/zephyr directory:
```
sudo python3 -m http.server 80
```
