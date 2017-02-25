/*
 * Filename: io.cpp
 * -----------------
 * This file implements the function defined in io.h.
 */

#include "header/caffe/util/io.hpp"
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>

using google::protobuf::io::FileInputStream;
using namespace caffe;
using namespace std;

bool ReadProtoFromTextFile(const char* filename, Message* proto) {
  int fd = open(filename, O_RDONLY);
  if (fd == -1) {
    return false; // file does NOT exist
  }

  FileInputStream* input = new FileInputStream(fd);
  bool success = google::protobuf::TextFormat::Parse(input, proto);
  delete input;
  close(fd);

  return success;
}
