/*! @file Debug.cpp  Debugging helpers. */
/*
 * Copyright (c) 2013 Jonathan Anderson
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "Debug.h"

#include <llvm/ADT/Twine.h>
#include <llvm/Support/raw_ostream.h>

#include <fnmatch.h>
#include <stdlib.h>
#include <unistd.h>

using namespace llvm;


#ifndef NDEBUG
static bool debugging(StringRef Name) {
#ifdef HAVE_ISSETUGID
  /*
   * Debugging paths could be more vulnerable to format string problems
   * than other code; don't allow when running setuid or setgid.
   */
  if (issetugid())
    return 0;
#endif

  // If TESLA_DEBUG isn't set, we don't do any debug printing.
  const char *rawEnv = getenv("TESLA_DEBUG");
  if (rawEnv == NULL)
    return false;

  // Also don't print anything if TESLA_DEBUG is set to the empty string.
  std::string env(rawEnv);
  if (env.empty())
    return false;

  // Let e.g. 'tesla.automata' match 'tesla.automata.anything'.
  if (Name.size() > env.length()
      and Name.startswith(env)
      and Name[env.length()] == '.')
    return true;

  // Use fnmatch()'s normal wildcard expansion.
  return (fnmatch(env.c_str(), Name.str().c_str(), 0) == 0);
}
#endif


void tesla::panic(Twine Message, bool PrintStackTrace) {
  report_fatal_error("TESLA: " + Message, PrintStackTrace);
}

raw_ostream& tesla::debugs(StringRef DebugModuleName) {
#ifndef NDEBUG
  if (debugging(DebugModuleName)) {
    static raw_ostream& ErrStream = llvm::errs();
    return ErrStream;
  }
#endif

  static raw_null_ostream NullStream;
  return NullStream;
}

#ifndef NDEBUG
#include <llvm/Support/Signals.h>

namespace {

class StaticDebugInit {
public:
  StaticDebugInit() {
    sys::PrintStackTraceOnErrorSignal();
  }

} DebugInit;

}
#endif
