#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

//16-bit memory locations
#define MEMORY_MAX (1<<16)
__uint16_t memory[MEMORY_MAX];

#define PC_START 0x3000
//10 16-bit wide registers
//R1 - R7 is general purpose registers
enum{
    R0 = 0, //constant zero
    R1,
    R2,
    R3,
    R4,
    R5,
    R6,
    R7,
    PC,     //Program counter
    COND,   //Condition flag register
    COUNT   //Reg count
};

__uint16_t reg[COUNT];

//LC-3 all opcodes
enum{
    OP_BR = 0, // branch
    OP_ADD,    // add  
    OP_LD,     // load 
    OP_ST,     // store 
    OP_JSR,    // jump register 
    OP_AND,    // bitwise and 
    OP_LDR,    // load register 
    OP_STR,    // store register 
    OP_RTI,    // unused 
    OP_NOT,    // bitwise not 
    OP_LDI,    // load indirect 
    OP_STI,    // store indirect 
    OP_JMP,    // jump 
    OP_RES,    // reserved (unused) 
    OP_LEA,    // load effective address 
    OP_TRAP    // execute trap 
};

//Condition flags, checks if statements and such
//Stored in the COND register
enum{
    FL_POS = 1 << 0,
    FL_ZRO = 1 << 1,
    FL_NEG = 1 << 2,
};

__uint16_t sign_extend(__uint16_t x, int bit_count){
    if((x >> (bit_count - 1)) & 1){
        /* use 16-bit mask to avoid sign issues with larger integer types */
        x |= (uint16_t)(0xFFFF << bit_count);
    }
    return x;
}

/* forward declaration */
void update_flags(__uint16_t r);

/* simple memory helpers */
__uint16_t mem_read(__uint16_t address){
    return memory[address];
}

void mem_write(__uint16_t address, __uint16_t val){
    memory[address] = val;
}

/* placeholder image loader - loads raw binary into memory */
int read_image(const char* path){
    FILE* file = fopen(path, "rb");
    if(!file){
        return 0;
    }

    /* first 16 bits of the file are the origin */
    uint16_t origin;
    if(fread(&origin, sizeof(origin), 1, file) != 1){
        fclose(file);
        return 0;
    }

    /* files are big endian */
    origin = (origin << 8) | (origin >> 8);
    uint16_t* mem = &memory[origin];
    size_t max_read = MEMORY_MAX - origin;
    size_t read = fread(mem, sizeof(uint16_t), max_read, file);
    for(size_t i = 0; i < read; ++i){
        mem[i] = (mem[i] << 8) | (mem[i] >> 8);
    }

    fclose(file);
    return 1;
}

/* stubs for operations not yet implemented */
void ldi(__uint16_t instr){
    __uint16_t r0 = (instr >> 9) & 0x7;
    __uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
    reg[r0] = mem_read(mem_read(reg[PC] + pc_offset));
    update_flags(r0);
}

void lea(__uint16_t instr){
    __uint16_t r0 = (instr >> 9) & 0x7;
    __uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
    reg[r0] = reg[PC] + pc_offset;
    update_flags(r0);
}

void not(__uint16_t instr){
    __uint16_t r0 = (instr >> 9) & 0x7;
    __uint16_t r1 = (instr >> 6) & 0x7;
    reg[r0] = ~reg[r1];
    update_flags(r0);
}

void st(__uint16_t instr){
    __uint16_t r0 = (instr >> 9) & 0x7;
    __uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
    mem_write(reg[PC] + pc_offset, reg[r0]);
}

void sti(__uint16_t instr){
    __uint16_t r0 = (instr >> 9) & 0x7;
    __uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
    mem_write(mem_read(reg[PC] + pc_offset), reg[r0]);
}

void str(__uint16_t instr){
    __uint16_t r0 = (instr >> 9) & 0x7;
    __uint16_t r1 = (instr >> 6) & 0x7;
    __uint16_t offset = sign_extend(instr & 0x3F, 6);
    mem_write(reg[r1] + offset, reg[r0]);
}

void trap(__uint16_t instr){
    switch(instr & 0xFF){
        case 0x25: /* HALT */
            puts("HALT");
            exit(0);
            break;
        default:
            break;
    }
}

void bad_op(){
    fprintf(stderr, "Bad opcode encountered\n");
    exit(1);
}

void update_flags(__uint16_t r){
    if(reg[r] == 0){
        reg[COND] = FL_ZRO;
    }
    else if(reg[r] >> 15){
        reg[COND] = FL_NEG;
    }
    else{
        reg[COND] = FL_POS;
    }
}

void add(__uint16_t instr){
    __uint16_t r0 = (instr >> 9) & 0x7;
    __uint16_t r1 = (instr >> 6) & 0x7;
    __uint16_t imm_flag = (instr >> 5) & 0x1;

    if(imm_flag){
        __uint16_t imm5 = sign_extend(instr & 0x1f, 5);
        reg[r0] = reg[r1] + imm5;
    }
    else{
        __uint16_t r2 = instr & 0x7;
        reg[r0] = reg[r1] + reg[r2];
    }

    update_flags(r0);
}

void and(__uint16_t instr){
    __uint16_t r0 = (instr >> 9) & 0x7;
    __uint16_t r1 = (instr >> 6) & 0x7;
    __uint16_t imm_flag = (instr >> 5) & 0x1;
    if(imm_flag){
        __uint16_t imm5 = sign_extend(instr & 0x1f, 5);
        reg[r0] = reg[r1] & imm5;
    }
    else{
        __uint16_t r2 = instr & 0x7;
        reg[r0] = reg[r1] & reg[r2];
    }

    update_flags(r0);
}

void br(__uint16_t instr){
    __uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
    __uint16_t cond_flag = (instr >> 9) & 0x7;
    if (cond_flag & reg[COND])
    {
        reg[PC] += pc_offset;
    }
}

void jmp(__uint16_t instr){
    __uint16_t r1 = (instr >> 6) & 0x7;
    reg[PC] = reg[r1];
}

void jsr(__uint16_t instr){
    __uint16_t long_flag = (instr >> 11) & 1;
    reg[R7] = reg[PC];
    if (long_flag)
    {
        __uint16_t long_pc_offset = sign_extend(instr & 0x7FF, 11);
        reg[PC] += long_pc_offset;  /* JSR */
    }
    else
    {
        __uint16_t r1 = (instr >> 6) & 0x7;
        reg[PC] = reg[r1]; /* JSRR */
    }
}

void ld(__uint16_t instr){
    __uint16_t r0 = (instr >> 9) & 0x7;
    __uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
    reg[r0] = mem_read(reg[PC] + pc_offset);
    update_flags(r0);
}

void ldr(__uint16_t instr){
    __uint16_t r0 = (instr >> 9) & 0x7;
    __uint16_t r1 = (instr >> 6) & 0x7;
    __uint16_t offset = sign_extend(instr & 0x3F, 6);
    reg[r0] = mem_read(reg[r1] + offset);
    update_flags(r0);
}

int main(int argc, char* argv[]){

    if (argc < 2){
        printf("Wrong usage \n Try: \n simplelc3-vm [image] ... \n");
        exit(2);
    }

    for(int i = 1; i < argc; ++i){
        if(!read_image(argv[i])){
            printf("Couldn't load image: %s \n", argv[i]);
            exit(1);
        }
    }
    reg[COND] = FL_ZRO;         //Only one condition flag can be set

    reg[PC] = PC_START;         //Default PC start address
    int run = 1;                //Run cond
    while(run){
        __uint16_t instr = mem_read(reg[PC]++); //Read next instruction, stored after the current PC
        __uint16_t op = instr >> 12;            //Shift 12 bits to read all op info

        switch (op){
            case OP_ADD:
                add(instr);
                break;
            case OP_AND:
                and(instr);
                break;
            case OP_BR:
                br(instr);
                break;
            case OP_JMP:
                jmp(instr);
                break;
            case OP_JSR:
                jsr(instr);
                break;
            case OP_LD:
                ld(instr);
                break;
            case OP_LDI:
                ldi(instr);
                break;
            case OP_LDR:
                ldr(instr);
                break;
            case OP_LEA:
                lea(instr);
                break;
            case OP_NOT:
                not(instr);
                break;
            case OP_ST:
                st(instr);
                break;
            case OP_STI:
                sti(instr);
                break;
            case OP_STR:
                str(instr);
                break;
            case OP_TRAP:
                trap(instr);
                break;
            case OP_RES:
            case OP_RTI:
            default:
                bad_op();
                break;
        }
    }
}