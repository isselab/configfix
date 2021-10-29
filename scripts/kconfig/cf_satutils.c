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

static void unfold_cnf_clause(struct pexpr *e);
static void build_cnf_tseytin(struct pexpr *e);

static void build_cnf_tseytin_top_and(struct pexpr *e);
static void build_cnf_tseytin_top_or(struct pexpr *e);

static void build_cnf_tseytin_tmp(struct pexpr *e, struct fexpr *t);
static void build_cnf_tseytin_and(struct pexpr *e, struct fexpr *t);
static void build_cnf_tseytin_or(struct pexpr *e, struct fexpr *t);
static int pexpr_satval(struct pexpr *e);

static PicoSAT *pico;

/* -------------------------------------- */

/*
 * initialize PicoSAT
 */
PicoSAT * initialize_picosat(void)
{
	printd("\nInitializing PicoSAT...");
	PicoSAT *pico = picosat_init();
	picosat_enable_trace_generation(pico);
	printd("done.\n");

	return pico;
}

/*
 * construct the CNF-clauses from the constraints
 */
void construct_cnf_clauses(PicoSAT *p)
{
	pico = p;
	unsigned int i;
	struct symbol *sym;

	/* adding unit-clauses for constants */
	sat_add_clause(2, pico, -(const_false->satval));
	sat_add_clause(2, pico, const_true->satval);

	for_all_symbols(i, sym) {
		if (sym->type == S_UNKNOWN)
			continue;

		struct pexpr_node *node;
		pexpr_list_for_each(node, sym->constraints) {
			if (pexpr_is_cnf(node->elem)) {
				unfold_cnf_clause(node->elem);
				picosat_add(pico, 0);
			} else {
				build_cnf_tseytin(node->elem);
			}

		}
	}
}

/*
 * helper function to add an expression to a CNF-clause
 */
static void unfold_cnf_clause(struct pexpr *e)
{
	switch (e->type) {
	case PE_SYMBOL:
		picosat_add(pico, e->left.fexpr->satval);
		break;
	case PE_OR:
		unfold_cnf_clause(e->left.pexpr);
		unfold_cnf_clause(e->right.pexpr);
		break;
	case PE_NOT:
		picosat_add(pico, -(e->left.pexpr->left.fexpr->satval));
		break;
	default:
		perror("Not in CNF, FE_EQUALS.");
	}
}

/*
 * build CNF-clauses for a pexpr not in CNF
 */
static void build_cnf_tseytin(struct pexpr *e)
{
	switch (e->type) {
	case PE_AND:
		build_cnf_tseytin_top_and(e);
		break;
	case PE_OR:
		build_cnf_tseytin_top_or(e);
		break;
	default:
		perror("Expression not a propositional logic formula. root.");
	}
}

/*
 * split up a pexpr of type AND as both sides must be satisfied
 */
static void build_cnf_tseytin_top_and(struct pexpr *e)
{
	if (pexpr_is_cnf(e->left.pexpr))
		unfold_cnf_clause(e->left.pexpr);
	else
		build_cnf_tseytin(e->left.pexpr);

	if (pexpr_is_cnf(e->right.pexpr))
		unfold_cnf_clause(e->right.pexpr);
	else
		build_cnf_tseytin(e->right.pexpr);

}

static void build_cnf_tseytin_top_or(struct pexpr *e)
{
	struct fexpr *t1 = NULL, *t2 = NULL;
	int a, b;

	/* set left side */
	if (pexpr_is_symbol(e->left.pexpr)) {
		a = pexpr_satval(e->left.pexpr);
	} else {
		t1 = create_tmpsatvar();
		a = t1->satval;
	}

	/* set right side */
	if (pexpr_is_symbol(e->right.pexpr)) {
		b = pexpr_satval(e->right.pexpr);
	} else {
		t2 = create_tmpsatvar();
		b = t2->satval;
	}

	/* A v B */
	sat_add_clause(3, pico, a, b);

	/* traverse down the tree to build more constraints if needed */
	if (!pexpr_is_symbol(e->left.pexpr)) {
		if (t1 == NULL)
			perror("t1 is NULL.");

		build_cnf_tseytin_tmp(e->left.pexpr, t1);

	}

	if (!pexpr_is_symbol(e->right.pexpr)) {
		if (t2 == NULL)
			perror("t2 is NULL.");

		build_cnf_tseytin_tmp(e->right.pexpr, t2);
	}
}

/*
 * build the sub-expressions
 */
static void build_cnf_tseytin_tmp(struct pexpr *e, struct fexpr *t)
{
	switch (e->type) {
	case PE_AND:
		build_cnf_tseytin_and(e, t);
		break;
	case PE_OR:
		build_cnf_tseytin_or(e, t);
		break;
	default:
		perror("Expression not a propositional logic formula. root.");
	}
}

/*
 * build the Tseytin sub-expressions for a pexpr of type AND
 */
static void build_cnf_tseytin_and(struct pexpr *e, struct fexpr *t)
{
	struct fexpr *t1 = NULL, *t2 = NULL;
	int a, b, c;

	/* set left side */
	if (pexpr_is_symbol(e->left.pexpr)) {
		a = pexpr_satval(e->left.pexpr);
	} else {
		t1 = create_tmpsatvar();
		a = t1->satval;
	}

	/* set right side */
	if (pexpr_is_symbol(e->right.pexpr)) {
		b = pexpr_satval(e->right.pexpr);
	} else {
		t2 = create_tmpsatvar();
		b = t2->satval;
	}

	c = t->satval;

	/* -A v -B v C */
	sat_add_clause(4, pico, -a, -b, c);
	/* A v -C */
	sat_add_clause(3, pico, a, -c);
	/* B v -C */
	sat_add_clause(3, pico, b, -c);

	/* traverse down the tree to build more constraints if needed */
	if (!pexpr_is_symbol(e->left.pexpr)) {
		if (t1 == NULL)
			perror("t1 is NULL.");

		build_cnf_tseytin_tmp(e->left.pexpr, t1);
	}
	if (!pexpr_is_symbol(e->right.pexpr)) {
		if (t2 == NULL)
			perror("t2 is NULL.");

		build_cnf_tseytin_tmp(e->right.pexpr, t2);
	}
}

/*
 * build the Tseytin sub-expressions for a pexpr of type
 */
static void build_cnf_tseytin_or(struct pexpr *e, struct fexpr *t)
{
	struct fexpr *t1 = NULL, *t2 = NULL;
	int a, b, c;

	/* set left side */
	if (pexpr_is_symbol(e->left.pexpr)) {
		a = pexpr_satval(e->left.pexpr);
	} else {
		t1 = create_tmpsatvar();
		a = t1->satval;
	}

	/* set right side */
	if (pexpr_is_symbol(e->right.pexpr)) {
		b = pexpr_satval(e->right.pexpr);
	} else {
		t2 = create_tmpsatvar();
		b = t2->satval;
	}

	c = t->satval;

	/* A v B v -C */
	sat_add_clause(4, pico, a, b, -c);
	/* -A v C */;
	sat_add_clause(3, pico, -a, c);
	/* -B v C */
	sat_add_clause(3, pico, -b, c);

	/* traverse down the tree to build more constraints if needed */
	if (!pexpr_is_symbol(e->left.pexpr)) {
		if (t1 == NULL)
			perror("t1 is NULL.");

		build_cnf_tseytin_tmp(e->left.pexpr, t1);
	}
	if (!pexpr_is_symbol(e->right.pexpr)) {
		if (t2 == NULL)
			perror("t2 is NULL.");
		build_cnf_tseytin_tmp(e->right.pexpr, t2);
	}
}

/*
 * add a clause to PicoSAT
 * First argument must be the SAT solver
 */
void sat_add_clause(int num, ...)
{
	if (num <= 1)
		return;

	va_list valist;
	int i, *lit;
	PicoSAT *pico;


	va_start(valist, num);

	pico = va_arg(valist, PicoSAT *);

	/* access all the arguments assigned to valist */
	for (i = 1; i < num; i++) {
		lit = xmalloc(sizeof(int));
		*lit = va_arg(valist, int);
		picosat_add(pico, *lit);
	}
	picosat_add(pico, 0);

	va_end(valist);
}

/*
 * return the SAT-variable for a pexpr that is a symbol
 */
static int pexpr_satval(struct pexpr *e)
{
	if (!pexpr_is_symbol(e)) {
		perror("pexpr is not a symbol.");
		return -1;
	}

	switch (e->type) {
	case PE_SYMBOL:
		return e->left.fexpr->satval;
	case PE_NOT:
		return -(e->left.pexpr->left.fexpr->satval);
	default:
		perror("Not a symbol.");
	}

	return -1;
}

/*
 * start PicoSAT
 */
void picosat_solve(PicoSAT *pico)
{
	printd("Solving SAT-problem...");

	clock_t start, end;
	double time;
	start = clock();

	int res = picosat_sat(pico, -1);

	end = clock();
	time = ((double) (end - start)) / CLOCKS_PER_SEC;
	printd("done. (%.6f secs.)\n\n", time);

	if (res == PICOSAT_SATISFIABLE) {
		printd("===> PROBLEM IS SATISFIABLE <===\n");

	} else if (res == PICOSAT_UNSATISFIABLE) {
		printd("===> PROBLEM IS UNSATISFIABLE <===\n");

		/* print unsat core */
		printd("\nPrinting unsatisfiable core:\n");
		struct fexpr *e;

		int *lit = malloc(sizeof(int));
		const int *i = picosat_failed_assumptions(pico);
		*lit = abs(*i++);

		while (*lit != 0) {
			e = &satmap[*lit];

			printd("(%d) %s <%d>\n", *lit, str_get(&e->name), e->assumption);
			*lit = abs(*i++);
		}
	}
	else {
		printd("Unknown if satisfiable.\n");
	}
}

/*
 * add assumption for a symbol to the SAT-solver
 */
void sym_add_assumption(PicoSAT *pico, struct symbol *sym)
{
	if (sym_is_boolean(sym)) {
		int tri_val = sym_get_tristate_value(sym);
		sym_add_assumption_tri(pico, sym, tri_val);
		return;
	}

	if (sym_is_nonboolean(sym)) {
		struct fexpr *e = sym->nb_vals->head->elem;

		struct fexpr_node *node;

		const char *string_val = sym_get_string_value(sym);

		if (sym->type == S_STRING && !strcmp(string_val, ""))
			return;

		/* symbol does not have a value */
		if (!sym_nonbool_has_value_set(sym)) {

			/* set value for sym=n */
			picosat_assume(pico, e->satval);
			e->assumption = true;

			struct fexpr_node *node;
			for (node = sym->nb_vals->head->next; node != NULL; node = node->next) {
				picosat_assume(pico, -(node->elem->satval));
				node->elem->assumption = false;
			}

			return;
		}

		/* symbol does have a value set */

		/* set value for sym=n */
		picosat_assume(pico, -(e->satval));
		e->assumption = false;

		/* set value for all other fexpr */
		fexpr_list_for_each(node, sym->nb_vals) {
			if (node->prev == NULL)
				continue;

			if (strcmp(str_get(&node->elem->nb_val), string_val) == 0) {
				picosat_assume(pico, node->elem->satval);
				node->elem->assumption = true;
			} else {
				picosat_assume(pico, -(node->elem->satval));
				node->elem->assumption = false;
			}
		}
	}
}

/*
 * add assumption for a boolean symbol to the SAT-solver
 */
void sym_add_assumption_tri(PicoSAT *pico, struct symbol *sym, tristate tri_val)
{
	if (sym->type == S_BOOLEAN) {
		int a = sym->fexpr_y->satval;
		switch (tri_val) {
		case no:
			picosat_assume(pico, -a);
			sym->fexpr_y->assumption = false;
			break;
		case mod:
			perror("Should not happen. Boolean symbol is set to mod.\n");
			break;
		case yes:

			picosat_assume(pico, a);
			sym->fexpr_y->assumption = true;
			break;
		}
	}
	if (sym->type == S_TRISTATE) {
		int a = sym->fexpr_y->satval;
		int a_m = sym->fexpr_m->satval;
		switch (tri_val) {
		case no:
			picosat_assume(pico, -a);
			picosat_assume(pico, -a_m);
			sym->fexpr_y->assumption = false;
			sym->fexpr_m->assumption = false;
			break;
		case mod:
			picosat_assume(pico, -a);
			picosat_assume(pico, a_m);
			sym->fexpr_y->assumption = false;
			sym->fexpr_m->assumption = true;
			break;
		case yes:
			picosat_assume(pico, a);
			picosat_assume(pico, -a_m);
			sym->fexpr_y->assumption = true;
			sym->fexpr_m->assumption = false;
			break;
		}
	}
}

/*
 * add assumptions for the symbols to be changed to the SAT solver
 */
void sym_add_assumption_sdv(PicoSAT *pico, struct sdv_list *list)
{
	struct symbol_dvalue *sdv;
	struct sdv_node *node;
	sdv_list_for_each(node, list) {
		sdv = node->elem;

		int lit_y = sdv->sym->fexpr_y->satval;

		if (sdv->sym->type == S_BOOLEAN) {
			switch (sdv->tri) {
			case yes:
				picosat_assume(pico, lit_y);
				break;
			case no:
				picosat_assume(pico, -lit_y);
				break;
			case mod:
				perror("Should not happen.\n");
			}
		} else if (sdv->sym->type == S_TRISTATE) {
			int lit_m = sdv->sym->fexpr_m->satval;
			switch (sdv->tri) {
			case yes:
				picosat_assume(pico, lit_y);
				picosat_assume(pico, -lit_m);
				break;
			case mod:
				picosat_assume(pico, -lit_y);
				picosat_assume(pico, lit_m);
				break;
			case no:
				picosat_assume(pico, -lit_y);
				picosat_assume(pico, -lit_m);
			}
		}
	}
}
