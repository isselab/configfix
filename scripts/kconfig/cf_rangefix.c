/* SPDX-License-Identifier: GPL-2.0 */
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

#define MAX_DIAGNOSES 3
#define MAX_SECONDS 120
#define PRINT_UNSAT_CORE true
#define PRINT_DIAGNOSES false
#define PRINT_DIAGNOSIS_FOUND true
#define MINIMISE_DIAGNOSES false
#define MINIMISE_UNSAT_CORE true

static struct fexl_list *diagnoses;
static struct sfl_list *diagnoses_symbol;

static struct fexl_list * generate_diagnoses(PicoSAT *pico);

static void add_fexpr_to_constraint_set(struct fexpr_list *C);
static void set_assumptions(PicoSAT *pico, struct fexpr_list *c);
static void fexpr_add_assumption(PicoSAT *pico, struct fexpr *e, int satval);
static struct fexpr_list * get_unsat_core_soft(PicoSAT *pico);
static struct fexpr_list * minimise_unsat_core(PicoSAT *pico, struct fexpr_list *C);


static struct fexpr_list * get_difference(struct fexpr_list *C, struct fexpr_list *E0);
static bool has_intersection(struct fexpr_list *e, struct fexpr_list *X);
static struct fexpr_list * fexpr_list_union(struct fexpr_list *A, struct fexpr_list *B);
static struct fexl_list * fexl_list_union(struct fexl_list *A, struct fexl_list *B);
static bool is_subset_of(struct fexpr_list *A, struct fexpr_list *B);
static void print_unsat_core(struct fexpr_list *list);
static bool diagnosis_contains_fexpr(struct fexpr_list *diagnosis, struct fexpr *e);
static bool diagnosis_contains_symbol(struct sfix_list *diagnosis, struct symbol *sym);

static void print_diagnoses(struct fexl_list *diag);
static void print_diagnoses_symbol(struct sfl_list *diag_sym);

static struct sfl_list * convert_diagnoses(struct fexl_list *diagnoses);
static struct sfix_list * convert_diagnosis(struct fexpr_list *diagnosis);
static struct symbol_fix * symbol_fix_create(struct fexpr *e, enum symbolfix_type type, struct fexpr_list *diagnosis);
static struct sfl_list * minimise_diagnoses(PicoSAT *pico, struct fexl_list *diagnoses);

static tristate calculate_new_tri_val(struct fexpr *e, struct fexpr_list *diagnosis);
static const char * calculate_new_string_value(struct fexpr *e, struct fexpr_list *diagnosis);

/* count assumptions, only used for debugging */
static unsigned int nr_of_assumptions = 0, nr_of_assumptions_true = 0;

/* -------------------------------------- */

struct sfl_list * rangefix_run(PicoSAT *pico)
{
	printd("Starting RangeFix...\n");
	printd("Generating diagnoses...");
	clock_t start, end;
	double time;
	start = clock();

	/* generate the diagnoses */
	diagnoses = generate_diagnoses(pico);

	end = clock();
	time = ((double) (end - start)) / CLOCKS_PER_SEC;
	printd("Generating diagnoses...done. (%.6f secs.)\n", time);

	if (PRINT_DIAGNOSES) {
		printd("Diagnoses (only for debugging):\n");
		print_diagnoses(diagnoses);
		printd("\n");
	}

	/* convert diagnoses of fexpr to diagnoses of symbols */
	if (MINIMISE_DIAGNOSES)
		diagnoses_symbol = minimise_diagnoses(pico, diagnoses);
	else
		diagnoses_symbol = convert_diagnoses(diagnoses);

	printd("\n");

	return diagnoses_symbol;
}

/*
 * generate the diagnoses
 */
static struct fexl_list * generate_diagnoses(PicoSAT *pico)
{
	struct fexpr_list *C = fexpr_list_init();
	struct fexl_list *E = fexl_list_init();
	struct fexl_list *R = fexl_list_init();
	struct fexpr_list *X, *e, *x_set, *E1, *E2;
	struct fexl_list *E_R_Union;

	/* create constraint set C */
	add_fexpr_to_constraint_set(C);

	if (PRINT_UNSAT_CORE)
		printd("\n");

	/* init E with an empty diagnosis */
	struct fexpr_list *empty_diagnosis = fexpr_list_init();
	fexl_list_add(E, empty_diagnosis);

	/* start the clock */
	clock_t start_t, end_t;
	double time_t;
	start_t = clock();

	while (E->size > 0) {
		/* get random diagnosis */
		struct fexpr_list *E0 = E->head->elem;

		/* calculate C\E0 */
		struct fexpr_list *c = get_difference(C, E0);

		/* set assumptions */
		nr_of_assumptions = 0;
		nr_of_assumptions_true = 0;
		set_assumptions(pico, c);

		int res = picosat_sat(pico, -1);

		if (res == PICOSAT_SATISFIABLE) {
			if (PRINT_DIAGNOSIS_FOUND && CFDEBUG)
				fexpr_list_print("DIAGNOSIS FOUND", E0);

			fexl_list_delete(E, E->head);
			if (E0->size > 0)
				fexl_list_add(R, E0);
			else
				fexpr_list_free(E0);

			fexpr_list_free(c);

			if (R->size >= MAX_DIAGNOSES)
				goto DIAGNOSES_FOUND;

			continue;

		} else if (res == PICOSAT_UNSATISFIABLE) {

		} else if (res == PICOSAT_UNKNOWN) {
			printd("UNKNOWN\n");
		} else {
			perror("Doh.");
		}

		/* check elapsed time */
		end_t = clock();
		time_t = ((double) (end_t - start_t)) / CLOCKS_PER_SEC;
		if (time_t > (double) MAX_SECONDS)
			goto DIAGNOSES_FOUND;

		/* get unsat core from SAT solver */
		X = get_unsat_core_soft(pico);

		/* minimise the unsat core */
		if (MINIMISE_UNSAT_CORE)
			X = minimise_unsat_core(pico, X);

		if (PRINT_UNSAT_CORE)
			print_unsat_core(X);

		struct fexl_node *node, *tmp;
		for (node = E->head; node != NULL;) {
			/* get partial diagnosis */
			e = node->elem;

			/* check, if there is an intersection between e and X
			 * if there is, go to the next partial diagnosis */
			if (has_intersection(e, X)) {
				node = node->next;
				continue;
			}

			/* for each fexpr in the core */
			struct fexpr_node *fnode;
			fexpr_list_for_each(fnode, X) {
				struct fexpr *x = fnode->elem;

				/* create {x} */
				x_set = fexpr_list_init();
				fexpr_list_add(x_set, x);

				/* create E' = e ∪ {x} */
				E1 = fexpr_list_union(e, x_set);

				/* create (E\e) ∪ R */
				E_R_Union = fexl_list_copy(E);
				fexl_list_delete_elem(E_R_Union, e);
				E_R_Union = fexl_list_union(E_R_Union, R);

				bool E2_subset_of_E1 = false;

				/* E" ∈ (E\e) ∪ R */
				struct fexl_node *lnode;
				fexl_list_for_each(lnode, E_R_Union) {
					E2 = lnode->elem;

					/* E" ⊆ E' ? */
					if (is_subset_of(E2, E1)) {
						E2_subset_of_E1 = true;
						break;
					}
				}

				fexl_list_free(E_R_Union);

				/* ∄ E" ⊆ E' */
				if (!E2_subset_of_E1)
					fexl_list_add(E, E1);
				else
					fexpr_list_free(E1);
			}

			fexpr_list_free(e);

			tmp = node->next;
			fexl_list_delete(E, node);
			node = tmp;
		}
		fexpr_list_free(X);
		fexpr_list_free(c);
	}

DIAGNOSES_FOUND:
	fexpr_list_free(C);
	fexl_list_free(E);

	return R;
}

/*
 * add the fexpr to the constraint set C
 */
static void add_fexpr_to_constraint_set(struct fexpr_list *C)
{
	unsigned int i, nr_sym = 0, nr_fexpr = 0;
	struct symbol *sym;
	for_all_symbols(i, sym) {
		/* must be a proper symbol */
		if (sym->type == S_UNKNOWN)
			continue;

		/* don't need the conflict symbols
		 * they are handled seperately */
		if (sym_is_sdv(sdv_symbols, sym))
			continue;

		/* must have a prompt and a name */
		if (!sym->name || !sym_has_prompt(sym))
			continue;

		nr_sym++;

		if (sym->type == S_BOOLEAN) {
			fexpr_list_add(C, sym->fexpr_y);
			nr_fexpr++;
		} else if (sym->type == S_TRISTATE) {
			fexpr_list_add(C, sym->fexpr_y);
			fexpr_list_add(C, sym->fexpr_m);
			nr_fexpr += 2;
		} else if (sym->type == S_INT || sym->type == S_HEX || sym->type == S_STRING) {
			struct fexpr_node *node;
			fexpr_list_for_each(node, sym->nb_vals) {
				fexpr_list_add(C, node->elem);
				nr_fexpr++;
			}
		} else {
			perror("Error adding variables to constraint set C.");
		}
	}
}

/*
 * check whether the fexpr symbolises the no-value-set fexpr for a non-boolean symbol
 */
static bool fexpr_is_novalue(struct fexpr *e)
{
	if (!sym_is_nonboolean(e->sym))
		return false;

	return e == e->sym->nb_vals->head->elem;
}

static void set_assumptions_sdv(PicoSAT *pico, struct sdv_list *arr)
{
	struct symbol_dvalue *sdv;
	struct sdv_node *node;
	struct symbol *sym;

	sdv_list_for_each(node, arr) {
		sdv = node->elem;
		sym = sdv->sym;

		int lit_y = sym->fexpr_y->satval;

		if (sym->type == S_BOOLEAN) {
			switch (sdv->tri) {
			case yes:
				picosat_assume(pico, lit_y);
				sym->fexpr_y->assumption = true;
				nr_of_assumptions_true++;
				break;
			case no:
				picosat_assume(pico, -lit_y);
				sym->fexpr_y->assumption = false;
				break;
			case mod:
				perror("Should not happen.\n");
			}
			nr_of_assumptions++;
		} else if (sym->type == S_TRISTATE) {
			int lit_m = sym->fexpr_m->satval;
			switch (sdv->tri) {
			case yes:
				picosat_assume(pico, lit_y);
				sym->fexpr_y->assumption = true;
				picosat_assume(pico, -lit_m);
				sym->fexpr_m->assumption = false;
				nr_of_assumptions_true++;
				break;
			case mod:
				picosat_assume(pico, -lit_y);
				sym->fexpr_y->assumption = false;
				picosat_assume(pico, lit_m);
				sym->fexpr_m->assumption = true;
				nr_of_assumptions_true++;
				break;
			case no:
				picosat_assume(pico, -lit_y);
				sym->fexpr_y->assumption = false;
				picosat_assume(pico, -lit_m);
				sym->fexpr_y->assumption = false;
			}
			nr_of_assumptions += 2;
		}
	}
}

/*
 * set the assumptions for the next run of Picosat
 */
static void set_assumptions(PicoSAT *pico, struct fexpr_list *c)
{
	struct fexpr_node *node;
	fexpr_list_for_each(node, c)
		fexpr_add_assumption(pico, node->elem, node->elem->satval);

	/* set assumptions for the conflict-symbols */
	set_assumptions_sdv(pico, sdv_symbols);
}

/*
 * set the assumtption for a fexpr for the next run of Picosat
 */
static void fexpr_add_assumption(PicoSAT *pico, struct fexpr *e, int satval)
{
	struct symbol *sym = e->sym;

	if (sym->type == S_BOOLEAN) {
		int tri_val = sym_get_tristate_value(sym);

		if (tri_val == yes) {
			picosat_assume(pico, satval);
			e->assumption = true;
			nr_of_assumptions_true++;
		} else {
			picosat_assume(pico, -satval);
			e->assumption = false;
		}
		nr_of_assumptions++;
	}

	if (sym->type == S_TRISTATE) {
		int tri_val = sym_get_tristate_value(sym);

		if (e->tri == yes) {
			if (tri_val == yes) {
				picosat_assume(pico, satval);
				e->assumption = true;
				nr_of_assumptions_true++;
			} else {
				picosat_assume(pico, -satval);
				e->assumption = false;
			}
		} else if (e->tri == mod) {
			if (tri_val == mod) {
				picosat_assume(pico, satval);
				e->assumption = true;
				nr_of_assumptions_true++;
			} else {
				picosat_assume(pico, -satval);
				e->assumption = false;
			}
		}
		nr_of_assumptions++;
	}

	if (sym->type == S_INT || sym->type == S_HEX || sym->type == S_STRING) {

		char *string_val = (char *) sym_get_string_value(sym);

		if (sym->type == S_STRING && !strcmp(string_val, ""))
			return;

		/* check, if e symbolises the no-value-set fexpr */
		if (fexpr_is_novalue(e)) {
			if (!sym_nonbool_has_value_set(sym)) {
				picosat_assume(pico, satval);
				e->assumption = true;
				nr_of_assumptions_true++;
			} else {
				picosat_assume(pico, -satval);
				e->assumption = false;
			}
		}
		/* check whena string-symbol has value "" */
		else if (sym->type == S_STRING && !strcmp(string_val, "")) {
			if (sym_nonbool_has_value_set(sym)) {
				picosat_assume(pico, satval);
				e->assumption = true;
				nr_of_assumptions_true++;
			} else {
				picosat_assume(pico, -satval);
				e->assumption = false;
			}
		}
		else {
			if (!strcmp(str_get(&e->nb_val), string_val)) {
				picosat_assume(pico, satval);
				e->assumption = true;
				nr_of_assumptions_true++;
			} else {
				picosat_assume(pico, -satval);
				e->assumption = false;
			}
		}
		nr_of_assumptions++;
	}
}

/*
 * get the unsatisfiable soft constraints from the last run of Picosat
 */
static struct fexpr_list * get_unsat_core_soft(PicoSAT *pico)
{
	struct fexpr_list *ret = fexpr_list_init();
	struct fexpr *e;

	int *lit = malloc(sizeof(int));
	const int *i = picosat_failed_assumptions(pico);
	*lit = abs(*i++);

	while (*lit != 0) {
		e = &satmap[*lit];

		if (!sym_is_sdv(sdv_symbols, e->sym))
			fexpr_list_add(ret, e);

		*lit = abs(*i++);
	}

	return ret;
}

/*
 * minimise the unsat core C
 */
static struct fexpr_list * minimise_unsat_core(PicoSAT *pico, struct fexpr_list *C)
{
	/* no need to check further */
	if (C->size == 1)
		return C;

	struct fexpr_list *c_set;
	struct fexpr_node *node, *tmp;

	for (node = C->head; node != NULL;) {
		if (C->size == 1)
			return C;

		/* create C\c */
		c_set = fexpr_list_init();
		fexpr_list_add(c_set, node->elem);
		struct fexpr_list *t = get_difference(C, c_set);

		/* invoke PicoSAT */
		set_assumptions(pico, t);

		int res = picosat_sat(pico, -1);

		tmp = node->next;

		if (res == PICOSAT_UNSATISFIABLE)
			fexpr_list_delete(C, node);

		node = tmp;

		fexpr_list_free(c_set);
		fexpr_list_free(t);
	}

	return C;
}


/*
 * Calculate C\E0
 */
static struct fexpr_list * get_difference(struct fexpr_list *C, struct fexpr_list *E0)
{
	struct fexpr_list *ret = fexpr_list_init();
	struct fexpr_node *node1, *node2;
	bool found;

	fexpr_list_for_each(node1, C) {
		found = false;
		fexpr_list_for_each(node2, E0) {
			if (node1->elem->satval == node2->elem->satval) {
				found = true;
				break;
			}
		}
		if (!found)
			fexpr_list_add(ret, node1->elem);
	}

	return ret;
}

/*
 * check, if there is an intersection between e and X
 */
static bool has_intersection(struct fexpr_list *e, struct fexpr_list *X)
{
	struct fexpr_node *node1, *node2;
	fexpr_list_for_each(node1, e)
		fexpr_list_for_each(node2, X)
			if (node1->elem->satval == node2->elem->satval)
				return true;

	return false;
}

/*
 * get the union of 2 fexpr_list
 */
static struct fexpr_list * fexpr_list_union(struct fexpr_list *A, struct fexpr_list *B)
{
	struct fexpr_list *ret = fexpr_list_copy(A);
	struct fexpr_node *node1, *node2;
	bool found;

	fexpr_list_for_each(node2, B) {
		found = false;
		fexpr_list_for_each(node1, A) {
			if (node2->elem->satval == node1->elem->satval) {
				found = true;
				break;
			}
		}
		if (!found)
			fexpr_list_add(ret, node2->elem);
	}

	return ret;
}

/*
 * get the union of 2 fexl_list
 */
static struct fexl_list * fexl_list_union(struct fexl_list *A, struct fexl_list *B)
{
	struct fexl_list *ret = fexl_list_copy(A);
	struct fexl_node *node1, *node2;
	bool found;

	fexl_list_for_each(node2, B) {
		found = false;
		fexl_list_for_each(node1, A) {
			if (node2->elem == node1->elem) {
				found = true;
				break;
			}
		}
		if (!found)
			fexl_list_add(ret, node2->elem);
	}

	return ret;
}

/*
 * check, whether A is a subset of B
 */
static bool is_subset_of(struct fexpr_list *A, struct fexpr_list *B)
{
	struct fexpr_node *node1, *node2;
	bool found;

	fexpr_list_for_each(node1, A) {
		found = false;
		fexpr_list_for_each(node2, B) {
			if (node1->elem->satval == node2->elem->satval) {
				found = true;
				break;
			}
		}
		if (!found)
			return false;
	}

	return true;
}

/*
 * print an unsat core
 */
static void print_unsat_core(struct fexpr_list *list)
{
	struct fexpr_node *node;
	printd("Unsat core: [");

	fexpr_list_for_each(node, list) {
		printd("%s", str_get(&node->elem->name));
		printd(" <%s>", node->elem->assumption == true ? "T" : "F");
		if (node->next != NULL)
			printd(", ");
	}

	printd("]\n");
}


/*
 * check if a diagnosis contains a fexpr
 */
static bool diagnosis_contains_fexpr(struct fexpr_list *diagnosis, struct fexpr *e)
{
	struct fexpr_node *node;

	fexpr_list_for_each(node, diagnosis)
		if (node->elem->satval == e->satval)
			return true;

	return false;
}

/*
 * check if a diagnosis contains a symbol
 */
static bool diagnosis_contains_symbol(struct sfix_list *diagnosis, struct symbol *sym)
{
	struct sfix_node *node;

	sfix_list_for_each(node, diagnosis)
		if (sym == node->elem->sym)
			return true;

	return false;
}

/*
 * print the diagnoses of type fexpr_list
 */
static void print_diagnoses(struct fexl_list *diag)
{
	struct fexl_node *lnode;
	unsigned int i = 1;

	fexl_list_for_each(lnode, diag) {
		printd("%d: [", i++);
		struct fexpr_node *node;
		fexpr_list_for_each(node, lnode->elem) {
			char *new_val = node->elem->assumption ? "false" : "true";
			printd("%s => %s", str_get(&node->elem->name), new_val);
			if (node->next != NULL)
				printd(", ");
		}
		printd("]\n");
	}
}

/*
 * print a single diagnosis of type symbol_fix
 */
void print_diagnosis_symbol(struct sfix_list *diag_sym)
{
	struct symbol_fix *fix;
	struct sfix_node *node;

	printd("[");

	sfix_list_for_each(node, diag_sym) {
		fix = node->elem;

		if (fix->type == SF_BOOLEAN) {
			printd("%s => %s", fix->sym->name, tristate_get_char(fix->tri));
		} else if (fix->type == SF_NONBOOLEAN) {
			printd("%s => %s", fix->sym->name, str_get(&fix->nb_val));
		} else {
			perror("NB not yet implemented.");
		}

		if (node->next != NULL)
			printd(", ");
	}
	printd("]\n");
}

/*
 * print the diagnoses of type symbol_fix
 */
static void print_diagnoses_symbol(struct sfl_list *diag_sym)
{
	struct sfl_node *arr;
	unsigned int i = 1;

	sfl_list_for_each(arr, diag_sym) {
		printd("%d: ", i++);
		print_diagnosis_symbol(arr->elem);
	}
}

/*
 * convert a single diagnosis of fexpr into a diagnosis of symbols
 */
static struct sfix_list * convert_diagnosis(struct fexpr_list *diagnosis)
{
	struct sfix_list *diagnosis_symbol = sfix_list_init();
	struct fexpr *e;
	struct symbol_fix *fix;
	struct symbol_dvalue *sdv;

	/* set the values for the conflict symbols */
	struct sdv_node *snode;
	sdv_list_for_each(snode, sdv_symbols) {
		sdv = snode->elem;
		fix = xcalloc(1, sizeof(*fix));
		fix->sym = sdv->sym;
		fix->type = SF_BOOLEAN;
		fix->tri = sdv->tri;
		sfix_list_add(diagnosis_symbol, fix);
	}

	struct fexpr_node * fnode;
	fexpr_list_for_each(fnode, diagnosis) {
		e = fnode->elem;

		/* diagnosis already contains symbol, so continue */
		if (diagnosis_contains_symbol(diagnosis_symbol, e->sym))
			continue;

		enum symbolfix_type type;
		if (sym_is_boolean(e->sym))
			type = SF_BOOLEAN;
		else if (sym_is_nonboolean(e->sym))
			type = SF_NONBOOLEAN;
		else
			type = SF_DISALLOWED;
		fix = symbol_fix_create(e, type, diagnosis);

		sfix_list_add(diagnosis_symbol, fix);
	}

	return diagnosis_symbol;
}

/*
 * convert the diagnoses of fexpr into diagnoses of symbols
 * it is easier to handle symbols when applying fixes
 */
static struct sfl_list * convert_diagnoses(struct fexl_list *diag_arr)
{
	diagnoses_symbol = sfl_list_init();

	struct fexl_node *lnode;
	fexl_list_for_each(lnode, diag_arr) {
		struct sfix_list *fix = convert_diagnosis(lnode->elem);
		sfl_list_add(diagnoses_symbol, fix);
	}

	return diagnoses_symbol;
}

/*
 * create a symbol_fix given a fexpr
 */
static struct symbol_fix * symbol_fix_create(struct fexpr *e, enum symbolfix_type type, struct fexpr_list *diagnosis)
{
	struct symbol_fix *fix = malloc(sizeof(struct symbol_fix));
	fix->sym = e->sym;
	fix->type = type;

	switch(type) {
	case SF_BOOLEAN:
		fix->tri = calculate_new_tri_val(e, diagnosis);
		break;
	case SF_NONBOOLEAN:
		fix->nb_val = str_new();
		str_append(&fix->nb_val, calculate_new_string_value(e, diagnosis));
		break;
	default:
		perror("Illegal symbolfix_type.\n");
	}

	return fix;
}

/*
 * remove symbols from the diagnosis, which will be set automatically:
 * 1. symbol gets selected
 * 2. choice symbol gets enabled/disabled automatically
 * 3. symbol uses a default value
 */
static struct sfl_list * minimise_diagnoses(PicoSAT *pico, struct fexl_list *diagnoses)
{
	clock_t start, end;
	double time;

	printd("Minimising diagnoses...");

	start = clock();

	struct fexpr_list *d;
	struct sfix_list *diagnosis_symbol;
	struct sfl_list *diagnoses_symbol = sfl_list_init();
	struct fexpr *e;
	int satval, deref = 0;
	struct symbol_fix *fix;

	/* create soft constraint set C */
	struct fexpr_list *C = fexpr_list_init();
	add_fexpr_to_constraint_set(C);

	struct fexl_node *flnode;
	fexl_list_for_each(flnode, diagnoses) {
		d = flnode->elem;

		/* set assumptions for those symbols that don't need to be changed */
		set_assumptions(pico, get_difference(C, d));

		/* flip the assumptions from the diagnosis */
		struct fexpr_node *fnode;
		fexpr_list_for_each(fnode, d) {
			e = fnode->elem;
			satval = e->assumption ? -(e->satval) : e->satval;
			picosat_assume(pico, satval);
		}

		int res = picosat_sat(pico, -1);
		if (res != PICOSAT_SATISFIABLE)
			perror("Diagnosis not satisfiable (minimise).");

		diagnosis_symbol = convert_diagnosis(d);

		/* check if symbol gets selected */
		struct sfix_node *snode;
		for (snode = diagnosis_symbol->head; snode != NULL;) {
			fix = snode->elem;

			/* symbol is never selected, continue */
			if (!fix->sym->fexpr_sel_y) {
				snode = snode->next;
				continue;
			}

			/* check, whether the symbol was selected anyway */
			if (fix->sym->type == S_BOOLEAN && fix->tri == yes) {
				deref = picosat_deref(pico, fix->sym->fexpr_sel_y->satval);
			} else if (fix->sym->type == S_TRISTATE && fix->tri == yes) {
				deref = picosat_deref(pico, fix->sym->fexpr_sel_y->satval);
			} else if (fix->sym->type == S_TRISTATE && fix->tri == mod) {
				deref = picosat_deref(pico, fix->sym->fexpr_sel_m->satval);
			}

			if (deref == 1) {
				struct sfix_node *tmp = snode->next;
				sfix_list_delete(diagnosis_symbol, snode);
				snode = tmp;
			} else {
				deref = 0;
				snode = snode->next;
			}
		}
		sfl_list_add(diagnoses_symbol, diagnosis_symbol);
	}

	end = clock();
	time = ((double) (end - start)) / CLOCKS_PER_SEC;

	printd("done. (%.6f secs.)\n", time);

	return diagnoses_symbol;
}

/*
 * list the diagnoses and let user choose a diagnosis to be applied
 */
struct sfix_list * choose_fix(struct sfl_list *diag)
{
	printd("=== GENERATED DIAGNOSES ===\n");
	printd("0: No changes wanted\n");
	print_diagnoses_symbol(diag);

	int choice;
	printd("\n> Choose option: ");
	scanf("%d", &choice);

	/* no changes wanted */
	if (choice == 0)
		return NULL;

	/* invalid choice */
	if (choice > diag->size)
		return NULL;

	unsigned int counter;
	struct sfl_node *node = diag->head;
	for (counter = 1; counter < choice; counter++)
		node = node->next;

	return node->elem;
}


/*
 * calculate the new value for a boolean symbol given a diagnosis and an fexpr
 */
static tristate calculate_new_tri_val(struct fexpr *e, struct fexpr_list *diagnosis)
{
	assert(sym_is_boolean(e->sym));

	/* return the opposite of the last assumption for booleans */
	if (e->sym->type == S_BOOLEAN)
		return e->assumption ? no : yes;

	/* new values for tristate must be deduced from the diagnosis */
	if (e->sym->type == S_TRISTATE) {
		/* fexpr_y */
		if (e->tri == yes) {
			if (e->assumption == true)
				/*
				 * if diagnosis contains fexpr_m, fexpr_m was false
				 * => new value is mod
				 */
				return diagnosis_contains_fexpr(diagnosis, e->sym->fexpr_m) ? mod : no;
			else if (e->assumption == false)
				/*
				 * if fexpr_y is set to true, the new value must be yes
				 */
				return yes;
		}
		/* fexpr_m */
		if (e->tri == mod) {
			if (e->assumption == true)
				/*
				 * if diagnosis contains fexpr_y, fexpr_y was false
				 * => new value is yes
				 */
				return diagnosis_contains_fexpr(diagnosis, e->sym->fexpr_m) ? yes : no;
			else if (e->assumption == false)
				/*
				 * if diagnosis contains fexpr_m, the new value must be mod
				 */
				return mod;
		}
		perror("Should not get here.\n");
	}

	perror("Error calculating new tristate value.\n");
	return no;
}

/*
 * calculate the new value for a non-boolean symbol given a diagnosis and an fexpr
 */
static const char * calculate_new_string_value(struct fexpr *e, struct fexpr_list *diagnosis)
{
	assert(sym_is_nonboolean(e->sym));

	/* if assumption was false before, this is the new value because only 1 variable can be true */
	if (e->assumption == false)
		return str_get(&e->nb_val);

	/* a diagnosis always contains 2 variables for the same non-boolean symbol
	* one is set to true, the other to false
	* otherwise you'd set 2 variables to true, which is not allowed */
	struct fexpr_node *node;
	struct fexpr *e2;
	fexpr_list_for_each(node, diagnosis) {
		e2 = node->elem;

		/* not interested in other symbols or the same fexpr */
		if (e->sym != e2->sym || e->satval == e2->satval)
			continue;

		return str_get(&e2->nb_val);
	}

	perror("Error calculating new string value.\n");
	return "";
}
