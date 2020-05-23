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

static struct k_expr * gcc_version_eval(struct expr *e);
static struct k_expr * expr_eval_unequal_bool(struct expr *e);

/*
 * parse Kconfig-file and read .config
 */
void init_config(const char *Kconfig_file)
{
	conf_parse(Kconfig_file);
	conf_read(NULL);
}

/*
 * initialize satmap and cnf_clauses
 */
void init_data(void)
{
	/* initialize array with all CNF clauses */
	cnf_clauses = g_array_new(false, false, sizeof(struct cnf_clause *));
	
	/* create hashtable with all fexpr */
	satmap = g_hash_table_new_full(
		g_int_hash, g_int_equal, //< This is an integer hash.
		NULL, //< Call "free" on the key (made with "malloc").
		free //< Call "free" on the value (made with "strdup").
	);
	
	printf("done.\n");
}

/*
 * bool-symbols have 1 variable (X), tristate-symbols have 2 variables (X, X_m)
 */
static void create_sat_variables(struct symbol *sym)
{
	sym->constraints = malloc(sizeof(struct garray_wrapper));
	sym->constraints->arr = g_array_new(false, false, sizeof(struct fexpr *));
	sym_create_fexpr(sym);
}

/*
 * assign SAT-variables to all fexpr and create the sat_map
 */
void assign_sat_variables(void)
{
	unsigned int i;
	struct symbol *sym;
	
	printf("Creating SAT-variables...");
	
	for_all_symbols(i, sym)
		create_sat_variables(sym);
	
	printf("done.\n");
}

/*
 * create True/False constants
 */
void create_constants(void)
{
	printf("Creating constants...");
	
	/* create TRUE and FALSE constants */
	const_false = malloc(sizeof(struct fexpr));
	const_false->name = str_new();
	str_append(&const_false->name, "0");
	const_false->type = FE_FALSE;
	const_false->satval = sat_variable_nr++;
	g_hash_table_insert(satmap, &const_false->satval, const_false);
	
	struct gstr tmp1 = str_new();
	str_append(&tmp1, "(#): False constant");
	
	// TODO
// 	build_cnf_clause(&tmp1, 1, -const_false->satval);

	
	const_true = malloc(sizeof(struct fexpr));
	const_true->name = str_new();
	str_append(&const_true->name, "1");
	const_true->type = FE_TRUE;
	const_true->satval = sat_variable_nr++;
	g_hash_table_insert(satmap, &const_true->satval, const_true);
	
	struct gstr tmp2 = str_new();
	str_append(&tmp2, "(#): True constant");
// 	build_cnf_clause(&tmp2, 1, const_true->satval);
	
	
	/* add fexpr of constants to tristate constants */
	symbol_yes.fexpr_y = const_true;
	symbol_yes.fexpr_m = const_false;
	
	symbol_mod.fexpr_y = const_false;
	symbol_mod.fexpr_m = const_true;
	
	symbol_no.fexpr_y = const_false;
	symbol_no.fexpr_m = const_false;
	
	/* create symbols yes/mod/no as fexpr */
	symbol_yes_fexpr = create_fexpr(0, FE_SYMBOL, "y");
	symbol_yes_fexpr->sym = &symbol_yes;
	symbol_yes_fexpr->tri = yes;
	
	symbol_mod_fexpr = create_fexpr(0, FE_SYMBOL, "m");
	symbol_mod_fexpr->sym = &symbol_mod;
	symbol_mod_fexpr->tri = mod;
	
	symbol_no_fexpr = create_fexpr(0, FE_SYMBOL, "n");
	symbol_no_fexpr->sym = &symbol_no;
	symbol_no_fexpr->tri = no;
	
	printf("done.\n");
}

/*
 *  create a fexpr
 */
struct fexpr * create_fexpr(int satval, enum fexpr_type type, char* name)
{
	struct fexpr *e = malloc(sizeof(struct fexpr));
	e->satval = satval;
	e->type = type;
	e->name = str_new();
	str_append(&e->name, name);
	
	return e;
}

/*
 * create a temporary SAT-variable
 */
struct fexpr * create_tmpsatvar(void)
{
	struct fexpr *t = create_fexpr(sat_variable_nr++, FE_TMPSATVAR, "");
	str_append(&t->name, get_tmp_var_as_char(tmp_variable_nr++));
	/* add it to satmap */
	g_hash_table_insert(satmap, &t->satval, t);
	
	return t;
}

/*
 * return a temporary SAT variable as string
 */
char * get_tmp_var_as_char(int i)
{
	char *val = malloc(sizeof(char) * 18);
	snprintf(val, 18,"T_%d", i);
	return val;
}

/*
 * return a tristate value as a char *
 */
char * tristate_get_char(tristate val)
{
	switch (val) {
	case yes:
		return "yes";
	case mod:
		return "mod";
	case no:
		return "no";
	default:
		return "";
	}
}

/*
 * check if a k_expr can evaluate to mod
 */
bool can_evaluate_to_mod(struct k_expr *e)
{
	if (!e) return false;
	
	switch (e->type) {
	case KE_SYMBOL:
		return e->sym == &symbol_mod || e->sym->type == S_TRISTATE ? true : false;
	case KE_AND:
	case KE_OR:
		return can_evaluate_to_mod(e->left) || can_evaluate_to_mod(e->right);
	case KE_NOT:
		return can_evaluate_to_mod(e->left);
	case KE_EQUAL:
	case KE_UNEQUAL:
	case KE_CONST_FALSE:
	case KE_CONST_TRUE:
		return false;
	}
	
	return false;
}

/*
 * return the constant FALSE as a k_expr
 */
struct k_expr * get_const_false_as_kexpr()
{
	struct k_expr *ke = malloc(sizeof(struct k_expr));
	ke->type = KE_CONST_FALSE;
	return ke;
}

/*
 * return the constant TRUE as a k_expr
 */
struct k_expr * get_const_true_as_kexpr()
{
	struct k_expr *ke = malloc(sizeof(struct k_expr));
	ke->type = KE_CONST_TRUE;
	return ke;
}

/*
 * given a satval, return the corresponding fexpr from satmap
 * return NULL, if key is not found
 */
struct fexpr * get_fexpr_from_satmap(int key)
{
	int *index = malloc(sizeof(*index));
	*index = abs(key);
	return (struct fexpr *) g_hash_table_lookup(satmap, index);
}

/*
 * evaluate an unequality with GCC_VERSION
 */
static struct k_expr * gcc_version_eval(struct expr* e)
{
	if (!e) get_const_false_as_kexpr();
	
	long long actual_gcc_ver, sym_gcc_ver;
	if (e->left.sym == sym_find("GCC_VERSION")) {
		actual_gcc_ver = strtoll(sym_get_string_value(e->left.sym), NULL, 10);
		sym_gcc_ver = strtoll(e->right.sym->name, NULL, 10);
	} else {
		actual_gcc_ver = strtoll(sym_get_string_value(e->right.sym), NULL, 10);
		sym_gcc_ver = strtoll(e->left.sym->name, NULL, 10);
	}
	
	switch (e->type) {
	case E_LTH:
		return actual_gcc_ver < sym_gcc_ver ? get_const_true_as_kexpr() : get_const_false_as_kexpr();
	case E_LEQ:
		return actual_gcc_ver <= sym_gcc_ver ? get_const_true_as_kexpr() : get_const_false_as_kexpr();
	case E_GTH:
		return actual_gcc_ver > sym_gcc_ver ? get_const_true_as_kexpr() : get_const_false_as_kexpr();
	case E_GEQ:
		return actual_gcc_ver >= sym_gcc_ver ? get_const_true_as_kexpr() : get_const_false_as_kexpr();
	default:
		perror("Wrong type in gcc_version_eval.");
	}
	
	return get_const_false_as_kexpr();
}

/*
 * evaluate an unequality with 2 boolean symbols
 */
static struct k_expr * expr_eval_unequal_bool(struct expr *e)
{
	if (!e) get_const_false_as_kexpr();
	
	assert(sym_is_boolean(e->left.sym));
	assert(sym_is_boolean(e->right.sym));
	
	int val_left = sym_get_tristate_value(e->left.sym);
	int val_right = sym_get_tristate_value(e->right.sym);
	
	switch (e->type) {
	case E_LTH:
		return val_left < val_right ? get_const_true_as_kexpr() : get_const_false_as_kexpr();
	case E_LEQ:
		return val_left <= val_right ? get_const_true_as_kexpr() : get_const_false_as_kexpr();
	case E_GTH:
		return val_left > val_right ? get_const_true_as_kexpr() : get_const_false_as_kexpr();
	case E_GEQ:
		return val_left >= val_right ? get_const_true_as_kexpr() : get_const_false_as_kexpr();
	default:
		perror("Wrong type in expr_eval_unequal_bool.");
	}
	
	return get_const_false_as_kexpr();
}


/*
 * parse an expr as a k_expr
 */
struct k_expr * parse_expr(struct expr *e, struct k_expr *parent)
{
	struct k_expr *ke = malloc(sizeof(struct k_expr));
	ke->parent = parent;
// 	print_expr("expr:", e, E_NONE);
// 	printf("type: %d\n", e->type);

	switch (e->type) {
	case E_SYMBOL:
		ke->type = KE_SYMBOL;
		ke->sym = e->left.sym;
		ke->tri = no;
		return ke;
	case E_AND:
		ke->type = KE_AND;
		ke->left = parse_expr(e->left.expr, ke);
		ke->right = parse_expr(e->right.expr, ke);
		return ke;
	case E_OR:
		ke->type = KE_OR;
		ke->left = parse_expr(e->left.expr, ke);
		ke->right = parse_expr(e->right.expr, ke);
		return ke;
	case E_NOT:
		ke->type = KE_NOT;
		ke->left = parse_expr(e->left.expr, ke);
		ke->right = NULL;
		return ke;
	case E_EQUAL:
		ke->type = KE_EQUAL;
		ke->eqsym = e->left.sym;
		ke->eqvalue = e->right.sym;
		return ke;
	case E_UNEQUAL:
		ke->type = KE_UNEQUAL;
		ke->eqsym = e->left.sym;
		ke->eqvalue = e->right.sym;
		return ke;
	case E_LTH:
	case E_LEQ:
	case E_GTH:
	case E_GEQ:
		// TODO
// 		print_expr("UNEQUAL:", e, 0);
		
		/* "special" hack for GCC_VERSION */
		if (expr_contains_symbol(e, sym_find("GCC_VERSION")))
			return gcc_version_eval(e);
		
		/* "special" hack for CRAMFS <= MTD */
		if (expr_contains_symbol(e, sym_find("CRAMFS")) && expr_contains_symbol(e, sym_find("MTD")))
			return expr_eval_unequal_bool(e);
		
		return get_const_false_as_kexpr();
	default:
		return NULL;
	}
}

/*
 * check, if the symbol is a tristate-constant
 */
bool is_tristate_constant(struct symbol *sym) {
	return sym == &symbol_yes || sym == &symbol_mod || sym == &symbol_no;
}

/*
 * check, if a symbol is of type boolean or tristate
 */
bool sym_is_boolean(struct symbol *sym)
{
	return sym->type == S_BOOLEAN || sym->type == S_TRISTATE;
}

/*
 * check, if a symbol is a boolean/tristate or a tristate constant
 */
bool sym_is_bool_or_triconst(struct symbol *sym)
{
	return is_tristate_constant(sym) || sym_is_boolean(sym);
}

/*
 * check, if a symbol is of type int, hex, or string
 */
bool sym_is_nonboolean(struct symbol *sym)
{
	return sym->type == S_INT || sym->type == S_HEX || sym->type == S_STRING;
}

/*
 * check, if a symbol has a prompt
 */
bool sym_has_prompt(struct symbol *sym)
{
	struct property *prop;

	for_all_prompts(sym, prop)
		return true;

	return false;
}

/*
 * return the prompt of the symbol if there is one, NULL otherwise
 */
struct property * sym_get_prompt(struct symbol *sym)
{
	struct property *prop;
	
	for_all_prompts(sym, prop)
		return prop;
	
	return NULL;
}

/*
 * return the condition for the property, True if there is none
 */
struct fexpr * prop_get_condition(struct property *prop)
{
	assert(prop != NULL);
	
	/* if there is no condition, return True */
	if (!prop->visible.expr)
		return const_true;
	
	struct k_expr *ke = parse_expr(prop->visible.expr, NULL);
	
	return calculate_fexpr_both(ke);
}

/*
 * return the name of the symbol or the prompt-text, if it is a choice symbol
 */
char * sym_get_name(struct symbol *sym)
{
	if (sym_is_choice(sym)) {
		struct property *prompt = sym_get_prompt(sym);
		assert(prompt);
		return strdup(prompt->text);
	} else {
		return sym->name;
	}
}

/*
 * add a constraint for a symbol
 */
void sym_add_constraint(struct symbol *sym, struct fexpr *constraint)
{
	if (!constraint) return;
	
	g_array_append_val(sym->constraints->arr, constraint);
}

/*
 * count the number of all constraints
 */
unsigned int count_counstraints(void)
{
	unsigned int i, c = 0;
	struct symbol *sym;
	for_all_symbols(i, sym) {
		if (sym->constraints->arr)
			c += sym->constraints->arr->len;
	}
	
	return c;
}


/*
 * print a warning about unmet dependencies
 */
void sym_warn_unmet_dep(struct symbol *sym)
{
	struct gstr gs = str_new();

	str_printf(&gs,
		   "\nWARNING: unmet direct dependencies detected for %s\n",
		   sym->name);
	str_printf(&gs,
		   "  Depends on [%c]: ",
		   sym->dir_dep.tri == mod ? 'm' : 'n');
	expr_gstr_print(sym->dir_dep.expr, &gs);
	str_printf(&gs, "\n");

	expr_gstr_print_revdep(sym->rev_dep.expr, &gs, yes,
			       "  Selected by [y]:\n");
	expr_gstr_print_revdep(sym->rev_dep.expr, &gs, mod,
			       "  Selected by [m]:\n");

	fputs(str_get(&gs), stderr);
}
