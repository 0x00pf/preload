#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

int main () {
	printf ("PID: %ld\n", getpid());
	printf ("Starting Main App]\n");
  long *got;
  __asm__ ("lea _GLOBAL_OFFSET_TABLE_(%%rip), %0" : "=r"(got));
  printf ("GOT: %p -> [0] -> %lx\n", got, got[1]);
	printf ("--------------------------------\n");
	// getchar () fires the PRELOAD library
	getchar();
	printf ("Press any key to finish...\n");
	// getc will fire the symbol resolution... we had removed test.so 
	// from the process otherwise the program will crash
	getc(stdin);
	printf ("Finishing Main App\n");
	return 0;
}
