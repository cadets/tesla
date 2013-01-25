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

#include "Manifest.h"
#include "tesla.pb.h"

#include "llvm/Function.h"
#include "llvm/Instructions.h"
#include "llvm/LLVMContext.h"
#include "llvm/Module.h"
#include "llvm/Pass.h"

#include "llvm/Support/raw_ostream.h"


using namespace llvm;
using namespace tesla;

using std::string;


int
main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage:  %s MANIFEST-FILE\n", argv[0]);
    return 1;
  }

  string ManifestName(argv[1]);

  auto& out = llvm::outs();
  auto& err = llvm::errs();

  OwningPtr<Manifest> Manifest(Manifest::load(llvm::errs(), ManifestName));
  if (!Manifest) {
    err << "Unable to read manifest '" << ManifestName << "'\n";
    return false;
  }

  for (auto& Fn : Manifest->FunctionsToInstrument()) {
    out << "Fn: " << Fn.ShortDebugString() << "\n";
    if (Fn.context() != FunctionEvent::Callee) continue;

    assert(Fn.has_function());
    auto Name = Fn.function().name();

    assert(Fn.has_direction());
    out << "Direction: " << Fn.direction() << "\n";
  }

  google::protobuf::ShutdownProtobufLibrary();
  return 0;
}

