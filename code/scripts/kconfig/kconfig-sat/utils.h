#ifndef UTILS_H
#define UTILS_H

/* parse Kconfig-file and read .config */
void init_config (const char *Kconfig_file);

/* initialize satmap and cnf_clauses */
void init_data(void);


/* assign SAT-variables to all fexpr and create the sat_map */
void assign_sat_variables(void);

/* create True/False constants */
void create_constants(void);


/* create a fexpr */
struct fexpr * create_fexpr(int satval, enum fexpr_type type, char *name);

/* create a temporary SAT-variable */
struct fexpr * create_tmpsatvar(void);

/* build a CNF clause with the SAT-variables given */
struct cnf_clause * build_cnf_clause(struct gstr *reason, int num, ...);

/* create a struct for a CNF clause */
struct cnf_clause * create_cnf_clause_struct(void);

/* add a literal to a CNF-clause */
void add_literal_to_clause(struct cnf_clause *cl, int val);


/* return a temporary SAT variable as string */
char * get_tmp_var_as_char(int i);

/* return a tristate value as a char * */
char * tristate_get_char(tristate val);


/* check if a k_expr can evaluate to mod */
bool can_evaluate_to_mod(struct k_expr *e);

/* return the constant FALSE as a k_expr */
struct k_expr * get_const_false_as_kexpr(void);

/* return the constant TRUE as a k_expr */
struct k_expr * get_const_true_as_kexpr(void);


/* retrieve value from satmap given a key (sat_variable_nr) */
struct fexpr * get_fexpr_from_satmap(int key);


/* parse an expr as a k_expr */
struct k_expr * parse_expr(struct expr *e, struct k_expr *parent);


/* check, if the symbol is a tristate-constant */
bool is_tristate_constant(struct symbol *sym);

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
struct fexpr * prop_get_condition(struct property *prop);

/* return the name of the symbol */
char * sym_get_name(struct symbol *sym);

/* add a constraint for a symbol */
void sym_add_constraint(struct symbol *sym, struct fexpr *constraint);

/* count the number of all constraints */
unsigned int count_counstraints(void);

/* check whether symbol is to be changed */
bool sym_is_sdv(GArray *arr, struct symbol *sym);

/* add an integer to a GArray */
void g_array_add_ints(int num, ...);

#endif
