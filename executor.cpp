#include "executor.h"

#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>

#include "json.h"

Executor::Executor(ISA::TextSegment &text, ISA::DataSegment &data): text_segment(text), data_segment(data) {
    // initialize register file:
    reg = std::vector<std::int32_t>(NUM_REG, 0x00000000);
}

/**
    Run program.

    @param MODE execution mode.
    @param N execution time.
*/
void Executor::run(const std::string &MODE, const int N) {
    // initialize pipeline:
    init();

    // initialize PC:
    PC = text_segment.get_address_first();
    const ISA::Address TEXT_SEGMENT_END = text_segment.get_address_last();

    // execute:
    while (DPC != TEXT_SEGMENT_END) {
        // termination check:
        if (is_terminated(MODE, N)) {
            return;
        }

        // dump pipeline state each cycle for better illustration:
        dump_pipeline_state();

        // execute pipeline:
        execute_pipeline();

        // update clock cycle count:
        monitor.total_clock_cycles += 1;
    }
}

/**
    Dump register contents, latch values & resource utilization report   
*/
void Executor::dump(const std::string &output_filename) {
    std::ofstream output(output_filename);

	if(!output) {
		std::cerr << "[MIPS simulator]: ERROR -- cannot open output resource utilization file "<< output_filename <<std::endl;
        return; 
	}

    nlohmann::json execution_report;

    // 1. register contents:
    execution_report["register contents"] = {};
    for (
        std::map<std::string, std::uint8_t>::const_iterator it = ISA::REGISTER_FILE.begin();
        ISA::REGISTER_FILE.end() != it;
        ++it
    ) {
        std::stringstream ss;
        ss << "0x" << std::setfill ('0') << std::setw(8) << std::hex << reg[it->second];
        execution_report["register contents"][it->first] = ss.str();
    }

    // 2. latch values:

    // 3. resource utilization report:
    execution_report["resource utilization"] = {};
    execution_report["resource utilization"]["total clock cycles"] = monitor.total_clock_cycles;
    execution_report["resource utilization"]["total instructions"] = monitor.total_instructions;
    execution_report["resource utilization"]["nop analysis"] = {};
    execution_report["resource utilization"]["nop analysis"]["IF"] = { 
        {"count", monitor.nop_count[Stage::IF]}, {"percentage", (100.0 * monitor.nop_count[Stage::IF]) / monitor.total_clock_cycles} 
    };
    execution_report["resource utilization"]["nop analysis"]["ID"] = { 
        {"count", monitor.nop_count[Stage::ID]}, {"percentage", (100.0 * monitor.nop_count[Stage::ID]) / monitor.total_clock_cycles} 
    };
    execution_report["resource utilization"]["nop analysis"]["EX"] = { 
        {"count", monitor.nop_count[Stage::EX]}, {"percentage", (100.0 * monitor.nop_count[Stage::EX]) / monitor.total_clock_cycles} 
    };
    execution_report["resource utilization"]["nop analysis"]["MEM"] = { 
        {"count", monitor.nop_count[Stage::MEM]}, {"percentage", (100.0 * monitor.nop_count[Stage::MEM]) / monitor.total_clock_cycles} 
    };
    execution_report["resource utilization"]["nop analysis"]["WB"] = { 
        {"count", monitor.nop_count[Stage::WB]}, {"percentage", (100.0 * monitor.nop_count[Stage::WB]) / monitor.total_clock_cycles} 
    };

    output << execution_report.dump(4) << std::endl;

	// close output file:
	output.close(); 
}

/*
    MIPS pipeline -- instruction fetch 
*/
void Executor::execute_IF() {
    if (hazard.control) {
        if (ISA::OpCode::BEQ == ISA::get_instruction_field(EX_MEM.IR, ISA::Field::OPCODE)) {
            // control hazard resolved:
            if (EX_MEM.Cond) {
                PC = EX_MEM.ALUOutput;
            }

            hazard.control = false;
        } else {
            // insert nop:
            IF_ID.reset();
            monitor.nop_count[Stage::IF] += 1;
            return;
        } 
    }

    if (hazard.data) {
        // insert nop:
        monitor.nop_count[Stage::IF] += 1;
        return;
    }

    if (text_segment.get_address_last() < PC) {
        // insert nop:
        IF_ID.reset();
        monitor.nop_count[Stage::IF] += 1;
        return;
    }

    IF_ID.nop = false;
    IF_ID.IPC = PC;

    // update instruction count:
    ISA::MachineCode instruction = text_segment.get_binary(PC);
    PC = PC + 4;
    monitor.total_instructions += 1;

    IF_ID.IR = instruction;
    IF_ID.NPC = PC;
}
/*
    MIPS pipeline -- instruction decoding 
*/
void Executor::execute_ID() {
    if (IF_ID.nop) {
        // insert nop:
        ID_EX.reset();
        monitor.nop_count[Stage::ID] += 1;
        return;
    }

    ISA::Word opcode = ISA::get_instruction_field(IF_ID.IR, ISA::Field::OPCODE);
    
    // control hazard detected:
    if (ISA::OpCode::BEQ == opcode) {
        hazard.control = true;
    }

    ISA::Word a_reg_addr = ISA::get_instruction_field(IF_ID.IR, ISA::Field::RS);
    ISA::Word b_reg_addr = ISA::get_instruction_field(IF_ID.IR, ISA::Field::RT);

    // data hazard detected:
    if (
        (EX_MEM.WriteRegAddr != 0x0 && EX_MEM.WriteRegAddr == a_reg_addr) ||
        (MEM_WB.WriteRegAddr != 0x0 && MEM_WB.WriteRegAddr == a_reg_addr) ||
        (EX_MEM.WriteRegAddr != 0x0 && EX_MEM.WriteRegAddr == b_reg_addr) ||
        (MEM_WB.WriteRegAddr != 0x0 && MEM_WB.WriteRegAddr == b_reg_addr)     
    ) {
        hazard.data = true;
    }

    if (hazard.data) {
        ID_EX.reset();
        monitor.nop_count[Stage::ID] += 1;
        return;
    }

    ID_EX.nop = false;

    ID_EX.IR = IF_ID.IR;
    ID_EX.IPC = IF_ID.IPC;
    ID_EX.NPC = IF_ID.NPC;

    ID_EX.A = reg[a_reg_addr];
    ID_EX.B = reg[b_reg_addr];

    if (ISA::OpCode::R_COMMON == opcode) {
        ID_EX.Imm = 0x00000000;
        ID_EX.WriteRegAddr = ISA::get_instruction_field(IF_ID.IR, ISA::Field::RD);
    } else {
        // load imm by sign extension:
        ID_EX.Imm = ISA::get_instruction_field(IF_ID.IR, ISA::Field::IMM) << 16;
        ID_EX.Imm >>= 16; 

        ID_EX.WriteRegAddr = ISA::get_instruction_field(IF_ID.IR, ISA::Field::RT);
    }
}
/*
    MIPS pipeline -- execution 
*/
void Executor::execute_mult(void) {
    // perform multiplication with extended precision:
    EX_MEM.ALUOutput = static_cast<std::int64_t>(ID_EX.A) * static_cast<std::int64_t>(ID_EX.B);
}

void Executor::execute_shift(ISA::Word funct) {
    ISA::Word shamt = ISA::get_instruction_field(ID_EX.IR, ISA::Field::SHAMT);

    EX_MEM.ALUOutput = ((ISA::Funct::SLL == funct) ? (ID_EX.B << shamt) : (ID_EX.B >> shamt));
}

void Executor::execute_r_type_instruction(void) {
    // extract funct:
    ISA::Word funct = ISA::get_instruction_field(ID_EX.IR, ISA::Field::FUNCT);

    switch (funct) {
        case ISA::Funct::ADD:
            EX_MEM.ALUOutput = ID_EX.A + ID_EX.B;
            break;
        case ISA::Funct::SUB:
            EX_MEM.ALUOutput = ID_EX.A - ID_EX.B;
            break;
        case ISA::Funct::AND:
            EX_MEM.ALUOutput = ID_EX.A & ID_EX.B;
            break;
        case ISA::Funct::OR:
            EX_MEM.ALUOutput = ID_EX.A | ID_EX.B;
            break;
        case ISA::Funct::MUL:
        case ISA::Funct::MULT:
            execute_mult();
            break;
        case ISA::Funct::SLL:
        case ISA::Funct::SRL:
            execute_shift(funct);
            break;
        default:
            break;
    }
}

void Executor::execute_set(ISA::Word opcode) {
    // extract operand:
    std::int32_t operand = ID_EX.Imm;
    if (ISA::OpCode::SLTIU == opcode) {
        operand &= 0xFFFF;
    }

    EX_MEM.ALUOutput = ((ID_EX.A < operand) ? (0x00000001) : (0x00000000));
}

void Executor::execute_i_type_instruction(void) {
    ISA::Word opcode = ISA::get_instruction_field(ID_EX.IR, ISA::Field::OPCODE);

    switch (opcode) {
        case ISA::OpCode::ADDI:
        case ISA::OpCode::LW:
        case ISA::OpCode::SW:
            EX_MEM.ALUOutput = ID_EX.A + ID_EX.Imm;
            break;
        case ISA::OpCode::ANDI:
            EX_MEM.ALUOutput = ID_EX.A & ID_EX.Imm;
            break;
        case ISA::OpCode::ORI:
            EX_MEM.ALUOutput = ID_EX.A | ID_EX.Imm;
            break;
        case ISA::OpCode::LUI:
            EX_MEM.ALUOutput = ID_EX.Imm << 16;
            break;
        case ISA::OpCode::SLTI:
        case ISA::OpCode::SLTIU:
            execute_set(opcode);
            break;
        case ISA::OpCode::BEQ:
            EX_MEM.ALUOutput = ID_EX.NPC + (ID_EX.Imm << 2);
            EX_MEM.Cond = (ID_EX.A == ID_EX.B);
            break;
        default:
            break;
    }
}

void Executor::execute_EX(void) {
    if (ID_EX.nop) {
        EX_MEM.reset();
        monitor.nop_count[Stage::EX] += 1;
        return;
    }

    EX_MEM.nop = false;
    EX_MEM.IR = ID_EX.IR;
    EX_MEM.IPC = ID_EX.IPC;
    EX_MEM.B = ID_EX.B;
    EX_MEM.WriteRegAddr = ID_EX.WriteRegAddr;

    // execute according to opcode:
    ISA::Word opcode = ISA::get_instruction_field(ID_EX.IR, ISA::Field::OPCODE);
    switch (opcode) {
        case ISA::OpCode::R_COMMON:
            execute_r_type_instruction();
            break;
        case ISA::OpCode::ADDI:
        case ISA::OpCode::LW:
        case ISA::OpCode::SW:
        case ISA::OpCode::ANDI:
        case ISA::OpCode::ORI:
        case ISA::OpCode::LUI:
        case ISA::OpCode::SLTI:
        case ISA::OpCode::SLTIU:
        case ISA::OpCode::BEQ:
            execute_i_type_instruction();
            break;
        default:
            break;
    }
}
/*
    MIPS pipeline -- memory access 
*/
void Executor::execute_MEM() {
    if (EX_MEM.nop) {
        MEM_WB.reset();
        monitor.nop_count[Stage::MEM] += 1;
        return;
    }

    MEM_WB.nop = false;
    MEM_WB.IR = EX_MEM.IR;
    MEM_WB.IPC = EX_MEM.IPC;

    switch (ISA::get_instruction_field(MEM_WB.IR, ISA::Field::OPCODE)) {
        case ISA::OpCode::R_COMMON:
        case ISA::OpCode::ADDI:
        case ISA::OpCode::ANDI:
        case ISA::OpCode::ORI:
        case ISA::OpCode::SLTI:
        case ISA::OpCode::SLTIU:
        case ISA::OpCode::LUI:
            MEM_WB.ALUOutput = EX_MEM.ALUOutput;
            MEM_WB.LMD = 0x00000000;
            MEM_WB.WriteRegAddr = EX_MEM.WriteRegAddr;
            monitor.nop_count[Stage::MEM] += 1;
            break;
        case ISA::OpCode::SW:
            data_segment.set(EX_MEM.ALUOutput, EX_MEM.B);
            MEM_WB.ALUOutput = 0x00000000;
            MEM_WB.LMD = 0x00000000;
            MEM_WB.WriteRegAddr = 0x00000000;
            break;
        case ISA::OpCode::LW:
            MEM_WB.ALUOutput = 0x00000000;
            MEM_WB.LMD = data_segment.get(EX_MEM.ALUOutput);
            MEM_WB.WriteRegAddr = EX_MEM.WriteRegAddr;
            break;
        default:
            MEM_WB.ALUOutput = 0x00000000;
            MEM_WB.LMD = 0x00000000;
            MEM_WB.WriteRegAddr = 0x00000000;
            monitor.nop_count[Stage::MEM] += 1;
            break;
    }
}
/*
    MIPS pipeline -- write back 
*/
void Executor::execute_reg_write(std::int32_t reg_addr, std::int32_t value) {
    if (0x0 != reg_addr) {
        // write back:
        reg[reg_addr] = value;

        // resolve data hazard:
        if (hazard.data) {
            hazard.data = false;
        }
    }
}

void Executor::execute_WB() {
    if (MEM_WB.nop) {
        monitor.nop_count[Stage::WB] += 1;
        return;
    }

    switch (ISA::get_instruction_field(MEM_WB.IR, ISA::Field::OPCODE)) {
        case ISA::OpCode::R_COMMON:
            switch (ISA::get_instruction_field(MEM_WB.IR, ISA::Field::FUNCT)) {
                case ISA::Funct::ADD:
                case ISA::Funct::SUB:
                case ISA::Funct::AND:
                case ISA::Funct::OR:
                case ISA::Funct::SLL:
                case ISA::Funct::SRL:
                    execute_reg_write(MEM_WB.WriteRegAddr, MEM_WB.ALUOutput);
                    break;
                case ISA::Funct::MUL:
                    execute_reg_write(
                        MEM_WB.WriteRegAddr, 
                        MEM_WB.ALUOutput
                    );
                    execute_reg_write(
                        MEM_WB.WriteRegAddr + 1, 
                        (MEM_WB.ALUOutput >> 32)
                    );
                    break;
                case ISA::Funct::MULT:
                    LO = MEM_WB.ALUOutput;
                    HI = MEM_WB.ALUOutput >> 32;
                default:
                    break;
            }
            break;
        case ISA::OpCode::ADDI:
        case ISA::OpCode::ANDI:
        case ISA::OpCode::ORI:
        case ISA::OpCode::SLTI:
        case ISA::OpCode::SLTIU:
        case ISA::OpCode::LUI:
            execute_reg_write(MEM_WB.WriteRegAddr, MEM_WB.ALUOutput); 
            break;
        case ISA::OpCode::LW:
            execute_reg_write(MEM_WB.WriteRegAddr, MEM_WB.LMD);
            break;
        default:
            monitor.nop_count[Stage::WB] += 1;
            break;
    }

    DPC = MEM_WB.IPC;
}

/**
    Run pipeline in reverse order to eliminate the need of intermediate buffer   
*/
void Executor::execute_pipeline(void) {
    /*
        Write Back
            Input: MEM/WB
            Output: DPC for just finished instruction
     */
    execute_WB();
    /*
        Memory Access
            Input: EX/MEM
            Output: MEM/WB
     */
    execute_MEM();
    /*
        Execution
            Input: ID/EX
            Output: EX/MEM
     */
    execute_EX();
    /*
        Instruction Decoding
            Input: IF/ID
            Output: ID/EX
     */
    execute_ID();
    /*
        Write Back
            Input: PC
            Output: IF/ID
     */
    execute_IF();
}

void Executor::init(void) {
    IF_ID.reset();
    ID_EX.reset();
    EX_MEM.reset();
    MEM_WB.reset();
    hazard.reset();
    monitor.reset();

    PC = DPC = 0x00000000;
}

bool Executor::is_terminated(const std::string &MODE, const int N) {
    bool result = false;

    if (
        ("instruction" == MODE && monitor.total_instructions >= N ) ||
        ("cycle" == MODE && monitor.total_clock_cycles >= N)
    ) {
        result = true;
    }

    return result;
}

void Executor::dump_pipeline_state(void) {
    // clock cycle:
    std::cout << "[Clock Cycle]: " << monitor.total_clock_cycles << std::endl;
    // pipeline state:
    std::cout << "\tIF: " << text_segment.get_text(PC) << std::endl;
    std::cout << "\tID: " << text_segment.get_text(IF_ID.IPC) << std::endl;
    std::cout << "\tEX: " << text_segment.get_text(ID_EX.IPC) << std::endl;
    std::cout << "\tMEM: " << text_segment.get_text(EX_MEM.IPC) << std::endl;
    std::cout << "\tWB: " << text_segment.get_text(MEM_WB.IPC) << std::endl;

    std::cout << std::endl;
}