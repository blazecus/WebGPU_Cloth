This is a cloth simulator written in WebGPU, using compute shaders to simulate a particle system that approximates cloth movement. It also includes a sphere that collides with the cloth, showing some movement. 

Parameters can be adjusted in a GUI element while running the demo. You can try the demo online here, but make sure your browser has WebGPU compatibility:

https://jackblazes.net/demos/cloth_demo/App.html

To run the demo locally, simply run these commands:

cmake . -B build
cmake --build build

Then run the resulting App/App.exe.

This was written in a C++ wrapper for WebGPU - big thanks to Élie Michel for his guide to WebGPU for C++ and the accompanying wrappers he wrote: https://eliemichel.github.io/LearnWebGPU/
