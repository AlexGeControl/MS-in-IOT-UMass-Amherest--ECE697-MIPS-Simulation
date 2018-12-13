#pragma once 

#include <string>
#include <cinttypes>
#include <map>

namespace ISA {
    /*
        instruction type
     */
    enum Type {
        R_TYPE,
        I_TYPE,
        J_TYPE
    };

    /*
        instruction fields
     */
    enum Field {
        OPCODE,
        RS,
        RT,
        RD,
        SHAMT,
        FUNCT,
        IMM
    };
    
    /*
        opcodes:
     */
    enum OpCode {
        R_COMMON = 0x00,
        ADDI = 0x08,
        ANDI = 0x0C,
        ORI = 0x0D,
        SLTI = 0x0A,
        SLTIU = 0x0B,
        BEQ = 0x04,
        LUI = 0x0F,
        LW = 0x23,
        SW = 0x2B
    };

    /*
        functs:
     */
    enum Funct {
        ADD = 0x20,
        SUB = 0x22,
        AND = 0x24,
        OR = 0x25,
        MUL = 0x26,
        MULT = 0x18,
        SLL = 0x00,
        SRL = 0x02
    };

    /*
        register file:
    */
    const std::map<std::string, std::uint8_t> REGISTER_FILE = {
        {"zero",  0},
        {  "at",  1},
        {  "v0",  2},
        {  "v1",  3},
        {  "a0",  4},
        {  "a1",  5},
        {  "a2",  6},
        {  "a3",  7},
        {  "t0",  8},
        {  "t1",  9},
        {  "t2", 10},
        {  "t3", 11},
        {  "t4", 12},
        {  "t5", 13},
        {  "t6", 14},
        {  "t7", 15},
        {  "s0", 16},
        {  "s1", 17},
        {  "s2", 18},
        {  "s3", 19},
        {  "s4", 20},
        {  "s5", 21},
        {  "s6", 22},
        {  "s7", 23},
        {  "t8", 24},
        {  "t9", 25},
        {  "k0", 26},
        {  "k1", 27},
        {  "gp", 28},
        {  "sp", 29},
        {  "fp", 30},
        {  "ra", 31}
    };

    typedef std::uint32_t Word;
    typedef std::uint32_t Address;
    typedef std::uint32_t MachineCode;

    void set_instruction_field(MachineCode &machine_code, Field field, Word value);
    Word get_instruction_field(const MachineCode &machine_code, Field field);

    /*
        instruction memory
     */
    struct Instruction {
    public:
        std::uint32_t binary;
        std::string text;
    };

    class TextSegment {
    public:
        TextSegment() {}
        /**
            Get first & last addresses of text segment.
        */
        Address get_address_first(void) {
            return instruction_memory.begin()->first;
        }
        Address get_address_last(void) {
            return instruction_memory.rbegin()->first;
        }

        /**
            Set instruction in text segment.

            @param address instruction address.
            @param machine_code instruction machine code.
        */
        void set(Address address, Instruction instruction) {
            instruction_memory.insert(
                {address, instruction}
            );
        }

        /**
            Get instruction from text segment.

            @param address instruction address.
        */
        std::string get_text(Address address) {
            if (address < instruction_memory.begin()->first || address > instruction_memory.rbegin()->first) {
                return "nop";
            }

            return (instruction_memory.find(address)->second).text;
        } 
        std::uint32_t get_binary(Address address) {
            if (address < instruction_memory.begin()->first || address > instruction_memory.rbegin()->first) {
                return 0x00000000;
            }

            return (instruction_memory.find(address)->second).binary;
        } 
    private:
        std::map<Address, Instruction> instruction_memory;
    };

    /*
        data memory
     */
    class DataSegment {
    public:
        DataSegment(Word default_word): DEFAULT(default_word) {}

        Word get(Address address) {
            auto result = data_memory.find(address);

            if (data_memory.end() == result) {
                data_memory.insert({address, DEFAULT});
                return DEFAULT;
            }

            return result->second;
        }

        void set(Address address, Word word) {
            data_memory.insert({address, word});
        }
    private:
        const Word DEFAULT;
        std::map<Address, Word> data_memory;
    };
}
