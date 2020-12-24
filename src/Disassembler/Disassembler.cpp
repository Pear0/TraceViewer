//
// Created by Will Gulian on 12/22/20.
//

#include <cstdio>
#include <cinttypes>
#include <capstone/capstone.h>

#include "Disassembler.h"

#define CODE "\xa8\x00\x38\xd5\x1f\x1d\x40\xf2"

struct Disassembler::Internal {
  csh cap_handle;
};

Disassembler::Disassembler(Disassembler::Arch arch)
        : arch(arch), internal(std::make_unique<Internal>()) {

  if (cs_err err = cs_open(CS_ARCH_ARM64, CS_MODE_ARM, &internal->cap_handle); err != CS_ERR_OK)
    throw std::runtime_error(cs_strerror(err));

  if (cs_err err = cs_option(internal->cap_handle, CS_OPT_DETAIL, CS_OPT_ON); err != CS_ERR_OK)
    throw std::runtime_error(cs_strerror(err));

}

int Disassembler::foo() {

  cs_insn *insn;
  size_t count;

  count = cs_disasm(internal->cap_handle, (const uint8_t *) CODE, sizeof(CODE) - 1, 0x1000, 0, &insn);
  if (count > 0) {
    size_t j;
    for (j = 0; j < count; j++) {
      printf("0x%" PRIx64 ":\t%s\t\t%s\n", insn[j].address, insn[j].mnemonic, insn[j].op_str);
    }
    cs_free(insn, count);
  } else
    printf("ERROR: Failed to disassemble given code!\\n");

  return 0;
}

Disassembler::~Disassembler() {
  cs_close(&internal->cap_handle);
}

Disassembler::Segment Disassembler::disassemble(const uint8_t *data, size_t dataLen, uint64_t address, size_t maxInstructions) {
  Segment segment;
  segment.startAddress = address;

  cs_insn *instructions;
  size_t count = cs_disasm(internal->cap_handle, data, dataLen, address, maxInstructions, &instructions);
  if (count == 0) {
    segment.nextAddress = address;
    return segment;
  }

  for (size_t j = 0; j < count; j++) {
    auto &insn = instructions[j];

    Instruction instr;
    instr.address = insn.address;
    instr.mnemonic = QString((char *) insn.mnemonic);
    instr.operands = QString((char *) insn.op_str);

    if (insn.detail) {

      for (size_t k = 0; k < insn.detail->arm64.op_count; k++) {
        auto &op = insn.detail->arm64.operands[k];

        if (op.type == ARM64_OP_IMM) {
          instr.jumpTarget = static_cast<uint64_t>(op.imm);
        }
      }

    } else {
      std::cout << "no insn detail\n";
    }

    segment.instructions.push_back(std::move(instr));
  }

  cs_free(instructions, count);

  return segment;
}

