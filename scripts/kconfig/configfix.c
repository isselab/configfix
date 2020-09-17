/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Patrick Franz <patfra71@gmail.com>
 */

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

#include "configfix.h"

unsigned int sat_variable_nr = 1;
unsigned int tmp_variable_nr = 1;

GHashTable *satmap = NULL; /* hash table with all SAT-variables and their fexpr */
GHashTable *cnf_clauses_map; /* hash-table with all CNF-clauses */
GArray *sdv_arr; /* array with conflict-symbols */

struct fexpr *const_false; /* constant False */
struct fexpr *const_true; /* constant True */
struct fexpr *symbol_yes_fexpr; /* symbol_yes as fexpr */
struct fexpr *symbol_mod_fexpr; /* symbol_mod as fexpr */
struct fexpr *symbol_no_fexpr; /* symbol_no_as fexpr */

static PicoSAT *pico;
static bool init_done = false;
static GArray *conflict_syms = NULL;

static bool sdv_within_range(GArray *arr);

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

	return EXIT_SUCCESS;
}

/*
 * called from satdvconfig
 */
GArray * run_satconf(GArray *arr)
{
	clock_t start, end;
	double time;
	
	/* check whether all values can be applied -> no need to run */
	if (sdv_within_range(arr)) {
		printf("\nAll symbols are already within range.\n\n");
		return g_array_new(false, false, sizeof(GArray *));
	}
	
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
		
		/* start PicoSAT */
		pico = initialize_picosat();
		printf("Building CNF-clauses...");
		start = clock();
		
		/* construct the CNF clauses */
		construct_cnf_clauses(pico);
		
		end = clock();
		time = ((double) (end - start)) / CLOCKS_PER_SEC;
		
		printf("done. (%.6f secs.)\n", time);
		
		printf("CNF-clauses added: %d\n", picosat_added_original_clauses(pico));
		
		init_done = true;
	}
	
// 	return EXIT_SUCCESS;

	/* copy array with symbols to change */
// 	sdv_arr = g_array_copy(arr);
	unsigned int i;
	struct symbol_dvalue *sdv;
	sdv_arr = g_array_new(false, false, sizeof(struct symbol_dvalue *));
	for (i = 0; i < arr->len; i++) {
		sdv = g_array_index(arr, struct symbol_dvalue *, i);
		g_array_append_val(sdv_arr, sdv);
	}
	
	/* add assumptions for conflict-symbols */
	sym_add_assumption_sdv(pico, sdv_arr);
	
	/* add assumptions for all other symbols */
	struct symbol *sym;
	for_all_symbols(i, sym) {
		if (sym->type == S_UNKNOWN) continue;
		
		if (!sym_is_sdv(sdv_arr, sym))
			sym_add_assumption(pico, sym);
	}
	
	/* store the conflict symbols */
	if (conflict_syms != NULL) g_array_free(conflict_syms, false);
	conflict_syms = g_array_new(false, false, sizeof(struct symbol *));
	for (i = 0; i < sdv_arr->len; i++) {
		sdv = g_array_index(sdv_arr, struct symbol_dvalue *, i);
		g_array_append_val(conflict_syms, sdv->sym);
	}

	printf("Solving SAT-problem...");
	start = clock();
	
	int res = picosat_sat(pico, -1);
	
	end = clock();
	time = ((double) (end - start)) / CLOCKS_PER_SEC;
	printf("done. (%.6f secs.)\n\n", time);
	
	GArray *ret;
	
	if (res == PICOSAT_SATISFIABLE) {
		printf("===> PROBLEM IS SATISFIABLE <===\n");
		
		ret = g_array_new(false, false, sizeof(GArray *));
		
	} else if (res == PICOSAT_UNSATISFIABLE) {
		printf("===> PROBLEM IS UNSATISFIABLE <===\n");
		printf("\n");
		
		ret = rangefix_run(pico);
	}
	else {
		printf("Unknown if satisfiable.\n");
		
		ret = NULL;
	}
	
// 	picosat_reset(pico);
// 	g_hash_table_remove_all(cnf_clauses_map);
	g_array_free(sdv_arr, true);
	return ret;
}

/*
 * check whether a symbol is a conflict symbol
 */
static bool sym_is_conflict_sym(struct symbol *sym)
{
	unsigned int i;
	struct symbol *sym2;
	for (i = 0; i < conflict_syms->len; i++) {
		sym2 = g_array_index(conflict_syms, struct symbol *, i);
		
		if (sym == sym2) return true;
	}
	
	return false;
}

/*
 * check whether all conflict symbols are set to their target values
 */
static bool syms_have_target_value(GArray *arr)
{
	unsigned int i;
	struct symbol_fix *fix;
	for (i = 0; i < arr->len; i++) {
		fix = g_array_index(arr, struct symbol_fix *, i);
		
		if (!sym_is_conflict_sym(fix->sym)) continue;
		
		if (sym_is_boolean(fix->sym)) {
			if (fix->tri != sym_get_tristate_value(fix->sym))
				return false;
		}
		else {
			if (strcmp(str_get(&fix->nb_val),sym_get_string_value(fix->sym)) != 0)
				return false;
		}
	}
	
	return true;
}
 
/*
 * 
 * apply the fixes from a diagnosis
 */
int apply_fix(GArray* diag)
{
	struct symbol_fix *fix;
	unsigned int i, no_symbols_set = 0, iterations = 0, manually_changed = 0;
// 	GArray *tmp = g_array_copy(diag);
	GArray *tmp = g_array_new(false, false, sizeof(struct symbol_fix *));
	for (i = 0; i < diag->len; i++) {
		fix = g_array_index(diag, struct symbol_fix *, i);
		g_array_append_val(tmp, fix);
	}

	printf("Trying to apply fixes now...\n");
	
	while (no_symbols_set < diag->len && !syms_have_target_value(diag)) {
// 	while (!syms_have_target_value(diag)) {
		if (iterations > diag->len * 2) {
			printf("\nCould not apply all values :-(.\n");
			return manually_changed;
		}
		
		for (i = 0; i < tmp->len; i++) {
			fix = g_array_index(tmp, struct symbol_fix *, i);
			
			/* update symbol's current value */
			sym_calc_value(fix->sym);
			
			/* value already set? */
			if (fix->type == SF_BOOLEAN) {
				if (fix->tri == sym_get_tristate_value(fix->sym)) {
					g_array_remove_index(tmp, i--);
					no_symbols_set++;
					continue;
				}
			} else if (fix->type == SF_NONBOOLEAN) {
				if (strcmp(str_get(&fix->nb_val),sym_get_string_value(fix->sym)) == 0) {
					g_array_remove_index(tmp, i--);
					no_symbols_set++;
					continue;
				}
			} else {
				perror("Error applying fix. Value set for disallowed.");
			}

				
			/* could not set value, try next */
			if (fix->type == SF_BOOLEAN) {
				if (!sym_set_tristate_value(fix->sym, fix->tri)) {
					continue;
				}
			} else if (fix->type == SF_NONBOOLEAN) {
				if (!sym_set_string_value(fix->sym, str_get(&fix->nb_val))) {
					continue;
				}
			} else {
				perror("Error applying fix. Value set for disallowed.");
			}

			
			/* could set value, remove from tmp */
			manually_changed++;
			if (fix->type == SF_BOOLEAN) {
				printf("%s set to %s.\n", sym_get_name(fix->sym), tristate_get_char(fix->tri));
			} else if (fix->type == SF_NONBOOLEAN) {
				printf("%s set to %s.\n", sym_get_name(fix->sym), str_get(&fix->nb_val));
			}
			
			g_array_remove_index(tmp, i--);
			no_symbols_set++;
		}
		iterations++;
	}

	printf("Fixes successfully applied.\n");
	
	return manually_changed;
}

/*
 * check whether all symbols are already within range
 */
static bool sdv_within_range(GArray *arr)
{
	unsigned int i;
	struct symbol_dvalue *sdv;
	for (i = 0; i < arr->len; i++) {
		sdv = g_array_index(arr, struct symbol_dvalue *, i);
		
		assert(sym_is_boolean(sdv->sym));
		
		if (sdv->tri == sym_get_tristate_value(sdv->sym)) continue;
		
		if (!sym_tristate_within_range(sdv->sym, sdv->tri))
			return false;
	}
	
	return true;
}
