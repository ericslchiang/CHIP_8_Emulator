## CHIP-8 Emulator by eslchiang
### March 18th 2025  

![](assets/output.gif)

######
An Chip-8 interpreter emulator written in C and rendered using raylib. Build using   
`cmake ..\`  
`cmake --build .`  
from the `build` directory to compile the project.  

To start the emulator, run `chip8emulator.exe` along with a the name of the desired ROM and game speed (default is 8 instructions per second).   

_ex:_ `.\chip8emulator.exe brix 9`  

To add more ROMs, copy + paste `.ch8` files into the ROM folder.

#### Notes:
`cmake` will clone the raylib repository directly into the `build` directory. I didn't want to bloat the code size, but I could not figure out how to properly structure the cmake code for Windows. 