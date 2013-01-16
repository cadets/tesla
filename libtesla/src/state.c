/*-
 * Copyright (c) 2011 Robert N. M. Watson
 * Copyright (c) 2011 Anil Madhavapeddy
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
 *
 * $Id$
 */

#include "tesla_internal.h"

#include <stdio.h>

#ifdef _KERNEL
MALLOC_DEFINE(M_TESLA, "tesla", "TESLA internal state");
#endif


int
tesla_class_init(struct tesla_class *tclass, u_int context, u_int instances)
{
	assert(tclass != NULL);
	// TODO: write a TESLA assertion about locking here.

	tclass->ts_limit = instances;

#ifdef _KERNEL
	tclass->ts_action = TESLA_ACTION_PRINTF;
#else
	tclass->ts_action = TESLA_ACTION_FAILSTOP;
#endif

	tclass->ts_scope = context;
	if (context == TESLA_SCOPE_GLOBAL)
		tesla_class_global_lock_init(tclass);

	tclass->ts_table = tesla_malloc(
		sizeof(struct tesla_table)
		+ instances * sizeof(struct tesla_instance)
	);
	tclass->ts_table->tt_length = instances;
	tclass->ts_table->tt_free = instances;

	return (TESLA_SUCCESS);
}

int
tesla_key_matches(struct tesla_key *pattern, struct tesla_key *k)
{
	assert(pattern != NULL);
	assert(k != NULL);

	// The pattern's mask must be a subset of the target's (ANY matches
	// 42 but not the other way around).
	if ((pattern->tk_mask & k->tk_mask) != pattern->tk_mask) return (0);

	for (size_t i = 0; i < TESLA_KEY_SIZE; i++) {
		// Only check keys specified by the bitmasks.
		register_t mask = (1 << i);
		if ((pattern->tk_mask & mask) != mask) continue;

		// A non-match of any sub-key implies a non-match of the key.
		if (pattern->tk_keys[i] != k->tk_keys[i]) return (0);
	}

	return (1);
}


int
tesla_instance_active(struct tesla_instance *i)
{
	assert(i != NULL);

	return ((i->ti_state != 0) || (i->ti_key.tk_mask != 0));
}


int
tesla_instance_new(struct tesla_class *tclass, struct tesla_key *name,
	register_t state, struct tesla_instance **out)
{
	assert(tclass != NULL);
	assert(name != NULL);
	assert(out != NULL);

	// A new instance must not look inactive.
	if ((state == 0) && (name->tk_mask == 0))
		return (TESLA_ERROR_EINVAL);

	if (tclass->ts_scope == TESLA_SCOPE_GLOBAL)
		tesla_class_global_lock(tclass);

	struct tesla_table *ttp = tclass->ts_table;
	assert(ttp != NULL);
	tesla_assert(ttp->tt_length != 0, "Uninitialized tesla_table");

	if (ttp->tt_free == 0)
		return (TESLA_ERROR_ENOMEM);

	for (size_t i = 0; i < ttp->tt_length; i++) {
		struct tesla_instance *inst = &ttp->tt_instances[i];
		if (tesla_instance_active(inst))
			continue;

		// Initialise the new instance.
		inst->ti_key = *name;
		inst->ti_state = state;

		ttp->tt_free--;
		*out = inst;
		break;
	}

	tesla_assert(*out != NULL, "no free instances but tt_free was > 0");

	if (tclass->ts_scope == TESLA_SCOPE_GLOBAL) {
		tesla_class_global_unlock(tclass);
	}

	return (TESLA_SUCCESS);
}

int
tesla_instance_find(struct tesla_class *tclass, struct tesla_key *pattern,
		   struct tesla_instance **out)
{
	assert(tclass != NULL);
	assert(pattern != NULL);
	assert(out != NULL);

	struct tesla_instance *instance;
	u_int i;

	if (tclass->ts_scope == TESLA_SCOPE_GLOBAL)
		tesla_class_global_lock(tclass);

	struct tesla_table *ttp = tclass->ts_table;
	assert(ttp != NULL);
	tesla_assert(ttp->tt_length != 0, "Uninitialized tesla_table");

	for (i = 0; i < ttp->tt_length; i++) {
		instance = &ttp->tt_instances[i];
		assert(instance != NULL);

		// If we found the droids we're looking for, go no further.
		if (tesla_key_matches(pattern, &instance->ti_key)) {
			*out = instance;
			break;
		}
	}

	if (tclass->ts_scope == TESLA_SCOPE_GLOBAL)
		tesla_class_global_unlock(tclass);

	if (*out == NULL)
		return (TESLA_ERROR_ENOENT);

	return (TESLA_SUCCESS);
}

void
tesla_instance_put(struct tesla_class *tsp, struct tesla_instance *tip)
{
#ifdef DEBUG
	assert_instanceof(tip, tsp);
#endif

	if (tsp->ts_scope == TESLA_SCOPE_GLOBAL)
		tesla_class_global_unlock(tsp);
	/* No action required for TESLA_SCOPE_PERTHREAD. */
}

void
tesla_assert_fail(struct tesla_class *tsp, struct tesla_instance *tip)
{

	if (tsp->ts_handler != NULL) {
		tsp->ts_handler(tip);
		return;
	}

	switch (tsp->ts_action) {
	case TESLA_ACTION_FAILSTOP:
		tesla_panic("tesla_assert_failed: %s: %s", tsp->ts_name,
		    tsp->ts_description);
		break;		/* A bit gratuitous. */
#ifdef NOTYET
	case TESLA_ACTION_DTRACE:
		dtrace_probe(...);
		return;
#endif
	case TESLA_ACTION_PRINTF:
#if defined(_KERNEL) && defined(KDB)
		kdb_backtrace();
#endif
		printf("tesla_assert_failed: %s: %s\n", tsp->ts_name,
		    tsp->ts_description);
		break;
	}
}

void
tesla_class_setaction(struct tesla_class *tsp,
    tesla_assert_fail_callback handler)
{

	tsp->ts_handler = handler;
}
