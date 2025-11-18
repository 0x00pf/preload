	.text
	.global _delete
_delete:
	mov $11, %rax
	syscall

#	mov $11, %rax
#	sub $0x1000, %rdi
#	syscall

	mov $11, %rax
	add $0x1000, %rdi
	syscall

	mov $11, %rax
	add $0x2000, %rdi
	syscall

#	mov $11, %rax
#	add $0x1000, %rdi
#	syscall

#	mov $1, %rax
#	mov $1, %rdi
#	lea    msg(%rip),%rsi
#	mov $15, %rdx
#	syscall

	# Stop execution
#	mov $0, %rax
#	mov $0, %rdi
#	lea    msg(%rip),%rsi
#	mov $15, %rdx
#	syscall

	leave
	ret
	
	
msg:
	.asciz "Hello, world!\n"
	
.section .note.GNU-stack,"",@progbits
