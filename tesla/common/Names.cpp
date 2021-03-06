/*! @file Names.cpp  Implementation of TESLA name helpers. */
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

#include "Names.h"
#include "tesla.pb.h"

#include <llvm/ADT/Twine.h>

#include <string>
#include <sstream>

using llvm::Twine;


static std::string ConstantName(const tesla::Argument* A) {
  assert(A->type() == tesla::Argument::Constant);

  if (A->has_name())
      return A->name();

  else
    return Twine(A->value()).str();
}

std::string tesla::ShortName(const Argument* A, bool DecorateIndirection) {
  if (A == NULL)
    return "X";

  switch (A->type()) {
  case Argument::Constant:
    return ConstantName(A);

  case Argument::Variable:
    return A->name();

  case Argument::Any:
    return "X";

  case Argument::Indirect:
    assert(A->has_indirection());
    return (DecorateIndirection ? "&" : "") + ShortName(&A->indirection());

  case Argument::Field:
    assert(A->has_field());
    return ShortName(&A->field().base()) + "." + A->field().name();
  }
}

std::string tesla::DotName(const Argument* A, bool DecorateIndirection) {
  const static std::string Star = "&#8902;";

  if (A == NULL)
    return Star;

  switch (A->type()) {
  case Argument::Constant:
    return ConstantName(A);

  case Argument::Variable:
    return A->name();

  case Argument::Any:
    return Star;

  case Argument::Indirect:
    assert(A->has_indirection());
    return (DecorateIndirection ? "&": "") + DotName(&A->indirection());

  case Argument::Field:
    assert(A->has_field());
    return ShortName(&A->field().base()) + "." + A->field().name();
  }
}

std::string tesla::ShortName(const Identifier& ID) {
  if (ID.has_name())
    return ID.name();

  return ShortName(ID.location());
}

std::string tesla::ShortName(const Location& Loc) {
  return (Twine()
    + Loc.filename()
    + ":"
    + Twine(Loc.line())
    + "#"
    + Twine(Loc.counter())
  ).str();
}

std::string tesla::InstanceName(const ReferenceVector& Refs,
                                bool PlainAscii, bool DecorateIndirection) {

  if (Refs.size() == 0)
    return "";

  std::stringstream InstanceName;

  InstanceName << "(";

  for (auto i = Refs.begin(); i != Refs.end(); ) {
    InstanceName << (PlainAscii
                     ? ShortName(*i, DecorateIndirection)
                     : DotName(*i, DecorateIndirection));

    if (++i != Refs.end())
      InstanceName << ",";
  }

  InstanceName << ")";

  return InstanceName.str();
}
