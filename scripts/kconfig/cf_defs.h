/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 Patrick Franz <deltaone@debian.org>
 */

#ifndef DEFS_H
#define DEFS_H

/* external variables */
extern unsigned int sat_variable_nr;
extern unsigned int tmp_variable_nr;
extern struct fexpr *satmap; // map SAT variables to fexpr
extern size_t satmap_size;

extern struct sdv_list *sdv_symbols; /* array with conflict-symbols */
extern bool CFDEBUG;
extern bool stop_rangefix;
extern struct fexpr *const_false;
extern struct fexpr *const_true;
extern struct fexpr *symbol_yes_fexpr;
extern struct fexpr *symbol_mod_fexpr;
extern struct fexpr *symbol_no_fexpr;

#define printd(fmt...) if (CFDEBUG) printf(fmt)

/* different types for f_expr */
enum fexpr_type {
	FE_SYMBOL,
	FE_NPC, /* no prompt condition */
	FE_TRUE,  /* constant of value True */
	FE_FALSE,  /* constant of value False */
	FE_NONBOOL,  /* for all non-(boolean/tristate) known values */
	FE_CHOICE, /* symbols of type choice */
	FE_SELECT, /* auxiliary variable for selected symbols */
	FE_TMPSATVAR /* temporary sat-variable (Tseytin) */
};

/* struct for a propositional logic formula */
struct fexpr {
	/* name of the feature expr */
	struct gstr name;

	/* associated symbol */
	struct symbol *sym;

	/* integer value for the SAT solver */
	int satval;

	/* assumption in the last call to PicoSAT */
	bool assumption;

	/* type of the fexpr */
	enum fexpr_type type;

	union {
		/* symbol */
		struct {
			tristate tri;
		};
		/* AND, OR, NOT */
		struct {
			struct fexpr *left;
			struct fexpr *right; /* not used for NOT */
		};
		/* EQUALS */
		struct {
			struct symbol *eqsym;
			struct symbol *eqvalue;
		};
		/* HEX, INTEGER, STRING */
		struct {
			struct gstr nb_val;
		};
	};

};

struct fexpr_list {
	struct fexpr_node *head, *tail;
	unsigned int size;
};

struct fexpr_node {
	struct fexpr *elem;
	struct fexpr_node *next, *prev;
};

struct fexl_list {
	struct fexl_node *head, *tail;
	unsigned int size;
};

struct fexl_node {
	struct fexpr_list *elem;
	struct fexl_node *next, *prev;
};

enum pexpr_type {
	PE_SYMBOL,
	PE_AND,
	PE_OR,
	PE_NOT
};

union pexpr_data {
	struct pexpr *pexpr;
	struct fexpr *fexpr;
};

struct pexpr {
	enum pexpr_type type;
	union pexpr_data left, right;
};

struct pexpr_list {
	struct pexpr_node *head, *tail;
	unsigned int size;
};

struct pexpr_node {
	struct pexpr *elem;
	struct pexpr_node *next, *prev;
};

struct default_map {
	struct fexpr *val;

	struct pexpr *e;
};

struct defm_list {
	struct defm_node *head, *tail;
	unsigned int size;
};

struct defm_node {
	struct default_map *elem;
	struct defm_node *next, *prev;
};

enum symboldv_type {
	SDV_BOOLEAN,	/* boolean/tristate */
	SDV_NONBOOLEAN	/* string/int/hex */
};

struct symbol_dvalue {
	struct symbol *sym;

	enum symboldv_type type;

	union {
		/* boolean/tristate */
		tristate tri;

		/* string/int/hex */
		struct gstr nb_val;
	};
};

struct sdv_list {
	struct sdv_node *head, *tail;
	unsigned int size;
};

struct sdv_node {
	struct symbol_dvalue *elem;
	struct sdv_node *next, *prev;
};

enum symbolfix_type {
	SF_BOOLEAN,	/* boolean/tristate */
	SF_NONBOOLEAN,	/* string/int/hex */
	SF_DISALLOWED	/* disallowed non-boolean values */
};

struct symbol_fix {
	struct symbol *sym;

	enum symbolfix_type type;

	union {
		/* boolean/tristate */
		tristate tri;

		/* string/int/hex */
		struct gstr nb_val;

		/* disallowed non-boolean values */
		struct gstr disallowed;
	};
};

struct sfix_list {
	struct sfix_node *head, *tail;
	unsigned int size;
};

struct sfix_node {
	struct symbol_fix *elem;
	struct sfix_node *next, *prev;
};

struct sfl_list {
	struct sfl_node *head, *tail;
	unsigned int size;
};

struct sfl_node {
	struct sfix_list *elem;
	struct sfl_node *next, *prev;
};

struct sym_list {
	struct sym_node *head, *tail;
	unsigned int size;
};

struct sym_node {
	struct symbol *elem;
	struct sym_node *next, *prev;
};

struct prop_list {
	struct prop_node *head, *tail;
	unsigned int size;
};

struct prop_node {
	struct property *elem;
	struct prop_node *next, *prev;
};

#endif
