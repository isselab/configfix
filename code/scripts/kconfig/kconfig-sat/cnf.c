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

static void unfold_cnf_clause(struct fexpr *e);
static void build_cnf_tseytin(struct fexpr *e);
static void build_cnf_tseytin_util(struct fexpr *e, struct fexpr *t);
static void build_cnf_tseytin_and(struct fexpr *e);
static void build_cnf_tseytin_and_util(struct fexpr *e, struct fexpr *t);
static void build_cnf_tseytin_or(struct fexpr *e);
static void build_cnf_tseytin_or_util(struct fexpr *e, struct fexpr *t);
static void build_cnf_tseytin_not(struct fexpr *e);
static void build_cnf_tseytin_not_util(struct fexpr *e, struct fexpr *t);

static struct fexpr * get_fexpr_tseytin(struct fexpr *e);

static PicoSAT *pico;

/*
 * construct the CNF-clauses from the constraints
 */
void construct_cnf_clauses(PicoSAT *p)
{
	pico = p;
	unsigned int i, j;
	struct symbol *sym;
	
	/* adding unit-clauses for constants */
	sat_add_clause(2, pico, -(const_false->satval));
	sat_add_clause(2, pico, const_true->satval);
	
// 	printf("\n");

	for_all_symbols(i, sym) {
		if (sym->type == S_UNKNOWN) continue;
		
// 		print_sym_name(sym);
// 		print_sym_constraint(sym);
		
		struct fexpr *e;
		for (j = 0; j < sym->constraints->arr->len; j++) {
			e = g_array_index(sym->constraints->arr, struct fexpr *, j);
			
// 			print_fexpr("e:", e, -1);
			
			if (fexpr_is_cnf(e)) {
// 				print_fexpr("CNF:", e, -1);
				unfold_cnf_clause(e);
			} else {
// 				print_fexpr("!Not CNF:", e, -1);
				build_cnf_tseytin(e);
			}
		}
	}
}

/*
 * check, if a fexpr is in CNF
 */
bool fexpr_is_cnf(struct fexpr *e)
{
	if (!e) return false;
	
	switch (e->type) {
	case FE_SYMBOL:
	case FE_TRUE:
	case FE_FALSE:
	case FE_NONBOOL:
	case FE_SELECT:
	case FE_CHOICE:
		return true;
	case FE_AND:
		return false;
	case FE_OR:
		return fexpr_is_cnf(e->left) && fexpr_is_cnf(e->right);
	case FE_NOT:
		return fexpr_is_symbol(e->left);
	default:
		perror("Not in CNF, FE_EQUALS.");
		return false;
	}
}

/*
 * helper function to add an expression to a CNF-clause
 */
static void unfold_cnf_clause_util(GArray *arr, struct fexpr *e)
{
	switch (e->type) {
	case FE_SYMBOL:
	case FE_TRUE:
	case FE_FALSE:
	case FE_NONBOOL:
	case FE_SELECT:
	case FE_CHOICE:
// 		picosat_add(pico, e->satval);
		g_array_add_ints(2, arr, e->satval);
		break;
	case FE_OR:
		unfold_cnf_clause_util(arr, e->left);
		unfold_cnf_clause_util(arr, e->right);
		break;
	case FE_NOT:
// 		picosat_add(pico, -(e->left->satval));
		g_array_add_ints(2, arr, -(e->left->satval));
		break;
	default:
		perror("Not in CNF, FE_EQUALS.");
	}
}

/*
 * extract the variables from a fexpr in CNF
 */
static void unfold_cnf_clause(struct fexpr *e)
{
	assert(fexpr_is_cnf(e));

	GArray *arr = g_array_new(false, false, sizeof(int *));
	
	unfold_cnf_clause_util(arr, e);

	sat_add_clause_garray(pico, arr);
}

/*
 * build CNF-clauses for a fexpr not in CNF
 */
static void build_cnf_tseytin(struct fexpr *e)
{
	assert(!fexpr_is_cnf(e));
	
// 	print_fexpr("e:", e, -1);

	switch (e->type) {
	case FE_AND:
		build_cnf_tseytin_and(e);
		break;
	case FE_OR:
		build_cnf_tseytin_or(e);
		break;
	case FE_NOT:
		build_cnf_tseytin_not(e);
		break;
	default:
		perror("Expression not a propositional logic formula. root.");
	}
}

/*
 * build CNF-clauses for a fexpr of type AND
 */
static void build_cnf_tseytin_and(struct fexpr *e)
{
	struct fexpr *t1, *t2;
	
	/* set left side */
	if (fexpr_is_symbol_or_neg_atom(e->left)) {
		t1 = e->left;
	} else {
		t1 = create_tmpsatvar();
		build_cnf_tseytin_util(e->left, t1);
	}
	
	/* set right side */
	if (fexpr_is_symbol_or_neg_atom(e->right)) {
		t2 = e->right;
	} else {
		t2 = create_tmpsatvar();
		build_cnf_tseytin_util(e->right, t2);
	}
	
	int a = t1->type == FE_NOT ? -(t1->left->satval) : t1->satval;
	int b = t2->type == FE_NOT ? -(t2->left->satval) : t2->satval;
	
// 	picosat_add_arg(pico, a, 0);
	sat_add_clause(2, pico, a);
// 	picosat_add_arg(pico, b, 0);
	sat_add_clause(2, pico, b);
	
// 	printf("\nWARNING!!\nCNF-CLAUSE JUST AND!!\n");
// 	print_fexpr("e:", e, -1);
}

/*
 * build CNF-clauses for a fexpr of type OR
 */
static void build_cnf_tseytin_or(struct fexpr *e)
{
	struct fexpr *t1, *t2;
	
	/* set left side */
	if (fexpr_is_symbol_or_neg_atom(e->left)) {
		t1 = e->left;
	} else {
		t1 = create_tmpsatvar();
		build_cnf_tseytin_util(e->left, t1);
	}
	
	/* set right side */
	if (fexpr_is_symbol_or_neg_atom(e->right)) {
		t2 = e->right;
	} else {
		t2 = create_tmpsatvar();
		build_cnf_tseytin_util(e->right, t2);
	}
	
	int a = t1->type == FE_NOT ? -(t1->left->satval) : t1->satval;
	int b = t2->type == FE_NOT ? -(t2->left->satval) : t2->satval;
	
// 	picosat_add_arg(pico, a, b, 0);
	sat_add_clause(3, pico, a, b);
}

/*
 * build CNF-clauses for a fexpr of type OR
 */
static void build_cnf_tseytin_not(struct fexpr *e)
{
	struct fexpr *t1;
	
	/* set left side */
	if (fexpr_is_symbol_or_neg_atom(e->left)) {
		t1 = e->left;
	} else {
		t1 = create_tmpsatvar();
		build_cnf_tseytin_util(e->left, t1);
	}
	
	int a = t1->type == FE_NOT ? -(t1->left->satval) : t1->satval;
	
// 	picosat_add_arg(pico, a, 0);
	sat_add_clause(2, pico, a);
	
	printf("\nWARNING!!\nCNF-CLAUSE JUST NOT!!\n");
}

/*
 * helper switch for Tseytin util-functions
 */
static void build_cnf_tseytin_util(struct fexpr *e, struct fexpr *t)
{
	switch (e->type) {
	case FE_AND:
		build_cnf_tseytin_and_util(e, t);
		break;
	case FE_OR:
		build_cnf_tseytin_or_util(e, t);
		break;
	case FE_NOT:
		build_cnf_tseytin_not_util(e, t);
		break;
	default:
		perror("Expression not a propositional logic formula.");
	}
}

/*
 * build Tseytin CNF-clauses for a fexpr of type AND
 */
static void build_cnf_tseytin_and_util(struct fexpr *e, struct fexpr *t)
{
	assert(e->type == FE_AND);
	
	struct fexpr *left = get_fexpr_tseytin(e->left);
	struct fexpr *right = get_fexpr_tseytin(e->right);
	
// 	print_fexpr("Left:", left, -1);
// 	print_fexpr("Right:", right, -1);
	
	int a = left->type == FE_NOT ? -(left->left->satval) : left->satval;
	int b = right->type == FE_NOT ? -(right->left->satval) : right->satval;
	int c = t->satval;
	
	/* -A v -B v C */
// 	picosat_add_arg(pico, -a, -b, c, 0);
	sat_add_clause(4, pico, -a, -b, c);
	/* A v -C */
// 	picosat_add_arg(pico, a, -c, 0);
	sat_add_clause(3, pico, a, -c);
	/* B v -C */
// 	picosat_add_arg(pico, b, -c, 0);
	sat_add_clause(3, pico, b, -c);
}

/*
 * build Tseytin CNF-clauses for a fexpr of type OR
 */
static void build_cnf_tseytin_or_util(struct fexpr *e, struct fexpr *t)
{
	assert(e->type == FE_OR);

	struct fexpr *left = get_fexpr_tseytin(e->left);
	struct fexpr *right = get_fexpr_tseytin(e->right);
	
// 	print_fexpr("Left:", left, -1);
// 	print_fexpr("Right:", right, -1);
	
	int a = left->type == FE_NOT ? -(left->left->satval) : left->satval;
	int b = right->type == FE_NOT ? -(right->left->satval) : right->satval;
	int c = t->satval;
    
	/* A v B v -C */
// 	picosat_add_arg(pico, a, b, -c, 0);
	sat_add_clause(4, pico, a, b, -c);
	/* -A v C */
// 	picosat_add_arg(pico, -a, c, 0);
	sat_add_clause(3, pico, -a, c);
	/* -B v C */
// 	picosat_add_arg(pico, -b, c, 0);
	sat_add_clause(3, pico, -b, c);
}

/*
 * build Tseytin CNF-clauses for a fexpr of type NOT
 */
static void build_cnf_tseytin_not_util(struct fexpr *e, struct fexpr *t)
{
	assert(e->type == FE_NOT);

	struct fexpr *left = get_fexpr_tseytin(e->left);
	
// 	print_fexpr("Left:", left, -1);
	
	int a = left->type == FE_NOT ? -(left->left->satval) : left->satval;
	int c = t->satval;
	
	/* -A v -C */
// 	picosat_add_arg(pico, -a, -c, 0);
	sat_add_clause(3, pico, -a, -c);
	/* A v C */
// 	picosat_add_arg(pico, a, c, 0);
	sat_add_clause(3, pico, a, c);
}

/*
 * return the SAT-variable/fexpr for a fexpr of type NOT.
 * If it is a symbol/atom, return that. Otherwise return the auxiliary SAT-variable.
 */
static struct fexpr * get_fexpr_tseytin_not(struct fexpr *e)
{
	if (fexpr_is_symbol_or_neg_atom(e))
		return e;
	
	struct fexpr *t = create_tmpsatvar();
	build_cnf_tseytin_not_util(e, t);
	
	return t;
}

/*
 * return the SAT-variable/fexpr needed for the CNF-clause.
 * If it is a symbol/atom, return that. Otherwise return the auxiliary SAT-variable.
 */
static struct fexpr * get_fexpr_tseytin(struct fexpr *e)
{
	if (!e) perror("Empty fexpr.");
	
// 	printf("e type: %d\n", e->type);
	
	struct fexpr *t;
	switch (e->type) {
	case FE_SYMBOL:
	case FE_TRUE:
	case FE_FALSE:
	case FE_NONBOOL:
	case FE_SELECT:
	case FE_CHOICE:
		return e;
	case FE_AND:
		t = create_tmpsatvar();
		build_cnf_tseytin_and_util(e, t);
		return t;
	case FE_OR:
		t = create_tmpsatvar();
		build_cnf_tseytin_or_util(e, t);
		return t;
	case FE_NOT:
		return get_fexpr_tseytin_not(e);
	default:
		perror("Not in CNF, FE_EQUALS.");
		return false;
	}
}

/*
 * check, if a CNF-clause is a tautology
 */
// bool cnf_is_tautology(struct cnf_clause *cl)
// {
// 	struct cnf_literal *lit, *lit2;
// 	unsigned int i, j;
// 	int curr;
// 	for (i = 0; i < cl->lits->len; i++) {
// 		lit = g_array_index(cl->lits, struct cnf_literal *, i);
// 		
// 		/* exception for the 2 unit clauses for the True/False constants */
// 		if (cl->lits->len == 1 && (lit->val == const_true->satval || lit->val == -(const_false->satval))) 
// 			return false;
// 		
// 		/* clause is tautology, if a constant evaluates to true */
// 		if (lit->val == -(const_false->satval) || lit->val == const_true->satval)
// 			return true;
// 		
// 		/* given X, check if -X is in the clause as well */
// 		TODO
// 		curr = lit->val;
// 		for (j = i + 1; j < cl->lits->len; j++) {
// 			lit2 = g_array_index(cl->lits, struct cnf_literal *, j);
// 			
// 			if (curr == -(lit2->val)) return true;
// 		}
// 	}
// 	
// 	return false;
// }
