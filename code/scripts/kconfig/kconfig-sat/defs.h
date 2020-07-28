#ifndef DEFS_H
#define DEFS_H

#define CNF_REASON_LENGTH 512
#define STRING_LENGTH 100
#define PRINT_ALL_CNF false
#define PRINT_CNF_REASONS true
#define SATVARS_PER_SYMBOL 3

/* external variables */
extern unsigned int sat_variable_nr;
extern unsigned int tmp_variable_nr;
extern GHashTable *satmap;
extern GHashTable *cnf_clauses_map; /* hash-table with all CNF-clauses */
extern GArray *sdv_arr; /* array with conflict-symbols */
extern struct fexpr *const_false;
extern struct fexpr *const_true;
extern struct fexpr *symbol_yes_fexpr;
extern struct fexpr *symbol_mod_fexpr;
extern struct fexpr *symbol_no_fexpr;

/* wrapper for GArray */
struct garray_wrapper {
	/* GArray */
	GArray *arr;
};

/* different types for k_expr */
enum kexpr_type {
	KE_SYMBOL,
	KE_AND,
	KE_OR,
	KE_NOT,
	KE_EQUAL,
	KE_UNEQUAL,
	KE_CONST_FALSE,
	KE_CONST_TRUE
};

/* struct for an expression, built like a tree structure */
struct k_expr {
	/* parent k_expr */
	struct k_expr *parent;
	
	/* type of the k_expr */
	enum kexpr_type type;
	
	/* temporary SAT variable associated with this k_expr */
	struct tmp_sat_variable *t;

	union {
		/* symbol */
		struct {
			struct symbol *sym;
			tristate tri;
		};
		/* AND, OR, NOT */
		struct {
			struct k_expr *left;
			struct k_expr *right;
		};
		/* EQUAL, UNEQUAL */
		struct {
			struct symbol *eqsym;
			struct symbol *eqvalue;
		};
	};
};

/* different types for f_expr */
enum fexpr_type {
	FE_SYMBOL,
	FE_AND,
	FE_OR,
	FE_NOT,
	FE_EQUALS,
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

struct default_map {
	struct fexpr *val;
	
	struct fexpr *e;
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

#endif
