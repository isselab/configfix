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
	
	return 0;

// 	init_data();
// 	create_constants();
// 
// 	PicoSAT *pico = picosat_init();
// 	picosat_enable_trace_generation(pico);
// 
// 	
// 	sat_add_clause(3, pico, 1, -2);
// 	sat_add_clause(3, pico, 1, -3);
// 	sat_add_clause(3, pico, -2, 3);
// 	
// 	sat_add_clause(2, pico, 1);
// 	sat_add_clause(2, pico, 2);
// 	sat_add_clause(2, pico, -3);
// 	
// 	picosat_assume(pico, 1);
// 	picosat_assume(pico, 2);
// 	picosat_assume(pico, -3);
// 	
// 	int res = picosat_sat(pico, -1);
// 	
// 	if (res == PICOSAT_UNSATISFIABLE) {
// 		printf("Unsatisfiable.\n");
// 		int i;
// 		for (i = 0; i < picosat_added_original_clauses(pico); i++) {
// 			if (picosat_coreclause(pico, i) == 1)
// 				printf("Failed clause: %d\n", i);
// 		}
// 		
// 		const int *j = picosat_failed_assumptions(pico);
// 		while (*j != 0) {
// 			printf("Failed lit: %d\n", *j++);
// 		}
// 	} else {
// 		printf("Satisfiable.\n");
// 	}
// 	
// 	return EXIT_SUCCESS;
}

