/*
Cache Simulator on Single-cycle MIPS
4 way set associative
block size / $line size = 8B
address size            = 32b
$ size                  = 512kb
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

#include "linkedlist.c"

int instruction_reg;
unsigned int PC;

#define MAX_REG 32
#define MAX_MEM 16 * 1024 * 1024 / sizeof(int)  //16MB
#define MAX_CACHE 1024 * 1024 / 2 / sizeof(int) //500kb cache size

unsigned int regs[MAX_REG];
unsigned int Mem[MAX_MEM];

struct inst_t
{
    unsigned int opcode;
    unsigned int r_data1, r_data2;
    unsigned int rd, rt;
    unsigned int shamt, func;
    unsigned int imm;

    int jump;
    int jumpreg;
    int regDst;
    int branch;
    int memRead;
    int memToReg;
    int memWrite;
    int ALUSrc;
    int regWrite;

    unsigned int ALUresult; //ALU result in execute func
    unsigned int m_data;    //data from memory
    unsigned int w_data;    //data to be written back to register
};

struct cache_t
{
    unsigned int offset;
    unsigned int index;
    unsigned int tag;

    int valid;
    int dirty;

    int data[8]; //8B offset
};

struct cache_t cache_1[MAX_CACHE / 4];
struct cache_t cache_2[MAX_CACHE / 4];
struct cache_t cache_3[MAX_CACHE / 4];
struct cache_t cache_4[MAX_CACHE / 4];

struct Node *head = NULL; //reference linked list

int missCount = 0;
int hitCount = 0;
float totalMissHit = 0;

unsigned int ALU_r(struct inst_t *inst, unsigned int r_data1, unsigned int r_data2, unsigned int imm);
unsigned int ALU_imm(unsigned int opcode, unsigned int r_data1, unsigned int r_data2, unsigned int imm);

void load_program(int argc, char *argv[], FILE *fd);
void initialize_program();
int fetch();
int decode(struct inst_t *inst);
int execute(struct inst_t *inst);
int memory(struct inst_t *inst);
int write_back(struct inst_t *inst);

void initialize_cache()
{
    //this function initializes all index in cache (4 way). from 0x0 to 0x7ffff
    //so all index are set in 4 ways.
    int count = 0;
    unsigned int index_bits = 0x0;
    for (int i = 0; i < (MAX_CACHE / 4); i++)
    {
        cache_1[i].valid = 0;
        cache_2[i].valid = 0;
        cache_3[i].valid = 0;
        cache_4[i].valid = 0;

        cache_1[i].dirty = 0;
        cache_2[i].dirty = 0;
        cache_3[i].dirty = 0;
        cache_4[i].dirty = 0;

        cache_1[i].index = index_bits;
        cache_2[i].index = index_bits;
        cache_3[i].index = index_bits;
        cache_4[i].index = index_bits;
        index_bits = index_bits + 1;
    }
    //print cache
    // for (int i = 0; i < (MAX_CACHE / 4); i++)
    // {
    //     printf("index at %d is: 0x%x\n", i, cache_1[i].index);
    // }
    return;
}

int main(int argc, char *argv[])
{
    FILE *fd;
    struct inst_t inst;
    int f = 1, d = 1, e = 1, m = 1, wb = 1;

    //printList(head);

    //take in file from arg
    load_program(argc, argv, fd);
    initialize_program();
    initialize_cache();

    //loop:
    //for (int i = 0; i < 30; i++)
    while (1)
    {

        f = fetch();
        d = decode(&inst);
        e = execute(&inst);
        m = memory(&inst);
        wb = write_back(&inst);
        // for (int i = 0; i < 32; i++)
        // {
        //     printf("r[%d] = %x\n", i, regs[i]);
        // }
        if (f == 0 || d == 0 || e == 0 || m == 0 || wb == 0)
        {
            break;
        }
        //printf("PC: 0x%x\n", PC);
    }
    totalMissHit = missCount + hitCount;
    printf("hit rate is: %.2f\%\n", (float)(hitCount / totalMissHit) * 100);
    printf("miss rate is: %.2f\%\n", (float)(missCount / totalMissHit) * 100);

    printf("end of program.\n");
    return 0;
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
        filename = "../test_prog/simple2.bin";
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
void initialize_program()
{
    bzero(regs, sizeof(regs));
    PC = 0;
    regs[0] = 0;                          //set $zero to 0
    regs[29] = (unsigned int *)0x1000000; //set sp to 0x100000
    regs[31] = 0xFFFFFFFF;                //set ra to -1
    return;
}

int locateEmptyBlockInCache(unsigned int index)
{
    if (!cache_1[index].valid)
    {
        return 1;
    }
    else if (!cache_2[index].valid)
    {
        return 2;
    }
    else if (!cache_1[index].valid)
    {
        return 3;
    }
    else if (!cache_1[index].valid)
    {
        return 4;
    }
    else
    {
        printf("it's not a cold miss bro. u messed up.\n");
        exit(1);
    }
}

unsigned int hitReadMemoryAccess(int way, unsigned int index, unsigned int tag, unsigned int offset)
{
    //update reference list: most recently used will be nearest to the head of list
    //---1---find position of [index] [tag] in list
    int position = findPositionInList(head, index, tag);
    //---2---delete node
    deleteNode(&head, position);
    //---3---push node
    push(&head, index, tag, way);

    //returning data
    if (way == 1)
    {
        return cache_1[index].data[offset];
    }
    else if (way == 2)
    {

        return cache_2[index].data[offset];
    }
    else if (way == 3)
    {

        return cache_3[index].data[offset];
    }
    else if (way == 4)
    {

        return cache_4[index].data[offset];
    }
    else
    {
        printf("bro, invalid way wtf? (hit read)\n");
        exit(1);
    }
}

unsigned int missReadMemoryAccess(int location, unsigned int memIndex, unsigned int index, unsigned int tag, unsigned int offset)
{
    if (location == 1)
    {
        //update tag
        cache_1[index].tag = tag;
        //update valid
        cache_1[index].valid = 1;
        //fill block with mem data,
        for (int i = 0; i < 8; i++)
        {
            cache_1[index].data[i] = Mem[(memIndex / 4) + i];
        }
        //update reference list
        push(&head, index, tag, location);
        return cache_1[index].data[offset];
    }
    else if (location == 2)
    {
        //update tag
        cache_2[index].tag = tag;
        //update valid
        cache_2[index].valid = 1;
        //fill block with mem data,
        for (int i = 0; i < 8; i++)
        {
            cache_2[index].data[i] = Mem[(memIndex / 4) + i];
        }
        //update reference list
        push(&head, index, tag, location);
        return cache_2[index].data[offset];
    }
    else if (location == 3)
    {
        //update tag
        cache_3[index].tag = tag;
        //update valid
        cache_3[index].valid = 1;
        //fill block with mem data,
        for (int i = 0; i < 8; i++)
        {
            cache_3[index].data[i] = Mem[(memIndex / 4) + i];
        }
        //update reference list
        push(&head, index, tag, location);
        return cache_3[index].data[offset];
    }
    else if (location == 4)
    {
        //update tag
        cache_4[index].tag = tag;
        //update valid
        cache_4[index].valid = 1;
        //fill block with mem data,
        for (int i = 0; i < 8; i++)
        {
            cache_4[index].data[i] = Mem[(memIndex / 4) + i];
        }
        //update reference list
        push(&head, index, tag, location);
        return cache_4[index].data[offset];
    }
    else
    {
        printf("invalid location bro in cold miss mem access.\n");
    }
}

int leastImportant(unsigned int index)
{
    //find which one is nearest to the end of the list.
    unsigned int tag1 = cache_1[index].tag;
    unsigned int tag2 = cache_2[index].tag;
    unsigned int tag3 = cache_3[index].tag;
    unsigned int tag4 = cache_4[index].tag;

    int pos1 = findPositionInList(head, index, tag1);
    int pos2 = findPositionInList(head, index, tag2);
    int pos3 = findPositionInList(head, index, tag3);
    int pos4 = findPositionInList(head, index, tag4);

    int arr[4] = {pos1, pos2, pos3, pos4};

    int max; //the higher the position, the further from head, the least recently accessed
    for (int i = 0; i < 4; i++)
    {
        if (arr[i] >= max)
        {
            max = arr[i];
        }
    }
    //which means the position of least recently accessed is max.
    if (max == pos1)
    {
        return 1; //meaning cache_1[index]
    }
    else if (max == pos2)
    {
        return 2; //meaning cache_2[index]
    }
    else if (max == pos3)
    {
        return 3; //meaning cache_3[index]
    }
    else if (max == pos4)
    {
        return 4; //meaning cache_4[index]
    }
}

void evictFromCache(int location, unsigned int index, unsigned int memIndex)
{
    /*
    1. check dirty bit, if dirty, write data to mem!
    2. delete from reference list
    */

    //getting needed data & writing dirty data to mem
    int dirtyBit;
    int tagToDelete; //tag of least important block, findPositionInList needs this
    if (location == 1)
    {
        tagToDelete = cache_1[index].tag;
        dirtyBit = cache_1[index].dirty;

        //--writing dirty data to mem
        if (dirtyBit = 1)
        {
            for (int i = 0; i < 8; i++)
            {
                Mem[(memIndex / 4) + i] = cache_1[index].data[i];
            }
        }
        //update dirty bity
        cache_1[index].dirty = 0;
    }
    else if (location == 2)
    {
        tagToDelete = cache_2[index].tag;
        dirtyBit = cache_2[index].dirty;

        //--writing dirty data to mem
        if (dirtyBit = 1)
        {
            for (int i = 0; i < 8; i++)
            {
                Mem[(memIndex / 4) + i] = cache_2[index].data[i];
            }
        }
        //update dirty bity
        cache_2[index].dirty = 0;
    }
    else if (location == 3)
    {
        tagToDelete = cache_3[index].tag;
        dirtyBit = cache_3[index].dirty;

        //--writing dirty data to mem
        if (dirtyBit = 1)
        {
            for (int i = 0; i < 8; i++)
            {
                Mem[(memIndex / 4) + i] = cache_3[index].data[i];
            }
        }
        //update dirty bity
        cache_3[index].dirty = 0;
    }
    else if (location == 4)
    {
        tagToDelete = cache_4[index].tag;
        dirtyBit = cache_4[index].dirty;

        //--writing dirty data to mem
        if (dirtyBit = 1)
        {
            for (int i = 0; i < 8; i++)
            {
                Mem[(memIndex / 4) + i] = cache_4[index].data[i];
            }
        }
        //update dirty bity
        cache_4[index].dirty = 0;
    }

    //--deleting from reference list:
    int position = findPositionInList(head, index, tagToDelete);
    deleteNode(&head, position);
    return;
}

void hitWriteToCache(int way, unsigned int index, unsigned int memIndex, unsigned int value, unsigned int tag, unsigned int offset)
{
    //update reference list: most recently used will be nearest to the head of list
    //---1---find position of [index] [tag] in list
    int position = findPositionInList(head, index, tag);
    //---2---delete node
    deleteNode(&head, position);
    //---3---push node
    push(&head, index, tag, way);

    //writing to cache
    if (way == 1)
    {
        cache_1[index].data[offset] = value;
        cache_1[index].dirty = 1;
    }
    else if (way == 2)
    {

        cache_2[index].data[offset] = value;
        cache_2[index].dirty = 1;
    }
    else if (way == 3)
    {

        cache_3[index].data[offset] = value;
        cache_1[index].dirty = 2;
    }
    else if (way == 4)
    {

        cache_4[index].data[offset] = value;
        cache_1[index].dirty = 2;
    }
    else
    {
        printf("way: %d ", way);
        printf("bro, invalid way wtf? (hit write)\n");
        exit(1);
    }
    return;
}

void missWriteToCache(int location, unsigned int memIndex, unsigned int index, unsigned int tag, unsigned int offset, unsigned int value)
{
    /*
    todo:
    1. write to cache,
    2. update reference & bits
    3. mark block as dirty
    */
    if (location == 1)
    {
        //update tag
        cache_1[index].tag = tag;
        //update valid
        cache_1[index].valid = 1;
        //writing to cache
        cache_1[index].data[offset] = value;
        //changint dirty bit
        cache_1[index].dirty = 1;
        //updating reference
        push(&head, index, tag, location);
    }
    else if (location == 2)
    {
        //update tag
        cache_2[index].tag = tag;
        //update valid
        cache_2[index].valid = 1;
        //writing to cache
        cache_2[index].data[offset] = value;
        //changint dirty bit
        cache_2[index].dirty = 1;
        //updating reference
        push(&head, index, tag, location);
    }
    else if (location == 3)
    {
        //update tag
        cache_3[index].tag = tag;
        //update valid
        cache_3[index].valid = 1;
        //writing to cache
        cache_3[index].data[offset] = value;
        //changint dirty bit
        cache_3[index].dirty = 1;
        //updating reference
        push(&head, index, tag, location);
    }
    else if (location == 4)
    {
        //update tag
        cache_4[index].tag = tag;
        //update valid
        cache_4[index].valid = 1;
        //writing to cache
        cache_4[index].data[offset] = value;
        //changint dirty bit
        cache_4[index].dirty = 1;
        //updating reference
        push(&head, index, tag, location);
    }
    else
    {
        printf("missWriteToCache funct fails\n");
        exit(1);
    }
    return;
}

unsigned int ReadMem(unsigned int memIndex)
{
    //printf("masuk ReadMem\n");
    unsigned int address = &Mem[memIndex];
    //decode address to index, tag, offset
    unsigned int offset = address & 0x00000007;
    unsigned int index = (address & 0x0001fff8) >> 3;
    unsigned int tag = (address & 0xfffe0000) >> 17;

    // printf("address: %x\n", address);
    // printf("offset: %x\n", offset);
    // printf("index: %x\n", index);
    // printf("tag: %x\n", tag);

    int sameIndexCount = countIndex(head, index);
    int location;
    unsigned int inst;

    if (checkHit(head, index, tag))
    {
        //its a hit
        hitCount++;
        printf("it's a read hit\n");
        int way = getWay(head, index, tag);
        inst = hitReadMemoryAccess(way, index, tag, offset);
    }
    else
    {
        //its a miss
        printf("it's a read cold miss\n");
        missCount++;
        if (sameIndexCount < 4)
        {
            //do cold miss operations
            int location = locateEmptyBlockInCache(index); //returns way
            //access memory, fills cache, updates stuff, returns instruction
            inst = missReadMemoryAccess(location, memIndex, index, tag, offset);
        }
        else if (sameIndexCount == 4)
        {
            printf("it's conflict read miss\n");
            //conflict operations
            int location = leastImportant(index);      //returns way
            evictFromCache(location, index, memIndex); //if dirty, writing data to memory &updating reference list before evicted block is overwritten
            inst = missReadMemoryAccess(location, memIndex, index, tag, offset);
        }
    }
    return inst;
}

void WriteMem(unsigned int memIndex, unsigned int value)
{
    unsigned int address = &Mem[memIndex];
    //decode address to index, tag, offset
    unsigned int offset = address & 0x00000007;
    unsigned int index = (address & 0x0001fff8) >> 3;
    unsigned int tag = (address & 0xfffe0000) >> 17;

    // printf("address: %x\n", address);
    // printf("offset: %x\n", offset);
    // printf("index: %x\n", index);
    // printf("tag: %x\n", tag);

    int sameIndexCount = countIndex(head, index);
    int location;
    unsigned int inst;

    if (checkHit(head, index, tag))
    {
        //its a hit
        hitCount++;
        printf("it's a write hit\n");
        int way = getWay(head, index, tag);
        hitWriteToCache(way, index, memIndex, value, tag, offset);
    }
    else
    {
        //its a miss
        /*
        check whether it's a cold miss or conflict:
        if cold miss
        --1--: locate empty
        --2--: writeToLocation (write to cache, update reference, mark block as dirty)
        */
        //printf("it's a miss\n");
        missCount++;
        if (sameIndexCount < 4)
        {
            printf("it's a cold write miss\n");
            //do cold miss operations
            int location = locateEmptyBlockInCache(index); //returns way
            missWriteToCache(location, memIndex, index, tag, offset, value);
        }
        else if (sameIndexCount == 4)
        {
            printf("it's a conflict write miss\n");
            //conflict operations
            int location = leastImportant(index);      //returns way
            evictFromCache(location, index, memIndex); //if dirty, writing data to memory &updating reference list before evicted block is overwritten
            missWriteToCache(location, memIndex, index, tag, offset, value);
        }
    }
}

int fetch()
{
    if (PC == 0xFFFFFFFF)
        return 0;
    memset(&instruction_reg, 0, sizeof(instruction_reg));

    //instruction_reg = Mem[PC / 4];
    printf("==================\n");
    instruction_reg = ReadMem(PC);
    //printf("fetch w mem: 0x%x\n", Mem[PC / 4]);

    //printf("==================\n");
    printf("Fet: 0x%08x\n", instruction_reg);
    //printf("==================\n");

    return 1;
}
int decode(struct inst_t *inst)
{
    //printf("decode\n");
    //using bitwise operators to extract parts from instruction
    unsigned int opcode = (instruction_reg >> 26) & 0x0000003f;
    unsigned int rs = (instruction_reg & 0x03e00000) >> 21;
    unsigned int rt = (instruction_reg & 0x001f0000) >> 16;
    unsigned int rd = (instruction_reg & 0x0000f800) >> 11;
    unsigned int shamt = (instruction_reg & 0x000007c0) >> 6;
    unsigned int imm = instruction_reg & 0x0000ffff;
    unsigned int jump_addr = (instruction_reg & 0x03ffffff) << 2;

    inst->opcode = opcode;
    inst->r_data1 = regs[rs];
    inst->r_data2 = regs[rt];
    inst->rt = rt;
    inst->rd = rd;
    inst->imm = imm;

    //printf("opcode: %x\n", opcode);
    //opcode sets controls

    switch (opcode)
    {
    case 0x0: //r-type
        inst->regDst = 1;
        inst->regWrite = 1;
        inst->ALUSrc = 0;
        inst->memWrite = 0;
        inst->memRead = 0;
        inst->memToReg = 0;
        inst->branch = 0;
        inst->jump = 0;
        inst->jumpreg = 0;
        break;
    case 0x23: //lw
    case 0x30:
        //case 0xf: lui
        inst->regDst = 0;
        inst->regWrite = 1;
        inst->ALUSrc = 1;
        inst->memWrite = 0;
        inst->memRead = 1;
        inst->memToReg = 1;
        inst->branch = 0;
        inst->jump = 0;
        inst->jumpreg = 0;
        break;
    case 0x2b: //sw
        //regDst = 0;
        inst->regWrite = 0;
        inst->ALUSrc = 1;
        inst->memWrite = 1;
        inst->memRead = 0;
        inst->branch = 0;
        inst->jump = 0;
        inst->jumpreg = 0;
        break;
    case 0x4: //beq
    case 0x5: //bne
        //regDst = 0;
        inst->regWrite = 0;
        inst->ALUSrc = 1;
        inst->memWrite = 0;
        inst->memRead = 0;
        inst->branch = 1;
        inst->jump = 0;
        inst->jumpreg = 0;
        break;
    case 0x2: //j
    case 0x3: //jal
        inst->regWrite = 0;
        inst->ALUSrc = 1;
        inst->memWrite = 0;
        inst->memRead = 0;
        inst->branch = 0;
        inst->jump = 1;
        inst->jumpreg = 0;
        break;
    case 0x8: //addi
    case 0x9: //addiu
    case 0xc: //andi
    case 0xd: //ori
    case 0xa: //slti
    case 0xb: //sltiu
        inst->regDst = 0;
        inst->regWrite = 1;
        inst->ALUSrc = 1;
        inst->memWrite = 0;
        inst->memRead = 0;
        inst->memToReg = 0;
        inst->branch = 0;
        inst->jump = 0;
        inst->jumpreg = 0;
        break;
    default:
        printf("opcode probz\n");
        return 0;
    }

    return 1;
}

int execute(struct inst_t *inst)
{

    unsigned int ALUresult;

    unsigned int opcode = inst->opcode;
    unsigned int r_data1 = inst->r_data1;
    unsigned int r_data2 = inst->r_data2;
    unsigned int imm = inst->imm;
    int ALUSrc = inst->ALUSrc;

    //ALU
    if (ALUSrc == 0) //r-format & beq
    {

        //printf("ALUSrc is 0\n");
        ALUresult = ALU_r(inst, r_data1, r_data2, imm);
    }
    else if (ALUSrc == 1) //lw/sw/imms
    {

        ALUresult = ALU_imm(opcode, r_data1, r_data2, imm);
    }

    inst->ALUresult = ALUresult;
    //printf("ALU result in exec = 0x%x\n", inst->ALUresult);

    //computing branch addr
    unsigned int b_imm;
    if (inst->imm >> 15)
    {
        b_imm = imm | 0xFFFF0000;
    }
    else
    {
        b_imm = imm;
    }
    unsigned int branch_addr = (PC + 4) + (b_imm * 4);
    //branch mux
    unsigned int target_addr;
    if (inst->branch && ALUresult)
    {
        target_addr = branch_addr;
    }
    else
    {
        target_addr = PC + 4;
    }

    //computing jump addr
    unsigned int jump_addr = (instruction_reg & 0x03ffffff) << 2;
    //mux for j
    if (inst->jump)
    {
        target_addr = jump_addr;
    } //target_addr as is

    //mux for jr
    if (inst->jumpreg)
    {
        PC = inst->r_data1;
    }
    else
    {
        PC = target_addr;
    }

    return 1;
}
int memory(struct inst_t *inst)
{
    if (inst->memWrite)
    {
        //Mem[inst->ALUresult] = inst->r_data2;
        unsigned int memIndex = inst->ALUresult;
        unsigned int value = inst->r_data2;
        WriteMem(memIndex, value);
        //printf("after memwrite Mem[0x%x] = 0x%x, 0x%x\n", inst->ALUresult, Mem[inst->ALUresult], inst->r_data2);
    }
    if (inst->memRead)
    {
        //inst->m_data = Mem[inst->ALUresult];
        inst->m_data = ReadMem(inst->ALUresult);
        //printf("after memread m_data = 0x%x\n", Mem[inst->ALUresult]);
    }
    return 1;
}
int write_back(struct inst_t *inst)
{
    unsigned int m_data = inst->m_data;
    unsigned int w_data = inst->w_data;
    unsigned int ALUresult = inst->ALUresult;
    if (inst->memToReg)
    {
        //The value fed to the register file input comes from Memory.
        w_data = m_data;
        //printf("w_data m data\n");
    }
    else
    {
        //printf("w data is from alu result: 0x%x\n", memwb->ALUresult);
        //The value fed to the register file is from the ALU.
        w_data = ALUresult;
        //printf("w_data alu reslut\n");
    }
    if (inst->regWrite)
    {
        //Write register is written with the value of the Write data input.
        regs[inst->regDst] = w_data;
        //printf("after regwrite regs[%d] = 0x%x\n", inst->regDst, w_data);
    }
    return 1;
}

unsigned int ALU_imm(unsigned int opcode, unsigned int r_data1, unsigned int r_data2, unsigned int imm)
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
        printf("beq\n");
        if (r_data1 == r_data2)
        {
            printf("branch\n");
            return 1;
        }
        else
        {
            return 0;
        }
        break;
    case 0x5:
        //bne
        printf("bne\n");
        if (r_data1 != r_data2)
        {
            printf("branch\n");
            return 1;
        }
        else
        {
            return 0;
        }
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
        regs[31] = PC + 8;
        // PC = target_addr;
        break;
    case 0x30:
        //ll
        printf("ll\n");
        printf("why do u even use ll bruh\n");
        exit(1);
        //return (Mem[(r_data1 + s_imm) / 4]); //ini di write back nd sih??
        return ((r_data1 + s_imm) / 4);
        break;
    case 0xf:
        //lui
        printf("lui\n");
        return (s_imm << 16);
        break;
    case 0x23:
        //lw
        printf("lw ");
        //return (Mem[(r_data1 + s_imm) / 4]);
        //printf("r_data1 (0x%x) + s_imm (0x%d), addr = 0x%x\n", r_data1, s_imm, r_data1 + s_imm);
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
        printf("ALU imm instruction not supported\n");

        return 0;
    }
}

unsigned int ALU_r(struct inst_t *inst, unsigned int r_data1, unsigned int r_data2, unsigned int imm)
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
        inst->jumpreg = 1;
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

        printf("ALU r instruction not supported\n");
        printf("opcode: 0x%x, func: 0x%x\n", inst->opcode, func);

        exit(1);
    }
}
