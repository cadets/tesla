/*
 * Copyright (c) 2012-2013 Jonathan Anderson
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

#ifndef	TESLA_CALLER_INSTRUMENTATION_H
#define	TESLA_CALLER_INSTRUMENTATION_H

#include "Instrumenter.h"
#include "Instrumentation.h"
#include "TranslationFn.h"

#include "tesla.pb.h"

#include <llvm/ADT/StringMap.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Pass.h>

namespace llvm {
  class CallSite;
  class Function;
  class Instruction;
  class LLVMContext;
  class Module;
}

namespace tesla {

class CallerInstrumentation;
class EventTranslator;
class InstrContext;
class Manifest;
class TranslationFn;


/// Instruments function calls in the caller context.
class FnCallerInstrumenter : public Instrumenter, public llvm::FunctionPass {
public:
  static char ID;
  FnCallerInstrumenter(const Manifest& M, bool SuppressDI)
    : Instrumenter(M, SuppressDI), FunctionPass(ID) {}
  ~FnCallerInstrumenter();

  const char* getPassName() const {
    return "TESLA function instrumenter (caller-side)";
  }

  virtual bool doInitialization(llvm::Module &M);
  virtual bool runOnFunction(llvm::Function &Fn);
  virtual bool runOnBasicBlock(llvm::BasicBlock &Block);

private:
  CallerInstrumentation* GetOrCreateInstr(llvm::Module&, llvm::Function*,
                                          FunctionEvent);

  llvm::OwningPtr<InstrContext> InstrCtx;

  llvm::StringMap<CallerInstrumentation*> Calls;
  llvm::StringMap<CallerInstrumentation*> Returns;
};



/// Function instrumentation (caller context).
class CallerInstrumentation
{
public:
  CallerInstrumentation(TranslationFn *TransFn, FunctionEvent Ev)
    : TransFn(TransFn), Ev(Ev)
  {
  }

  static CallerInstrumentation* Build(llvm::Module& M,
                                      llvm::StringRef Selector,
                                      llvm::FunctionType* Ty,
                                      FunctionEvent::Direction Dir,
                                      bool SuppressDebugInstr);

  EventTranslator AddInstrumentation(const FunctionEvent&, llvm::StringRef);

  bool Instrument(llvm::CallSite);

private:
  llvm::OwningPtr<TranslationFn> TransFn;
  FunctionEvent Ev;
};

}

#endif	/* !TESLA_CALLER_INSTRUMENTATION_H */
