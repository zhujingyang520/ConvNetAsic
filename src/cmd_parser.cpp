/*
 * Filename: cmd_parser.cpp
 * -------------------------
 * This file implements the class CmdParser.
 */

#include "header/cmd_parser.hpp"
#include "header/caffe/util/io.hpp"
#include "proto/config.pb.h"
#include <string>
#include <iostream>
#include <getopt.h>

using namespace std;
using namespace config;

// Definition for short & long options
const char* const CmdParser::short_options = "hc:";
const struct option CmdParser::long_options[] = {
  {"help", 0, NULL, 'h'},
  {"config", 1, NULL, 'c'},
  {NULL, 0, NULL, 0}
};

/*
 * Implementation notes: Parse
 * ----------------------------
 * Parse the command option using API getopt_long.
 */
void CmdParser::Parse() {
  int next_option;
  program_name = argv_[0];

  do {
    next_option = getopt_long(argc_, argv_, short_options, long_options, NULL);
    switch (next_option) {
      case 'h':   // -h or --help
        PrintUsage();
        break;

      case 'c':   // -c or --config
        config_filename = optarg;
        break;

      case -1:    // end of option
        break;

      case '?':   // invalid option
        PrintUsage();
        break;

      default:    // unexpected
        exit(1);
    }
  } while (next_option != -1);

  if (config_filename == "") {
    cout << "please specify the configuration file" << endl;
    PrintUsage();
  }

  // Parse the configuration prototxt file
  if (!ReadProtoFromTextFile(config_filename, &config_param)) {
    cerr << "Error to parse the configuration file: " << config_filename
      << endl;
    exit(1);
  }

  // store the results from the configuration prototxt file
  model_filename = config_param.model_file();
}

void CmdParser::PrintUsage() const {
  cout << "Usage: " << program_name << " options " << endl;
  cout << "\t-h --help                   Display this usage information"
    << endl;
  cout << "\t-c --config config_filename Input the configuration filename"
    << endl;
  exit(0);
}
