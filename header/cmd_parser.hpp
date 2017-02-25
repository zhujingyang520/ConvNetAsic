/*
 * Filename: cmd_parser.hpp
 * -------------------------
 * This file exports the class `CmdParser`, which is used for parsing the inline
 * command of the program.
 */

#ifndef __CMD_PARSER_HPP__
#define __CMD_PARSER_HPP__

#include <string>
#include <getopt.h>
#include "proto/config.pb.h"

class CmdParser {
  public:
    // Constructor
    explicit CmdParser(int argc, char *argv[])
      : argc_(argc), argv_(argv) {}
    ~CmdParser() {}

    // main parse method
    void Parse();

  public:
    // public variables for main program to access
    std::string program_name;     // program name
    std::string config_filename;  // configuration filename

    std::string model_filename;   // neural net model filename
    config::ConfigParameter config_param; // parsed configuration parameter

  private:
    // command option configurations
    // short options
    static const char* const short_options;
    // long options
    static const struct option long_options[];

    // print usage
    void PrintUsage() const;

  private:
    // instance variables for command arguments
    int argc_;
    char** argv_;
};

#endif
