#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>

int _delete (int ptri, int size);

static int initialised = 0;

#define HOOK fflush

//__attribute__((constructor)) int startup () {
int HOOK(FILE *p) {
  long            ptr;
  unsigned char  *code, *orig;
  int            v, (*f)();
  long           size = 0x1000;  // PAGE size
  
  // Print PID for convenience
  printf ("PRELOAD: PID %ld\n", getpid());

  // Calculate beginning of page... should use sysconf
  ptr = ((long)HOOK & 0xFFFFFFFFFFFFF000);
  
  printf ("DEBUG: HOOK: %p page%p\n", HOOK, ptr);

  // Allocate memory for unmaping code
  code = mmap (NULL, 0x1000, PROT_READ | PROT_WRITE | PROT_EXEC,
	       MAP_ANON | MAP_PRIVATE, 0, 0);
  orig  = (unsigned char*)ptr;
  
  printf ("DEBUG: Copying memory from %p to %p\n", orig, code);
  for (int i =0; i < size; i++) {
    code[i] = orig[i];
  }

  // Calculate offset to unmaping code
  f = code + ((unsigned char*)_delete-orig);
  
  printf ("DEBUG: delete: %p  new_delete: %p\n", _delete, f);

  // Pause execution... HOOK is hooked so use scanf
  getchar ();
  
  printf ("DEBUG: About to Unmap %p, %ld\n", orig, size);
  printf ("DEBUG: Original PRELOAD execution finish here\n");
  printf ("DEBUG:-----------------------------------------\n");
  __asm ( ".intel_syntax noprefix\n"
	  "mov rdi, %0\n"   // RDI = ptr
	  "mov rsi, %1\n"   // RSI = val
	  "jmp %2\n"         // jump to *target
	  :
	  : "r"(orig), "r"(size), "r"(f)
	  : "rdi", "rsi"
	  );
  
  //f(orig,0x1000);
}


