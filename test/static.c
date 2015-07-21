/*!
 * \file  static-functions.c
 * \brief Tests instrumentation of static functions.
 *
 * Commands for llvm-lit:
 * RUN: %cpp -DINSTRUMENTATION_FILE %s > %t.yaml
 * RUN: %cpp %s > %t.c
 * RUN: %clang -S -emit-llvm %cflags %t.c -o %t.ll
 * RUN: %opt -S %t.ll -tesla-file %t.yaml -o %t.instr.ll
 * RUN: %filecheck -input-file %t.instr.ll %s
 */

#if defined (INSTRUMENTATION_FILE)

hook_prefix: __test_hook

functions:
    - name: foo
      callee: [ entry, exit ]
      caller: [ entry, exit ]

#else

static void foo();


int
main(int argc, char *argv[])
{
	// CHECK: define i{{[0-9]+}} @main

	// CHECK: call void @[[PREFIX:__test_hook]]_call_foo
	foo();
	// CHECK: call void @[[PREFIX]]_return_foo

	return 0;
}

static void
foo()
{
	/*
	 * CHECK: define internal void @foo()
	 * CHECK: call void @[[PREFIX]]_enter_foo
	 * CHECK: call void @[[PREFIX]]_leave_foo
	 */
}
#endif /* !INSTRUMENTATION_FILE */
