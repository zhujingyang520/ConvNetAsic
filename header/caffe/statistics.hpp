/*
 * Filename: statistics.hpp
 * -------------------------
 * This file exports the structure related with the statistics. In includes:
 *
 * a) Operation: the breakdown of the number of operations
 */

#ifndef __STATISTICS_HPP__
#define __STATISTICS_HPP__

#include <iostream>

/*
 * Struct: Operation
 * ------------------
 * Includes the detailed breakdown of each operations.
 */
struct Operation {
  long int num_mac;      // MAC No.
  long int num_exp;      // Exp No.
  long int num_div;      // Div No.
  long int num_comp;     // Compare No.
  long int num_add;      // Add No.

  // handy operator overload
  struct Operation& operator+(const Operation& rhs) {
    num_mac += rhs.num_mac;
    num_exp += rhs.num_exp;
    num_div += rhs.num_div;
    num_comp += rhs.num_comp;
    num_add += rhs.num_add;
    return *this;
  }

  struct Operation& operator+=(const Operation& rhs) {
    num_mac += rhs.num_mac;
    num_exp += rhs.num_exp;
    num_div += rhs.num_div;
    num_comp += rhs.num_comp;
    num_add += rhs.num_add;
    return *this;
  }
};

inline std::ostream& operator<<(std::ostream& os, const Operation& op) {
  os << "num_mac=" << op.num_mac << ", " << "num_exp=" << op.num_exp << ", "
    << "num_div=" << op.num_div << ", " << "num_comp=" << op.num_comp << ", "
    << "num_add=" << op.num_add;
  return os;
}

#endif
