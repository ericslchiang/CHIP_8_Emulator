// #include <stdlib.h>
#include <stdio.h>

#define MEM_SIZE 4096
#define STACK_SIZE 12
#define NUM_REG 16
#define DISPLAY_X 64
#define DISPLAY_Y 32

//Stored in memory from 0x50 to 0x9F
unsigned char font[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

//Setup virtual memory and registers
unsigned short opcode;
unsigned char memory[MEM_SIZE] = {0};
unsigned vRegister[NUM_REG] = {0}; //Vf register doubles as a flag for certain operations, avoid using as general purpose register
unsigned short iReg = 0; //Points to location in memory. 12 bits wide
unsigned short stack[STACK_SIZE] = {0}; //Used for storing address when entering subroutines
unsigned short stackIndex = 0;
unsigned short pc = 0x200; //Program Counter. chip-8 expects programs to be loaded in at 0x200
unsigned char delayTimer;
unsigned char soundTimer;
unsigned long pixelMap[32] = {0}; //Each long is a horizontal row of 64 pixels, 32 rows high (left(MSB)->right(LSB), top(0)->bottom(31)) 

void clearScreen(void) {
    
}

char executeOpcode(unsigned short opcode){
    char incrementPC = 1;
    switch(opcode & 0xF000) {
        case 0x0000:
            switch(opcode & 0x00FF){
                case 0x00E0: //Clears Screen
                    //clearScreen();
                    for (int i = 0; i < DISPLAY_Y; i++)
                        pixelMap[i] = 0;
                    break;
                case 0x00EE: //Returns from subroutine
                    pc = stack[stackIndex--];
                    break;
                default: //Execute machine code 
                    printf("opcode 0x0NNN is left unimplemented");
                    break;
            }
            break;
        case 0x1000: //GOTO address in opcode 0x1NNN
            pc = opcode & 0x0FFF;
            incrementPC = 0;
            break;
        case 0x2000: //Execute subroutine at address in opcode 0x2NNN
            stack[stackIndex++] = pc;
            pc = opcode & 0x0FFF;
            incrementPC = 0;
            break;
        case 0x3000: //Skips next instruction if VX == NN for 0x3XNN
            if ((opcode & 0x00FF) == vRegister[(opcode & 0x0F00) >> 8])
                pc+=2;
            break;
        case 0x4000: //Skips next instruction if VX != NN for 0x4XNN
            if ((opcode & 0x00FF) != vRegister[(opcode & 0x0F00) >> 8])
                pc+=2;
            break;
        case 0x5000: //Skips next instruction if VX == VY for 0x5XY0
            if ((vRegister[(opcode & 0x0F00) >> 8]) == (vRegister[(opcode & 0x00F0) >> 4]))
                pc+=2;
            break;
        case 0x9000: //Skips next instruction if VX != VY for 0x9XY0
            if ((vRegister[(opcode & 0x0F00) >> 8]) != (vRegister[(opcode & 0x00F0) >> 4]))
                pc+=2;
            break;
        case 0x6000: //Sets VX = NN for 0x6XNN
            vRegister[((opcode & 0x0F00) >> 8)] = (opcode & 0x00FF);
            break;
        case 0x7000: //Adds NN to VX for 0x7XNN
            vRegister[((opcode & 0x0F00) >> 8)]+=(opcode & 0x00FF);
            break;
        case 0xA000: //Sets I register to NNN for 0xANNN
            iReg = (opcode & 0x0FFF);
            break;
        case 0xD000: //Draw sprite on screen. This is done by inverting (XOR) the sprite with the pixels currently on screen.
            unsigned char x, y;
            x = vRegister[(opcode & 0x0F00) >> 8] % 64; 
            y = vRegister[(opcode & 0x00F0) >> 4] % 32;
            //Screen is 64 x 32 pixels. Any sprite that exceeds those values will be clipped.
            //x and y are modulo'd to reset their coordinate position to the remainder

            //Vf is set to 1 if any pixels are flipped from set to unset, and to 0 if the former does not happen
            vRegister[0xF] = 0; 

            for (int j = 0; j++; j < opcode & 0x000F) { //For N rows in opcode = 0xDXYN
                unsigned char spriteData = memory[iReg + j];
                unsigned char bitCount = 7; //Start with leftmost pixel, which is the MSB of the byte

                for (int bitCount = 0; bitCount < 8; bitCount++) { //Iterate through each bit in the row
                    //is this pixel in this row of the sprite ON && is this pixel on the screen currently ON
                    if ((spriteData & (0x80 >> bitCount)) && (pixelMap[y] & (0x80 >> bitCount))) {  
                        pixelMap[y] &= ~(1 << x); //turn off this pixel
                        vRegister[0xF] = 1;
                    } else {
                        pixelMap[y] |= (1 << x); //Turn on this pixel
                    }

                    bitCount--;
                    x++;
                    if (x == 63) //Check if we are at the rightmost edge of the screen
                        break;
                }
                y++;
                if (y == 31) //Check if we are at the bottom edge of the screen
                    break;
            }
            break;
    }
    return incrementPC;
}

int loadProgram(unsigned char memory[]) { //Load program into memory
    FILE *program;
    if ((program = fopen("test_opcode.ch8", "rb")) == NULL) {
        printf("Error opening ROM\n");
        return 1;
    }

    unsigned char c;
    unsigned short i = 0;
    while (fread(&c, sizeof(char), 1, program) > 0){
        // printf("%02X\n", c);
        memory[i + 200] = c;
        i++;
    }

    fclose(program);
    return 0;
}


int main(void) {
    char incrementPC = 0;
    //Load font into memory
    for (int i = 0; i < 80; i++) 
        memory[i + 80] = font[i]; //80 bytes in font array, and 80 = 0x50

    // for (int i = 0; i < 200; i++){
    //     printf("%d: %x\n", i, memory[i]);
    // }

    loadProgram(memory);

    // while(1) {
    //     opcode = memory[pc] << 8 | memory[pc + 1];
    //     incrementPC = executeOpcode(opcode);
    //     
        // if (incrementPC) 
        //     pc+=2;
    // }
}

