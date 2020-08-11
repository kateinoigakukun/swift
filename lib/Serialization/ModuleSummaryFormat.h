#ifndef SWIFT_SERIALIZATION_MODULE_SUMMARY_FILE_H
#define SWIFT_SERIALIZATION_MODULE_SUMMARY_FILE_H

#include "llvm/Bitcode/RecordLayout.h"
#include "llvm/Support/MemoryBuffer.h"
#include <memory>

namespace swift {

namespace modulesummary {

using llvm::BCArray;
using llvm::BCBlob;
using llvm::BCFixed;
using llvm::BCGenericRecordLayout;
using llvm::BCRecordLayout;
using llvm::BCVBR;

const unsigned char MODULE_SUMMARY_SIGNATURE[] = {'M', 'O', 'D', 'S'};
const unsigned RECORD_BLOCK_ID = llvm::bitc::FIRST_APPLICATION_BLOCKID;

namespace record_block {
enum {
  MODULE_METADATA,
  FUNC_METADATA,
  CALL_GRAPH_EDGE,
  METHOD_METADATA,
  METHOD_IMPL,
};

using ModuleMetadataLayout = BCRecordLayout<MODULE_METADATA,
                                            BCBlob // Module name
                                            >;


using MethodMetadataLayout =
    BCRecordLayout<METHOD_METADATA,
                   BCFixed<1>, // KindTy (WitnessTable or VTable)
                   BCVBR<16>   // VirtualFunc GUID
                   >;

using MethodImplLayout = BCRecordLayout<METHOD_IMPL,
                                        BCVBR<16> // Impl func GUID
                                        >;

using FunctionMetadataLayout = BCRecordLayout<FUNC_METADATA,
                                              BCVBR<16>,  // Function GUID
                                              BCFixed<1>, // live
                                              BCFixed<1>, // preserved
                                              BCBlob      // Name string
                                              >;
using CallGraphEdgeLayout =
    BCRecordLayout<CALL_GRAPH_EDGE,
                   BCFixed<32>, // FunctionSummary::Edge::Kind
                   BCVBR<16>,   // Target Function GUID
                   BCBlob       // Name string
                   >;
} // namespace record_block
} // namespace modulesummary
} // namespace swift

#endif
