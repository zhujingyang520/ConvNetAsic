/*
 * Filename: data_type.hpp
 * ------------------------
 * This file exports all the data types required to be used in the ConvNet ASIC.
 */

#ifndef __DATA_TYPE_HPP__
#define __DATA_TYPE_HPP__

#include <iostream>
#include <systemc.h>
#include <string>

/*
 * Struct: Payload
 * ----------------
 * Payload data type, representing the format for each activation in the feature
 * map. It is necessary to export the overload functions for user-defined data
 * type for systemc utility.
 */
struct Payload {
  double data;    // presumably using double, can be changed

  // constructor
  Payload(double d=0.0) : data(d) {}
  ~Payload() {}

  // overload operator functions
  inline const Payload operator+(const Payload& other) const {
    Payload result = *this;
    result.data += other.data;
    return result;
  }

  inline const Payload operator-(const Payload& other) const {
    Payload result = *this;
    result.data -= other.data;
    return result;
  }

  inline const Payload operator*(const Payload& other) const {
    Payload result = *this;
    result.data *= other.data;
    return result;
  }

  inline const Payload operator/(const Payload& other) const {
    Payload result = *this;
    result.data /= other.data;
    return result;
  }

  inline Payload& operator=(const Payload& rhs) {
    data = rhs.data;
    return *this;
  }

  inline bool operator<(const Payload& rhs) const {
    return data < rhs.data;
  }

  inline bool operator>(const Payload& rhs) const {
    return data > rhs.data;
  }

  inline bool operator<=(const Payload& rhs) const {
    return data <= rhs.data;
  }

  inline bool operator>=(const Payload& rhs) const {
    return data >= rhs.data;
  }

  inline bool operator==(const Payload& rhs) const {
    return data == rhs.data;
  }

  inline bool operator!=(const Payload& rhs) const {
    return data != rhs.data;
  }

  inline friend std::ostream& operator<<(std::ostream& os,
      const Payload& payload) {
    os << "Payload: " << payload.data;
    return os;
  }

  // trace overload
  inline friend void sc_trace(sc_trace_file* tf, const Payload& payload,
      const std::string& name) {
    sc_trace(tf, payload.data, name+".data");
  }
};

#endif
