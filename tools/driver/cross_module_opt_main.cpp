#define DEBUG_TYPE "lto-cross-module-opt"

#include "swift/AST/DiagnosticsFrontend.h"
#include "swift/Basic/LLVMInitialize.h"
#include "swift/Frontend/Frontend.h"
#include "swift/Frontend/PrintingDiagnosticConsumer.h"
#include "swift/Option/Options.h"
#include "swift/SIL/SILModule.h"
#include "swift/SIL/TypeLowering.h"
#include "swift/Serialization/ModuleSummary.h"
#include "swift/Serialization/Validation.h"
#include "swift/Subsystems.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Bitstream/BitstreamReader.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Option/Option.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/TargetSelect.h"

using namespace llvm::opt;
using namespace swift;
using namespace modulesummary;

static llvm::cl::opt<std::string>
    LTOPrintLiveTrace("lto-print-live-trace", llvm::cl::init(""),
                      llvm::cl::desc("Print liveness trace for the symbol"));

static llvm::cl::list<std::string>
    InputFilenames(llvm::cl::Positional, llvm::cl::desc("[input files...]"));
static llvm::cl::opt<std::string>
    OutputFilename("o", llvm::cl::desc("output filename"));

static llvm::DenseSet<GUID> computePreservedGUIDs(ModuleSummaryIndex *summary) {
  llvm::DenseSet<GUID> Set(1);
  Set.insert(getGUIDFromUniqueName("main"));
  for (auto FI = summary->functions_begin(), FE = summary->functions_end();
       FI != FE; ++FI) {
    auto summary = FI->second.get();
    if (summary->isPreserved()) {
      Set.insert(FI->first);
    }
  }
  return Set;
}

class LivenessTrace {
public:
  enum ReasonTy { Preserved, StaticReferenced, IndirectReferenced };
  std::shared_ptr<LivenessTrace> markedBy;
  std::string symbol;
  GUID guid;
  ReasonTy reason;

  LivenessTrace(std::shared_ptr<LivenessTrace> markedBy, GUID guid,
                ReasonTy reason)
      : markedBy(markedBy), guid(guid), reason(reason) {}

  void setName(std::string name) { this->symbol = name; }

  void dump() { dump(llvm::errs()); }
  void dump(llvm::raw_ostream &os) {
    if (!symbol.empty()) {
      os << symbol;
    } else {
      os << "**missing name**"
         << " (" << guid << ")";
    }
    os << "is referenced by:\n";

    auto target = markedBy;
    while (target) {
      os << " - ";
      if (!target->symbol.empty()) {
        os << target->symbol;
      } else {
        os << "**missing name**";
      }
      os << " (" << target->guid << ")";
      os << "\n";
      target = target->markedBy;
    }
  }
};

VFuncSlot createVFuncSlot(FunctionSummary::Call call) {
  VFuncSlot::KindTy slotKind;
  switch (call.getKind()) {
    case FunctionSummary::Call::Witness: {
      slotKind = VFuncSlot::Witness;
      break;
    }
    case FunctionSummary::Call::VTable: {
      slotKind = VFuncSlot::VTable;
      break;
    }
    case FunctionSummary::Call::Direct: {
      llvm_unreachable("Can't get slot for static call");
    }
    case FunctionSummary::Call::kindCount: {
      llvm_unreachable("impossible");
    }
  }
  return VFuncSlot(slotKind, call.getCallee());
}

void markDeadSymbols(ModuleSummaryIndex &summary, llvm::DenseSet<GUID> &PreservedGUIDs) {

  SmallVector<std::shared_ptr<LivenessTrace>, 8> Worklist;
  std::set<GUID> UseMarkedTypes;
  unsigned LiveSymbols = 0;

  for (auto GUID : PreservedGUIDs) {
    Worklist.push_back(std::make_shared<LivenessTrace>(
        nullptr, GUID, LivenessTrace::Preserved));
  }
  std::shared_ptr<LivenessTrace> dumpTarget;
  while (!Worklist.empty()) {
    auto trace = Worklist.pop_back_val();

    auto maybeSummary = summary.getFunctionSummary(trace->guid);
    if (!maybeSummary) {
      llvm_unreachable("Bad GUID");
    }
    auto FS = maybeSummary;
    if (!FS->getName().empty()) {
      trace->setName(FS->getName());
      if (LTOPrintLiveTrace == FS->getName()) {
        dumpTarget = trace;
      }
    }
    if (FS->isLive()) continue;

    if (!FS->getName().empty()) {
      LLVM_DEBUG(llvm::dbgs() << "Mark " << FS->getName() << " as live\n");
    } else {
      LLVM_DEBUG(llvm::dbgs() << "Mark (" << FS->getGUID() << ") as live\n");
    }
    FS->setLive(true);
    LiveSymbols++;

    for (auto typeRef : FS->typeRefs()) {
      if (UseMarkedTypes.insert(typeRef.Guid).second) {
        summary.markUsedType(typeRef.Guid);
      }
    }
    for (auto Call : FS->calls()) {
      switch (Call.getKind()) {
      case FunctionSummary::Call::Direct: {
        Worklist.push_back(std::make_shared<LivenessTrace>(
            trace, Call.getCallee(), LivenessTrace::StaticReferenced));
        continue;
      }
      case FunctionSummary::Call::Witness:
      case FunctionSummary::Call::VTable: {
        VFuncSlot slot = createVFuncSlot(Call);
        auto Impls = summary.getImplementations(slot);
        for (auto Impl : Impls) {
          Worklist.push_back(std::make_shared<LivenessTrace>(
              trace, Impl, LivenessTrace::IndirectReferenced));
        }
        break;
      }
      case FunctionSummary::Call::kindCount:
        llvm_unreachable("impossible");
      }
    }
  }
  if (dumpTarget) {
    dumpTarget->dump();
  }
}

int cross_module_opt_main(ArrayRef<const char *> Args, const char *Argv0,
                          void *MainAddr) {
  INITIALIZE_LLVM();

  llvm::cl::ParseCommandLineOptions(Args.size(), Args.data(), "Swift LTO\n");

  CompilerInstance Instance;
  PrintingDiagnosticConsumer PDC;
  Instance.addDiagnosticConsumer(&PDC);

  if (InputFilenames.empty()) {
    Instance.getDiags().diagnose(SourceLoc(),
                                 diag::error_mode_requires_an_input_file);
    return 1;
  }

  auto TheSummary = std::make_unique<ModuleSummaryIndex>();

  for (auto Filename : InputFilenames) {
    LLVM_DEBUG(llvm::dbgs() << "Loading module summary " << Filename << "\n");
    auto ErrOrBuf = llvm::MemoryBuffer::getFile(Filename);
    if (!ErrOrBuf) {
      Instance.getDiags().diagnose(
          SourceLoc(), diag::error_no_such_file_or_directory, Filename);
      return 1;
    }

    auto HasErr = swift::modulesummary::loadModuleSummaryIndex(
        ErrOrBuf.get()->getMemBufferRef(), *TheSummary.get());

    if (HasErr)
      llvm::report_fatal_error("Invalid module summary");
  }

  TheSummary->setName("combined");
  
  auto PreservedGUIDs = computePreservedGUIDs(TheSummary.get());
  markDeadSymbols(*TheSummary.get(), PreservedGUIDs);

  modulesummary::writeModuleSummaryIndex(*TheSummary, Instance.getDiags(),
                                         OutputFilename);
  return 0;
}
