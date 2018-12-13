#pragma once 

#include <cinttypes>
#include <vector>

#include "isa.h"

/**
 *  MIPS pipelined processor.
 */
class Executor {
public:
    Executor(ISA::TextSegment &text, ISA::DataSegment &data);

    /**
        Run program.

        @param text_segment reference to text segment.
    */
    void run(const std::string &MODE, const int N);

    /**
        Dump register contents, latch values & resource utilization report   
    */
    void dump(const std::string &output_filename);
private:
    /*
        register file
    */
    static const std::size_t NUM_REG = 32;
    std::vector<std::int32_t> reg;
    std::int32_t HI, LO;
    ISA::Address PC;
    ISA::Address DPC;
    
    /*
        pipeline
     */
    // state:
    enum Stage {
        IF = 0,
        ID = 1,
        EX = 2,
        MEM = 3,
        WB = 4,
        NUM_STAGES = 5
    };

    // logic -- instruction fetch:
    void execute_IF();
    // logic -- instruction decoding:
    void execute_ID();
    // logic -- execution:
    void execute_mult();
    void execute_shift(ISA::Word);
    void execute_r_type_instruction(void);
    void execute_set(ISA::Word opcode);
    void execute_i_type_instruction(void);
    void execute_EX();
    // logic -- memory access:
    void execute_MEM();
    // logic -- write back:
    void execute_reg_write(std::int32_t reg_addr, std::int32_t value);
    void execute_WB();

    // register -- IF/ID:
    struct {
        bool nop;

        ISA::MachineCode IR;
        ISA::Address IPC;
        ISA::Address NPC;

        void reset(void) {
            nop = true;
            IR = IPC = NPC = 0x00000000;
        }
    } IF_ID;
    // register -- ID/EX:
    struct {
        bool nop;

        ISA::MachineCode IR;
        ISA::Address IPC;
        ISA::Address NPC;
        std::int32_t A;
        std::int32_t B;
        std::int32_t Imm;
        std::int32_t WriteRegAddr;

        void reset(void) {
            nop = true;
            IR = IPC = NPC = A = B = Imm = WriteRegAddr = 0x00000000;
        }
    } ID_EX;
    // register -- EX/MEM:
    struct {
        bool nop;

        ISA::MachineCode IR;
        ISA::Address IPC;
        std::int64_t ALUOutput;
        std::int32_t B;
        std::uint8_t Cond;
        std::int32_t WriteRegAddr;

        void reset(void) {
            nop = true;
            IR = IPC = ALUOutput = B = Cond = WriteRegAddr = 0x00000000;
        }
    } EX_MEM;
    // register -- MEM/WB
    struct {
        bool nop;

        ISA::MachineCode IR;
        ISA::Address IPC;
        std::int64_t ALUOutput;
        std::int32_t LMD;
        std::int32_t WriteRegAddr;

        void reset(void) {
            nop = true;
            IR = IPC = ALUOutput = LMD = WriteRegAddr = 0x00000000;
        }
    } MEM_WB;

    // hazard 
    struct {
        bool data;
        bool control;

        void reset(void) {
            data = control = false;
        }
    } hazard;

    // monitor:
    struct {
        // total number of clock cycles:
        std::int32_t total_clock_cycles;
        // total number of instructions:
        std::int32_t total_instructions;
        // utilization:
        std::int32_t nop_count[Stage::NUM_STAGES];

        void reset(void) {
            total_clock_cycles = total_instructions = 0;
            for (std::size_t i = 0; i < Stage::NUM_STAGES; ++i) {
                nop_count[i] = 0;
            }
        }
    } monitor;

    ISA::TextSegment &text_segment;
    ISA::DataSegment &data_segment;

    void init(void);
    bool is_terminated(const std::string &MODE, const int N);
    void execute_pipeline(void);
    void dump_pipeline_state(void);
};