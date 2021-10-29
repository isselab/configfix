/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 Patrick Franz <deltaone@debian.org>
 */

#ifndef CF_UTILS_H
#define CF_UTILS_H

/* parse Kconfig-file and read .config */
void init_config (const char *Kconfig_file);

/* initialize satmap and cnf_clauses */
void init_data(void);

/* assign SAT-variables to all fexpr and create the sat_map */
void create_sat_variables(void);

/* create True/False constants */
void create_constants(void);

/* create a temporary SAT-variable */
struct fexpr * create_tmpsatvar(void);

/* return a temporary SAT variable as string */
char * get_tmp_var_as_char(int i);

/* return a tristate value as a char * */
char * tristate_get_char(tristate val);

/* check whether an expr can evaluate to mod */
bool expr_can_evaluate_to_mod(struct expr *e);

/* check whether an expr is a non-Boolean constant */
bool expr_is_nonbool_constant(struct expr *e);

/* check whether a symbol is a non-Boolean constant */
bool sym_is_nonbool_constant(struct symbol *sym);

/* print an expr */
void print_expr(char *tag, struct expr *e, int prevtoken);

/* check, if the symbol is a tristate-constant */
bool sym_is_tristate_constant(struct symbol *sym);

/* check, if a symbol is of type boolean or tristate */
bool sym_is_boolean(struct symbol *sym);

/* check, if a symbol is a boolean/tristate or a tristate constant */
bool sym_is_bool_or_triconst(struct symbol *sym);

/* check, if a symbol is of type int, hex, or string */
bool sym_is_nonboolean(struct symbol *sym);

/* check, if a symbol has a prompt */
bool sym_has_prompt(struct symbol *sym);

/* return the prompt of the symbol, if there is one */
struct property * sym_get_prompt(struct symbol *sym);

/* return the condition for the property, True if there is none */
struct pexpr * prop_get_condition(struct property *prop);

/* return the default property, NULL if none exists or can be satisfied */
struct property *sym_get_default_prop(struct symbol *sym);

/* check whether a non-boolean symbol has a value set */
bool sym_nonbool_has_value_set(struct symbol *sym);

/* return the name of the symbol */
char * sym_get_name(struct symbol *sym);

/* check whether symbol is to be changed */
bool sym_is_sdv(struct sdv_list *list, struct symbol *sym);

/* print a symbol's name */
void print_sym_name(struct symbol *sym);

/* print all constraints for a symbol */
void print_sym_constraint(struct symbol *sym);

/* print a default map */
void print_default_map(struct defm_list *map);

/* check whether a string is a number */
bool string_is_number(char *s);

/* check whether a string is a hexadecimal number */
bool string_is_hex(char *s);

#endif
