# v4l2loopback playground

This repository contains code used to play around with v4l2loopback devices.

Currently there is code to :
* List loopback devices 
* Write static images to the device
* Write OpenGL textures to the loopback device (!)

## Build the Playground

Simply run
```
make
```

## Add a Loopback Device

For the more advanced examples, a loopback device should be created. Create one with

```
modprobe v4l2loopback video_nr=10 card_label="Loopback Playground Cam"
```

## Run the Experiments

After building the executable the usage can be printed by just executing

```
./main
```

The most advanced example proberbly is:

```
./main send-texture /dev/video10 <path-to-image>
```
This example loads an image into an OpenGL texture applies a greyscale shader and animates the saturation in a loop while seinding frames to the loopback device.

## Display the Output

There a proberbly a lot of utilities to test the devices output. But I like to use:

* `ffplay /dev/video10`
* https://de.webcamtests.com/


```
Copyright (C) 2024 Mario Aichinger <aichingm@gmail.com>

This program is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License as published by the Free Software Foundation, version 3.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.

```

