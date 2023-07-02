# Neural Network Visualization in Vulkan/C++

![Language: C++17](https://img.shields.io/badge/Language-C%2B%2B17-blue)
![Graphics API: Vulkan](https://img.shields.io/badge/Graphics%20API-Vulkan-A41D21)
[![license](https://img.shields.io/badge/license-MIT-green.svg)](https://git.stabletec.com/utilities/vksbc/blob/master/LICENSE)


Visualization of a trained neural network from my other [repository](https://github.com/aotodev/artificial_neural_networks).

The application is interactive and the entire model is rendered (all neurons and synapses).

### Controls:
- Desktop: mouse-scroll to zoom in and out and middle mouse button to rotate the model
- Android: Move your finger to move the model and pinch to zoom in and out

The best way to play with it is to just download the compiled builds on the Release section of this repository. There are builds for 64-bit Windows, Linux and 32-bit & 64-bit APKs for Android.

If you prefer to build it from source, follow the instructions bellow.


https://github.com/aotodev/neural_network_visualization/assets/85173635/96b13468-031e-457a-9053-e0c0996b90f8


## Build
### Android
You can build it for android using android studio with the latest Android SDK and NDK, which will already include all the necessary Vulkan dependencies.

### Windows and Linux
To build on Windows or Linux you need to have the [Vulkan SDK](https://www.lunarg.com/vulkan-sdk) installed.

If you already have it you can easily build by running the build scripts on the folder scripts/windows or scripts/linux.
