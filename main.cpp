/**
    ECE-697 Project 3
    main.cpp
    Purpose: Pipelined MIPS processor simulation

    @author Ge Yao
    @version 1.0 13/12/2018 
*/
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>

#include <boost/program_options.hpp>

#include "assembler.h"
#include "executor.h"

namespace po = boost::program_options;

/**
    Parse command-line arguments for MIPS simulator.

    @param argc the argc from main.
    @param argv the argv from main.
    @param input_asm the input MIPS ASM file.
    @param mode execution mode
    @param N execution count
    @return true for successful parsing otherwise false.
*/
bool parse_command_line_args(
    int argc, char** argv,
    std::string& input_asm, std::string& mode,int& N
) {
    try {
        // set parser:
        po::options_description desc("MIPS simulator usage");
        desc.add_options()
          ("help",    "produce help message")
          ("input",   po::value<std::string>(&input_asm)->required(), "set input ASM")
          ("mode",    po::value<std::string>(&mode)->required(),      "set execution mode")
          ("number",  po::value<int>(&N)->required(),                 "set execution number")
        ;

        // parse arguments:
        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);

        // a. help
        if (vm.count("help")) {
            std::cout << desc << "\n";
            return false;
        }
        po::notify(vm);
        
        // b. execution mode:
        if (vm.count("mode") && !("instruction" == mode || "cycle" == mode)) {
            throw std::runtime_error("invalid execution mode -- (either instruction or cycle ONLY)");    
        }
    }
    catch(std::runtime_error& e) {
        std::cerr << "[MIPS simulator]: ERROR -- " << e.what() << "\n";
        return false;
    }

    return true;
}

int main(int argc, char* argv[]) {
    // simulator configuration:
    std::string input_asm, mode;
    int N;   

    // parse configuration:
    if (parse_command_line_args(argc, argv, input_asm, mode, N)) {
        std::cout << "[MIPS simulator]: input ASM -- " << input_asm << ", mode -- " << mode << ", number -- " << N << std::endl; 

        // assemble:
        Assembler assembler(input_asm);
        // dump output for debugging:
        assembler.dump("../output/instruction-image.bin");

        // execute:
        ISA::TextSegment text_segment = assembler.get_text_segment();
        ISA::DataSegment data_segment(0x00000000);

        Executor executor(text_segment, data_segment);

        executor.run(mode, N);

        executor.dump("../output/resource-utilization.json");
    }
    
    return 0;
}