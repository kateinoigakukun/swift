//===--- ObjectSectionReader.h - Read sections from objects ----*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2017 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
// Implements utilities to read sections from object files.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_REMOTE_OBJECTSECTIONREADER_H
#define SWIFT_REMOTE_OBJECTSECTIONREADER_H

#include "llvm/BinaryFormat/Wasm.h"
#include "llvm/Support/DataExtractor.h"
#include "llvm/Support/Error.h"
#include <cstdint>
#include <optional>
#include <vector>

namespace swift {
namespace reflection {

class WasmSectionReader {

public:
  /// Iterate over the body of `__wasm_init_memory` function and
  /// yield pairs of (segment index, offset) for each passive data segment
  /// that is initialized with (memory.init) instruction in the function body.
  class InitMemoryInterpreter {
    llvm::DataExtractor Body;
    llvm::DataExtractor::Cursor Cursor;

    /// The stack of Wasm core values.
    std::vector<int64_t> ValueStack;

  public:
    InitMemoryInterpreter(const llvm::DataExtractor &Body)
        : Body(Body), Cursor(0) {}

    using SegmentInitInfo = std::pair<uint64_t, uint64_t>;

    llvm::Expected<std::optional<SegmentInitInfo>> next() {
      if (Cursor.tell() >= Body.size())
        return std::nullopt;
      while (Cursor.tell() < Body.size()) {
        llvm::Expected<std::optional<SegmentInitInfo>> Result = interpretInstruction();
        if (!Result) {
          return Result.takeError();
        }
        if (Result.get()) {
          return Result.get();
        }
        // Continue to the next instruction
      }
      return std::nullopt;
    }
  private:

    llvm::Expected<std::optional<SegmentInitInfo>> interpretInstruction() {
      uint8_t Opcode = Body.getU8(Cursor);
      if (!Cursor) {
        return Cursor.takeError();
      }
      switch (Opcode) {
      case llvm::wasm::WASM_OPCODE_BLOCK: {
        uint8_t BlockType = Body.getU8(Cursor);
        if (!Cursor) {
          return Cursor.takeError();
        }
        // Accept only empty block type (0x40)
        if (BlockType != 0x40) {
          return llvm::createStringError(llvm::inconvertibleErrorCode(),
                                         "Unexpected non-empty block type");
        }
        break;
      }
      case llvm::wasm::WASM_OPCODE_I32_CONST:
      case llvm::wasm::WASM_OPCODE_I64_CONST: {
        int64_t Value = Body.getSLEB128(Cursor);
        if (!Cursor) {
          return Cursor.takeError();
        }
        ValueStack.push_back(Value);
        break;
      }
      case llvm::wasm::WASM_OPCODE_DROP: {
        ValueStack.pop_back();
        break;
      }
      case llvm::wasm::WASM_OPCODE_ATOMICS_PREFIX: {
        uint8_t Opcode = Body.getU8(Cursor);
        if (!Cursor) {
          return Cursor.takeError();
        }
        switch (Opcode) {
        case llvm::wasm::WASM_OPCODE_I32_RMW_CMPXCHG: {
          auto MemArg = readMemArg(Body, Cursor);
          if (!MemArg) {
            return MemArg.takeError();
          }
          ValueStack.pop_back();   // new value
          ValueStack.pop_back();   // expected value
          ValueStack.pop_back();   // address
          ValueStack.push_back(0); // success
          break;
        }
        case llvm::wasm::WASM_OPCODE_I32_ATOMIC_STORE: {
          auto MemArg = readMemArg(Body, Cursor);
          if (!MemArg) {
            return MemArg.takeError();
          }
          ValueStack.pop_back(); // value
          ValueStack.pop_back(); // address
          break;
        }
        case llvm::wasm::WASM_OPCODE_ATOMIC_NOTIFY: {
          auto MemArg = readMemArg(Body, Cursor);
          if (!MemArg) {
            return MemArg.takeError();
          }
          ValueStack.pop_back();   // count
          ValueStack.pop_back();   // address
          ValueStack.push_back(0); // the number of awakened threads
          break;
        }
        case llvm::wasm::WASM_OPCODE_I32_ATOMIC_WAIT: {
          auto MemArg = readMemArg(Body, Cursor);
          if (!MemArg) {
            return MemArg.takeError();
          }
          ValueStack.pop_back();   // timeout
          ValueStack.pop_back();   // expected value
          ValueStack.pop_back();   // address
          ValueStack.push_back(0); // not-equal
          break;
        }
        default: {
          return llvm::createStringError(llvm::inconvertibleErrorCode(),
                                         "Unexpected opcode");
        }
        }
        break;
      }
      case llvm::wasm::WASM_OPCODE_BR: {
        uint64_t LabelIndex = Body.getULEB128(Cursor);
        if (!Cursor) {
          return Cursor.takeError();
        }
        (void)LabelIndex;
        break;
      }
      case llvm::wasm::WASM_OPCODE_BR_TABLE: {
        uint64_t LabelCount = Body.getULEB128(Cursor);
        if (!Cursor) {
          return Cursor.takeError();
        }
        // The last label index is the default label.
        for (uint64_t i = 0; i < LabelCount + 1; ++i) {
          uint64_t LabelIndex = Body.getULEB128(Cursor);
          if (!Cursor) {
            return Cursor.takeError();
          }
          (void)LabelIndex;
        }
        ValueStack.pop_back(); // destination value
        break;
      }
      case llvm::wasm::WASM_OPCODE_END: {
        break;
      }
      case llvm::wasm::WASM_OPCODE_GLOBAL_SET: {
        uint64_t GlobalIndex = Body.getULEB128(Cursor);
        if (!Cursor) {
          return Cursor.takeError();
        }
        ValueStack.pop_back(); // value
        (void)GlobalIndex;
        break;
      }
      case llvm::wasm::WASM_OPCODE_MISC_PREFIX: {
        uint8_t Opcode = Body.getU8(Cursor);
        if (!Cursor) {
          return Cursor.takeError();
        }
        switch (Opcode) {
        case llvm::wasm::WASM_OPCODE_DATA_DROP: {
          uint64_t SegmentIndex = Body.getULEB128(Cursor);
          if (!Cursor) {
            return Cursor.takeError();
          }
          (void)SegmentIndex;
          break;
        }
        case llvm::wasm::WASM_OPCODE_MEMORY_INIT: {
          uint64_t SegmentIndex = Body.getULEB128(Cursor);
          if (!Cursor) {
            return Cursor.takeError();
          }
          uint64_t MemoryIndex = Body.getULEB128(Cursor);
          if (!Cursor) {
            return Cursor.takeError();
          }
          ValueStack.pop_back(); // size
          uint64_t Offset = ValueStack.back();
          ValueStack.pop_back();
          uint64_t DestAddress = ValueStack.back();
          ValueStack.pop_back();

          if (Offset != 0) {
            return llvm::createStringError(llvm::inconvertibleErrorCode(),
                                           "Unexpected offset");
          }
          (void)MemoryIndex;
          // Return the segment index and the destination address.
          return std::make_pair(SegmentIndex, DestAddress);
        }
        case llvm::wasm::WASM_OPCODE_MEMORY_FILL: {
          uint64_t MemoryIndex = Body.getULEB128(Cursor);
          if (!Cursor) {
            return Cursor.takeError();
          }
          (void)MemoryIndex;
          ValueStack.pop_back(); // size
          ValueStack.pop_back(); // value
          ValueStack.pop_back(); // address
          break;
        }
        default: {
          return llvm::createStringError(llvm::inconvertibleErrorCode(),
                                         "Unexpected opcode");
        }
        }
        break;
      }
      default: {
        return llvm::createStringError(llvm::inconvertibleErrorCode(),
                                       "Unexpected opcode");
      }
      }
      return std::nullopt;
    }

    static llvm::Expected<std::pair<uint64_t, uint64_t>> readMemArg(
        const llvm::DataExtractor &DE, llvm::DataExtractor::Cursor &Cursor) {
      uint64_t Alignment = DE.getULEB128(Cursor);
      if (!Cursor) {
        return Cursor.takeError();
      }
      uint64_t Offset = DE.getULEB128(Cursor);
      if (!Cursor) {
        return Cursor.takeError();
      }
      return std::make_pair(Alignment, Offset);
    }
  };
};

} // namespace reflection
} // namespace swift

#endif
