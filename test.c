#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include <elf.h>
#include <stdint.h>

#include <dlfcn.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>
#include <arpa/inet.h>

#include <link.h>

// This is the C function the LD_PRELOAD library will overrite
#define HOOK getchar
#define HOOK_STR "getchar"

// Assembler code to unmap library memory
int _delete (int ptri, int size);

/******************************************
 * Helper functions
 *******************************************/

// Get Dynamic section of an ELF mapped in memory
// Returns a pointer to the section and the number of entries in size
Elf64_Dyn*
get_dynsection (void *prg, size_t *size) {
  Elf64_Ehdr *ehdr = (Elf64_Ehdr *)prg;
  
  /* Program header table */
  Elf64_Phdr *phdr = (Elf64_Phdr *)((uint8_t *)prg + ehdr->e_phoff);
  Elf64_Dyn  *dyn = NULL;
  size_t      dyn_cnt = 0;
  
  /* Locate PT_DYNAMIC segment */
  for (int i = 0; i < ehdr->e_phnum; i++) {
    if (phdr[i].p_type == PT_DYNAMIC) {
      dyn = (Elf64_Dyn *)((uint8_t *)prg + phdr[i].p_offset);
      dyn_cnt = phdr[i].p_filesz / sizeof(Elf64_Dyn);
      break;
    }
  }
  
  if (!dyn) {
    fprintf(stderr, "No PT_DYNAMIC segment found.\n");
    return NULL;
  }
  
  *size = dyn_cnt;
  
  return dyn;
}


// Gets the GOT pointer for a given symbol
// Returns an absolute pointer, base has to be the current memory base address
void *
get_got_ptr (void *base, Elf64_Dyn *dyn, int dyn_count, Elf64_Ehdr *ehdr, const char *name)
{
  /* Extract dynamic section pointers */
  Elf64_Sym  *symtab = NULL;
  char       *strtab = NULL;
  Elf64_Rela *jmprel = NULL;
  size_t     pltrelsz = 0;
  int        pltrel_type = 0;
  
  for (size_t i = 0; i < dyn_count; i++) {
    switch (dyn[i].d_tag) {
    case DT_SYMTAB:   symtab      = (Elf64_Sym *)(base + dyn[i].d_un.d_ptr); break;
    case DT_STRTAB:   strtab      = (char *)(base + dyn[i].d_un.d_ptr); break;
    case DT_JMPREL:   jmprel      = (Elf64_Rela *)(base + dyn[i].d_un.d_ptr); break;
    case DT_PLTRELSZ: pltrelsz    = dyn[i].d_un.d_val; break;
    case DT_PLTREL:   pltrel_type = dyn[i].d_un.d_val; break;
    }
  }

  if (!symtab || !strtab || !jmprel || pltrel_type != DT_RELA) 
    return NULL;
  
  size_t count = pltrelsz / sizeof(Elf64_Rela);
  
  /* Iterate PLT relocations */
  for (size_t i = 0; i < count; i++) {
    Elf64_Rela *r = &jmprel[i];
    
    unsigned sym_index = ELF64_R_SYM(r->r_info);
    const char *sym_name = &strtab[symtab[sym_index].st_name];
    
    if (strcmp(sym_name, name) == 0) {
      /* GOT entry location */
      void *got_entry_address = (uint8_t *)base + r->r_offset;
      return got_entry_address;
    }
  }
  
  return NULL;
}


// Get DT_PLTGOT from the provided dynamic section
void*
get_pltgot (Elf64_Dyn  *dyn, size_t dyn_cnt) {
  
  /* Search for DT_PLTGOT entry */
  for (size_t i = 0; i < dyn_cnt; i++) {
    if (dyn[i].d_tag == DT_PLTGOT) {
      /* DT_PLTGOT is usually an address in memory space, not an offset */
      void *pltgot = (void *)dyn[i].d_un.d_ptr;
      return pltgot;
    }
  }
  
  fprintf(stderr, "DT_PLTGOT not found.\n");
  return NULL;
}

int dump (long *p) {
  for (int i= 0; i <512;i++)
    printf ("%p [%4d] %p\n", p+i, i*8, p[i]);
}

int HOOK () {
  char          buffer[1024];
  char          *binary_name;
  unsigned char *prog;
  FILE          *f;
  long           ptr, pagesize;
  unsigned char  *code, *orig;
  int            v, (*func)();
  long           mem_start;
  char           *aux;
  int            (*original_hook)();

  
  // Get Process cmdline (process name)
  if ((f = fopen ("/proc/self/cmdline", "rt")) == NULL) {
    perror ("fopen:");
    return -1;
  }
  fgets (buffer, 1024, f);
  fclose (f);
  aux = buffer + strlen(buffer);
  while (*aux != '/' && aux > buffer) aux--;
  binary_name = strdup (*aux == '/' ? aux + 1 : aux);
  printf ("DEBUG: Process %ld '%s´\n", getpid(), binary_name);

  // Get Process base address
  if ((f = fopen ("/proc/self/maps", "rt")) == NULL) {
    perror ("fopen:");
    return -1;
  }
  while (!feof(f)) {
    fgets (buffer, 1024, f);
    //printf ("-- Testing %s\n", buffer);
    // TODO:
    // Store base address of the different modules found
    // and pass them to the _delete function
    // --------------------------
    // Memory map is sorted by address, the first address is the lower
    if (strstr (buffer, binary_name) == NULL) continue;
    else break; 
  };
  fclose (f);
  if ((aux = strchr (buffer, '-')) == NULL) {
    printf ("Malformed memory map entry\n");
    return -1;
  }
  *aux=0;
  sscanf (buffer, "%lx", &mem_start);
  printf ("DEBUG: Process mapped at: 0x%lx \n", mem_start);
  // Got the ELF Mapped segment
  prog = (unsigned char *)mem_start;

  // Now we have to patch the HOOK function to the one provided by the next
  // library in the search chain, otherwise we will be called again and again
  // Also as the memory will be unmapped when we are done, any further
  // call to HOOK will crash the main program.

  // Get next HOOK pointer in the shared library list
  // HOOK would usually be a libc function like getchar,...
  original_hook = dlsym(RTLD_NEXT, HOOK_STR);
  printf ("DEBUG: Original %s: %p\n", HOOK_STR, original_hook);
 
  // Get DT_PCLGOT (pointer to .got.plt)
  size_t     d_size = 0;
  Elf64_Dyn  *dyn = get_dynsection(prog, &d_size);
  long got1 = (long) get_pltgot (dyn, d_size);
  printf ("DEBUG: PLTGOT located at: %p + %p = %p\n",
	  mem_start, got1, mem_start+got1);
  
  // Get HOOK function's  PLT entry
  long *hook_got = get_got_ptr (prog, dyn, d_size, (Elf64_Ehdr *)prog, HOOK_STR);
  printf ("DEBUG: %s_got entry : %p\n", HOOK_STR, hook_got);

  // If RELO is activated we may have to change permissions. 
  //mprotect (mem_start + 0x4000, 0x1000, PROT_READ | PROT_WRITE | PROT_EXEC);
  *(long*)(hook_got) = (long)original_hook;

  // Now remove library from the search list
  // we should actually remove it from the l_scope field in the link_map
  // What we do below is what is causing the error when the program finish
  // However the change below actually prevents to use the library in resolving
  // new symbols.
  
  long *got = (long*)(mem_start + got1);

  // Navigate got[1] (link-map_) to find ourselves
  struct link_map *p = (struct link_map*) got[1];
  printf ("DEBUG: GOT: %p -> GOT[1] -> %lx\n", got, got[1]);

  while (p) {
    printf ("DEBUG: => '%s' %p", p->l_name, p->l_ld);
    if (strstr (p->l_name, "test")) {
      // When found, patch libname so the library won't be used for
      // symbol resolution
      printf ("  **PATCHED**");
      //dump (p);
      // TODO: We should actually patch l_scope in link_map
      *((long*)((unsigned char *)p + 40)) = 0;
    }
    printf ("\n");
    p = p->l_next;
  }
  
  // Copy over the _delete function so the program doesn't crash
  // when we unmap the memory
  
  // Calculate beginning of page library code page
  pagesize = getpagesize ();
  ptr = ((long) HOOK & ~(pagesize - 1));
  printf ("DEBUG: HOOK: %p  HOOK Page:%p (pagesize 0x%x)\n",
	  HOOK, ptr, pagesize);

  // TODO: GEt this from memory map. Usually code is one page away from PHDR
  unsigned char *pt = (unsigned char*)ptr - 0x1000;
  //printf ("%02X %02X %02X %02X\n", pt[0], pt[1], pt[2], pt[3]); // this shows ELF Magic
  
  // Allocate memory to copy memory unmapping code over
  code = mmap (NULL, pagesize, PROT_READ | PROT_WRITE | PROT_EXEC,
	       MAP_ANON | MAP_PRIVATE, 0, 0);
  orig  = (unsigned char*)ptr;
 
  printf ("DEBUG: Copying memory from %p to %p\n", orig, code);
  memcpy (code, orig, pagesize);
  
  // Calculate offset to the _delete function in the new allocated block
  func = (int (*)()) (code + ((unsigned char*) _delete - orig));
  printf ("DEBUG: delete: %p  new_delete: %p\n", _delete, f);
  
  printf ("DEBUG: About to Unmap %p, %ld\n", orig, pagesize);
  printf ("DEBUG: Original PRELOAD execution finish here\n");
  printf ("DEBUG:-----------------------------------------\n");
  getc (stdin);
  
  __asm ( ".intel_syntax noprefix\n"
	  "mov rdi, %0\n"   // RDI = ptr
	  "mov rsi, %1\n"   // RSI = val
	  "jmp %2\n"         // jump to *target
	  :
	  : "r"(orig), "r"(pagesize), "r"(func)
	  : "rdi", "rsi"
	  );
  
}
