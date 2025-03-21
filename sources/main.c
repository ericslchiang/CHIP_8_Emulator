#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "raylib.h"

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
1 2 3 C     1 2 3 4
4 5 6 D =>  Q W E R
7 8 9 E     A S D F
A 0 B F     Z X C V
*/

//Setup virtual memory and registers
uint16_t opcode;
uint8_t memory[MEM_SIZE] = {0};
uint8_t vReg[NUM_REG] = {0}; //General purpose registers. vReg[0] is used overflow flag
uint16_t iReg = 0; //Points to location in memory. 12 bits wide
uint16_t stack[STACK_SIZE] = {0}; //Used for storing address when entering subroutines
uint16_t stackIndex = 0;
uint16_t pc = 0x200; //Program Counter. chip-8 expects programs to be loaded in memory at 0x200
uint8_t delayTimer;
uint8_t soundTimer;
uint8_t pixelMap[DISPLAY_X * DISPLAY_Y] = {0};

void executeOpcode(uint16_t opcode){
    switch(opcode & 0xF000) {
        case 0x0000:
            switch(opcode & 0x00FF){
                case 0x00E0: //Clears Screen
                    memset(pixelMap, 0, sizeof(pixelMap));
                    break;
                case 0x00EE: //Returns from subroutine
                    pc = stack[--stackIndex];
                    break;
                default: //Execute machine code 
                    printf("opcode 0x0NNN is left unimplemented\n");
                    break;
            }
            break;
        case 0x1000: //GOTO address in opcode 0x1NNN
            pc = opcode & 0x0FFF;
            break;
        case 0x2000: //Execute subroutine at address in opcode 0x2NNN
            stack[stackIndex++] = pc;
            pc = opcode & 0x0FFF;
            break;
        case 0x3000: //Skips next instruction if VX == NN for 0x3XNN
            if ((opcode & 0x00FF) == vReg[(opcode & 0x0F00) >> 8])
                pc+=2;
            break;
        case 0x4000: //Skips next instruction if VX != NN for 0x4XNN
            if ((opcode & 0x00FF) != vReg[(opcode & 0x0F00) >> 8])
                pc+=2;
            break;
        case 0x5000: //Skips next instruction if VX == VY for 0x5XY0
            if ((vReg[(opcode & 0x0F00) >> 8]) == (vReg[(opcode & 0x00F0) >> 4]))
                pc+=2;
            break;
        case 0x9000: //Skips next instruction if VX != VY for 0x9XY0
            if ((vReg[(opcode & 0x0F00) >> 8]) != (vReg[(opcode & 0x00F0) >> 4]))
                pc+=2;
            break;
        case 0x6000: //Sets VX = NN for 0x6XNN
            vReg[(opcode & 0x0F00) >> 8] = (opcode & 0x00FF);
            break;
        case 0x7000: //Adds NN to VX for 0x7XNN
            uint16_t total = vReg[(opcode & 0x0F00) >> 8] + (opcode & 0x00FF); 
            vReg[(opcode & 0x0F00) >> 8] = total % 256;
            break;
        case 0x8000: //Check if V[0xF] is suppose to hold result or carry bit
            uint8_t overflow = 0;
            switch(opcode & 0x000F){
                case 0x0000: //Set VX to value of VY for 0x8XY0
                    vReg[(opcode & 0x0F00) >> 8] = vReg[(opcode & 0x00F0) >> 4];
                    break;
                case 0x0001: //Bitwise OR: VX |= VY for 0x8XY1
                    vReg[(opcode & 0x0F00) >> 8] |= vReg[(opcode & 0x00F0) >> 4];
                    vReg[0xF] = 0;
                    break;
                case 0x0002: //Bitwise AND: VX &= VY for 0x8XY2
                    vReg[(opcode & 0x0F00) >> 8] &= vReg[(opcode & 0x00F0) >> 4];
                    vReg[0xF] = 0;
                    break;
                case 0x0003: //Bitwise XOR: VX |= VY for 0x8XY3
                    vReg[(opcode & 0x0F00) >> 8] ^= vReg[(opcode & 0x00F0) >> 4];
                    vReg[0xF] = 0;
                    break;
                case 0x0004: //VX += VY for 0x8XY4
                    overflow = vReg[(opcode & 0x0F00) >> 8] > (0xFF - (vReg[(opcode & 0x00F0) >> 4]));
                    uint16_t sum = vReg[(opcode & 0x0F00) >> 8] + vReg[(opcode & 0x00F0) >> 4]; 
                    vReg[(opcode & 0x0F00) >> 8] = sum & 255;
                    vReg[0xF] = overflow ? 1 : 0;
                    break;
                case 0x0005: //VX -= VY for 0x8XY5
                    overflow = vReg[(opcode & 0x0F00) >> 8] >= vReg[(opcode & 0x00F0) >> 4]; //Check if VX underflows
                    vReg[(opcode & 0x0F00) >> 8] -= vReg[(opcode & 0x00F0) >> 4];
                    vReg[0xF] = overflow ? 1 : 0;
                    break;
                case 0x0006: //Right shift VX by 1 for 0x8XY6
                    vReg[(opcode & 0x0F00) >> 8] = vReg[(opcode & 0x00F0) >> 4]; //cosmac instruction
                    overflow = vReg[(opcode & 0x0F00) >> 8] & 0x01; //Save the shifted LSB into VF
                    vReg[(opcode & 0x0F00) >> 8] >>= 1;
                    vReg[0xF] = overflow;
                    break;
                case 0x0007: //VX = VY - Vx for 0x8XY7    
                    overflow = vReg[(opcode & 0x00F0) >> 4] >= vReg[(opcode & 0x0F00) >> 8]; //Check if VX underflows
                    vReg[(opcode & 0x0F00) >> 8] = vReg[(opcode & 0x00F0) >> 4] - vReg[(opcode & 0x0F00) >> 8];
                    vReg[0xF] = overflow ? 1 : 0;
                    break;
                case 0x000E: //Left shft VX by 1 for 0x8XYE
                    vReg[(opcode & 0x0F00) >> 8] = vReg[(opcode & 0x00F0) >> 4]; //cosmac instruction
                    overflow = (vReg[(opcode & 0x0F00) >> 8] & 0x80) >> 7; //Save the shifted MSB into VF
                    vReg[(opcode & 0x0F00) >> 8] <<= 1;
                    vReg[0xF] = overflow;
                    break;
            }
            break;
        case 0xA000: //Sets I register to NNN for 0xANNN
            iReg = (opcode & 0x0FFF);
            break;
        case 0xB000: //Jumps to address NNN + V0 for 0xBNNN
            pc = (opcode & 0x0FFF) + vReg[0];
            break;
        case 0xC000: //Sets VX to bitwise AND between NN and random number for 0xCXNN
            srand(time(NULL));
            vReg[(opcode & 0x0F00) >> 8] = (opcode & 0x00FF) & (rand() & 255);
            break;
        case 0xD000: //Draw sprite on screen. This is done by inverting (XOR) the sprite with the pixels currently on screen.
            uint8_t x = vReg[(opcode & 0x0F00) >> 8] % DISPLAY_X; 
            uint8_t y = vReg[(opcode & 0x00F0) >> 4] % DISPLAY_Y;
            uint8_t height = (opcode & 0x000F);
            vReg[0xF] = 0; 

            for (uint8_t row = 0; row <  height && (y + row) < DISPLAY_Y; row++){
                uint8_t spriteRow = memory[iReg + row];
                for (uint8_t bitCount = 0; bitCount < 8 && (x + bitCount) < DISPLAY_X; bitCount++) {
                    uint8_t spritePixel = spriteRow & (0x80u >> bitCount);
                    uint8_t *screenPixel = &pixelMap[x + bitCount + (y + row) * DISPLAY_X];
                    if(spritePixel) {
                        if(*screenPixel == 0xFF) {
                            vReg[0xF] = 1;
                        }
                        *screenPixel ^= 0xFF;
                        
                    }
                }
            }
            break;
        case 0xE000: //Skips condition based on if a key was pressed or NOT pressed
            if ((opcode & 0x000F) == 0x000E){
                if (keyPress[vReg[(opcode & 0x0F00) >> 8]])
                    pc+=2;
            } else if ((opcode & 0x000F) == 0x0001) {
                if (!keyPress[vReg[(opcode & 0x0F00) >> 8]])
                    pc+=2;
            }
            break;
        case 0xF000:
            uint8_t Vx = (opcode & 0x0F00) >> 8;
            switch (opcode & 0x00FF) {
                case 0x0007: //Set VX to the value of the delay timer for 0xFX07
                    vReg[Vx] = delayTimer;
                    break;
                case 0x000A: //Waits for keypress
                    for (uint8_t i = 0; i < 16; i++) {
                        if (keyPress[i]){
                            vReg[Vx] = i;
                            break;
                        } else if (i == 15){
                            pc -= 2;
                        }
                    }
                    break;
                case 0x0015: //Sets delay timer to VX
                    delayTimer = vReg[Vx];
                    break;
                case 0x0018: //Sets sound timer to VX
                    soundTimer = vReg[Vx];
                    break;
                case 0x001E: //Adds VX to I
                    iReg += vReg[Vx];
                    break;
                case 0x0029: //Sets I to the address of sprite of the character (0-F) stored in lower nibble of VX. 
                    iReg = (vReg[Vx] & 0x0F) * 5 + 0x50; //Font is stored in memory starting at 0x50. Each character is 5 bytes, starting from 0 to F 
                    break;
                case 0x0033: //Stores the values of each decimal digit of VX into memory at I(hundreds), I+1(tens), I+2(ones)
                    uint8_t val = vReg[Vx];
                    memory[iReg + 2] = val % 10;
                    val /= 10;
                    memory[iReg + 1] = val % 10;
                    val /= 10;
                    memory[iReg] = val % 10;
                    break;
                case 0x0055: //Stores in memory starting from I values from V0~VX
                    for (uint8_t i = 0; i <= Vx; i++) {//VX is inclusive
                        memory[iReg + i] = vReg[i];
                    }
                    iReg += Vx;
                    break;
                case 0x0065: //Stores in V0~VX values incrementing in memory starting from I
                    for (uint8_t i = 0; i <= Vx; i++) {//VX is inclusive
                        vReg[i] = memory[iReg + i];
                    }
                    iReg += Vx;
                    break; 

            }
            break;
    }
}

//Load program into memory
int loadProgram(uint8_t memory[], char *gameName) { 
    FILE *program;
    char gameLocation[32] = "..\\ROM\\";
    strcat(gameLocation, gameName);
    strcat(gameLocation, ".ch8");
    
    if ((program = fopen(gameLocation, "rb")) == NULL) {
        printf("Error opening ROM\n");
        return 1;
    }

    uint8_t c;
    uint16_t i = 0;
    while (fread(&c, sizeof(char), 1, program) > 0){
        memory[i + 0x200] = c;
        i++;
    }

    fclose(program);
    return 0;
}

//Check if a key is held at this frame
void checkInput(void) {
    keyPress[0] = IsKeyDown(KEY_X) ? 1 : 0;
    keyPress[1] = IsKeyDown(KEY_ONE) ? 1 : 0;
    keyPress[2] = IsKeyDown(KEY_TWO) ? 1 : 0;
    keyPress[3] = IsKeyDown(KEY_THREE) ? 1 : 0;
    keyPress[4] = IsKeyDown(KEY_Q) ? 1 : 0;
    keyPress[5] = IsKeyDown(KEY_W) ? 1 : 0;
    keyPress[6] = IsKeyDown(KEY_E) ? 1 : 0;
    keyPress[7] = IsKeyDown(KEY_A) ? 1 : 0;
    keyPress[8] = IsKeyDown(KEY_S) ? 1 : 0;
    keyPress[9] = IsKeyDown(KEY_D) ? 1 : 0;
    keyPress[10] = IsKeyDown(KEY_Z) ? 1 : 0;
    keyPress[11] = IsKeyDown(KEY_C) ? 1 : 0;
    keyPress[12] = IsKeyDown(KEY_FOUR) ? 1 : 0;
    keyPress[13] = IsKeyDown(KEY_R) ? 1 : 0;
    keyPress[14] = IsKeyDown(KEY_F) ? 1 : 0;
    keyPress[15] = IsKeyDown(KEY_V) ? 1 : 0;
}

int main(int argc, char** argv) {
    if (argc > 3 || argc < 2) {
        printf("Invalid Inputs. Start the emulator calling the .exe with format: \"main.exe <game name> <delay>\"");
        return 1;
    }

    if(loadProgram(memory, argv[1])) {
        return 1;
    }
    
    //Load font into memory
    for (uint8_t i = 0; i < 80; i++) 
        memory[0x50 + i] = font[i]; //80 bytes in font array, stored in memory 0x50 to 0x9F

    //Initialize raylib window
    InitWindow(DISPLAY_X * PIXEL_SIZE, DISPLAY_Y * PIXEL_SIZE, "CHIP-8 Emulator");
    SetTargetFPS(60);

    double now = 0, last = 0; //Used for tracking frame rate
    uint8_t delay = 0; //Used for user setting game speed
    if (argv[2] != NULL)
        delay = atoi(argv[2]);
    else 
        delay = 8; //Execute instructions at ~8 opcodes per frame (500Hz)


    //Start raylib drawing loop
    while(!WindowShouldClose()){
        uint8_t timesDrawn = 0;

        
        for (uint8_t temp = 0; temp < delay; temp++) {
            checkInput();
            opcode = memory[pc] << 8 | memory[pc + 1];

            //Avoid rendering twice in the same frame
            //TODO: Implement a frame buffer solution
            if ((opcode & 0xF000) == 0xD000){
                timesDrawn++;
                if (timesDrawn > 1){
                    break;
                }
            }

            pc+=2;
            executeOpcode(opcode);
        }

        //Decrement timers if they've been set
        if (delayTimer > 0) delayTimer--;
        if (soundTimer > 0) soundTimer--;

        BeginDrawing();
        for (uint8_t i = 0; i < DISPLAY_Y; i++) {
            for (uint8_t j = 0; j < DISPLAY_X; j++){
                if (!pixelMap[i*64 + j]){
                    DrawRectangle(
                        j * PIXEL_SIZE, 
                        i * PIXEL_SIZE, 
                        PIXEL_SIZE, 
                        PIXEL_SIZE, 
                        BLACK
                    );
                } else {
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

        //Check if buzzer is at zero
        if (!soundTimer){} 
            // printf("BEEEEEEEEEP!\n");
            // PlaySound(); //Use raylib to play sound file, for now just printf
        
        
        //Update frame timer
        while((now = GetTime() - last) < 0.01666667f);
        last = GetTime();
    }

    CloseWindow();
}

