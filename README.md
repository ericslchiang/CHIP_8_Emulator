## CHIP-8 Emulator by eslchiang
### March 18th 2025

![](assets/output.gif)

######
An Chip-8 interpreter emulator written in C and rendered using raylib.  
Run `cmake ..\`, then `cmake --build .` from the `build` directory to build the project.  
To start the emulator, run `chip8emulator.exe` along with a the name of the desire ROM and desired game speed (default is 8 instructions per second).   

ex: `.\chip8emulator.exe brix 9`  

To add more ROMs, simply drag `.ch8` files into the ROM folder.

##### Notes
`cmake` will clone the raylib repository directly into the `build` directory. I didn't want to bloat the code size, but I could not figure out how to properly structure the cmake code for Windows. 