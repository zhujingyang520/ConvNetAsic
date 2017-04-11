/*
 * Filename: extract_throughput.cpp
 * ---------------------------------
 * This file implements the throughput extraction from the RTL dumped
 * transmission time file.
 */

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
using namespace std;

// global varibale
string tx_time_file;

// CmdParser: Parse the inline option
bool CmdParser(int argc, char** argv);

int main (int argc, char** argv) {
  if (!CmdParser(argc, argv)) {
    return 1;
  }
  ifstream infile(tx_time_file.c_str());
  if (!infile) {
    cout << "specified tx file " << tx_time_file << " does not exist!" << endl;
    return 1;
  }
  double tx_time;
  int count = 0;
  double start_time, end_time;
  vector<double> time_stamp;

  while (infile >> tx_time) {
    if (count == 0) {
      start_time = tx_time;
    }
    end_time = tx_time;
    time_stamp.push_back(tx_time);
    count++;
  }
  cout << "Avg clock cycle: " << (end_time - start_time) / (count - 1) << endl;

  double t1 = time_stamp[time_stamp.size()-1-299*299];
  double t2 = time_stamp[time_stamp.size()-1];
  cout << "Avg clk 2: " << (t2 - t1) / (299*299-1) << endl;
  infile.close();
  return 0;
}

bool CmdParser(int argc, char** argv) {
  if (argc != 2) {
    cout << "Unexpected inline option!" << endl;
    cout << "[Usage]: ./extract_throughput.bin [path to tx_time]" << endl;
    return false;
  }
  tx_time_file = argv[1];
  return true;
}
