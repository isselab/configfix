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
#include <ctype.h>

#include "configfix.h"

#define SATMAP_INIT_SIZE 2

/*
 * parse Kconfig-file and read .config
 */
void init_config(const char *Kconfig_file)
{
	conf_parse(Kconfig_file);
	conf_read(NULL);
}

/*
 * initialize satmap
 */
void init_data(void)
{
	/* create hashtable with all fexpr */
	satmap = xcalloc(SATMAP_INIT_SIZE, sizeof(*satmap));
	satmap_size = SATMAP_INIT_SIZE;

	printd("done.\n");
}

/*
 * create SAT-variables for all fexpr
 */
void create_sat_variables(void)
{
	unsigned int i;
	struct symbol *sym;

	printd("Creating SAT-variables...");

	for_all_symbols(i, sym) {
		sym->constraints = pexpr_list_init();
		sym_create_fexpr(sym);
	}

	printd("done.\n");
}

/*
 * create various constants
 */
void create_constants(void)
{
	printd("Creating constants...");

	/* create TRUE and FALSE constants */
	const_false = fexpr_create(sat_variable_nr++, FE_FALSE, "False");
	fexpr_add_to_satmap(const_false);

	const_true = fexpr_create(sat_variable_nr++, FE_TRUE, "True");
	fexpr_add_to_satmap(const_true);

	/* add fexpr of constants to tristate constants */
	symbol_yes.fexpr_y = const_true;
	symbol_yes.fexpr_m = const_false;

	symbol_mod.fexpr_y = const_false;
	symbol_mod.fexpr_m = const_true;

	symbol_no.fexpr_y = const_false;
	symbol_no.fexpr_m = const_false;

	/* create symbols yes/mod/no as fexpr */
	symbol_yes_fexpr = fexpr_create(0, FE_SYMBOL, "y");
	symbol_yes_fexpr->sym = &symbol_yes;
	symbol_yes_fexpr->tri = yes;

	symbol_mod_fexpr = fexpr_create(0, FE_SYMBOL, "m");
	symbol_mod_fexpr->sym = &symbol_mod;
	symbol_mod_fexpr->tri = mod;

	symbol_no_fexpr = fexpr_create(0, FE_SYMBOL, "n");
	symbol_no_fexpr->sym = &symbol_no;
	symbol_no_fexpr->tri = no;

	printd("done.\n");
}

/*
 * create a temporary SAT-variable
 */
struct fexpr * create_tmpsatvar(void)
{
	struct fexpr *t = fexpr_create(sat_variable_nr++, FE_TMPSATVAR, "");
	str_append(&t->name, get_tmp_var_as_char(tmp_variable_nr++));
	fexpr_add_to_satmap(t);

	return t;
}

/*
 * return a temporary SAT variable as string
 */
char * get_tmp_var_as_char(int i)
{
	char *val = malloc(sizeof(char) * 18);
	snprintf(val, 18, "T_%d", i);
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
 *check whether an expr can evaluate to mod
 */
bool expr_can_evaluate_to_mod(struct expr *e)
{
	if (!e)
		return false;

	switch (e->type) {
	case E_SYMBOL:
		return e->left.sym == &symbol_mod || e->left.sym->type == S_TRISTATE ? true : false;
	case E_AND:
	case E_OR:
		return expr_can_evaluate_to_mod(e->left.expr) || expr_can_evaluate_to_mod(e->right.expr);
	case E_NOT:
		return expr_can_evaluate_to_mod(e->left.expr);
	default:
		return false;
	}
}

/*
 * check whether an expr is a non-Boolean constant
 */
bool expr_is_nonbool_constant(struct expr *e)
{
	if (e->type != E_SYMBOL)
		return false;
	if (e->left.sym->type != S_UNKNOWN)
		return false;

	if (e->left.sym->flags & SYMBOL_CONST)
		return true;

	return string_is_number(e->left.sym->name) || string_is_hex(e->left.sym->name);
}

/*
 * check whether a symbol is a non-Boolean constant
 */
bool sym_is_nonbool_constant(struct symbol *sym)
{
	if (sym->type != S_UNKNOWN)
		return false;

	if (sym->flags & SYMBOL_CONST)
		return true;

	return string_is_number(sym->name) || string_is_hex(sym->name);
}

/*
 * print an expr
 */
static void print_expr_util(struct expr *e, int prevtoken)
{
	if (!e)
		return;

	switch (e->type) {
	case E_SYMBOL:
		if (sym_get_name(e->left.sym) != NULL)
			printf("%s", sym_get_name(e->left.sym));
		else
			printf("left was null\n");
		break;
	case E_NOT:
		printf("!");
		print_expr_util(e->left.expr, E_NOT);
		break;
	case E_AND:
		if (prevtoken != E_AND && prevtoken != 0)
			printf("(");
		print_expr_util(e->left.expr, E_AND);
		printf(" && ");
		print_expr_util(e->right.expr, E_AND);
		if (prevtoken != E_AND && prevtoken != 0)
			printf(")");
		break;
	case E_OR:
		if (prevtoken != E_OR && prevtoken != 0)
			printf("(");
		print_expr_util(e->left.expr, E_OR);
		printf(" || ");
		print_expr_util(e->right.expr, E_OR);
		if (prevtoken != E_OR && prevtoken != 0)
			printf(")");
		break;
	case E_EQUAL:
	case E_UNEQUAL:
		if (e->left.sym->name)
			printf("%s", e->left.sym->name);
		else
			printf("left was null\n");
		printf("%s", e->type == E_EQUAL ? "=" : "!=");
		printf("%s", e->right.sym->name);
		break;
	case E_LEQ:
	case E_LTH:
		if (e->left.sym->name)
			printf("%s", e->left.sym->name);
		else
			printf("left was null\n");
		printf("%s", e->type == E_LEQ ? "<=" : "<");
		printf("%s", e->right.sym->name);
		break;
	case E_GEQ:
	case E_GTH:
		if (e->left.sym->name)
			printf("%s", e->left.sym->name);
		else
			printf("left was null\n");
		printf("%s", e->type == E_GEQ ? ">=" : ">");
		printf("%s", e->right.sym->name);
		break;
	case E_RANGE:
		printf("[");
		printf("%s", e->left.sym->name);
		printf(" ");
		printf("%s", e->right.sym->name);
		printf("]");
		break;
	default:
		break;
	}
}
void print_expr(char *tag, struct expr *e, int prevtoken)
{
	printf("%s ", tag);
	print_expr_util(e, prevtoken);
	printf("\n");
}

/*
 * check, if the symbol is a tristate-constant
 */
bool sym_is_tristate_constant(struct symbol *sym) {
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
	return sym_is_tristate_constant(sym) || sym_is_boolean(sym);
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
struct pexpr * prop_get_condition(struct property *prop)
{
	if (prop == NULL)
		return NULL;

	/* if there is no condition, return True */
	if (!prop->visible.expr)
		return pexf(const_true);

	return expr_calculate_pexpr_both(prop->visible.expr);
}

/*
 * return the default property, NULL if none exists or can be satisfied
 */
struct property *sym_get_default_prop(struct symbol *sym)
{
	struct property *prop;

	for_all_defaults(sym, prop) {
		prop->visible.tri = expr_calc_value(prop->visible.expr);
		if (prop->visible.tri != no)
			return prop;
	}
	return NULL;
}

/*
 * check whether a non-boolean symbol has a value set
 */
bool sym_nonbool_has_value_set(struct symbol *sym)
{
	if (!sym_is_nonboolean(sym))
		return false;

	const char *string_val = sym_get_string_value(sym);

	if (strcmp(string_val, "") != 0)
		return true;

	/* a HEX/INT symbol cannot have value "" */
	if (sym->type == S_HEX || sym->type == S_INT)
		return false;

	/* cannot have a value with unmet dependencies */
	if (sym->dir_dep.expr && sym->dir_dep.tri == no)
		return false;

	/* visible prompt => value set */
	struct property *prompt = sym_get_prompt(sym);
	if (prompt != NULL && prompt->visible.tri != no)
		return true;

	/* invisible prompt => must get value from default value */
	struct property *p = sym_get_default_prop(sym);
	if (p == NULL)
		return false;

	if (!strcmp(sym_get_string_default(sym), ""))
		return true;

	return false;
}

/*
 * return the name of the symbol or the prompt-text, if it is a choice symbol
 */
char * sym_get_name(struct symbol *sym)
{
	if (sym_is_choice(sym)) {
		struct property *prompt = sym_get_prompt(sym);
		if (prompt == NULL)
			return "";

		return strdup(prompt->text);
	} else {
		return sym->name;
	}
}

/*
 * check whether symbol is to be changed
 */
bool sym_is_sdv(struct sdv_list *list, struct symbol *sym)
{
	struct sdv_node *node;
	sdv_list_for_each(node, list)
		if (sym == node->elem->sym)
			return true;

	return false;
}

/*
 * print a symbol's name
 */
void print_sym_name(struct symbol *sym)
{
	printf("Symbol: ");
	if (sym_is_choice(sym)) {
		struct property *prompt = sym_get_prompt(sym);
		printf("(Choice) %s", prompt->text);
	} else  {
		printf("%s", sym->name);
	}
	printf("\n");
}

/*
 * print all constraints for a symbol
 */
void print_sym_constraint(struct symbol* sym)
{
	struct pexpr_node *node;
	pexpr_list_for_each(node, sym->constraints)
		pexpr_print("::", node->elem, -1);
}

/*
 * print a default map
 */
void print_default_map(struct defm_list *map)
{
	struct default_map *entry;
	struct defm_node *node;

	defm_list_for_each(node, map) {
		entry = node->elem;
		struct gstr s = str_new();
		str_append(&s, "\t");
		str_append(&s, str_get(&entry->val->name));
		str_append(&s, " ->");
		pexpr_print(strdup(str_get(&s)), entry->e, -1);
		str_free(&s);
	}
}

/*
 * check whether a string is a number
 */
bool string_is_number(char *s)
{
	int len = strlen(s);
	int i = 0;
	while (i < len) {
		if (!isdigit(s[i]))
			return false;
		i++;
	}

	return true;
}

/*
 * check whether a string is a hexadecimal number
 */
bool string_is_hex(char *s)
{
	int len = strlen(s);
	int i = 2;
	if (len >= 3 && s[0] == '0' && s[1] == 'x') {
		while (i < len) {
			if (!isxdigit(s[i]))
				return false;
			i++;
		}
		return true;
	} else {
		return false;
	}
}
