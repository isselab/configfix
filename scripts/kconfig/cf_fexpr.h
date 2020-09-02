/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Patrick Franz <patfra71@gmail.com>
 */

#ifndef CF_FEXPR_H
#define CF_FEXPR_H

/* create a fexpr */
struct fexpr * fexpr_create(int satval, enum fexpr_type type, char *name);

/* create the fexpr for a symbol */
void sym_create_fexpr (struct symbol *sym);

struct fexpr * calculate_fexpr_both(struct k_expr *e);
struct fexpr * calculate_fexpr_y(struct k_expr *e);
struct fexpr * calculate_fexpr_m(struct k_expr *e);
struct fexpr * calculate_fexpr_y_and(struct k_expr *a, struct k_expr *b);
struct fexpr * calculate_fexpr_m_and(struct k_expr *a, struct k_expr *b);
struct fexpr * calculate_fexpr_y_or(struct k_expr *a, struct k_expr *b);
struct fexpr * calculate_fexpr_m_or(struct k_expr *a, struct k_expr *b);
struct fexpr * calculate_fexpr_y_not(struct k_expr *a);
struct fexpr * calculate_fexpr_m_not(struct k_expr *a);
struct fexpr * calculate_fexpr_y_equals(struct k_expr *a);
struct fexpr * calculate_fexpr_y_unequals(struct k_expr *a);


/* macro to create a fexpr of type AND */
struct fexpr * fexpr_and(struct fexpr *a, struct fexpr *b);

/* macro to create a fexpr of type OR */
struct fexpr * fexpr_or(struct fexpr *a, struct fexpr *b);

/* macro to create a fexpr of type NOT */
struct fexpr * fexpr_not(struct fexpr *a);

/* check whether a fexpr is in CNF */
bool fexpr_is_cnf(struct fexpr *e);

/* return fexpr_both for a symbol */
struct fexpr * sym_get_fexpr_both(struct symbol *sym);

/* return fexpr_sel_y for a symbol */
struct fexpr * sym_get_fexpr_sel_y(struct symbol *sym);

/* return fexpr_sel_m for a symbol */
struct fexpr * sym_get_fexpr_sel_m(struct symbol *sym);

/* return fexpr_sel_both for a symbol */
struct fexpr * sym_get_fexpr_sel_both(struct symbol *sym);

/* create the fexpr of a non-boolean symbol for a specific value */
struct fexpr * sym_create_nonbool_fexpr(struct symbol *sym, char *value);

/* return the fexpr of a non-boolean symbol for a specific value, NULL if non-existent */
struct fexpr * sym_get_nonbool_fexpr(struct symbol *sym, char *value);

/*
 * return the fexpr of a non-boolean symbol for a specific value, if it exists
 * otherwise create it
 */
struct fexpr * sym_get_or_create_nonbool_fexpr(struct symbol *sym, char *value);

/* macro to construct a fexpr for "A implies B" */
struct fexpr * implies(struct fexpr *a, struct fexpr *b);

/* check, if the fexpr is a symbol, a True/False-constant, a literal symbolizing a non-boolean or a choice symbol */
bool fexpr_is_symbol(struct fexpr *e);

/* check, if the fexpr is a symbol, a True/False-constant, a literal symbolizing a non-boolean, a choice symbol or NOT */
bool fexpr_is_symbol_or_not(struct fexpr *e);

/* check, if a fexpr is a symbol, a True/False-constant, a literal symbolizing a non-boolean, a choice symbol or a negated "symbol" */
bool fexpr_is_symbol_or_neg_atom(struct fexpr *e);

/* convert a fexpr into negation normal form */
void convert_fexpr_to_nnf(struct fexpr *e);

/* convert a fexpr from negation normal form into conjunctive normal form */
void convert_fexpr_to_cnf(struct fexpr *e);

/* print an fexpr */
void fexpr_print(char *tag, struct fexpr *e, int parent);

/* write an fexpr into a string (format needed for testing) */
void fexpr_as_char(struct fexpr *e, struct gstr *s, int parent);

/* write an fexpr into a string */
void fexpr_as_char_short(struct fexpr *e, struct gstr *s, int parent);

#endif
