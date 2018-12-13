#include "assembler.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <algorithm> 
#include <regex>

#include "isa.h"

/*
    opcode + funct
*/
const std::map<std::string, Assembler::RTypeField> Assembler::R_TYPE_FIELD = {
    {  "add", {ISA::OpCode::R_COMMON, ISA::Funct::ADD}}, 
    {  "sub", {ISA::OpCode::R_COMMON, ISA::Funct::SUB}},   
    {  "and", {ISA::OpCode::R_COMMON, ISA::Funct::AND}}, 
    {   "or", {ISA::OpCode::R_COMMON, ISA::Funct::OR}}, 
    {  "mul", {ISA::OpCode::R_COMMON, ISA::Funct::MUL}},
    { "mult", {ISA::OpCode::R_COMMON, ISA::Funct::MULT}},
    {  "sll", {ISA::OpCode::R_COMMON, ISA::Funct::SLL}}, 
    {  "srl", {ISA::OpCode::R_COMMON, ISA::Funct::SRL}}
};
const std::map<std::string, Assembler::ITypeField> Assembler::I_TYPE_FIELD = {
    { "addi", {ISA::OpCode::ADDI}},
    { "andi", {ISA::OpCode::ANDI}}, 
    {  "ori", {ISA::OpCode::ORI}}, 
    { "slti", {ISA::OpCode::SLTI}}, 
    {"sltiu", {ISA::OpCode::SLTIU}},
    {  "beq", {ISA::OpCode::BEQ}},
    {  "lui", {ISA::OpCode::LUI}}, 
    {   "lw", {ISA::OpCode::LW}}, 
    {   "sw", {ISA::OpCode::SW}}
};

/*
    instruction decoder:

        R-TYPE: opcode[31-26] $rs[25-21] $rt[20-16] $rd[15-11] shamt[10-06] funct[05-00]
        I-TYPE: opcode[31-26] $rs[25-21] $rt[20-16] imm[15-00]
        J-Type: opcode[31-26] address[25-00]
*/
// 1. Decoders for R-type instructions:
const Assembler::Decoder Assembler::R_TYPE_Decoder_1 = {
    ISA::Type::R_TYPE, 
    "^(\\w+)\\s+\\$(\\w+)[\\s|,]+\\$(\\w+)[\\s|,]+\\$(\\w+)$",
    {
        {1, ISA::Field::OPCODE},
        {2, ISA::Field::RD},
        {3, ISA::Field::RS},
        {4, ISA::Field::RT}
    }
};
const Assembler::Decoder Assembler::R_TYPE_Decoder_2 = {
    ISA::Type::R_TYPE, 
    "^(\\w+)\\s+\\$(\\w+)[\\s|,]+\\$(\\w+)$",
    {
        {1, ISA::Field::OPCODE},
        {2, ISA::Field::RS},
        {3, ISA::Field::RT}
    }
};
const Assembler::Decoder Assembler::R_TYPE_Decoder_3 = {
    ISA::Type::R_TYPE, 
    "^(\\w+)\\s+\\$(\\w+)[\\s|,]+\\$(\\w+)[\\s|,]+(\\w+)$",
    {
        {1, ISA::Field::OPCODE},
        {2, ISA::Field::RD},
        {3, ISA::Field::RT},
        {4, ISA::Field::SHAMT}
    }
};
// 2. Decoders for I-type instructions:
const Assembler::Decoder Assembler::I_TYPE_Decoder_1 = {
    ISA::Type::I_TYPE, 
    "^(\\w+)\\s+\\$(\\w+)[\\s|,]+\\$(\\w+)[\\s|,]+(\\w+)$",
    {
        {1, ISA::Field::OPCODE},
        {2, ISA::Field::RT},
        {3, ISA::Field::RS},
        {4, ISA::Field::IMM}
    }
};
const Assembler::Decoder Assembler::I_TYPE_Decoder_2 = {
    ISA::Type::I_TYPE, 
    "^(\\w+)\\s+\\$(\\w+)[\\s|,]+\\$(\\w+)[\\s|,]+(\\w+)$",
    {
        {1, ISA::Field::OPCODE},
        {2, ISA::Field::RS},
        {3, ISA::Field::RT},
        {4, ISA::Field::IMM}
    }
};
const Assembler::Decoder Assembler::I_TYPE_Decoder_3 = {
    ISA::Type::I_TYPE, 
    "^(\\w+)\\s+\\$(\\w+)[\\s|,]+(\\w+)$",
    {
        {1, ISA::Field::OPCODE},
        {2, ISA::Field::RT},
        {3, ISA::Field::IMM}
    }
};
const Assembler::Decoder Assembler::I_TYPE_Decoder_4 = {
    ISA::Type::I_TYPE, 
    "^(\\w+)\\s+\\$(\\w+)[\\s|,]+(\\w+)[\\s|\\(]+\\$(\\w+)[\\s|\\)]*$",
    {
        {1, ISA::Field::OPCODE},
        {2, ISA::Field::RT},
        {3, ISA::Field::IMM},
        {4, ISA::Field::RS}
    }
};
// 3. decoders:
const std::map<std::string, const Assembler::Decoder&> Assembler::INSTRUCTION_DECODER = {
    {  "add", R_TYPE_Decoder_1}, 
    {  "sub", R_TYPE_Decoder_1},   
    {  "and", R_TYPE_Decoder_1}, 
    {   "or", R_TYPE_Decoder_1}, 
    {  "mul", R_TYPE_Decoder_1},
    { "mult", R_TYPE_Decoder_2},
    {  "sll", R_TYPE_Decoder_3}, 
    {  "srl", R_TYPE_Decoder_3}, 

    { "addi", I_TYPE_Decoder_1},
    { "andi", I_TYPE_Decoder_1}, 
    {  "ori", I_TYPE_Decoder_1}, 
    { "slti", I_TYPE_Decoder_1}, 
    {"sltiu", I_TYPE_Decoder_1},
    {  "beq", I_TYPE_Decoder_2},
    {  "lui", I_TYPE_Decoder_3}, 
    {   "lw", I_TYPE_Decoder_4}, 
    {   "sw", I_TYPE_Decoder_4}
};

Assembler::Assembler(const std::string &input_filename, std::uint32_t text_starting_addr): TEXT_STARTING_ADDR(text_starting_addr) {
    // load instructions:
    load(input_filename);

    // parse instructions into machine code:
    parse();

    // build instruction memory image:
    build();
}

/**
    Dump parsed machine code to output file.
    for online validation, please go to https://www.eg.bucknell.edu/%7Ecsci320/mips_web/

    @param output_filename output filename.    
*/
void Assembler::dump(const std::string &output_filename) {
    std::ofstream output_machine_code(output_filename);

	if(!output_machine_code) {
		std::cerr << "[MIPS simulator]: ERROR -- cannot open output machine code file "<< output_filename <<std::endl;
        return; 
	}

    for (
        ISA::Address address = text_segment.get_address_first();
        text_segment.get_address_last() >= address;
        address += 0x00000004
    ) {
        output_machine_code << "0x" << std::setfill('0') << std::setw(8) << std::hex << address;
        output_machine_code << ": ";
        output_machine_code << "0x" << std::setfill('0') << std::setw(8) << std::hex << text_segment.get_binary(address);
        output_machine_code << ";\t" << text_segment.get_text(address);
        output_machine_code << std::endl;
    }
    
	// close input file:
	output_machine_code.close(); 
}
/**
    Normalize each instruction before parsing.

    @param instruction input instruction.
    @return true for valid instruction otherwise false.
*/
bool Assembler::normalize(std::string &instruction) {
    // a. remove comments:
    std::size_t found = instruction.find("//");
    if (std::string::npos != found) {
        instruction.erase(found, std::string::npos);
    }
    if (0 == instruction.size()) {return false;}

    // b. remove left & right hand side whitespaces:
    // left trim:
    instruction.erase(
        instruction.begin(), std::find_if(
            instruction.begin(), instruction.end(), 
            [](int ch) {return !std::isspace(ch);}
        )
    );
    if (0 == instruction.size()) {return false;}

    // right trim:
    instruction.erase(
        std::find_if(
            instruction.rbegin(), instruction.rend(), 
            [](int ch) {return !std::isspace(ch);}
        ).base(), 
        instruction.end()
    );

    // c. to lowercase:
    std::transform(instruction.begin(), instruction.end(), instruction.begin(), ::tolower);

    return (0 != instruction.size());
}

/**
    Load instructions from input ASM.

    @param input_filename input ASM filename.
*/
void Assembler::load(const std::string &input_filename) {
	// open input file:
	std::ifstream input_asm(input_filename);
	if(!input_asm) {
		std::cerr << "[MIPS simulator]: ERROR -- cannot open input ASM file "<< input_filename <<std::endl;
        return; 
	}
 
	// read instruction line by line:
	std::string instruction;
	while (std::getline(input_asm, instruction)) {
        // if get a valid instruction:
        if (normalize(instruction)) {
            // save instruction:••••••••
            instructions.push_back(instruction);
        }  
	}

	// close input file:
	input_asm.close();   
}

/**
    Set opcode & funct for R-type instruction.

    @param operation instruction operation.
    @param machine_code output machine code reference.
*/
void Assembler::set_r_type_opcode(const std::string &operation, ISA::MachineCode &machine_code) {
    const auto &r_type_field = R_TYPE_FIELD.find(operation)->second;
    ISA::set_instruction_field(machine_code, ISA::Field::OPCODE, r_type_field.opcode);
    ISA::set_instruction_field(machine_code, ISA::Field::FUNCT, r_type_field.funct);
}

/**
    Set opcode & funct for I-type instruction.

    @param operation instruction operation.
    @param machine_code output machine code reference.
*/
void Assembler::set_i_type_opcode(const std::string &operation, ISA::MachineCode &machine_code) {
    const auto &i_type_field = I_TYPE_FIELD.find(operation)->second;
    ISA::set_instruction_field(machine_code, ISA::Field::OPCODE, i_type_field.opcode);
}

/**
    Set opcode & funct for R-type instruction and opcode only for I-type & J-type instructions.

    @param operation instruction operation.
    @param machine_code output machine code reference.
*/
void Assembler::set_opcode(
    const std::string &operation, 
    const Assembler::Decoder& decoder, 
    ISA::MachineCode &machine_code
) {
    switch (decoder.type) {
        case ISA::Type::R_TYPE:
            set_r_type_opcode(operation, machine_code);
            break;
        case ISA::Type::I_TYPE:
            set_i_type_opcode(operation, machine_code);
            break;
        case ISA::Type::J_TYPE:
            break;
        default:
            break;
    }
}

/**
    Set other fields (rs, rt, rd, shamt & imm) for instruction.

    @param instruction instruction statement.
    @param decoder instruction decoder.
    @param machine_code output machine code reference.
*/
void Assembler::set_fields(
    const std::string &instruction, 
    const Assembler::Decoder& decoder, 
    ISA::MachineCode &machine_code
) {
    std::regex pattern(decoder.regex);
    std::smatch matches;

    if(std::regex_search(instruction, matches, pattern)) {
        for (size_t i = 1; i < matches.size(); ++i) {
            auto group_name = decoder.group_name.find(i)->second;
            auto value = matches[i].str();
            switch (group_name) {
                case ISA::Field::RS:
                    ISA::set_instruction_field(machine_code, ISA::Field::RS, ISA::REGISTER_FILE.find(value)->second);
                    break;
                case ISA::Field::RT:
                    ISA::set_instruction_field(machine_code, ISA::Field::RT, ISA::REGISTER_FILE.find(value)->second);
                    break;
                case ISA::Field::RD:
                    ISA::set_instruction_field(machine_code, ISA::Field::RD, ISA::REGISTER_FILE.find(value)->second);
                    break;
                case ISA::Field::SHAMT:
                    ISA::set_instruction_field(machine_code, ISA::Field::SHAMT, std::stoi(value, nullptr, 16));
                    break;
                case ISA::Field::IMM:
                    ISA::set_instruction_field(machine_code, ISA::Field::IMM, std::stoi(value, nullptr, 16));
                    break;
                default:
                    break;
            }
        }
    } else {
        std::cout << instruction << std::endl;
        std::cout << "\t" << decoder.regex << std::endl;
        std::cout << "\t" << "MATCH FAILED" << std::endl;
        std::cout <<  std::endl;
    }
}

/**
    Parse instructions into machine code.
*/
void Assembler::parse(void) {
    for (const auto &instruction: instructions) {
        // parse operation:
        auto operation = instruction.substr(0, instruction.find(" "));
        auto result = INSTRUCTION_DECODER.find(operation);
        if (INSTRUCTION_DECODER.end() != result) {
            // get decoder handler:
            const Assembler::Decoder& decoder = result->second;

            // init machine code:
            ISA::MachineCode machine_code = 0x00000000;

            // set opcode:
            set_opcode(operation, decoder, machine_code);

            // set other fields:
            set_fields(instruction, decoder, machine_code);

            // save parsed machine code:
            machine_codes.push_back(machine_code);
        }
    }
}

/**
    Build instruction memory image.
*/
void Assembler::build(void) {
    for (std::size_t i = 0; i < machine_codes.size(); ++i) {
        // update image:
        text_segment.set(TEXT_STARTING_ADDR + (i << 2), {machine_codes[i], instructions[i]});
    }

    std::cout << "[MIPS simulator]: Assembler -- text segment [";
    std::cout << "0x" << std::setfill('0') << std::setw(8) << std::hex << text_segment.get_address_first();
    std::cout << ", ";
    std::cout << "0x" << std::setfill('0') << std::setw(8) << std::hex << text_segment.get_address_last();
    std::cout << "]" << std::endl;
}