#include "asm.h"

	.globl start
start:
	addi.d $t0, $r0, -1
	syscall1_a NR_fileSize, file
exit:
	syscall0 NR_exit
file:
	.string "Test.lt"
buf_read:
	.fill 512,1,0
