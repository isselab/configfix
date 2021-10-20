/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 Patrick Franz <deltaone@debian.org>
 */

#ifndef CF_EXPR_H
#define CF_EXPR_H

#define fexpr_list_for_each(node, list) \
	for (node = list->head; node != NULL; node = node->next)

#define fexl_list_for_each(node, list) \
	fexpr_list_for_each(node, list)

#define pexpr_list_for_each(node, list) \
	fexpr_list_for_each(node, list)

#define sdv_list_for_each(node, list) \
	fexpr_list_for_each(node, list)

#define sfix_list_for_each(node, list) \
	fexpr_list_for_each(node, list)

#define sfl_list_for_each(node, list) \
	fexpr_list_for_each(node, list)

#define sym_list_for_each(node, list) \
	fexpr_list_for_each(node, list)

#define defm_list_for_each(node, list) \
	fexpr_list_for_each(node, list)

#define prop_list_for_each(node, list) \
	fexpr_list_for_each(node, list)

/* create a fexpr */
struct fexpr * fexpr_create(int satval, enum fexpr_type type, char *name);

/* create the fexpr for a symbol */
void sym_create_fexpr (struct symbol *sym);

struct pexpr * expr_calculate_pexpr_both(struct expr *e);
struct pexpr * expr_calculate_pexpr_y(struct expr *e);
struct pexpr * expr_calculate_pexpr_m(struct expr *e);
struct pexpr * expr_calculate_pexpr_y_and(struct expr *a, struct expr *b);
struct pexpr * expr_calculate_pexpr_m_and(struct expr *a, struct expr *b);
struct pexpr * expr_calculate_pexpr_both_and(struct expr *a, struct expr *b);
struct pexpr * expr_calculate_pexpr_y_or(struct expr *a, struct expr *b);
struct pexpr * expr_calculate_pexpr_m_or(struct expr *a, struct expr *b);
struct pexpr * expr_calculate_pexpr_both_or(struct expr *a, struct expr *b);
struct pexpr * expr_calculate_pexpr_y_not(struct expr * e);
struct pexpr * expr_calculate_pexpr_m_not(struct expr *e);
struct pexpr * expr_calculate_pexpr_y_equals(struct expr *e);
struct pexpr * expr_calculate_pexpr_y_unequals(struct expr *e);
struct pexpr * expr_calculate_pexpr_y_comp(struct expr *e);

/* macro to create a pexpr of type AND */
struct pexpr * pexpr_and(struct pexpr *a, struct pexpr *b);

/* macro to create a pexpr of type OR */
struct pexpr * pexpr_or(struct pexpr *a, struct pexpr *b);

/* macro to create a pexpr of type NOT */
struct pexpr * pexpr_not(struct pexpr *a);

/* check whether a pexpr is in CNF */
bool pexpr_is_cnf(struct pexpr *e);

/* check whether a pexpr is in NNF */
bool pexpr_is_nnf(struct pexpr *e);

/* return fexpr_both for a symbol */
struct pexpr * sym_get_fexpr_both(struct symbol *sym);

/* return fexpr_sel_both for a symbol */
struct pexpr * sym_get_fexpr_sel_both(struct symbol *sym);

/* create the fexpr of a non-boolean symbol for a specific value */
struct fexpr * sym_create_nonbool_fexpr(struct symbol *sym, char *value);

/* return the fexpr of a non-boolean symbol for a specific value, NULL if non-existent */
struct fexpr * sym_get_nonbool_fexpr(struct symbol *sym, char *value);

/*
 * return the fexpr of a non-boolean symbol for a specific value, if it exists
 * otherwise create it
 */
struct fexpr * sym_get_or_create_nonbool_fexpr(struct symbol *sym, char *value);

/* macro to construct a pexpr for "A implies B" */
struct pexpr * pexpr_implies(struct pexpr *a, struct pexpr *b);

/* check, if the fexpr is a symbol, a True/False-constant, a literal symbolising a non-boolean or a choice symbol */
bool fexpr_is_symbol(struct fexpr *e);

/* check whether a pexpr is a symbol or a negated symbol */
bool pexpr_is_symbol(struct pexpr *e);

/* check whether the fexpr is a constant (true/false) */
bool fexpr_is_constant(struct fexpr *e);

/* add a fexpr to the satmap */
void fexpr_add_to_satmap(struct fexpr *e);

/* print an fexpr */
void fexpr_print(char *tag, struct fexpr *e);

/* write an fexpr into a string (format needed for testing) */
void fexpr_as_char(struct fexpr *e, struct gstr *s);

/* write pn pexpr into a string */
void pexpr_as_char_short(struct pexpr *e, struct gstr *s, int parent);

/* write an fexpr into a string (format needed for testing) */
void pexpr_as_char(struct pexpr *e, struct gstr *s, int parent);

/* check whether a pexpr contains a specific fexpr */
bool pexpr_contains_fexpr(struct pexpr *e, struct fexpr *fe);

/* init list of fexpr */
struct fexpr_list * fexpr_list_init(void);

/* init list of fexpr_list */
struct fexl_list * fexl_list_init(void);

/* init list of pexpr */
struct pexpr_list * pexpr_list_init(void);

/* init list of symbol_fix */
struct sfix_list * sfix_list_init(void);

/* init list of sfix_list */
struct sfl_list * sfl_list_init(void);

/* init list of symbol_dvalue */
struct sdv_list * sdv_list_init(void);

/* init list of symbols */
struct sym_list * sym_list_init(void);

/* init list of default_maps */
struct defm_list * defm_list_init(void);

/* init list of properties */
struct prop_list *prop_list_init(void);

/* add element to tail of a fexpr_list */
void fexpr_list_add(struct fexpr_list *list, struct fexpr *fe);

/* add element to tail of a fexl_list */
void fexl_list_add(struct fexl_list *list, struct fexpr_list *elem);

/* add element to tail of a pexpr_list */
void pexpr_list_add(struct pexpr_list *list, struct pexpr *e);

/* add element to tail of a sfix_list */
void sfix_list_add(struct sfix_list *list, struct symbol_fix *fix);

/* add element to tail of a sfl_list */
void sfl_list_add(struct sfl_list *list, struct sfix_list *elem);

/* add element to tail of a sdv_list */
void sdv_list_add(struct sdv_list *list, struct symbol_dvalue *sdv);

/* add element to tail of a sym_list */
void sym_list_add(struct sym_list *list, struct symbol *sym);

/* add element to tail of a defm_list */
void defm_list_add(struct defm_list *list, struct default_map *map);

/* add element to tail of a prop_list */
void prop_list_add(struct prop_list *list, struct property *prop);

/* delete an element from a fexpr_list */
void fexpr_list_delete(struct fexpr_list *list, struct fexpr_node *node);

/* delete an element from a fexpr_list */
void fexl_list_delete(struct fexl_list *list, struct fexl_node *node);

/* delete the first occurence of elem in an fexl_list */
void fexl_list_delete_elem(struct fexl_list *list, struct fexpr_list *elem);

/* delete an element from a pexpr_list */
void pexpr_list_delete(struct pexpr_list *list, struct pexpr_node *node);

/* delete an element from a sfix_list */
void sfix_list_delete(struct sfix_list *list, struct sfix_node *node);

/* make a shallow copy of a fexpr_list */
struct fexpr_list * fexpr_list_copy(struct fexpr_list *list);

/* make a shallow copy of a fexpr_list */
struct fexl_list * fexl_list_copy(struct fexl_list *list);

/* make a shallow copy of a sdv_list */
struct sdv_list * sdv_list_copy(struct sdv_list *list);

/* make a shallow copy of a sfix_list */
struct sfix_list * sfix_list_copy(struct sfix_list *list);

/* print a fexpr_list */
void fexpr_list_print(char *title, struct fexpr_list *list);

/* print a fexl_list */
void fexl_list_print(char *title, struct fexl_list *list);

/* print a pexpr_list */
void pexpr_list_print(char *title, struct pexpr_list *list);

/* free an fexpr_list */
void fexpr_list_free(struct fexpr_list *list);

/* free an fexl_list */
void fexl_list_free(struct fexl_list *list);

/* free a sdv_list */
void sdv_list_free(struct sdv_list *list);

/* check whether 2 pexpr are equal */
bool pexpr_eq(struct pexpr *e1, struct pexpr *e2);

/* copy a pexpr */
struct pexpr * pexpr_copy(const struct pexpr *org);

/* free a pexpr */
void pexpr_free(struct pexpr *e);

/* print a pexpr  */
void pexpr_print(char *tag, struct pexpr *e, int prevtoken);

/* convert a fexpr to a pexpr */
struct pexpr * pexf(struct fexpr *fe);

/* eliminate duplicate and redundant operands */
struct pexpr * pexpr_eliminate_dups(struct pexpr *e);

#endif
