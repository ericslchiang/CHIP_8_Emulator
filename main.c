#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "include\raylib.h"

#define MEM_SIZE 4096
#define STACK_SIZE 12
#define NUM_REG 16
#define DISPLAY_X 64
#define DISPLAY_Y 32
#define PIXEL_SIZE 10

//Stored in memory from 0x50 to 0x9F
uint8_t font[80] = {
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

//Keypress matrix
uint8_t keyPress[16] = {0};
/*
1 2 3 C
4 5 6 D
7 8 9 E
A 0 B F
*/

//Setup virtual memory and registers
uint16_t opcode;
uint8_t memory[MEM_SIZE] = {0};
uint8_t vRegister[NUM_REG] = {0}; //Vf register doubles as a flag for certain operations, avoid using as general purpose register
uint16_t iReg = 0; //Points to location in memory. 12 bits wide
uint16_t stack[STACK_SIZE] = {0}; //Used for storing address when entering subroutines
uint16_t stackIndex = 0;
uint16_t pc = 0x200; //Program Counter. chip-8 expects programs to be loaded in at 0x200
uint8_t delayTimer;
uint8_t soundTimer;
// unsigned long long pixelMap[32] = {0}; //Each long is a horizontal row of 64 pixels, 32 rows high (left(MSB)->right(LSB), top(0)->bottom(31)) 
uint8_t pixelMap[DISPLAY_X * DISPLAY_Y] = {0};


uint8_t executeOpcode(uint16_t opcode){
    // printf("%04X\n", opcode);
    uint8_t incrementPC = 1;
    switch(opcode & 0xF000) {
        case 0x0000:
            switch(opcode & 0x00FF){
                case 0x00E0: //Clears Screen
                    //clearScreen();
                    memset(pixelMap, 0, sizeof(pixelMap));
                    // for (int i = 0; i < DISPLAY_Y  * DISPLAY_X; i++)
                    //     printf("%d\n", pixelMap[i]);
                    break;
                case 0x00EE: //Returns from subroutine
                    pc = stack[--stackIndex];
                    break;
                default: //Execute machine code 
                    //printf("opcode 0x0NNN is left unimplemented\n");
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
            vRegister[(opcode & 0x0F00) >> 8] = (opcode & 0x00FF);
            break;
        case 0x7000: //Adds NN to VX for 0x7XNN
            vRegister[(opcode & 0x0F00) >> 8]+=(opcode & 0x00FF);
            break;
        case 0x8000: 
            switch(opcode & 0x000F){
                case 0x0000: //Set VX to value of VY for 0x8XY0
                    vRegister[(opcode & 0x0F00) >> 8] = vRegister[(opcode & 0x00F0) >> 4];
                    break;
                case 0x0001: //Bitwise OR: VX |= VY for 0x8XY1
                    vRegister[(opcode & 0x0F00) >> 8] |= vRegister[(opcode & 0x00F0) >> 4];
                    break;
                case 0x0002: //Bitwise AND: VX &= VY for 0x8XY2
                    vRegister[(opcode & 0x0F00) >> 8] &= vRegister[(opcode & 0x00F0) >> 4];
                    break;
                case 0x0003: //Bitwise XOR: VX |= VY for 0x8XY3
                    vRegister[(opcode & 0x0F00) >> 8] ^= vRegister[(opcode & 0x00F0) >> 4];
                    break;
                case 0x0004: //VX += VY for 0x8XY4
                    if (vRegister[(opcode & 0x0F00) >> 8] > (0xFF - (vRegister[(opcode & 0x00F0) >> 4]))) //Check if VX overflows
                        vRegister[0xF] = 1;
                    else
                        vRegister[0xF] = 0; 

                    vRegister[(opcode & 0x0F00) >> 8] += vRegister[(opcode & 0x00F0) >> 4];
                    break;
                case 0x0005: //VX -= VY for 0x8XY5
                    if (vRegister[(opcode & 0x0F00) >> 8] < vRegister[(opcode & 0x00F0) >> 4]) //Check if VX underflows
                        vRegister[0xF] = 0;
                    else
                        vRegister[0xF] = 1; 

                    vRegister[(opcode & 0x0F00) >> 8] -= vRegister[(opcode & 0x00F0) >> 4];
                    break;
                case 0x0006: //Right shift VX by 1 for 0x8XY6
                    vRegister[0xF] = vRegister[(opcode & 0x0F00) >> 8] & 0x01; //Save the shifted LSB into VF
                    vRegister[(opcode & 0x0F00) >> 8] >>= 1;
                    //Super chip implementation
                    // vRegister[(opcode & 0x0F00) >> 8] = vRegister[(opcode & 0x00F0) >> 4];
                    // vRegister[(opcode & 0x0F00) >> 8] >>= 1;
                    // vRegister[0xF] = vRegister[(opcode & 0x00F0) >> 4] & 0x80 ? 1 : 0;
                    break;
                case 0x0007: //VX = VY - Vx for 0x8XY7    
                    if (vRegister[(opcode & 0x00F0) >> 4] < vRegister[(opcode & 0x0F00) >> 8]) //Check if VX underflows
                        vRegister[0xF] = 0;
                    else
                        vRegister[0xF] = 1; 

                    vRegister[(opcode & 0x0F00) >> 8] = vRegister[(opcode & 0x00F0) >> 4] - vRegister[(opcode & 0x0F00) >> 8];
                    break;
                case 0x000E: //Left shft VX by 1 for 0x8XYE
                    vRegister[0xF] = vRegister[(opcode & 0x0F00) >> 8] & 0x80; //Save the shifted MSB into VF
                    vRegister[(opcode & 0x0F00) >> 8] <<= 1;
                    //Super chip implementation
                    // vRegister[(opcode & 0x0F00) >> 8] = vRegister[(opcode & 0x00F0) >> 4];
                    // vRegister[(opcode & 0x0F00) >> 8] <<= 1;
                    // vRegister[0xF] = vRegister[(opcode & 0x00F0) >> 4] & 0x80 ? 1 : 0;
                    break;
            }
        case 0xA000: //Sets I register to NNN for 0xANNN
            iReg = (opcode & 0x0FFF);
            break;
        case 0xB000: //Jumps to address NNN + V0 for 0xBNNN
            pc = (opcode & 0x0FFF) + vRegister[0];
            incrementPC = 0; 
            break;
        case 0xC000: //Sets VX to bitwise AND between NN and random number for 0xCXNN
            vRegister[(opcode & 0x0F00) >> 8] = (opcode & 0x00FF) & (rand() % 256);
            break;
        case 0xD000: //Draw sprite on screen. This is done by inverting (XOR) the sprite with the pixels currently on screen.
            uint8_t x = vRegister[(opcode & 0x0F00) >> 8] % 64; 
            uint8_t y = vRegister[(opcode & 0x00F0) >> 4] % 32;
            vRegister[0xF] = 0; 

            for (uint8_t row = 0; row < (opcode & 0x000F); row++){
                uint8_t spriteRow = memory[iReg + row];

                for (uint8_t bitCount = 0; bitCount < 8; bitCount++) {
                    uint8_t spritePixel = spriteRow & (0x80 >> bitCount);
                    if(spritePixel) {
                        if(pixelMap[x + bitCount + (y + row) * 64]) {
                            vRegister[0xF] = 1;
                        }
                        pixelMap[x + bitCount + (y + row) * 64] ^=1;
                    }
                }
            }
            break;
        /* OLD DRAW FUNCTION
            uint8_t x, y;
            x = vRegister[(opcode & 0x0F00) >> 8] % 64; 
            y = vRegister[(opcode & 0x00F0) >> 4] % 32;
            //Screen is 64 x 32 pixels. Any sprite that exceeds those values will be clipped.
            //x and y are modulo'd to reset their coordinate position to the remainder

            //Vf is set to 1 if any pixels are flipped from set to unset, and to 0 if the former does not happen
            vRegister[0xF] = 0; 

            for (int j = 0; j < (opcode & 0x000F); j++) { //For N rows in opcode = 0xDXYN
                uint8_t spriteData = memory[iReg + j];
                // uint8_t bitCount = 7; //Start with leftmost pixel, which is the MSB of the byte

                for (int bitCount = 0; bitCount < 8; bitCount++) { //Iterate through each bit in the row
                    //is this pixel in this row of the sprite ON && is this pixel on the screen currently ON
                        // printf("%llu\n", pixelMap[y]);

                    if ((spriteData & (0x80 >> bitCount)) && (pixelMap[y] & (1 << x))) {  
                        // printf("%llu\n", pixelMap[y]);
                        pixelMap[y] &= ~(1 << x); //turn off this pixel
                        // printf("%llu\n", pixelMap[y]);

                        vRegister[0xF] = 1;
                    } else {
                        pixelMap[y] |= (1 << x); //Turn on this pixel
                    }

                    x++;
                    if (x == 64) //Check if we are at the rightmost edge of the screen
                        break;
                }
                y++;
                if (y == 32) //Check if we are at the bottom edge of the screen
                    break;
            }
            break;
            */
        case 0xE000: //Skips condition based on if a key was pressed or NOT pressed
            if ((opcode & 0x000F) == 0x000E){
                if (keyPress[vRegister[(opcode & 0x0F00) >> 8]])
                    pc+=2;
            } else if ((opcode & 0x000F) == 0x0001) {
                if (!keyPress[vRegister[(opcode & 0x0F00) >> 8]])
                    pc+=2;
            }
            break;
        case 0xF000:
            uint8_t Vx = (opcode & 0x0F00) >> 8;
            switch (opcode & 0x00FF) {
                case 0x0007: //Set VX to the value of the delay timer for 0xFX07
                    vRegister[Vx] = delayTimer;
                    break;
                case 0x000A: //Sets VX to keypress
                    for (uint8_t i = 0; i < 16; i++) {
                        if (keyPress[i]){
                            vRegister[(opcode & 0x0F00) >> 8] = keyPress[i];
                            break;
                        } else {
                            incrementPC = 0;
                            break;
                        }
                    }
                    break;
                case 0x0015: //Sets delay timer to VX
                    delayTimer = vRegister[Vx];
                    break;
                case 0x0018: //Sets sound timer to VX
                    soundTimer = vRegister[Vx];
                    break;
                case 0x001E: //Adds VX to I
                    iReg += vRegister[Vx];
                    break;
                case 0x0029: //Sets I to the address of sprite of the character (0-F) stored in lower nibble of VX. 
                    iReg = memory[(vRegister[Vx] & 0x0F) * 5 + 0x50]; //Font is stored in memory starting at 0x50. Each character is 5 bytes, starting from 0 to F 
                    break;
                case 0x0033: //Stores the values of each decimal digit of VX into memory at I(hundreds), I+1(tens), I+2(ones)
                    uint8_t val = vRegister[Vx];
                    memory[iReg + 2] = val % 10;
                    val /= 10;
                    memory[iReg + 1] = val % 10;
                    val /= 10;
                    memory[iReg] = val;
                    break;
                case 0x0055: //Stores in memory starting from I values from V0~VX
                    for (uint8_t i = 0; i <= Vx; i++) //VX is inclusive
                        memory[iReg + i] = vRegister[i];
                    break;
                case 0x0065: //Stores in V0~VX values incrementing in memory starting from I
                    for (uint8_t i = 0; i <= Vx; i++) //VX is inclusive
                        vRegister[i] = memory[iReg + i];
                    break; 

            }
    }
    return incrementPC;
}

int loadProgram(uint8_t memory[]) { //Load program into memory
    FILE *program;
    if ((program = fopen("ROM\\3-corax+.ch8", "rb")) == NULL) {
        printf("Error opening ROM\n");
        return 1;
    }

    uint8_t c;
    uint16_t i = 0;
    while (fread(&c, sizeof(char), 1, program) > 0){
        // printf("%02X\n", c);
        memory[i + 0x200] = c;
        i++;
    }

    fclose(program);
    return 0;
}

void checkInput(void) {
    keyPress[0] = IsKeyPressed(KEY_ONE) ? 1 : 0;
    keyPress[1] = IsKeyPressed(KEY_TWO) ? 1 : 0;
    keyPress[2] = IsKeyPressed(KEY_THREE) ? 1 : 0;
    keyPress[3] = IsKeyPressed(KEY_FOUR) ? 1 : 0;
    keyPress[4] = IsKeyPressed(KEY_Q) ? 1 : 0;
    keyPress[5] = IsKeyPressed(KEY_W) ? 1 : 0;
    keyPress[6] = IsKeyPressed(KEY_E) ? 1 : 0;
    keyPress[7] = IsKeyPressed(KEY_R) ? 1 : 0;
    keyPress[8] = IsKeyPressed(KEY_A) ? 1 : 0;
    keyPress[9] = IsKeyPressed(KEY_S) ? 1 : 0;
    keyPress[10] = IsKeyPressed(KEY_D) ? 1 : 0;
    keyPress[11] = IsKeyPressed(KEY_F) ? 1 : 0;
    keyPress[12] = IsKeyPressed(KEY_Z) ? 1 : 0;
    keyPress[13] = IsKeyPressed(KEY_X) ? 1 : 0;
    keyPress[14] = IsKeyPressed(KEY_C) ? 1 : 0;
    keyPress[15] = IsKeyPressed(KEY_V) ? 1 : 0;
}


int main(void) {
    char incrementPC = 0;
    //Load font into memory
    for (uint8_t i = 0; i < 80; i++) 
        memory[0x50 + i] = font[i]; //80 bytes in font array, stored in memory 0x50 to 0x9F

    loadProgram(memory);

    // for (int i = 0; i < 4096; i++){
    //     printf("%d: %x\n", i, memory[i]);
    // }

    //Initialize raylib window
    InitWindow(DISPLAY_X * PIXEL_SIZE, DISPLAY_Y * PIXEL_SIZE, "CHIP-8 Emulator");
    SetTargetFPS(30);
    
    // printf("\n1 2 3 4 Q W E R A S D F Z X C V\n");

    //Start raylib drawing loop
    while(!WindowShouldClose()){
        //Update variables everyloop
        opcode = memory[pc] << 8 | memory[pc + 1];
        incrementPC = executeOpcode(opcode);
        
        if (incrementPC) 
            pc+=2;

        //Decrement timers if they've been set
        if (delayTimer > 0) delayTimer--;
        if (soundTimer > 0) soundTimer--;

        //Check for inputs
        checkInput();
        // printf("\n");
        // for (uint8_t i = 0; i < 16; i++) {
        //     printf("%d ", keyPress[i]);
        //     printf("%d: %d\n", i, vRegister[i]);
        // }

        //Update screen
        BeginDrawing();
        {
            ClearBackground(BLACK);
            for (uint8_t i = 0; i < DISPLAY_Y; i++) {
                // printf("%llu\n", pixelMap[i]);
                for (uint8_t j = 0; j < DISPLAY_X; j++){
                    if (pixelMap[i*64 + j])
                        DrawRectangle(
                            j * PIXEL_SIZE, 
                            i * PIXEL_SIZE, 
                            PIXEL_SIZE, 
                            PIXEL_SIZE, 
                            WHITE
                        );
                }
            }

        }
        EndDrawing();
    }
    CloseWindow();
}

