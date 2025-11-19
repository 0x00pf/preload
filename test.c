#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dlfcn.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>
#include <arpa/inet.h>
#include <link.h>

int _delete (int ptri, int size);

//static int initialised = 0;
//extern long _GLOBAL_OFFSET_TABLE_;
#define HOOK getchar 

//__attribute__((constructor)) int startup () {
int HOOK() {
  long            ptr;
  unsigned char  *code, *orig;
  int            v, (*f)();
  long           size = 0x1000;  // PAGE size
  
  // Print PID for convenience
  printf ("DEBUG: PID %ld\n", getpid());

  // Calculate beginning of page... should use sysconf
  ptr = ((long)HOOK & 0xFFFFFFFFFFFFF000);
  
  printf ("DEBUG: HOOK: %p page%p\n", HOOK, ptr);

  // Stop before doing anything to check default memory map
  getc (stdin);
  
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

 
  // Get return address from stack so we can calculate the main program GOT
  long *got, base;
  __asm__ (".intel_syntax noprefix\n"
  	   "mov rax, [rsp +88]\n"
	   "mov %0, rax\n"
	   ".att_syntax\n"
	   : "=r"(got)::"rax");

  printf ("DEBUG: RETURN ADDress: %p\n", got);

  // Get base address... This may be retrieved from /proc/PID/maps
  base = ((long)got & 0xFFFFFFFFFFFFF000);

  // 0x3f8 is PLTGOT value from Dynamic section (check readelf)
  // The code is mapped one page above the base address
  got = base +   0x3fe8 - 0x1000;

  // Navigate the link_map to find ourselves
  struct link_map *p = (struct link_map*) got[1];
  printf ("DEBUG: GOT: %p -> GOT[1] (link_map) -> %p\n", got, got[1]);
  while (p) {
    if (strstr (p->l_name, "test")) {
      // When found, patch libname so the library won't be used for
      // symbol resolution
      *((long*)((unsigned char *)p + 40)) = 0;
    }
    p = p->l_next;
  }

  printf ("DEBUG: About to Unmap %p, %ld\n", orig, size);
  printf ("DEBUG: Original PRELOAD execution finish here\n");
  printf ("DEBUG:-----------------------------------------\n");
	printf ("DEBUG: Press any key to delete and return to main program\n");
  getc (stdin);
  
  __asm ( ".intel_syntax noprefix\n"
	  "mov rdi, %0\n"   // RDI = ptr
	  "mov rsi, %1\n"   // RSI = val
	  "jmp %2\n"         // jump to *target
	  :
	  : "r"(orig), "r"(size), "r"(f)
	  : "rdi", "rsi"
	  );
  
}


