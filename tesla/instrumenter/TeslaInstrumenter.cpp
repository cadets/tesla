/*
 * Copyright (c) 2012 Jonathan Anderson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "Instrumentation.h"

#include "llvm/Function.h"
#include "llvm/Instructions.h"
#include "llvm/LLVMContext.h"
#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

#include <map>
#include <set>
#include <vector>

using namespace llvm;

using std::map;
using std::set;
using std::string;
using std::vector;

namespace tesla {

/// Instruments function calls in the callee context.
class TeslaCalleeInstrumenter : public FunctionPass {
public:
  static char ID;
  TeslaCalleeInstrumenter() : FunctionPass(ID) {}

  virtual bool doInitialization(Module &M) {
    // TODO: remove hardcoded function names
    vector<string> FnNames;
    FnNames.push_back("example_syscall");
    FnNames.push_back("some_helper");
    FnNames.push_back("void_helper");

    for (auto Name : FnNames)
      FunctionsToInstrument[Name] =
        CalleeInstrumentation::Build(getGlobalContext(), M, Name, FE_Both);

    return false;
  }

  virtual bool runOnFunction(Function &F) {
    auto I = FunctionsToInstrument.find(F.getName());
    if (I == FunctionsToInstrument.end()) return false;

    auto *FnInstrumenter = I->second;
    assert(FnInstrumenter != NULL);

    bool modifiedIR = false;
    modifiedIR |= FnInstrumenter->InstrumentEntry(F);
    modifiedIR |= FnInstrumenter->InstrumentReturn(F);

    return modifiedIR;
  }

private:
  map<string,CalleeInstrumentation*> FunctionsToInstrument;
};


/// Instruments function calls in the caller context.
class TeslaCallerInstrumenter : public BasicBlockPass {
public:
  static char ID;
  TeslaCallerInstrumenter() : BasicBlockPass(ID) {}

  virtual bool doInitialization(Module &M) {
    // TODO: remove hardcoded function names
    vector<string> FnNames;
    FnNames.push_back("example_syscall");
    FnNames.push_back("some_helper");
    FnNames.push_back("void_helper");

    for (auto Name : FnNames)
      FunctionsToInstrument[Name] =
        CallerInstrumentation::Build(getGlobalContext(), M, Name, FE_Both);

    return false;
  }

  virtual bool runOnBasicBlock(BasicBlock &Block) {
    bool modifiedIR = false;

    for (auto &Inst : Block) {
      if (!isa<CallInst>(Inst)) continue;
      CallInst &Call = cast<CallInst>(Inst);

      auto I = FunctionsToInstrument.find(Call.getCalledFunction()->getName());
      if (I == FunctionsToInstrument.end()) continue;

      auto *Instrumenter = I->second;
      assert(Instrumenter != NULL);

      modifiedIR |= Instrumenter->Instrument(Inst);
    }

    return modifiedIR;
  }

private:
  map<string,CallerInstrumentation*> FunctionsToInstrument;
};

/// Strips out calls to TESLA pseudo-functions.
class TeslaPseudoStrip : public ModulePass {
public:
  static char ID;
  TeslaPseudoStrip() : ModulePass(ID) {}

  virtual bool runOnModule(Module &M) {
    bool modifiedIR = false;

    set<CallInst*> Calls;
    set<Function*> Fns;

    for (auto &Fn : M.getFunctionList()) {
      StringRef Name = Fn.getName();

      if (Name.startswith("__tesla_")
          && !Name.startswith("__tesla_instrumentation_")) {
        for (auto I = Fn.use_begin(); I != Fn.use_end(); ++I) {
          assert(isa<CallInst>(*I));
          Calls.insert(cast<CallInst>(*I));
        }

        Fns.insert(&Fn);
      }
    }

    for (CallInst *Call : Calls) {
      Call->removeFromParent();
      delete Call;
      modifiedIR = true;
    }

    for (Function *Fn : Fns) {
      assert(Fn->use_empty());

      Fn->removeFromParent();
      delete Fn;

      modifiedIR = true;
    }

    return modifiedIR;
  }
};

}

char tesla::TeslaCalleeInstrumenter::ID = 0;
char tesla::TeslaCallerInstrumenter::ID = 0;
char tesla::TeslaPseudoStrip::ID = 0;

static RegisterPass<tesla::TeslaCalleeInstrumenter> Callee("tesla-callee",
  "TESLA instrumentation: callee context");

static RegisterPass<tesla::TeslaCallerInstrumenter> Caller("tesla-caller",
  "TESLA instrumentation: caller context");

static RegisterPass<tesla::TeslaPseudoStrip> Stripper("tesla-strip",
  "TESLA: strip pseudo-calls");

