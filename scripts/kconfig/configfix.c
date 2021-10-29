// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Patrick Franz <deltaone@debian.org>
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

struct fexpr *satmap;
size_t satmap_size;

struct sdv_list *sdv_symbols; /* array with conflict-symbols */

bool CFDEBUG = false;
bool stop_rangefix = false;

struct fexpr *const_false; /* constant False */
struct fexpr *const_true; /* constant True */
struct fexpr *symbol_yes_fexpr; /* symbol_yes as fexpr */
struct fexpr *symbol_mod_fexpr; /* symbol_mod as fexpr */
struct fexpr *symbol_no_fexpr; /* symbol_no_as fexpr */

static PicoSAT *pico;
static bool init_done = false;
static struct sym_list *conflict_syms;

static bool sdv_within_range(struct sdv_list *symbols);

/* -------------------------------------- */

int run_satconf_cli(const char *Kconfig_file)
{
	clock_t start, end;
	double time;

	if (!init_done) {
		printd("Init...");
		/* measure time for constructing constraints and clauses */
		start = clock();

		/* parse Kconfig-file and read .config */
		init_config(Kconfig_file);

		/* initialize satmap and cnf_clauses */
		init_data();

		/* creating constants */
		create_constants();

		/* assign SAT variables & create sat_map */
		create_sat_variables();

		/* get the constraints */
		get_constraints();

		/* print all symbols and its constraints */
		// 		print_all_symbols();

		end = clock();
		time = ((double)(end - start)) / CLOCKS_PER_SEC;

		printd("done. (%.6f secs.)\n", time);

		init_done = true;
	}

	/* start PicoSAT */
	PicoSAT *pico = initialize_picosat();
	printd("Building CNF-clauses...");
	start = clock();

	/* construct the CNF clauses */
	construct_cnf_clauses(pico);

	end = clock();
	time = ((double)(end - start)) / CLOCKS_PER_SEC;

	printd("done. (%.6f secs.)\n", time);

	/* add assumptions for all other symbols */
	printd("Adding assumptions...");
	start = clock();

	unsigned int i;
	struct symbol *sym;
	for_all_symbols(i, sym) {
		if (sym->type == S_UNKNOWN)
			continue;

		if (!sym->name || !sym_has_prompt(sym))
			continue;

		sym_add_assumption(pico, sym);

	}

	end = clock();
	time = ((double)(end - start)) / CLOCKS_PER_SEC;

	printd("done. (%.6f secs.)\n", time);

	picosat_solve(pico);

	printd("\n===> STATISTICS <===\n");
	printd("Constraints  : %d\n", count_counstraints());
	printd("CNF-clauses  : %d\n", picosat_added_original_clauses(pico));
	printd("SAT-variables: %d\n", picosat_variables(pico));
	printd("Temp vars    : %d\n", tmp_variable_nr - 1);
	printd("PicoSAT time : %.6f secs.\n", picosat_seconds(pico));

	return EXIT_SUCCESS;
}

/*
 * called from satdvconfig
 */
struct sfl_list *run_satconf(struct sdv_list *symbols)
{
	clock_t start, end;
	double time;

	/* check whether all values can be applied -> no need to run */
	if (sdv_within_range(symbols)) {
		printd("\nAll symbols are already within range.\n\n");
		return sfl_list_init();
	}

	if (!init_done) {
		printd("\n");
		printd("Init...");

		/* measure time for constructing constraints and clauses */
		start = clock();

		/* initialize satmap and cnf_clauses */
		init_data();

		/* creating constants */
		create_constants();

		/* assign SAT variables & create sat_map */
		create_sat_variables();

		/* get the constraints */
		get_constraints();

		end = clock();
		time = ((double)(end - start)) / CLOCKS_PER_SEC;

		printd("done. (%.6f secs.)\n", time);

		/* start PicoSAT */
		pico = initialize_picosat();
		printd("Building CNF-clauses...");
		start = clock();

		/* construct the CNF clauses */
		construct_cnf_clauses(pico);

		end = clock();
		time = ((double)(end - start)) / CLOCKS_PER_SEC;

		printd("done. (%.6f secs.)\n", time);

		printd("CNF-clauses added: %d\n",
		       picosat_added_original_clauses(pico));

		init_done = true;
	}

	/* copy array with symbols to change */
	sdv_symbols = sdv_list_copy(symbols);

	/* add assumptions for conflict-symbols */
	sym_add_assumption_sdv(pico, sdv_symbols);

	/* add assumptions for all other symbols */
	struct symbol *sym;
	unsigned int i;
	for_all_symbols(i, sym) {
		if (sym->type == S_UNKNOWN)
			continue;

		if (!sym_is_sdv(sdv_symbols, sym))
			sym_add_assumption(pico, sym);
	}

	/* store the conflict symbols */
	conflict_syms = sym_list_init();
	struct sdv_node *node;
	sdv_list_for_each(node, sdv_symbols)
		sym_list_add(conflict_syms, node->elem->sym);

	printd("Solving SAT-problem...");
	start = clock();

	int res = picosat_sat(pico, -1);

	end = clock();
	time = ((double)(end - start)) / CLOCKS_PER_SEC;
	printd("done. (%.6f secs.)\n\n", time);

	struct sfl_list *ret;
	if (res == PICOSAT_SATISFIABLE) {
		printd("===> PROBLEM IS SATISFIABLE <===\n");

		ret = sfl_list_init();

	} else if (res == PICOSAT_UNSATISFIABLE) {
		printd("===> PROBLEM IS UNSATISFIABLE <===\n");
		printd("\n");

		ret = rangefix_run(pico);
	} else {
		printd("Unknown if satisfiable.\n");

		ret = sfl_list_init();
	}

	sdv_list_free(sdv_symbols);

	return ret;
}

/*
 * check whether a symbol is a conflict symbol
 */
static bool sym_is_conflict_sym(struct symbol *sym)
{
	struct sym_node *node;
	sym_list_for_each(node,
			  conflict_syms) if (sym == node->elem) return true;

	return false;
}

/*
 * check whether all conflict symbols are set to their target values
 */
static bool syms_have_target_value(struct sfix_list *list)
{
	struct symbol_fix *fix;
	struct sfix_node *node;

	sfix_list_for_each(node, list) {
		fix = node->elem;

		if (!sym_is_conflict_sym(fix->sym))
			continue;

		sym_calc_value(fix->sym);

		if (sym_is_boolean(fix->sym)) {
			if (fix->tri != sym_get_tristate_value(fix->sym))
				return false;
		} else {
			if (strcmp(str_get(&fix->nb_val),
				   sym_get_string_value(fix->sym)) != 0)
				return false;
		}
	}

	return true;
}

/*
 *
 * apply the fixes from a diagnosis
 */
int apply_fix(struct sfix_list *fix)
{
	struct symbol_fix *sfix;
	struct sfix_node *node, *next;
	unsigned int no_symbols_set = 0, iterations = 0, manually_changed = 0;

	struct sfix_list *tmp = sfix_list_copy(fix);

	printd("Trying to apply fixes now...\n");

	while (no_symbols_set < fix->size && !syms_have_target_value(fix)) {
		if (iterations > fix->size * 2) {
			printd("\nCould not apply all values :-(.\n");
			return manually_changed;
		}

		for (node = tmp->head; node != NULL;) {
			sfix = node->elem;

			/* update symbol's current value */
			sym_calc_value(sfix->sym);

			/* value already set? */
			if (sfix->type == SF_BOOLEAN) {
				if (sfix->tri == sym_get_tristate_value(sfix->sym)) {
					next = node->next;
					sfix_list_delete(tmp, node);
					node = next;
					no_symbols_set++;
					continue;
				}
			} else if (sfix->type == SF_NONBOOLEAN) {
				if (strcmp(str_get(&sfix->nb_val),
					   sym_get_string_value(sfix->sym)) == 0) {
					next = node->next;
					sfix_list_delete(tmp, node);
					node = next;
					no_symbols_set++;
					continue;
				}
			} else {
				perror("Error applying fix. Value set for disallowed.");
			}

			/* could not set value, try next */
			if (sfix->type == SF_BOOLEAN) {
				if (!sym_set_tristate_value(sfix->sym,
							    sfix->tri)) {
					node = node->next;
					continue;
				}
			} else if (sfix->type == SF_NONBOOLEAN) {
				if (!sym_set_string_value(
					    sfix->sym,
					    str_get(&sfix->nb_val))) {
					node = node->next;
					continue;
				}
			} else {
				perror("Error applying fix. Value set for disallowed.");
			}

			/* could set value, remove from tmp */
			manually_changed++;
			if (sfix->type == SF_BOOLEAN) {
				printd("%s set to %s.\n",
				       sym_get_name(sfix->sym),
				       tristate_get_char(sfix->tri));
			} else if (sfix->type == SF_NONBOOLEAN) {
				printd("%s set to %s.\n",
				       sym_get_name(sfix->sym),
				       str_get(&sfix->nb_val));
			}

			next = node->next;
			sfix_list_delete(tmp, node);
			node = next;
			no_symbols_set++;
		}

		iterations++;
	}

	printd("Fixes successfully applied.\n");

	return manually_changed;
}

/*
 * stop RangeFix after the next iteration
 */
void interrupt_rangefix(void)
{
	stop_rangefix = true;
}

/*
 * check whether all symbols are already within range
 */
static bool sdv_within_range(struct sdv_list *symbols)
{
	struct symbol_dvalue *sdv;
	struct sdv_node *node;

	sdv_list_for_each(node, symbols) {
		sdv = node->elem;

		assert(sym_is_boolean(sdv->sym));

		if (sdv->tri == sym_get_tristate_value(sdv->sym))
			continue;

		if (!sym_tristate_within_range(sdv->sym, sdv->tri))
			return false;
	}

	return true;
}

struct sfix_list *select_solution(struct sfl_list *solutions, int index)
{
	struct sfl_node *node = solutions->head;
	unsigned int counter;
	for (counter = 0; counter < index; counter++)
		node = node->next;

	return node->elem;
}

struct symbol_fix *select_symbol(struct sfix_list *solution, int index)
{
	struct sfix_node *node = solution->head;
	unsigned int counter;
	for (counter = 0; counter < index; counter++)
		node = node->next;

	return node->elem;
}
