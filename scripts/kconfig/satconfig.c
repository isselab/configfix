#define _GNU_SOURCE
#include <assert.h>
#include <locale.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "kconfig-sat/satconf.h"


/* -------------------------------------- */


int main(int argc, char *argv[])
{
	printf("\nHello satconfig!\n");
	
	run_satconf_cli(argv[1]);

// 	PicoSAT *pico = picosat_init();
// 	picosat_enable_trace_generation(pico);
// 	
// 	picosat_add_arg(pico, 1, -2, 0);
// 	picosat_add_arg(pico, 1, -3, 0);
// 	picosat_add_arg(pico, -2, 3, 0);
// 	picosat_add_arg(pico, 1, 0);
// 	picosat_add_arg(pico, 2, 0);
// 	picosat_add_arg(pico, -3, 0);
// 	
// 	int res = picosat_sat(pico, -1);
// 	
// 	if (res == PICOSAT_UNSATISFIABLE) {
// 		printf("Unsatisfiable.\n");
// 		int i;
// 		for (i = 0; i < picosat_added_original_clauses(pico); i++) {
// 			if (picosat_coreclause(pico, i) == 1)
// 				printf("%d\n", i);
// 		}
// 	} else {
// 		printf("Satisfiable.\n");
// 	}
	
	return EXIT_SUCCESS;
}

