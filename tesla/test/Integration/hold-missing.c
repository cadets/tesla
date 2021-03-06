//! @file hold.c  Tests a missing 'hold' event.
/*
 * Commands for llvm-lit:
 * RUN: tesla analyse %s -o %t.tesla -- %cflags -D TESLA
 * RUN: clang -S -emit-llvm %cflags %s -o %t.ll
 * RUN: tesla instrument -S -tesla-manifest %t.tesla %t.ll -o %t.instr.ll
 * RUN: clang %ldflags %t.instr.ll -o %t
 * RUN: %t > %t.out 2>%t.err || true
 * RUN: %filecheck -input-file %t.out %s
 * RUN: %filecheck -check-prefix=ERR -input-file %t.err %s
 */

#include <errno.h>
#include <stddef.h>

#include <tesla-macros.h>


/*
 * A few declarations of things that look a bit like real code:
 */
struct object {
	int	refcount;
};

struct credential {};

int get_object(int index, struct object **o);
int example_syscall(struct credential *cred, int index, int op);


/*
 * Some functions we can reference in assertions:
 */
static void	hold(struct object *o) { o->refcount += 1; }


int
perform_operation(int op, struct object *o)
{
	TESLA_WITHIN(example_syscall, previously(call(hold(o))));
	TESLA_WITHIN(example_syscall, previously(returnfrom(hold(o))));

	return 0;
}


int
example_syscall(struct credential *cred, int index, int op)
{
	/*
	 * Entering the system call should update all automata:
	 *
	 * CHECK: [CALE] example_syscall
         * CHECK: sunrise
	 */

	struct object *o;

	/*
	 * Error: get_object() does *not* call hold(o)!
	 */
	int error = get_object(index, &o);
	if (error != 0)
		return (error);

	error = get_object(index + 1, &o);
	if (error != 0)
		return (error);

	/*
         * CHECK: [ASRT]
	 * CHECK: new    0: 1:0x0 ('{{.*}}')
         *
	 * ERR: TESLA failure
	 * ERR: No instance matched key '0x1 [ {{[0-9a-f]+}} X X X ]' for transition(s) [ ({{[0-9]+}}:0x1 -> {{[0-9]+}}:0x1) ]
	 */
	return perform_operation(op, o);
}


int
main(int argc, char *argv[]) {
	struct credential cred;
	return example_syscall(&cred, 0, 0);
}




int
get_object(int index, struct object **o)
{
	static struct object objects[] = {
		{ .refcount = 0 },
		{ .refcount = 0 },
		{ .refcount = 0 },
		{ .refcount = 0 }
	};
	static const size_t MAX = sizeof(objects) / sizeof(struct object);

	if ((index < 0) || (index >= MAX))
		return (EINVAL);

	struct object *obj = objects + index;

	// Error: failed to hold(obj)!
	//hold(obj);

	*o = obj;

	return (0);
}

