#include "isa.h"

void ISA::set_instruction_field(ISA::MachineCode &machine_code, ISA::Field field, ISA::Word value) {
    switch (field) {
        case OPCODE:
            machine_code |= (0x3F & value) << 26;
            break;
        case RS:
            machine_code |= (0x1F & value) << 21;
            break;
        case RT:
            machine_code |= (0x1F & value) << 16;
            break;
        case RD:
            machine_code |= (0x1F & value) << 11;
            break;
        case SHAMT:
            machine_code |= (0x1F & value) << 6;
            break;
        case FUNCT:
            machine_code |= (0x3F & value);
            break;
        case IMM:
            machine_code |= (0xFFFF & value);
            break;
        default:
            break;
    }
}

ISA::Word ISA::get_instruction_field(const ISA::MachineCode &machine_code, ISA::Field field) {
    Word value = 0x00000000;

    switch (field) {
        case OPCODE:
            value = ((0x3F << 26) & machine_code) >> 26;
            break;
        case RS:
            value = ((0x1F << 21) & machine_code) >> 21;
            break;
        case RT:
            value = ((0x1F << 16) & machine_code) >> 16;
            break;
        case RD:
            value = ((0x1F << 11) & machine_code) >> 11;
            break;
        case SHAMT:
            value = ((0x1F << 6) & machine_code) >> 6;
            break;
        case FUNCT:
            value = (0x3F & machine_code);
            break;
        case IMM:
            value = (0xFFFF & machine_code);
            break;
        default:
            break;
    }

    return value;
}