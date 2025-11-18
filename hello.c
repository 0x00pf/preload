#include <stdio.h>
#include <unistd.h>

int main () {
	printf ("PID: %ld\n", getpid());
	printf ("Starting Main App]\n");
	fflush(NULL);
	printf ("Press any key to finish...\n");
	getchar();
	printf ("Finishing Main App\n");
	return 0;
}
