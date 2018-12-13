#pragma once 

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <cstdio>
#include <cinttypes>

#include "isa.h"

/**
 *  MIPS ASM assembler.
 */
class Assembler {
public:
    Assembler(const std::string &input_filename, std::uint32_t text_starting_addr = 0x00400000);

    /**
        Get built text segment    
    */
    ISA::TextSegment get_text_segment(void) {return text_segment;}

    /**
        Dump parsed machine code to output file.
        for online validation, please go to https://www.eg.bucknell.edu/%7Ecsci320/mips_web/

        @param output_filename output filename.    
    */
    void dump(const std::string &output_filename);
private:
    struct RTypeField {
        std::uint32_t opcode;
        std::uint32_t funct;
    };
    struct ITypeField {
        std::uint32_t opcode;
    };
    struct Decoder {
        ISA::Type type;
        std::string regex;
        std::map<std::size_t, ISA::Field> group_name;
    };

    /*
        opcode + funct
    */
    static const std::map<std::string, RTypeField> R_TYPE_FIELD;
    static const std::map<std::string, ITypeField> I_TYPE_FIELD;
    
    /*
        instruction decoder:
    */
    // 1. Decoders for R-type instructions:
    static const Decoder R_TYPE_Decoder_1;
    static const Decoder R_TYPE_Decoder_2;
    static const Decoder R_TYPE_Decoder_3;
    // 2. Decoders for I-type instructions:
    static const Decoder I_TYPE_Decoder_1;
    static const Decoder I_TYPE_Decoder_2;
    static const Decoder I_TYPE_Decoder_3;
    static const Decoder I_TYPE_Decoder_4;
    // 3. decoders:
    static const std::map<std::string, const Decoder&> INSTRUCTION_DECODER;

    std::vector<std::string> instructions;
    std::vector<ISA::MachineCode> machine_codes;

    const std::uint32_t TEXT_STARTING_ADDR;
    ISA::TextSegment text_segment;

    /**
        Normalize each instruction before parsing.

        @param instruction input instruction.
        @return true for valid instruction otherwise false.
    */
    bool normalize(std::string &instruction);

    /**
        Load instructions from input ASM.

        @param input_filename input ASM filename.
    */
    void load(const std::string &input_filename);

    /**
        Set opcode & funct for R-type instruction.

        @param operation instruction operation.
        @param machine_code output machine code reference.
    */
    void set_r_type_opcode(const std::string &operation, ISA::MachineCode &machine_code);
    /**
        Set opcode & funct for I-type instruction.

        @param operation instruction operation.
        @param machine_code output machine code reference.
    */
    void set_i_type_opcode(const std::string &operation, ISA::MachineCode &machine_code);
    /**
        Set opcode & funct for R-type instruction and opcode only for I-type & J-type instructions.

        @param operation instruction operation.
        @param machine_code output machine code reference.
    */
    void set_opcode(const std::string &operation, const Decoder& decoder, ISA::MachineCode &machine_code);
    /**
        Set other fields (rs, rt, rd, shamt & imm) for instruction.

        @param instruction instruction statement.
        @param decoder instruction decoder.
        @param machine_code output machine code reference.
    */
    void set_fields(const std::string &instruction, const Decoder& decoder, ISA::MachineCode &machine_code);

    /**
        Parse instructions into machine code.
    */
    void parse(void);

    /**
        Build instruction memory image.
    */
    void build(void);    
};