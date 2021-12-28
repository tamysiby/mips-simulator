#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>

//debugging
//#define DEBUG
//#define PRINT_DECODE
//#define PRINT_REG_STATE
//#define PRINT_LOAD_MEM
//#define PRINT_FETCH
//#define PRINT_PC

int instruction_reg;
unsigned int PC;

#define MAX_REG 32
#define MAX_MEM 16 * 1024 * 1024 / sizeof(int)

//global register file
unsigned int regfile[MAX_REG];
//instruction pointer
unsigned int Mem[MAX_MEM];

struct inst_t
{
    unsigned int opcode;
    unsigned int rs, rd, rt;
    unsigned int shamt, func;
    unsigned int imm;
    unsigned int j_target;
    int pc_changed_flag; //whuz is dis
};

void initialize_program();
void load_program(FILE *fd);
int fetch();
void decode(struct inst_t *inst);
int execute(struct inst_t *inst);
int exec_r_type(struct inst_t *inst);

int main(int argc, char *argv[])
{

    FILE *fd;
    struct inst_t inst;
    int data = 0;
    int f = 1;
    int e = 1;

    
    load_program(fd);
    initialize_program();

#ifdef DEBUG
    printf("sp is at 0x%x\n", regfile[29]);
#endif

    fclose(fd);

    //loop:
    while (1)
    {
        f = fetch(); //this function puts the current mem[pc/4] to the instruction registerrrrrrr
        decode(&inst);
        e = execute(&inst);

#ifdef PRINT_PC
        printf("PC: %x\n", PC);
#endif

#ifdef PRINT_REG_STATE
        for (int i = 0; i < 32; i++)
        {
            printf("r[%d] = %x\n", i, regfile[i]);
        }
#endif
        if (!f || !e)
        {
            break;
        }
    }
    printf("end of program.\n");
    return 0;
}

void initialize_program()
{
    bzero(regfile, sizeof(regfile));
    PC = 0;
    regfile[0] = 0;                          //set $zero to 0
    regfile[29] = (unsigned int *)0x1000000; //set sp to 0x100000
    regfile[31] = 0xFFFFFFFF;                //set ra to -1
}

void load_program(FILE *fd)
{
    size_t ret = 0;
    int mips_val, i = 0;
    int mem_val;

    do
    {
        mips_val = 0;
        ret = fread(&mips_val, 1, 4, fd);
        mem_val = ntohl(mips_val);
        Mem[i] = mem_val;
#ifdef PRINT_LOAD_MEM
        printf("(%d) load Mem[%x] pa: 0x%x val: 0x%x\n", (int)ret, i, &Mem[i], Mem[i]);
#endif
        i++;
    } while (ret == 4);
    printf("\n");
}

int fetch()
{
    size_t len = 0;

    if (PC == 0xFFFFFFFF)
        return 0;
    memset(&instruction_reg, 0, sizeof(instruction_reg));
    //--this causes segmentation fault
    instruction_reg = Mem[PC / 4];

#ifdef PRINT_FETCH
    printf("==================\n");
    printf("Fet: 0x%08x\n", instruction_reg);
    printf("==================\n");
#endif

    return 1;
}

void decode(struct inst_t *inst)
{

    unsigned int opcode = (instruction_reg >> 26) & 0x0000003f;
    unsigned int rs = (instruction_reg & 0x03e00000) >> 21;
    unsigned int rt = (instruction_reg & 0x001f0000) >> 16;
    unsigned int rd = (instruction_reg & 0x0000f800) >> 11;
    unsigned int shamt = (instruction_reg & 0x000007c0) >> 6;
    unsigned int func = instruction_reg & 0x0000003f;
    unsigned int imm = instruction_reg & 0x0000ffff;
    unsigned int j_target = instruction_reg & 0x03ffffff;

    bzero(inst, sizeof(*inst));
    inst->opcode = opcode;
    inst->rs = rs;
    inst->rt = rt;
    inst->rd = rd;
    inst->shamt = shamt;
    inst->func = func;
    inst->imm = imm;
    inst->j_target = j_target;

#ifdef PRINT_DECODE
    printf("opcode: 0x%x", inst->opcode);

    if (inst->opcode == 0x00)
    {
        //show values needed for R inst
        printf("\trs: 0x%x", inst->rs);
        printf("\trt: 0x%x", inst->rt);
        printf("\trd: 0x%x", inst->rd);
        printf("\tshamt: 0x%x", inst->shamt);
        printf("\tfunction: 0x%x\n", inst->func);
    }
    else
    {
        printf("\trs: 0x%x", inst->rs);
        printf("\trt: 0x%x", inst->rt);
        printf("\timmediate: 0x%x\n", inst->imm);
    }

#endif

    return;
}

int execute(struct inst_t *inst)
{
#ifdef DEBUG
    printf("\t\t[DEBUG] masuk exec\n");
#endif

    unsigned int rs = inst->rs;
    unsigned int rt = inst->rt;
    unsigned int imm = inst->imm;
    unsigned int j_target = inst->j_target;

    unsigned int s_imm;
    unsigned int z_imm;
    unsigned int b_addr;

    if (imm >> 15)
    {
        s_imm = imm | 0xFFFF0000;
    }
    else
    {
        s_imm = imm;
    }

    z_imm = imm;
    b_addr = imm << 2;

#ifdef DEBUG
    printf("\t\t[DEBUG] after initialization\n");
    printf("\t\t[DEBUG]opcode: 0x%x, rs: 0x%x, rt: 0x%x, imm: 0x%x\n", inst->opcode, rs, rt, imm);
    // if (inst->opcode == 0x2b)
    //     printf("hiiii\n");
#endif

    if (inst->opcode == 0x00) //R-type instructions + JALR & JR
    {
        //R-type instructions
#ifdef DEBUG
        printf("\t\t[DEBUG]masuk opcode 00\n");

#endif
        int er = exec_r_type(inst);
        if (!er)
        {
            return 0;
        }
    }
    else if (inst->opcode == 0x08) //I-type instructions
    {
        //addi
        regfile[rt] = regfile[rs] + s_imm;
        printf("addi\t R%d(0x%x) R%d %d\n", rt, regfile[rt], rs, s_imm);
        printf("\t\t R%d = R%d + %d\n", rt, rs, s_imm);
        PC = PC + 4;
    }
    else if (inst->opcode == 0x09)
    {

        //addiu
        regfile[rt] = regfile[rs] + s_imm;
        printf("addiu\t R%d R%d %d\t", rt, rs, s_imm);
        printf("[ R%d = R%d + %d, R%d = 0x%x ]\n", rt, rs, s_imm, rt, regfile[rt]);
        PC = PC + 4;
    }
    else if (inst->opcode == 0x0c)
    {
        //andi
        regfile[rt] = regfile[rs] & z_imm;
        printf("andi\t R%d R%d %d\n", rt, rs, z_imm);
        PC = PC + 4;
    }
    else if (inst->opcode == 0x04)
    {
        //beq
        if (regfile[rs] == regfile[rt])
        {
            PC = PC + 4 + b_addr;
        }
        else
        {
            PC = PC + 4;
        }
    }
    else if (inst->opcode == 0x05)
    {
        //bne
        if (regfile[rs] != regfile[rt])
        {
            PC = PC + 4 + b_addr;
        }
        else
        {
            PC = PC + 4;
        }
    }
    else if (inst->func == 0x3) //jal
    {
        //jal
        regfile[31] = PC + 8;
        PC = Mem[j_target / 4];
        printf("jal\t 0x%x\n", j_target);
    }
    else if (inst->func == 0x08) //j
    {
        //j jump
        PC = Mem[j_target / 4];
        printf("j\t 0x%x\n", j_target);
    }
    else if (inst->opcode == 0xf)
    {
        //lui
        regfile[rt] = imm << 16; //check
        printf("lui\t R%d R%d %d\n", rt, rs, s_imm);
        printf("\t\t R%d(0x%x) Memory[%x]\n", rt, regfile[rt], (regfile[rs] + s_imm));
        PC = PC + 4;
    }
    else if (inst->opcode == 0x23)
    {
        //lw
        regfile[rt] = Mem[(regfile[rs] + s_imm) / 4];
        regfile[rt] = imm << 16; //check
        printf("lw\t\t R%d R%d %d", rt, rs, s_imm);
        printf("\t\t R%d(0x%x) Memory[%x]\n", rt, regfile[rt], (regfile[rs] + s_imm));
        PC = PC + 4;
    }
    else if (inst->opcode == 0xd)
    {
        //ori
        regfile[rt] = regfile[rs] | z_imm;
        printf("ori\t R%d R%d %d\n", rt, rs, s_imm);
        printf("\t\t R%d(0x%x) Memory[%x]\n", rt, regfile[rt], (regfile[rs] + s_imm));
        PC = PC + 4;
    }
    else if (inst->opcode == 0xa)
    {
        //slti
        regfile[rt] = (regfile[rs] < s_imm) ? 1 : 0;
        printf("slti\t R%d R%d %d\n", rt, rs, s_imm);
        PC = PC + 4;
    }
    else if (inst->opcode == 0xb)
    {
        //sltiu
        regfile[rt] = (regfile[rs] < s_imm) ? 1 : 0;
        printf("sltiu\t R%d R%d %d\n", rt, rs, s_imm);
        PC = PC + 4;
    }
    else if (inst->opcode == 0x2b)
    {
#ifdef DEBUG
        printf("[DEBUG] masuk sw\n");
#endif
        //sw
        unsigned int addr = (regfile[rs] + s_imm) >> 2;
        Mem[addr] = regfile[rt];
        printf("sw\t\t R%d %d(R%d)", rt, s_imm, rs);
        printf("\t[ Mem[0x%x] = R%d ]\n", addr, rt);
        PC = PC + 4;
    }
    else
    {
        printf("Instruction is not supported: 0x%x\n", inst->opcode);
        return 0;
    }
#ifdef DEBUG
    printf("\t\t[DEBUG] keluar execute\n");
#endif
    return 1;
}

int exec_r_type(struct inst_t *inst)
{
#ifdef DEBUG
    printf("\t\t[DEBUG] masuk exec r type\n");
#endif
    unsigned int rs = inst->rs;
    unsigned int rd = inst->rd;
    unsigned int rt = inst->rt;
    unsigned int shamt = inst->shamt;

    if (inst->func == 0x20)
    {
        //add
        regfile[rd] = regfile[rs] + regfile[rt];
        PC = PC + 4;
        printf("add\t R%d R%d R%d\t", rd, rs, rt);
        printf("[ R%d = R%d + R%d, R%d = 0x%x ]\n", rd, rs, rt, rd, regfile[rd]);
    }
    else if (inst->func == 0x21)
    {
        //addu
        regfile[rd] = regfile[rs] + regfile[rt];
        PC = PC + 4;
        printf("addu\t R%d R%d R%d\t", rd, rs, rt);
        printf("[ R%d = R%d + R%d, R%d = 0x%x ]\n", rd, rs, rt, rd, regfile[rd]);
    }
    else if (inst->func == 0x24)
    {
        //and
        regfile[rd] = regfile[rs] & regfile[rt];
        PC = PC + 4;
        printf("and\t R%d R%d R%d\t", rd, rs, rt);
        printf("[ R%d = R%d & R%d, R%d = 0x%x ]\n", rd, rs, rt, rd, regfile[rd]);
    }
    // else if (inst->func == 0x09) //jalr
    // {
    //     //jalr
    //     regfile[31] = regfile[rd];
    //      what do i do here hadodo
    //      Rs: jump target address
    // }
    else if (inst->func == 0x08) //jr                       //PC modified in if functionnn
    {
        //jr
        PC = regfile[rs];
        printf("jr\t R%d(0x%x)\n", rs, regfile[rs]);
    }
    else if (inst->func == 0x27)
    {
        //nor
        regfile[rd] = ~(regfile[rs] | regfile[rt]);
        PC = PC + 4;
        printf("nor\t R%d R%d R%d\t", rd, rs, rt);
        printf("[ R%d = ~(R%d & R%d), R%d = 0x%x ]\n", rd, rs, rt, rd, regfile[rd]);
    }
    else if (inst->func == 0x25)
    {
        //or
        regfile[rd] = regfile[rs] | regfile[rt];
        PC = PC + 4;
        printf("or\t R%d R%d R%d\t", rd, rs, rt);
        printf("[ R%d = R%d | R%d, R%d = 0x%x ]\n", rd, rs, rt, rd, regfile[rd]);
    }
    else if (inst->func == 0x2a)
    {
        //slt
        regfile[rd] = (regfile[rs] < regfile[rt]) ? 1 : 0;
        PC = PC + 4;
        printf("slt\t R%d(0x%x) R%d R%d\t", rd, regfile[rd], rs, rt);
        if (regfile[rs] < regfile[rt])
        {
            //rd = 1 bcs rs < rt
            printf("[ R%d = %d bcs R%d < R%d ]\n", rd, regfile[rd], rs, rt);
        }
        else
        { //rd = 0 bcs rs > rt
            printf("[ R%d = %d bcs R%d > R%d ]\n", rd, regfile[rd], rs, rt);
        }
    }
    else if (inst->func == 0x2b)
    {
        //sltu
        regfile[rd] = (regfile[rs] < regfile[rt]) ? 1 : 0;
        PC = PC + 4;
        printf("slt\t R%d(0x%x) R%d R%d\t", rd, regfile[rd], rs, rt);
        if (regfile[rs] < regfile[rt])
        {
            //rd = 1 bcs rs < rt
            printf("[ R%d = %d bcs R%d < R%d ]\n", rd, regfile[rd], rs, rt);
        }
        else
        { //rd = 0 bcs rs > rt
            printf("[ R%d = %d bcs R%d > R%d ]\n", rd, regfile[rd], rs, rt);
        }
    }
    else if (inst->func == 0x00)
    {
        //sll
        regfile[rd] = regfile[rt] << shamt;
        PC = PC + 4;
        if (shamt != 0)
        {
            printf("sll\t R%d(0x%x) R%d %d\n", rd, regfile[rd], rt, shamt);
        }
        else
        {
            printf("nop  \t 00000000\n");
        }
    }
    else if (inst->func == 0x02)
    {
        //srl
        regfile[rd] = regfile[rt] >> shamt;
        PC = PC + 4;
        printf("srl\t R%d(0x%x) R%d %d\t", rd, regfile[rd], rt, shamt);
    }
    else if (inst->func == 0x22)
    {
        //sub
        regfile[rd] = regfile[rs] - regfile[rt];
        PC = PC + 4;
        printf("sub\t R%d(0x%x) R%d R%d\t", rd, regfile[rd], rs, rt);
        printf("[ R%d = R%d - R%d, R%d = 0x%x ]\n", rd, rs, rt, rd, regfile[rd]);
    }
    else if (inst->func == 0x23)
    {
        //subu
        regfile[rd] = regfile[rs] - regfile[rt];
        PC = PC + 4;
        printf("subu\t R%d(0x%x) R%d R%d\t", rd, regfile[rd], rs, rt);
        printf("[ R%d = R%d - R%d, R%d = 0x%x ]\n", rd, rs, rt, rd, regfile[rd]);
    }
    else
    {
        printf("R-type function not supported.\n");
        return 0;
    }
    return 1;
}
