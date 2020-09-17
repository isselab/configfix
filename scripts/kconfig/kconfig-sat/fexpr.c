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

static void create_fexpr_bool(struct symbol *sym);
static void create_fexpr_nonbool(struct symbol *sym);
static void create_fexpr_unknown(struct symbol *sym);
static void create_fexpr_choice(struct symbol *sym);

/*
 * create the fexpr for a symbol
 */
void sym_create_fexpr(struct symbol *sym)
{	
	if (sym_is_choice(sym))
		create_fexpr_choice(sym);
	else if (sym_is_boolean(sym))
		create_fexpr_bool(sym);
	else if (sym_is_nonboolean(sym))
		create_fexpr_nonbool(sym);
	else
		create_fexpr_unknown(sym);
}

/*
 * create the fexpr for symbols with reverse dependencies
 */
static void create_fexpr_selected(struct symbol *sym)
{
	/* fexpr_sel_y */
	struct fexpr *fexpr_sel_y = create_fexpr(sat_variable_nr++, FE_SELECT, sym->name);
	str_append(&fexpr_sel_y->name, "_sel_y");
	fexpr_sel_y->sym = sym;
	/* add it to satmap */
	g_hash_table_insert(satmap, &fexpr_sel_y->satval, fexpr_sel_y);
	
	sym->fexpr_sel_y = fexpr_sel_y;
	
	/* fexpr_sel_m */
	if (sym->type == S_BOOLEAN) return;
	struct fexpr *fexpr_sel_m = create_fexpr(sat_variable_nr++, FE_SELECT, sym->name);
	str_append(&fexpr_sel_m->name, "_sel_m");
	fexpr_sel_m->sym = sym;
	/* add it to satmap */
	g_hash_table_insert(satmap, &fexpr_sel_m->satval, fexpr_sel_m);
	
	sym->fexpr_sel_m = fexpr_sel_m;
}

/*
 * create the fexpr for a boolean/tristate symbol
 */
static void create_fexpr_bool(struct symbol *sym)
{
	struct fexpr *fexpr_y = create_fexpr(sat_variable_nr++, FE_SYMBOL, sym->name);
	fexpr_y->sym = sym;
	fexpr_y->tri = yes;
	/* add it to satmap */
	g_hash_table_insert(satmap, &fexpr_y->satval, fexpr_y);
	
	sym->fexpr_y = fexpr_y;
	
	struct fexpr *fexpr_m;
	if (sym->type == S_TRISTATE) {
		fexpr_m = create_fexpr(sat_variable_nr++, FE_SYMBOL, sym->name);
		str_append(&fexpr_m->name, "_MODULE");
		fexpr_m->sym = sym;
		fexpr_m->tri = mod;
		/* add it to satmap */
		g_hash_table_insert(satmap, &fexpr_m->satval, fexpr_m);
	} else {
		fexpr_m = const_false;
	}
	
	sym->fexpr_m = fexpr_m;
	
	if (sym->rev_dep.expr)
		create_fexpr_selected(sym);
}

/*
 * create the fexpr for a non-boolean symbol
 */
static void create_fexpr_nonbool(struct symbol *sym)
{
	sym->fexpr_y = NULL;
	sym->fexpr_m = NULL;
	sym->fexpr_nonbool = malloc(sizeof(struct garray_wrapper *));
	sym->fexpr_nonbool->arr = g_array_new(false, false, sizeof(struct fexpr *));

	/* default values */
	char int_values[][2] = {"n", "0", "1"};
	char hex_values[][4] = {"n", "0x0", "0x1"};
	char string_values[][9] = {"n", "", "nonempty"};
	
	int i;
	for (i = 0; i < 3; i++) {
		struct fexpr *e = create_fexpr(sat_variable_nr++, FE_NONBOOL, sym->name);
		e->sym = sym;
		str_append(&e->name, "=");
		e->nb_val = str_new();
		
		switch (sym->type) {
		case S_INT:
			str_append(&e->name, int_values[i]);
			str_append(&e->nb_val, int_values[i]);
			break;
		case S_HEX:
			str_append(&e->name, hex_values[i]);
			str_append(&e->nb_val, hex_values[i]);
			break;
		case S_STRING:
			str_append(&e->name, string_values[i]);
			str_append(&e->nb_val, string_values[i]);
			break;
		default:
			break;
		}

		g_array_append_val(sym->fexpr_nonbool->arr, e);

		/* add it to satmap */
		g_hash_table_insert(satmap, &e->satval, e);
	}
}


/*
 * set fexpr_y and fexpr_m simply to False
 */
static void create_fexpr_unknown(struct symbol *sym)
{
	sym->fexpr_y = const_false;
	sym->fexpr_m = const_false;
}


/*
 * create the fexpr for a choice symbol
 */
static void create_fexpr_choice(struct symbol *sym)
{
	assert(sym_is_boolean(sym));
	
	struct property *prompt = sym_get_prompt(sym);
	assert(prompt);
	
	char *name = strdup(prompt->text);
	
	//printf("\nChoice: %s, prop_type %s\n", sym->name, prompt->text);
	struct fexpr *fexpr_y = create_fexpr(sat_variable_nr++, FE_CHOICE, name);
	fexpr_y->sym = sym;
	fexpr_y->tri = yes;
	/* add it to satmap */
	g_hash_table_insert(satmap, &fexpr_y->satval, fexpr_y);
	
	sym->fexpr_y = fexpr_y;
	
	struct fexpr *fexpr_m;
	if (sym->type == S_TRISTATE) {
		fexpr_m = create_fexpr(sat_variable_nr++, FE_CHOICE, name);
		str_append(&fexpr_m->name, "_MODULE");
		fexpr_m->sym = sym;
		fexpr_m->tri = mod;
		/* add it to satmap */
		g_hash_table_insert(satmap, &fexpr_m->satval, fexpr_m);
	} else {
// 		printf("is NOT tristate\n");
		fexpr_m = const_false;
	}
	sym->fexpr_m = fexpr_m;
}


/*
 * calculate, when k_expr will evaluate to yes or mod
 */
struct fexpr * calculate_fexpr_both(struct k_expr *e)
{
	return can_evaluate_to_mod(e) ? fexpr_or(calculate_fexpr_m(e), calculate_fexpr_y(e)) : calculate_fexpr_y(e);
}


/*
 * calculate, when k_expr will evaluate to yes
 */
struct fexpr * calculate_fexpr_y(struct k_expr *e)
{
	if (!e) return NULL;

	switch (e->type) {
	case KE_SYMBOL:
		return e->sym->fexpr_y;
	case KE_AND:
		return calculate_fexpr_y_and(e->left, e->right);
	case KE_OR:
		return calculate_fexpr_y_or(e->left, e->right);
	case KE_NOT:
		return calculate_fexpr_y_not(e->left);
	case KE_EQUAL:
		return calculate_fexpr_y_equals(e);
	case KE_UNEQUAL:
		return calculate_fexpr_y_unequals(e);
	case KE_CONST_FALSE:
		return const_false;
	case KE_CONST_TRUE:
		return const_true;
	}
	
	return NULL;
}

/*
 * calculate, when k_expr will evaluate to mod
 */
struct fexpr * calculate_fexpr_m(struct k_expr *e)
{
	if (!e) return NULL;
	
	if (!can_evaluate_to_mod(e)) return const_false;

	switch (e->type) {
	case KE_SYMBOL:
		return e->sym->fexpr_m;
	case KE_AND:
		return calculate_fexpr_m_and(e->left, e->right);
	case KE_OR:
		return calculate_fexpr_m_or(e->left, e->right);
	case KE_NOT:
		return calculate_fexpr_m_not(e->left);
	default:
		return const_false;
	}
	
	return NULL;
}

/*
 * calculate, when k_expr of type AND will evaluate to yes
 * A && B
 */
struct fexpr * calculate_fexpr_y_and(struct k_expr *a, struct k_expr *b)
{
	return fexpr_and(calculate_fexpr_y(a), calculate_fexpr_y(b));
}

/*
 * calculate, when k_expr of type AND will evaluate to mod
 * (A || A_m) && (B || B_m) && !(A && B)
 */
struct fexpr * calculate_fexpr_m_and(struct k_expr *a, struct k_expr *b)
{
	struct fexpr *topright = fexpr_not(fexpr_and(calculate_fexpr_y(a), calculate_fexpr_y(b)));
	struct fexpr *ll_left = fexpr_or(calculate_fexpr_y(a), calculate_fexpr_m(a));
	struct fexpr *ll_right = fexpr_or(calculate_fexpr_y(b), calculate_fexpr_m(b));
	struct fexpr *topleft = fexpr_and(ll_left, ll_right);

	return fexpr_and(topleft, topright);
}

/*
 * calculate, when k_expr of type OR will evaluate to yes
 * A || B
 */
struct fexpr * calculate_fexpr_y_or(struct k_expr *a, struct k_expr *b)
{
	return fexpr_or(calculate_fexpr_y(a), calculate_fexpr_y(b));
}

/*
 * calculate, when k_expr of type OR will evaluate to mod
 * (A_m || B_m) && !A && !B
 */
struct fexpr * calculate_fexpr_m_or(struct k_expr *a, struct k_expr *b)
{
	struct fexpr *topright = fexpr_not(calculate_fexpr_y(b));
	struct fexpr *lowerleft = fexpr_or(calculate_fexpr_m(a), calculate_fexpr_m(b));
	struct fexpr *topleft = fexpr_and(lowerleft, fexpr_not(calculate_fexpr_y(a)));
	
	return fexpr_and(topleft, topright);
}

/*
 * calculate, when k_expr of type NOT will evaluate to yes
 * !(A || A_m)
 */
struct fexpr * calculate_fexpr_y_not(struct k_expr *a)
{
	return fexpr_not(fexpr_or(calculate_fexpr_y(a), calculate_fexpr_m(a)));
}

/*
 * calculate, when k_expr of type NOT will evaluate to mod
 * A_m
 */
struct fexpr * calculate_fexpr_m_not(struct k_expr *a)
{
	return calculate_fexpr_m(a);
}

static struct fexpr * equiv_fexpr(struct fexpr *a, struct fexpr *b)
{
	struct fexpr *yes = fexpr_and(a, b);
	struct fexpr *not = fexpr_and(fexpr_not(a), fexpr_not(b));
	
	return fexpr_or(yes, not);
}

/*
 * create the fexpr of a non-boolean symbol for a specific value
 */
struct fexpr * sym_create_nonbool_fexpr(struct symbol *sym, char *value)
{
	struct fexpr *e = sym_get_nonbool_fexpr(sym, value);
	
	/* fexpr already exists */
	if (e != NULL) return e;
	
	e = create_fexpr(sat_variable_nr++, FE_NONBOOL, sym->name);
	e->sym = sym;
	str_append(&e->name, "=");
	str_append(&e->name, value);
	e->nb_val = str_new();
	str_append(&e->nb_val, value);
	
	/* add it to the sat-map */
	g_hash_table_insert(satmap, &e->satval, e);
	
	/* add it to the symbol's list */
	g_array_append_val(sym->fexpr_nonbool->arr, e);
	
	return e;
}

/*
 * return the fexpr of a non-boolean symbol for a specific value, NULL if non-existent
 */
struct fexpr * sym_get_nonbool_fexpr(struct symbol *sym, char *value)
{
	struct fexpr *e;
	int i;
	for (i = 0; i < sym->fexpr_nonbool->arr->len; i++) {
		e = g_array_index(sym->fexpr_nonbool->arr, struct fexpr *, i);
		if (strcmp(str_get(&e->nb_val), value) == 0)
			return e;
	}
	
	return NULL;
}

/*
 * return the fexpr of a non-boolean symbol for a specific value, if it exists
 * otherwise create it
 */
struct fexpr * sym_get_or_create_nonbool_fexpr(struct symbol *sym, char *value)
{
	struct fexpr *e = sym_get_nonbool_fexpr(sym, value);
	
	if (e != NULL)
		return e;
	else
		return sym_create_nonbool_fexpr(sym, value);
}

/*
 * calculate, when k_expr of type EQUAL will evaluate to yes
 * (A=B) && (A_m=B_m)
 */
struct fexpr * calculate_fexpr_y_equals(struct k_expr *a)
{
	/* comparing 2 tristate constants */
	if (is_tristate_constant(a->eqsym) && is_tristate_constant(a->eqvalue))
		return a->eqsym == a->eqvalue ? const_true : const_false;
	
	/* comparing 2 nonboolean constants */
	if (a->eqsym->type == S_UNKNOWN && a->eqvalue->type == S_UNKNOWN) {
		return strcmp(a->eqsym->name, a->eqvalue->name) == 0 ? const_true : const_false;
	}
	
	/* comparing 2 boolean/tristate incl. yes/mod/no constants */
	if (sym_is_bool_or_triconst(a->eqsym) && sym_is_bool_or_triconst(a->eqvalue)) {
		struct fexpr *yes = equiv_fexpr(a->eqsym->fexpr_y, a->eqvalue->fexpr_y);
		struct fexpr *mod = equiv_fexpr(a->eqsym->fexpr_m, a->eqvalue->fexpr_m);
		
		return fexpr_and(yes, mod);
	}
	
	/* comparing nonboolean with a constant */
	if (sym_is_nonboolean(a->eqsym) && a->eqvalue->type == S_UNKNOWN) {
		return sym_get_or_create_nonbool_fexpr(a->eqsym, a->eqvalue->name);
	}
	if (a->eqsym->type == S_UNKNOWN && sym_is_nonboolean(a->eqvalue)) {
		return sym_get_or_create_nonbool_fexpr(a->eqvalue, a->eqsym->name);
	}
	
	/* comparing nonboolean with tristate constant, will never be true */
	if (sym_is_nonboolean(a->eqsym) && is_tristate_constant(a->eqvalue))
		return const_false;
	if (is_tristate_constant(a->eqsym) && sym_is_nonboolean(a->eqvalue))
		return const_false;
	
	/* comparing 2 nonboolean symbols */
	// TODO
	
	/* comparing boolean item with nonboolean constant, will never be true */
	// TODO
	
	return const_false;
}

/*
 * transform an UNEQUAL into a Not(EQUAL)
 */
struct fexpr * calculate_fexpr_y_unequals(struct k_expr *a)
{
	return fexpr_not(calculate_fexpr_y_equals(a));
}

/*
 * macro to create a fexpr of type AND
 */
struct fexpr * fexpr_and(struct fexpr *a, struct fexpr *b)
{
	if (a == const_false || b == const_false)
		return const_false;
	
	if (a == const_true)
		return b;
	if (b == const_true)
		return a;
	
	struct fexpr *fe = malloc(sizeof(struct fexpr));
	fe->type = FE_AND;
	fe->left = a;
	fe->right = b;
	
	return fe;
}

/*
 * macro to create a fexpr of type OR
 */
struct fexpr * fexpr_or(struct fexpr *a, struct fexpr *b)
{	
	if (a == const_false)
		return b;
	if (b == const_false)
		return a;
	
	if (a == const_true || b == const_true)
		return const_true;
	
	struct fexpr *fe = malloc(sizeof(struct fexpr));
	fe->type = FE_OR;
	fe->left = a;
	fe->right = b;
	
	return fe;
}

/*
 * macro to create a fexpr of type NOT
 */
struct fexpr * fexpr_not(struct fexpr *a)
{
	if (a == const_false)
		return const_true;
	if (a == const_true)
		return const_false;
	
	struct fexpr *fe = malloc(sizeof(struct fexpr));
	fe->type = FE_NOT;
	fe->left = a;

	return fe;
}

/*
 * return fexpr_both for a symbol
 */
struct fexpr * sym_get_fexpr_both(struct symbol *sym)
{
	return sym->type == S_TRISTATE ? fexpr_or(sym->fexpr_m, sym->fexpr_y) : sym->fexpr_y;
}

/*
 * return fexpr_sel_y for a symbol
 */
struct fexpr * sym_get_fexpr_sel_y(struct symbol *sym)
{
	if (!sym->rev_dep.expr)
		return const_false;
	
	return sym->fexpr_sel_y;
}

/*
 * return fexpr_sel_m for a symbol
 */
struct fexpr * sym_get_fexpr_sel_m(struct symbol *sym)
{
	if (!sym->rev_dep.expr || sym->type == S_TRISTATE)
		return const_false;
	
	return sym->fexpr_sel_m;
}

/*
 * return fexpr_sel_both for a symbol
 */
struct fexpr * sym_get_fexpr_sel_both(struct symbol *sym)
{
	if (!sym->rev_dep.expr)
		return const_false;
	
	return sym->type == S_TRISTATE ? fexpr_or(sym->fexpr_sel_m, sym->fexpr_sel_y) : sym->fexpr_sel_y;
}

/*
 * macro to construct a fexpr for "A implies B"
 */
struct fexpr * implies(struct fexpr *a, struct fexpr *b)
{
	return fexpr_or(fexpr_not(a), b);
}

/*
 * check, if the fexpr is a symbol, a True/False-constant, a literal symbolizing a non-boolean or a choice symbol
 */
bool fexpr_is_symbol(struct fexpr *e)
{
	return e->type == FE_SYMBOL || e->type == FE_FALSE || e->type == FE_TRUE || e->type == FE_NONBOOL || e->type == FE_CHOICE || e->type == FE_SELECT;
}

/*
 * check, if the fexpr is a symbol, a True/False-constant, a literal symbolizing a non-boolean, a choice symbol or NOT
 */
bool fexpr_is_symbol_or_not(struct fexpr *e)
{
	return fexpr_is_symbol(e) || e->type == FE_NOT;
}

/*
 * check, if a fexpr is a symbol, a True/False-constant, a literal symbolizing a non-boolean, a choice symbol or a negated "symbol"
 */
bool fexpr_is_symbol_or_neg_atom(struct fexpr *e)
{
	return fexpr_is_symbol(e) || (e->type == FE_NOT && fexpr_is_symbol(e->left));
}

/*
 * convert a fexpr into negation normal form
 */
static bool convert_fexpr_to_nnf_util(struct fexpr *e)
{
	if (!e) return false;
	
	switch (e->type) {
	case FE_SYMBOL:
	case FE_CHOICE:
	case FE_FALSE:
	case FE_TRUE:
	case FE_SELECT:
	case FE_TMPSATVAR:
		return false;
	case FE_AND:
	case FE_OR:
		return convert_fexpr_to_nnf_util(e->left) || convert_fexpr_to_nnf_util(e->right);
	case FE_NOT:
		/* De Morgan */
		if (e->left->type == FE_AND) {
			struct fexpr *left = malloc(sizeof(struct fexpr));
			left->type = FE_NOT;
			left->left = e->left->left;
			struct fexpr *right = malloc(sizeof(struct fexpr));
			right->type = FE_NOT;
			right->left = e->left->right;
			
			e->type = FE_OR;
			e->left = left;
			e->right = right;
			
			return true;
		} else if (e->left->type == FE_OR) {
			struct fexpr *left = malloc(sizeof(struct fexpr));
			left->type = FE_NOT;
			left->left = e->left->left;
			struct fexpr *right = malloc(sizeof(struct fexpr));
			right->type = FE_NOT;
			right->left = e->left->right;
			
			e->type = FE_AND;
			e->left = left;
			e->right = right;
			
			return true;
		} 
		/* double negation */
		else if (e->left->type == FE_NOT) {
			if (e->left->left->type == FE_SYMBOL || e->left->left->type == FE_FALSE) {
				e->type = FE_SYMBOL;
				//str_free(&e->name);
				e->name = str_new();
				str_append(&e->name, str_get(&e->left->left->name));
				e->satval = e->left->left->satval;
				e->sym = e->left->left->sym;
				// TODO double check that
				/* if (e->left->left->tristate)
					e->tristate = e->left->left->tristate;
				*/
				return true;
			} else if (e->left->left->type == FE_AND || e->left->left->type == FE_OR) {
				e->type = e->left->left->type;
				e->right = e->left->left->right;
				e->left = e->left->left->left;
				return true;
			} else if (e->left->left->type == FE_EQUALS) {
				e->type = e->left->left->type;
				e->eqsym = e->left->left->eqsym;
				e->eqvalue = e->left->left->eqvalue;
				return true;
			}
			return false;
		}
		return false;
	case FE_EQUALS:
	case FE_NONBOOL:
		return false;
	}
	return false;
}
void convert_fexpr_to_nnf(struct fexpr *e) {
	while (convert_fexpr_to_nnf_util(e))
		;
}

/* 
 * convert a fexpr from negation normal form into conjunctive normal form
 */
static bool convert_nnf_to_cnf_util(struct fexpr *e)
{
// 	print_fexpr("fe:", e, -1);
// 	printf("Type: %d\n", e->type);
// 	getchar();
	
	switch (e->type) {
	case FE_SYMBOL:
	case FE_CHOICE:
	case FE_FALSE:
	case FE_TRUE:
	case FE_SELECT:
	case FE_TMPSATVAR:
		return false;
	case FE_AND:
		return convert_nnf_to_cnf_util(e->left) || convert_nnf_to_cnf_util(e->right);
	case FE_OR:
		if (e->left->type == FE_AND) {
			e->type = FE_AND;
			struct fexpr *fe_left = malloc(sizeof(struct fexpr));
			struct fexpr *fe_right = malloc(sizeof(struct fexpr));
			
			fe_left->type = FE_OR;
			fe_left->left = e->left->left;
			fe_left->right = e->right;
	
			fe_right->type = FE_OR;
			fe_right->left = e->left->right;
			fe_right->right = e->right;
			
			e->left = fe_left;
			e->right = fe_right;
			
			return true;
		}
		else if (e->right->type == FE_AND) {
			e->type = FE_AND;
			struct fexpr *fe_left = malloc(sizeof(struct fexpr));
			struct fexpr *fe_right = malloc(sizeof(struct fexpr));
			
			fe_left->type = FE_OR;
			fe_left->left = e->left;
			fe_left->right = e->right->left;
	
			fe_right->type = FE_OR;
			fe_right->left = e->left;
			fe_right->right = e->right->right;
			
			e->left = fe_left;
			e->right = fe_right;
			
			return true;
		}
		return convert_nnf_to_cnf_util(e->left) || convert_nnf_to_cnf_util(e->right);
	case FE_NOT:
	case FE_EQUALS:
	case FE_NONBOOL:
		return false;
	}
	return false;
}
void convert_fexpr_to_cnf(struct fexpr *e) {
// 	printf("\n");
// 	print_fexpr("fexpr:", e, -1);
// 	printf("Type: %d\n", e->type);
	while (convert_nnf_to_cnf_util(e))
		;
}

// /*
//  * helper function to add an expression to a CNF-clause
//  */
// static void add_cnf_clause(struct fexpr *e, struct cnf_clause *cl)
// {
// 	if (!e || !cl) return;
// 	
// 	if (e->left->type == FE_SYMBOL || e->left->type == FE_FALSE || e->left->type == FE_TRUE || e->left->type == FE_NONBOOL) {
// 		add_literal_to_clause(cl, e->left->satval);
// 	} else if (e->left->type == FE_NOT) {
// 		add_literal_to_clause(cl, -(e->left->left->satval));
// 	} else if (e->left->type == FE_OR) {
// 		add_cnf_clause(e->left, cl);
// 	}
// 	
// 	if (e->right->type == FE_SYMBOL || e->right->type == FE_FALSE || e->right->type == FE_TRUE || e->right->type == FE_NONBOOL) {
// 		add_literal_to_clause(cl, e->right->satval);
// 	} else if (e->right->type == FE_NOT) {
// 		add_literal_to_clause(cl, -(e->right->left->satval));
// 	} else if (e->right->type == FE_OR) {
// 		add_cnf_clause(e->right, cl);
// 	}
// }
// 
// /*
//  * extract the CNF-clauses from an fexpr in CNF
//  */
// void unfold_cnf_clause(struct fexpr *e)
// {
// 	if (!e) return;
// 	
// 	struct gstr empty_string = str_new();
// 	struct cnf_clause *cl;
// 	
// 	switch (e->type) {
// 	case FE_AND:
// 		unfold_cnf_clause(e->left);
// 		unfold_cnf_clause(e->right);
// 		break;
// 	case FE_OR:
// 		add_cnf_clause(e, build_cnf_clause(&empty_string, 0));
// 		break;
// 	case FE_NOT:
// 		cl = build_cnf_clause(&empty_string, 0);
// 		add_literal_to_clause(cl, -(e->left->satval));
// 		break;
// 	default:
// 		TODO
// 		print_fexpr(e, -1);
// 		printf("type %d\n", e->type);
// 		break;
// 	}
// }
