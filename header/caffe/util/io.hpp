/*
 * Filename: io.hpp
 * -----------------
 * This file exports the functions to read the Google Protocol Buffer. It is a
 * simplified version of the original caffe io.hpp. 
 */

#ifndef __IO_HPP__
#define __IO_HPP__

#include "proto/caffe.pb.h"
#include <google/protobuf/message.h>
#include <string>

using ::google::protobuf::Message;
using std::string;

/*
 * Function: ReadProtoFromTextFile
 * Usage: if (ReadProtoFromTextFile(filename, proto)) ...
 * -------------------------------------------------------
 * Read the protocol file from the specified filename, and pass the parameter to
 * the proto. Returns true if suceeds, false otherwise.
 */
bool ReadProtoFromTextFile(const char* filename, Message* proto);

/*
 * Function: ReadProtoFromTextFile
 * Usage: if (ReadProtoFromTextFile(filename, proto)) ...
 * -------------------------------------------------------
 * Read the protocol file from the specified filename, and pass the parameter to
 * the proto. Returns true if succeed, false otherwise. It is a wrap function
 * taking the filename of C++ string type.
 */
inline bool ReadProtoFromTextFile(const string& filename, Message* proto) {
  return ReadProtoFromTextFile(filename.c_str(), proto);
}

#endif
