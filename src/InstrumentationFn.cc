//! @file InstrumentationFn.cpp  Definition of @ref tesla::InstrumentationFn.
/*
 * Copyright (c) 2013,2015 Jonathan Anderson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme, as well as at
 * Memorial University under the NSERC Discovery program (RGPIN-2015-06048).
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

#include "InstrumentationFn.hh"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>

using namespace llvm;
using namespace tesla;

using std::string;
using std::unique_ptr;


static const char* PrintfFormat(Type *T) {
    if (T->isPointerTy()) return "%p";
    if (T->isIntegerTy()) return "%d";
    if (T->isFloatTy()) return "%f";
    if (T->isDoubleTy()) return "%f";

    assert(false && "Unhandled arg type");
    abort();
}

static BasicBlock *FindBlock(StringRef Name, Function& Fn) {
  for (auto& B : Fn)
    if (B.getName() == Name)
      return &B;

  return NULL;
}


unique_ptr<InstrumentationFn>
InstrumentationFn::Create(StringRef Name, ArrayRef<Type*> ParamTypes,
                          GlobalValue::LinkageTypes Linkage, Module& M) {

  LLVMContext& Ctx = M.getContext();

  FunctionType *T = FunctionType::get(Type::getVoidTy(Ctx), ParamTypes, false);
  auto *InstrFn = dyn_cast<Function>(M.getOrInsertFunction(Name, T));
  InstrFn->setLinkage(Linkage);

  //
  // Invariant: instrumentation functions should have two exit blocks, one for
  // normal termination and one for abnormal termination.
  //
  // The function starts out with the entry block jumping to the exit block.
  // Instrumentation is added in new BasicBlocks in between.
  //
  BasicBlock *EndBlock;

  if (InstrFn->empty()) {
    BasicBlock *Entry = BasicBlock::Create(Ctx, "entry", InstrFn);
    BasicBlock *Preamble = BasicBlock::Create(Ctx, "preamble", InstrFn);
    EndBlock = BasicBlock::Create(T->getContext(), "exit", InstrFn);

    IRBuilder<>(Entry).CreateBr(Preamble);
    IRBuilder<>(Preamble).CreateBr(EndBlock);
    IRBuilder<>(EndBlock).CreateRetVoid();

  } else
    EndBlock = FindBlock("exit", *InstrFn);

  return unique_ptr<InstrumentationFn> {
    new InstrumentationFn(InstrFn, EndBlock)
  };
}


void InstrumentationFn::CallBefore(Instruction *I, ArrayRef<Value*> Args) {
  CallInst::Create(InstrFn, Args)->insertBefore(I);
}

void InstrumentationFn::CallAfter(Instruction *I, ArrayRef<Value*> Args) {
  CallInst::Create(InstrFn, Args)->insertAfter(I);
}


#if 0
EventTranslator InstrumentationFn::AddInstrumentation(StringRef Label,
                                                  ArrayRef<Argument> Patterns,
                                                  bool IndirectionRequired,
                                                  uint32_t FreeMask) {
  LLVMContext& Ctx = InstrCtx.Ctx;

  //
  // An instrumentation function is a linear chain of event pattern
  // matchers and instrumentation blocks. Insert the new instrumentation
  // at the end of this chain.
  //
  BasicBlock *Instr = BasicBlock::Create(Ctx, Label + ":instr", InstrFn, End);
  End->replaceAllUsesWith(Instr);

  //
  // Match instrumented values against the given pattern, saving variables
  // to a struct tesla_key.
  //
  Function::ArgumentListType& InstrumentedValues = InstrFn->getArgumentList();
  assert(Patterns.size() == InstrumentedValues.size());

  vector<Value*> KeyArgs(TESLA_KEY_SIZE, NULL);
  IRBuilder<> Builder(Instr);

  size_t i = 0;
  for (Value& Val : InstrumentedValues) {
    const Argument& Pattern = Patterns[i];

    Match(Label + ":match:" + Twine(i), Instr, Pattern, &Val);

    if (Pattern.has_index()) {
      int Index = Pattern.index();

      assert(Index < TESLA_KEY_SIZE);
      assert(KeyArgs[Index] == NULL);

      if (IndirectionRequired)
        KeyArgs[Index] = GetArgumentValue(&Val, Pattern, Builder);

      else
        KeyArgs[Index] = &Val;
    }

    i++;
  }

  Value *Key = ConstructKey(Builder, InstrCtx.M, KeyArgs, FreeMask);

  TerminatorInst *Branch = Builder.CreateBr(End);
  Builder.SetInsertPoint(Branch);

  return EventTranslator(Builder, Key, InstrCtx);
}


BasicBlock* InstrumentationFn::Match(Twine Label, BasicBlock *InstrBlock,
                                 const Argument& Pattern, Value *Val) {

  if (Pattern.type() != Argument::Constant)
    return InstrBlock;

  BasicBlock *MatchBlock
    = BasicBlock::Create(InstrCtx.Ctx, Label, InstrFn, InstrBlock);

  InstrBlock->replaceAllUsesWith(MatchBlock);

  IRBuilder<> M(MatchBlock);
  Value *PatternValue = ConstantInt::getSigned(Val->getType(), Pattern.value());
  Value *Cmp;

  switch (Pattern.constantmatch()) {
  case Argument::Exact:
    Cmp = M.CreateICmpEQ(Val, PatternValue);
    break;

  case Argument::Flags:
    // test that x contains mask: (val & pattern) == pattern
    Cmp = M.CreateICmpEQ(M.CreateAnd(Val, PatternValue), PatternValue);
    break;

  case Argument::Mask:
    // test that x contains no more than mask: (val & pattern) == val
    Cmp = M.CreateICmpEQ(M.CreateAnd(Val, PatternValue), Val);
    break;
  }

  M.CreateCondBr(Cmp, InstrBlock, End);

  return MatchBlock;
}


BasicBlock* InstrumentationFn::CreatePreamble(Function *InstrFn, Twine Prefix) {

  Module& M = InstrFn->getModule();
  LLVMContext& Ctx = M.getContext();

  BasicBlock *Entry = BasicBlock::Create(Ctx, "entry", InstrFn);

  if (InstrCtx.SuppressDebugPrintf)
    return Entry;

  assert(InstrCtx.Debugging != NULL);
  assert(InstrCtx.Printf != NULL);

  BasicBlock *Preamble = BasicBlock::Create(Ctx, "preamble", InstrFn, Entry);
  IRBuilder<> Builder(Preamble);

#if 0
  //
  // Only print if TESLA_DEBUG indicates that we want output.
  //
  Value *DebugName = M.getGlobalVariable("debug_name", true);
  if (!DebugName)
    DebugName = Builder.CreateGlobalStringPtr("tesla.events", "debug_name");

  Value *Debugging = Builder.CreateCall(InstrCtx.Debugging, DebugName);

  Constant *Zero = ConstantInt::get(IntegerType::get(Ctx, 32), 0);
  Debugging = Builder.CreateICmpNE(Debugging, Zero);

  BasicBlock *PrintBB = BasicBlock::Create(Ctx, "printf", InstrFn, Entry);
  Builder.CreateCondBr(Debugging, PrintBB, Entry);

  string FormatStr(Prefix.str());
  for (Value& Arg : InstrFn->getArgumentList())
    FormatStr += Format(Arg.getType());
  FormatStr += "\n";

  ArgVector PrintfArgs(1, Builder.CreateGlobalStringPtr(FormatStr));
  for (Value& Arg : InstrFn->getArgumentList()) PrintfArgs.push_back(&Arg);

  IRBuilder<> PrintBuilder(PrintBB);
  PrintBuilder.CreateCall(InstrCtx.Printf, PrintfArgs);
  PrintBuilder.CreateBr(Entry);
#endif

  return Entry;
}
#endif
