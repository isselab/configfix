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

#include "satconf.h"

unsigned int sat_variable_nr = 1;
unsigned int tmp_variable_nr = 1;

GHashTable *satmap = NULL; /* hash table with all SAT-variables and their fexpr */
GArray *cnf_clauses; /* array with all CNF-clauses */
struct tmp_sat_variable *tmp_sat_vars;

unsigned int nr_of_clauses = 0; /* number of CNF-clauses */

struct fexpr *const_false; /* constant False */
struct fexpr *const_true; /* constant True */
struct fexpr *symbol_yes_fexpr; /* symbol_yes as fexpr */
struct fexpr *symbol_mod_fexpr; /* symbol_mod as fexpr */
struct fexpr *symbol_no_fexpr; /* symbol_no_as fexpr */

static bool init_done = false;

static bool sym_is_sdv(GArray *arr, struct symbol *sym);

/* -------------------------------------- */

int run_satconf_cli(const char *Kconfig_file)
{
	clock_t start, end;
	double time;
	
	if (!init_done) {
		printf("Init...");
		/* measure time for constructing constraints and clauses */
		start = clock();
		
		/* parse Kconfig-file and read .config */
		init_config(Kconfig_file);

		/* initialize satmap and cnf_clauses */
		init_data();

		/* creating constants */
		create_constants();

		/* assign SAT variables & create sat_map */
		assign_sat_variables();

		/* get the constraints */
		get_constraints();

		/* print all symbols and its constraints */
// 		print_all_symbols();

		end = clock();
		time = ((double) (end - start)) / CLOCKS_PER_SEC;
		
		printf("done. (%.6f secs.)\n", time);
		
// 		printf("Building CNF-clauses...");
// 		start = clock();
// 		
// 		/* construct the CNF clauses */
// 		construct_cnf_clauses();
// 		
// 		end = clock();
// 		time = ((double) (end - start)) / CLOCKS_PER_SEC;
// 		
// 		printf("done. (%.6f secs.)\n", time);
		
		/* write constraints to file */
// 		write_constraints_to_file();
		
		init_done = true;
	}
	
	
// 	return EXIT_SUCCESS;
	
	/* print all symbols and its constraints */
// 	print_all_symbols();

	/* print all CNFs */
// 	printf("All CNFs:\n");
// 	print_all_cnf_clauses( cnf_clauses );

//  	return EXIT_SUCCESS;
	
	/* print the satmap */
// 	g_hash_table_foreach(satmap, print_satmap, NULL);
	
	/* start PicoSAT */
	PicoSAT *pico = initialize_picosat();
	printf("Building CNF-clauses...");
	start = clock();
	
	/* construct the CNF clauses */
	construct_cnf_clauses(pico);
	
	end = clock();
	time = ((double) (end - start)) / CLOCKS_PER_SEC;
	
	printf("done. (%.6f secs.)\n", time);
// 	picosat_add_clauses(pico);
	
	/* add assumptions for all other symbols */
	printf("Adding assumptions...");
	start = clock();
	
	unsigned int i;
	struct symbol *sym;
	for_all_symbols(i, sym) {
		if (sym->type == S_UNKNOWN) continue;

		sym_add_assumption(pico, sym);
	}
	
	end = clock();
	time = ((double) (end - start)) / CLOCKS_PER_SEC;
	
	printf("done. (%.6f secs.)\n", time);

	
	picosat_solve(pico);
	
	printf("\n===> STATISTICS <===\n");
	printf("Constraints  : %d\n", count_counstraints());
	printf("CNF-clauses  : %d\n", picosat_added_original_clauses(pico));
	printf("SAT-variables: %d\n", picosat_variables(pico));
	printf("Temp vars    : %d\n", tmp_variable_nr - 1);
	printf("PicoSAT time : %.6f secs.\n", picosat_seconds(pico));

	/* 
	 * write CNF-clauses in DIMACS-format to file 
	 * NOTE: this output is without the unit-clauses
	 */
// 	write_cnf_to_file(cnf_clauses, sat_variable_nr, nr_of_clauses);

// 	FILE *fd = fopen("dimacs.out", "w");
// 	picosat_print(pico, fd);
// 	fclose(fd);
	
	return EXIT_SUCCESS;
}

/*
 * called from satdvconfig
 */
GArray * run_satconf(GArray *arr)
{
	clock_t start, end;
	double time;
	
	if (!init_done) {
		printf("\n");
		printf("Init...");
		
		/* measure time for constructing constraints and clauses */
		start = clock();

		/* initialize satmap and cnf_clauses */
		init_data();
		
		/* creating constants */
		create_constants();
		
		/* assign SAT variables & create sat_map */
		assign_sat_variables();
		
		/* get the constraints */
		get_constraints();

		
		end = clock();
		time = ((double) (end - start)) / CLOCKS_PER_SEC;

		printf("done. (%.6f secs.)\n", time);
		
		init_done = true;
	}
	
// 	return EXIT_SUCCESS;
	
	/* start PicoSAT */
	PicoSAT *pico = initialize_picosat();
	printf("Building CNF-clauses...");
	start = clock();
	
	/* construct the CNF clauses */
	construct_cnf_clauses(pico);
	
	end = clock();
	time = ((double) (end - start)) / CLOCKS_PER_SEC;
	
	printf("done. (%.6f secs.)\n", time);

	/* add unit clauses for each symbol */
	unsigned int i;
	struct symbol_dvalue *sdv;
	for (i = 0; i < arr->len; i++) {
		sdv = g_array_index(arr, struct symbol_dvalue *, i);
		
		if (sdv->sym->type == S_BOOLEAN) {
			switch (sdv->tri) {
			case yes:
				picosat_add_arg(pico, sdv->sym->fexpr_y->satval, 0);
				break;
			case no:
				picosat_add_arg(pico, -(sdv->sym->fexpr_y->satval), 0);
				break;
			case mod:
				perror("Should not happen.\n");
			}
		} else if (sdv->sym->type == S_TRISTATE) {
			switch (sdv->tri) {
			case yes:
				picosat_add_arg(pico, sdv->sym->fexpr_y->satval, 0);
				picosat_add_arg(pico, -(sdv->sym->fexpr_m->satval), 0);
				break;
			case mod:
				picosat_add_arg(pico, -(sdv->sym->fexpr_y->satval), 0);
				picosat_add_arg(pico, sdv->sym->fexpr_m->satval, 0);
				break;
			case no:
				picosat_add_arg(pico, -(sdv->sym->fexpr_y->satval), 0);
				picosat_add_arg(pico, -(sdv->sym->fexpr_m->satval), 0);
			}
		}
	}
	
	printf("CNF-clauses added: %d\n", picosat_added_original_clauses(pico));
	
	/* add assumptions for all other symbols */
	struct symbol *sym;
	for_all_symbols(i, sym) {
		if (sym->type == S_UNKNOWN) continue;
		
		if (!sym_is_sdv(arr, sym))
			sym_add_assumption(pico, sym);
	}
	
	/* print CNF-problem into file */
// 	FILE *fd = fopen("satdv.out", "w");
// 	picosat_print(pico, fd);
// 	fclose(fd);

	printf("Solving SAT-problem...");
	start = clock();
	
	int res = picosat_sat(pico, -1);
	
	end = clock();
	time = ((double) (end - start)) / CLOCKS_PER_SEC;
	printf("done. (%.6f secs.)\n\n", time);
	
	if (res == PICOSAT_SATISFIABLE) {
		printf("===> PROBLEM IS SATISFIABLE <===\n");
		
		GArray *ret = g_array_new(false, false, sizeof(GArray *));
// 		GArray *ret = rangefix_init(pico);
		free(pico);
		return ret;
		
	} else if (res == PICOSAT_UNSATISFIABLE) {
		printf("===> PROBLEM IS UNSATISFIABLE <===\n");
		printf("\n");
		
		GArray *ret = rangefix_init(pico);
		free(pico);
		return ret;
	}
	else {
		printf("Unknown if satisfiable.\n");
		free(pico);
		return NULL;
	}
	
	free(pico);
	return NULL;
}

/*
 * apply a fix
 */
int apply_satfix(GArray *fix)
{
	printf("\nApplying fixes...\n");
	print_diagnosis_symbol(fix);
	
	apply_fix(fix);
	
	return EXIT_SUCCESS;
}

static bool sym_is_sdv(GArray *arr, struct symbol *sym)
{
	unsigned int i;
	struct symbol_dvalue *sdv;
	for (i = 0; i < arr->len; i++) {
		sdv = g_array_index(arr, struct symbol_dvalue *, i);
		
		if (sym == sdv->sym)
			return true;
	}
	
	return false;
}
