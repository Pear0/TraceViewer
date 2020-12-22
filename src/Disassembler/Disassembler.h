//
// Created by Will Gulian on 12/22/20.
//

#ifndef TRACEVIEWER2_DISASSEMBLER_H
#define TRACEVIEWER2_DISASSEMBLER_H

#include <memory>

#include "../DwarfInfo.h"

class Disassembler {
  struct Internal;
public:
  enum class Arch {
    Aarch64,
  };

  struct Instruction {
    uint64_t address;
    QString mnemonic;
    QString operands;
  };

  struct Segment {
    uint64_t startAddress;
    uint64_t nextAddress; // address AFTER this segment
    std::vector<Instruction> instructions;
  };
private:

  Arch arch;
  std::unique_ptr<Internal> internal;

public:
  explicit Disassembler(Arch arch);
  virtual ~Disassembler();

  Segment disassemble(const uint8_t *data, size_t dataLen, uint64_t address, size_t maxInstructions = 0);

  int foo();
};


#endif //TRACEVIEWER2_DISASSEMBLER_H
