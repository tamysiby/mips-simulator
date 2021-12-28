#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

//#PRINT_LOAD_MEM

unsigned int PC;

#define MAX_REG 32
#define MAX_MEM 16 * 1024 * 1024 / sizeof(int)

unsigned int regs[MAX_REG];
unsigned int Mem[MAX_MEM];

//unsigned int bubble = 0x00000000;
int stall = 0;
int flush = 0;
int inst_count = 0;

//dec_count
//dll

struct ifid_struc
{
    int inst;
    int nextPC;
    int PCSrc;
};
struct idex_struc
{
    unsigned int r_data1;
    unsigned int r_data2;
    unsigned int imm;
    unsigned int rt;
    unsigned int rd;
    unsigned int nextPC;
    unsigned int opcode;

    unsigned int target_addr;

    unsigned int inst;

    //for forwarding unit
    unsigned int rs;

    //control
    int regDst, regWrite, ALUSrc, memWrite, memRead, memToReg, branch;
};
struct exmem_struc
{

    unsigned int data2;
    unsigned int regdest; //rt or rd
    unsigned int ALUresult;

    unsigned int inst;

    //for forwarding unit
    unsigned int rd;

    //controls
    int branch, memRead, memWrite, regWrite, memToReg;
};
struct memwb_struc
{
    //control
    unsigned int m_data;
    unsigned int addr;
    unsigned int regdest;
    unsigned int ALUresult;

    unsigned int inst;

    //for forwarding unit
    unsigned int rd;

    //controls
    int regWrite, memToReg;
};

struct ifid_struc if_id[2];
struct idex_struc id_ex[2];
struct exmem_struc ex_mem[2];
struct memwb_struc mem_wb[2];

void flush_e();
unsigned int ALU_r(unsigned int r_data1, unsigned int r_data2, unsigned int imm);
unsigned int ALU_imm(unsigned int opcode, unsigned int r_data1, unsigned int imm);

void load_program(int argc, char *argv[], FILE *fd);
void initialize_program(struct ifid_struc *ifid);
int fetch(struct ifid_struc *ifid, struct exmem_struc *exmem);
int decode(struct ifid_struc *ifid, struct idex_struc *idex);
int execute(struct idex_struc *idex, struct exmem_struc *exmem, struct exmem_struc *exmem_o, struct memwb_struc *memwb_o, unsigned int wb);
int memory(struct exmem_struc *exmem, struct memwb_struc *memwb);
int write_back(struct memwb_struc *memwb, unsigned int *wd);

int main(int argc, char *argv[])
{
    FILE *fd;

    int data = 0;
    int f = 1;
    int d = 1;
    int e = 1;
    int m = 1;
    int wb = 1;
    unsigned int w_data;

    //take in file from arg
    load_program(argc, argv, fd);
    initialize_program(&if_id[0]);
    int count = 1;

    //loop:
    //for (int i = 0; i < 50; i++)
    while (1)
    {
        printf("\n==============clock: %d==============\n", count);
        f = fetch(&if_id[0], &ex_mem[0]);
        d = decode(&if_id[1], &id_ex[0]);
        e = execute(&id_ex[1], &ex_mem[0], &ex_mem[1], &mem_wb[1], w_data);
        m = memory(&ex_mem[1], &mem_wb[0]);
        wb = write_back(&mem_wb[1], &w_data);
        if (f == 0 || d == 0 || e == 0 || m == 0 || wb == 0)
        {
            break;
        }

        //flushing
        if_id[1] = if_id[0];
        id_ex[1] = id_ex[0];
        ex_mem[1] = ex_mem[0];
        mem_wb[1] = mem_wb[0];
        count++;
    }
    printf("end of program.\n");
    printf("instruction count: %d\n", inst_count);
    return 0;
}

void initialize_program(struct ifid_struc *ifid)
{
    bzero(regs, sizeof(regs));
    PC = 0;
    regs[0] = 0;                          //set $zero to 0
    regs[29] = (unsigned int *)0x1000000; //set sp to 0x100000
    regs[31] = 0xFFFFFFFF;                //set ra to -1

    //initialize all regs!!!
    ifid->nextPC = 0;

    return;
}

void load_program(int argc, char *argv[], FILE *fd)
{
    char *filename;
    if (argc == 2)
    {
        filename = argv[1];
    }
    else
    {
        filename = "input4.bin";
    }

    fd = fopen(filename, "rb");
    if (fd == NULL)
    {
        printf("file can't be opened.\n");
        exit(1); //make program ask for another file instead of exit!
    }

    size_t ret = 0;
    int mips_val, i = 0;
    int mem_val;

    do
    {
        mips_val = 0;
        ret = fread(&mips_val, 1, 4, fd);
        mem_val = ntohl(mips_val);
        Mem[i] = mem_val;

        printf("(%d) load Mem[%x] pa: 0x%x val: 0x%x\n", (int)ret, i, &Mem[i], Mem[i]);

        i++;
    } while (ret == 4);
    printf("\n");
    fclose(fd);
    return;
}

int fetch(struct ifid_struc *ifid, struct exmem_struc *exmem)
{
    //printf("masuk fetch\n");

    size_t len = 0;
    int instruction_reg;

    if (PC == 0xFFFFFFFF)
        return 0;

    memset(&instruction_reg, 0, sizeof(instruction_reg));

    //mux that decides where to fetch inst reg, either pc+4 or branch target
    inst_count = inst_count + 1;

    //printf("pc source: %d\n", if_id[0].PCSrc);
    if (if_id[0].PCSrc)
    {
        printf("pcsrc control, target addr: 0x%x\n", id_ex[0].target_addr);
        //ifid->nextPC = ex_mem[0].target_addr;
        PC = id_ex[0].target_addr;
        instruction_reg = Mem[PC / 4];
        if_id[0].PCSrc = 0;
    }
    else
    {
        //PC = ifid->nextPC;
        instruction_reg = Mem[PC / 4];
    }

    printf("[fetch] %x: 0x%x \n", PC, instruction_reg);

    if (stall)
    {
        //setting EX MEM WB control signals to 0
        printf("stall\n");
        id_ex[0].regDst = 0;
        id_ex[0].regWrite = 0;
        id_ex[0].ALUSrc = 0;
        id_ex[0].memWrite = 0;
        id_ex[0].memRead = 0;
        id_ex[0].memToReg = 0;
        id_ex[0].branch = 0;
        id_ex[1].regDst = 0;
        id_ex[1].regWrite = 0;
        id_ex[1].ALUSrc = 0;
        id_ex[1].memWrite = 0;
        id_ex[1].memRead = 0;
        id_ex[1].memToReg = 0;
        id_ex[1].branch = 0;
        ex_mem[0].branch = 0;
        ex_mem[0].memRead = 0;
        ex_mem[0].memWrite = 0;
        ex_mem[0].regWrite = 0;
        ex_mem[0].memToReg = 0;
        ex_mem[1].branch = 0;
        ex_mem[1].memRead = 0;
        ex_mem[1].memWrite = 0;
        ex_mem[1].regWrite = 0;
        ex_mem[1].memToReg = 0;
        mem_wb[0].regWrite = 0;
        mem_wb[0].memToReg = 0;
        mem_wb[1].regWrite = 0;
        mem_wb[0].memToReg = 0;
        if_id[0].PCSrc = 0;
        if_id[1].PCSrc = 0;

        stall = 0;
        // PC = PC + 4;
    }
    else
    {
        PC = PC + 4;
        ifid->inst = instruction_reg;
    }
    //now in case of branch, need to not update PC
    ifid->nextPC = PC; //this is just passing pc + 4 to the pipeline, not moving to pc+4
    //printf("nextPC: 0x%x\n", ifid->nextPC);
    return 1;
}

int decode(struct ifid_struc *ifid, struct idex_struc *idex)
{
    //printf("masuk decode\n");
    //unsigned int currPC = ifid->nextPC;
    idex->nextPC = ifid->nextPC;
    unsigned int inst = ifid->inst;

    unsigned int opcode = (inst >> 26) & 0x0000003f;
    unsigned int rs = (inst & 0x03e00000) >> 21;
    unsigned int rt = (inst & 0x001f0000) >> 16;
    unsigned int rd = (inst & 0x0000f800) >> 11;
    unsigned int shamt = (inst & 0x000007c0) >> 6;
    unsigned int imm = inst & 0x0000ffff;

    //unsigned int target_addr = (ifid->nextPC + 4) + (imm * 4);
    //unsigned int target_addr = ifid->nextPC + (imm * 4);
    unsigned int target_addr;
    unsigned int jump_addr = (inst & 0x03ffffff) << 2;
    unsigned int branch_addr = (ifid->nextPC + 4) + (imm * 4);

    //control to stall
    if (idex->memRead && ((idex->rt == rs) || (idex->rt == rt)))
    {
        stall = 1;
    }

    //bzero(idex, sizeof(*idex));
    idex->r_data1 = regs[rs];
    idex->r_data2 = regs[rt];
    idex->rs = rs;
    idex->rt = rt;
    idex->rd = rd;
    idex->imm = imm;
    idex->opcode = opcode;
    printf("[decode 0x%x] opcode = 0x%x imm=0x%x nextPC=0x%x\n", inst, opcode, imm, idex->nextPC);

    //CONTROLS:
    int regDst, regWrite, ALUSrc, memWrite, memRead, memToReg, branch;

    switch (opcode)
    {
    case 0x0: //r-type
        regDst = 1;
        regWrite = 1;
        ALUSrc = 0;
        memWrite = 0;
        memRead = 0;
        memToReg = 0;
        branch = 0;
        break;
    case 0x23: //lw
    case 0x30:
        //case 0xf: lui
        regDst = 0;
        regWrite = 1;
        ALUSrc = 1;
        memWrite = 0;
        memRead = 1;
        memToReg = 1;
        branch = 0;
        break;
    case 0x2b: //sw
        //regDst = 0;
        regWrite = 0;
        ALUSrc = 1;
        memWrite = 1;
        memRead = 0;
        branch = 0;
        break;
    case 0x4: //beq
    case 0x5: //bne
        //regDst = 0;
        regWrite = 0;
        ALUSrc = 1;
        memWrite = 0;
        memRead = 0;
        branch = 1;
        break;
    case 0x2: //j
    case 0x3: //jal
        regWrite = 0;
        ALUSrc = 1;
        memWrite = 0;
        memRead = 0;
        branch = 1;
        break;
    case 0x8: //addi
    case 0x9: //addiu
    case 0xc: //andi
    case 0xd: //ori
    case 0xa: //slti
    case 0xb: //sltiu
        regDst = 0;
        regWrite = 1;
        ALUSrc = 1;
        memWrite = 0;
        memRead = 0;
        memToReg = 0;
        branch = 0;
        break;
    default:
        printf("opcode probz\n");
        return 0;
    }

    //branch
    if (opcode == 0x4) //beq
    {
        printf("beq\n");
        if (idex->r_data1 == idex->r_data2)
        {
            printf("branch taken\n");
            target_addr = branch_addr;
            if_id[0].PCSrc = 1;
            flush_e();
        }
    }
    if (opcode == 0x5) //bne
    {
        printf("bne\n");
        if (idex->r_data1 != idex->r_data2)
        {
            printf("branch taken\n");
            target_addr = branch_addr;
            if_id[0].PCSrc = 1;
            flush_e();
        }
    }

    if (opcode == 0x2) //j
    {
        printf("j\n");
        target_addr = jump_addr;
        if_id[0].PCSrc = 1;
        flush_e();
    }
    if (opcode == 0x3)
    {
        printf("jal\n");
        target_addr = jump_addr;
        if_id[0].PCSrc = 1;
        flush_e();
    }

    idex->target_addr = target_addr;

    //passing the control signals to the id_ex register
    idex->inst = inst;
    idex->regDst = regDst;
    idex->ALUSrc = ALUSrc;

    idex->branch = branch;
    idex->memWrite = memWrite;
    idex->memRead = memRead;

    idex->regWrite = regWrite;
    idex->memToReg = memToReg;
}

int execute(struct idex_struc *idex, struct exmem_struc *exmem, struct exmem_struc *exmem_o, struct memwb_struc *memwb_o, unsigned int wb)
{
    //printf("masuk execute\n");

    //getting control signals from idex reg
    int regDst = idex->regDst;
    int ALUSrc = idex->ALUSrc;

    unsigned int inst = idex->inst;
    unsigned int r_data1 = idex->r_data1;
    unsigned int r_data2 = idex->r_data2;
    unsigned int imm = idex->imm;
    unsigned int rt = idex->rt;
    unsigned int rd = idex->rd;
    unsigned int nextPC = idex->nextPC;
    unsigned int opcode = idex->opcode;
    printf("[execute 0x%x] opcode= 0x%x rt=0x%x rd=0x%x nextPC=0x%x\n", inst, opcode, rt, rd, nextPC);

    exmem->data2 = r_data2;
    exmem->inst = inst;

    unsigned int ALUresult;

    //A forwarding unit selects the correct ALU inputs for the EX stage (r_data1, r_data2) from ALUsrc.
    int forwardA = 0, forwardB = 0;

    if (exmem->regWrite && exmem_o->rd != 0 && exmem_o->rd == idex->rs)
    {

        forwardA = 2;
    }
    if (exmem->regWrite && exmem_o->rd != 0 && exmem_o->rd == idex->rt)
    {

        forwardB = 2;
    }

    if ((memwb_o->regWrite && memwb_o->rd != 0 && memwb_o->rd == idex->rs) && !(exmem->regWrite && exmem_o->rd != 0 && exmem_o->rd == idex->rs))
    {

        forwardA = 1;
    }
    if (memwb_o->regWrite && memwb_o->rd != 0 &&
        memwb_o->rd == idex->rt && !(exmem->regWrite && exmem_o->rd != 0 && exmem_o->rd == idex->rt))
    {

        forwardB = 1;
    }

    //mux for r_data1
    if (forwardA == 1)
    {
        //printf("forward: r_data1 = wb\n");
        r_data1 = wb;
    }
    else if (forwardA == 2)
    {
        // printf("forward: r_data1 = exmem-data2\n");
        //printf("exmem_o: 0x%x, ex_mem[1]: 0x%x, ex_mem[0]: 0x%x\n", exmem_o->data2, ex_mem[0].data2, ex_mem[1].data2);
        r_data1 = exmem_o->data2;
    }

    // printf("r_data2: 0x%x, exmem_o->data2: 0x%x, wb: 0x%x\n", r_data2, exmem->data2, wb);

    //mux for r_data2
    if (forwardA == 1)
    {
        // printf("forward: r_data2 = wb\n");
        r_data2 = wb;
    }
    else if (forwardA == 2)
    {
        // printf("forward: r_data2 = exmem-data2\n");
        r_data2 = exmem_o->data2;
    }

    exmem->data2 = r_data2;

    if (ALUSrc == 0) //r-format & beq
    {
        //printf("ALUSrc is 0\n");
        ALUresult = ALU_r(r_data1, r_data2, imm);
        //regs[rd] = ALUresult;
    }
    else if (ALUSrc == 1) //lw/sw/imms
    {

        ALUresult = ALU_imm(opcode, r_data1, imm);
        //regs[rt] = ALUresult;
    }

    exmem->ALUresult = ALUresult;
    //regDest mux
    if (regDst)
    {
        exmem->regdest = rd;
    }
    else
    {
        exmem->regdest = rt;
    }

    //passing rd
    exmem->rd = idex->rd;

    //passing control to ex_mem register
    exmem->branch = idex->branch;
    exmem->memWrite = idex->memWrite;
    exmem->memRead = idex->memRead;

    exmem->regWrite = idex->regWrite;
    exmem->memToReg = idex->memToReg;
}

int memory(struct exmem_struc *exmem, struct memwb_struc *memwb)
{
    //printf("masuk memory\n");

    //getting control signals from exmem reg
    int branch = exmem->branch;
    int memWrite = exmem->memWrite;
    int memRead = exmem->memRead;
    int ALUresult = exmem->ALUresult;

    //getting data from exmem reg
    unsigned int data2 = exmem->data2;
    unsigned int regdest = exmem->regdest;
    unsigned int rd = exmem->rd;

    unsigned int inst = exmem->inst;

    printf("[memory 0x%x]\n", inst);

    if (memWrite)
    {
        Mem[ALUresult] = data2;
        printf("after memwrite Mem[0x%x] = 0x%x\n", ALUresult, Mem[ALUresult]);
    }
    if (memRead)
    {
        memwb->m_data = Mem[ALUresult];
        //printf("after memread Mem[0x%x] = 0x%x\n", ALUresult, Mem[ALUresult]);
    }

    memwb->inst = inst;
    //printf("memory, ALUresult: 0x%x\n", ALUresult);
    //passing data to wb register
    memwb->regdest = regdest;
    memwb->ALUresult = ALUresult;

    //passing control to wb register
    memwb->regWrite = exmem->regWrite;
    memwb->memToReg = exmem->memToReg;
}

int write_back(struct memwb_struc *memwb, unsigned int *wd)
{
    //printf("masuk writeback \n");
    unsigned inst = memwb->inst;
    printf("[write back 0x%x]\n", inst);
    unsigned int m_data = memwb->m_data;
    unsigned int addr = memwb->addr;
    unsigned int regdest = memwb->regdest;

    //getting control signals from memwb reg
    int regWrite = memwb->regWrite;
    int memToReg = memwb->memToReg;

    unsigned int w_data; //write data

    if (memToReg)
    {
        //The value fed to the register file input comes from Memory.
        w_data = m_data;
        *wd = w_data;
    }
    else
    {
        //printf("w data is from alu result: 0x%x\n", memwb->ALUresult);
        //The value fed to the register file is from the ALU.
        w_data = memwb->ALUresult;
        *wd = w_data;
    }
    if (regWrite)
    {
        //Write register is written with the value of the Write data input.
        regs[regdest] = w_data;
        printf("after regwrite regs[%d] = 0x%x\n", regdest, w_data);
    }

    return 1;
}

unsigned int ALU_r(unsigned int r_data1, unsigned int r_data2, unsigned int imm)
{
    //printf("masuk ALU_r\n");
    //switch thru functions
    unsigned int func = imm & 0x003f;
    unsigned int shamt = (imm & 0x07c0) >> 6;

    switch (func)
    {
    case 0x20:
        //add
        printf("add\n");
        return (r_data1 + r_data2);
        break;
    case 0x21:
        //addu
        printf("addu\n");
        //printf("r_data1 (0x%x) + r_data2 (0x%x) = 0x%x\n", r_data1, r_data2, r_data1 + r_data2);
        return (r_data1 + r_data2);
        break;
    case 0x24:
        //and
        printf("and\n");
        return (r_data1 & r_data2);
        break;
    case 0x08:
        //jr
        printf("jr\n");
        //if_id[0].nextPC = r_data1; //in PC ato nextPC kanggggg IDIKIKDIKD
        PC = r_data1;
        return 0;
        break;
    case 0x27:
        //nor
        printf("nor\n");
        return (~(r_data1 | r_data2));
        break;
    case 0x25:
        //or
        printf("or\n");
        return (r_data1 | r_data2);
        break;
    case 0x2a:
        //slt
        printf("slt\n");
        return (r_data1 < r_data2 ? 1 : 0);
        break;
    case 0x2b:
        //slty
        printf("slty\n");
        return (r_data1 < r_data2 ? 1 : 0);
        break;
    case 0x00:
        //sll
        printf("nop\n");
        return (r_data2 << shamt);
        break;
    case 0x02:
        //srl
        printf("srl\n");
        return (r_data2 >> shamt);
        break;
    case 0x22:
        //sub
        printf("sub\n");
        return (r_data1 - r_data2);
        break;
    case 0x23:
        //subu
        printf("subu\n");
        return (r_data1 - r_data2);
        break;
    default:
        //inst not supported
        if (flush)
        {
            printf("execute controls flushed!\n");
            flush = 0;
        }
        else
        {
            printf("ALU imm instruction not supported\n");
        }
        return 0;
    }
}

unsigned int ALU_imm(unsigned int opcode, unsigned int r_data1, unsigned int imm)
{
    //printf("masuk ALU imm\n");
    unsigned int s_imm;
    unsigned int z_imm;

    if (imm >> 15)
    {
        s_imm = imm | 0xFFFF0000;
    }
    else
    {
        s_imm = imm;
    }

    z_imm = imm;

    //switch thru i instructions
    switch (opcode)
    {
    case 0x8:
        //addi
        printf("addi\n");
        return (r_data1 + s_imm);
        break;
    case 0x9:
        //addiu
        printf("addiu ");
        printf("r_data1 (0x%x) + s_imm (%d) = 0x%x\n", r_data1, s_imm, r_data1 + s_imm);
        return (r_data1 + s_imm);
        break;
    case 0xc:
        //andi
        printf("andi\n");
        return (r_data1 & z_imm);
        break;
    case 0x4:
        //beq
        //printf("beq\n");
        //if(r_data1 == )
        break;
    case 0x5:
        //bne
        //printf("bne\n");
        break;
    case 0x2:
        //j
        //printf("j to 0x%x\n", target_addr);

        //PC = target_addr;
        //jump = 1;
        break;
    case 0x3:
        //jal
        // printf("jal\n");
        // regs[31] = PC + 8;
        // PC = target_addr;
        break;
    case 0x30:
        //ll
        printf("ll\n");
        return (Mem[(r_data1 + s_imm) / 4]); //ini di write back nd sih??
        break;
    case 0xf:
        //lui
        printf("lui\n");
        return (s_imm << 16);
        break;
    case 0x23:
        //lw
        printf("lw\n");
        //return (Mem[(r_data1 + s_imm) / 4]);
        return ((r_data1 + s_imm) / 4);
        break;
    case 0xd:
        //ori
        printf("ori\n");
        return (r_data1 | z_imm);
        break;
    case 0xa:
        //slti
        printf("slti\n");
        return ((r_data1 < s_imm) ? 1 : 0);
        break;
    case 0xb:
        //sltiu
        printf("sltiu\n");
        return ((r_data1 < s_imm) ? 1 : 0);
        break;
    case 0x2b:
        //sw
        printf("sw ");
        printf("r_data1 (0x%x) + s_imm (0x%d), addr = 0x%x\n", r_data1, s_imm, r_data1 + s_imm);
        return ((r_data1 + s_imm) / 4);
        break;
    default:
        //inst not supported
        if (flush)
        {
            printf("execute controls flushed!\n");
            flush = 0;
        }
        else
        {
            printf("ALU imm instruction not supported\n");
        }

        return 0;
    }
}

void flush_e()
{

    flush = 1;
    id_ex[1].regDst = 0;
    id_ex[1].regWrite = 0;
    id_ex[1].ALUSrc = 0;
    id_ex[1].memWrite = 0;
    id_ex[1].memRead = 0;
    id_ex[1].memToReg = 0;
    id_ex[1].branch = 0;
    return;
}
