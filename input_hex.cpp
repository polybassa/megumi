#include <fstream>
#include <iostream>
#include <sstream>
#include <exception>
#include "input_hex.h"

class hex_parsing_error: public std::exception
{
 public:
  hex_parsing_error(unsigned int lineno, const std::string& msg):
      lineno_(lineno), msg_(msg)
  {
  }

  virtual const char* what() const throw()
  {
    if(what_.empty()) {
      std::ostringstream ss;
      ss << "line " << lineno_ << ": " << msg_;
      what_ = ss.str();
    }
    return what_.c_str();
  }

 private:
  unsigned int lineno_;
  const std::string msg_;
  mutable std::string what_;
};



static int parse_hex_digit(char c)
{
  if(c >= '0' && c <= '9') return c-'0';
  c |= ' ';
  if(c >= 'a' && c <= 'f') return (c-'a')+0xa;
  return -1;
}


std::vector<uint8_t> parse_hex_file(const std::string& filename)
{
  std::ifstream fin(filename);
  std::vector<uint8_t> ret;

  std::string line;
  unsigned int lineno = 0;
  uint32_t ex_addr_mask = 0;
  bool eof = false;
  while(std::getline(fin, line)) {
    if(eof) {
      throw hex_parsing_error(lineno, "end of file record not in the last line");
    }
    lineno++;
    // strip CR/LF
    size_t pos = line.find_first_of("\r\n");
    if(pos != std::string::npos) { // should always be true
      line.erase(pos);
    }

    if(line.size() < 11) {
      throw hex_parsing_error(lineno, "line is too short");
    }
    if(line[0] != ':') {
      throw hex_parsing_error(lineno, "invalid start code (':' expected)");
    }
    if((line.size()-1) % 2 != 0) {
      throw hex_parsing_error(lineno, "odd number of hex digits");
    }

    // parse line bytes
    std::vector<uint8_t> bytes((line.size()-1)/2);
    for(unsigned int i=1; i<line.size(); i+=2) {
      int d0 = parse_hex_digit(line[i]);
      int d1 = parse_hex_digit(line[i+1]);
      if(d0 == -1 || d1 == -1) {
        throw hex_parsing_error(lineno, "invalid hex digit");
      }
      bytes[(i-1)/2] = (d0 << 4) + d1;
    }

    // check checksum
    uint8_t checksum = 0;
    for(unsigned int i=0; i<bytes.size(); i++) {
      checksum = (checksum + bytes[i]) & 0xff;
    }
    if(checksum != 0) {
      throw hex_parsing_error(lineno, "checksum mismatch");
    }

    // check byte count
    unsigned int byte_count = bytes[0];
    if(byte_count != bytes.size()-5) {
      throw hex_parsing_error(lineno, "invalid byte count");
    }

    uint32_t address = (bytes[1] << 8) + bytes[2] + ex_addr_mask;
    switch(bytes[3]) {
      case 0:  // data
        if(address < ret.size()) {
	  std::cout << "Addr: " << address << std::endl;
	  std::cout << line << std::endl;
          throw hex_parsing_error(lineno, "address decreased");
        }
        ret.resize(address, 0xff);
        ret.insert(ret.end(), bytes.begin()+4, bytes.begin()+4+byte_count);
        break;

      case 1:  // end of file
        if(byte_count != 0)  {
          throw hex_parsing_error(lineno, "unexpected data in end of file record");
        }
        eof = true;
        break;

      case 2:  // extended segment address
        if(byte_count != 2)  {
          throw hex_parsing_error(lineno, "invalid byte count for extended segment address record");
        }
        if(address != 0) {
          throw hex_parsing_error(lineno, "address must be 0000 for extended segment address record");
        }
        if((bytes[5] & 0xf) != 0) {
          throw hex_parsing_error(lineno, "extended segment address least-signficant digit must be 0");
        }
        ex_addr_mask &= ~0x0ffff0;
        ex_addr_mask |= (((uint32_t)bytes[4] << 8) + bytes[5]) << 4;
        break;

      case 3:  // start segment address record
        // ignored
        break;

      case 4:  // extended linear address
        if(byte_count != 2)  {
          throw hex_parsing_error(lineno, "invalid byte count for extended linear address record");
        }
        if(address != 0) {
          throw hex_parsing_error(lineno, "address must be 0000 for extended linear address record");
        }
        if((bytes[5] & 0xf) != 0) {
          throw hex_parsing_error(lineno, "extended linear address least-signficant digit must be 0");
        }
        ex_addr_mask &= 0xffff;
        ex_addr_mask |= (((uint32_t)bytes[4] << 8) + bytes[5]) << 16;
        break;

      case 5:  // start linear address record
        // ignored
        break;

      default:
        throw hex_parsing_error(lineno, "invalid record type");
    }
  }

  return ret;
}

