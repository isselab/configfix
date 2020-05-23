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

#define KCR_CMP false
#define DEBUG_FUN false

static void debug_info(void);

static void build_tristate_constraint_clause(struct symbol *sym);

static void add_selects_kcr(struct symbol *sym);
static void add_selects(struct symbol *sym);

static void add_dependencies_bool(struct symbol* sym);
static void add_dependencies_bool_kcr(struct symbol* sym);
static void add_dependencies_nonbool(struct symbol *sym);

static void add_choice_prompt_cond(struct symbol *sym);
static void add_choice_dependencies(struct symbol *sym);
static void add_choice_constraints(struct symbol *sym);
static void add_invisible_constraints(struct symbol *sym, struct property *prompt);
static void sym_nonbool_at_least_1(struct symbol *sym);
static void sym_nonbool_at_most_1(struct symbol *sym);
static void sym_add_nonbool_values_from_ranges(struct symbol *sym);
static void sym_add_range_constraints(struct symbol *sym);
static void sym_add_nonbool_prompt_constraint(struct symbol *sym);

static struct default_map * create_default_map_entry(struct fexpr *val, struct fexpr *e);
static GArray * get_defaults(struct symbol *sym);
static struct fexpr * get_default_y(GArray *arr);
static struct fexpr * get_default_m(GArray *arr);
static long long sym_get_range_val(struct symbol *sym, int base);

/* -------------------------------------- */

static void debug_info(void)
{
	unsigned int i;
	struct symbol *sym;
	for_all_symbols(i, sym) {
		if (sym->name && strcmp(sym->name, "MTRR_SANITIZER_SPARE_REG_NR_DEFAULT") == 0) {
			printf("PRINTING MTRR_SANITIZER_SPARE_REG_NR_DEFAULT\n");
			print_symbol(sym);
			print_sym_constraint(sym);
		}
	}
}

/*
 * build the constraints for each symbol
 */
void get_constraints(void)
{
	unsigned int i;
	struct symbol *sym;
	
	if (DEBUG_FUN) {
		debug_info();
		return;
	}
	
	printf("Building constraints...");
	
	/* build constraints for boolean symbols */
	for_all_symbols(i, sym) {
		
		if (!sym_is_boolean(sym)) continue;
		
		
// 		if (sym->name && strcmp(sym->name, "FIRMWARE_MEMMAP") == 0) {
// 			printf("PRINTING CRAMFS_MTD\n");
// 			print_symbol(sym);
// 			print_sym_constraint(sym);
// 		} else {
// 			continue;
// 		}
		

		/* build tristate constraints */
		if (sym->type == S_TRISTATE)
			build_tristate_constraint_clause(sym);
		
		/* build constraints for select statements
		 * need to treat choice symbols seperately */
		// TODO check choice values for rev. dependencies
		if (!KCR_CMP) {
			add_selects(sym);
		} else {
			if (sym->rev_dep.expr)
				add_selects_kcr(sym);
		}
		
		/* build constraints for dependencies for booleans */
		if (sym->dir_dep.expr && !sym_is_choice(sym) && !sym_is_choice_value(sym)) {
// 		if (sym->dir_dep.expr &&  !sym_is_choice(sym)) {
			if (!KCR_CMP)
				add_dependencies_bool(sym);
			else
				add_dependencies_bool_kcr(sym);
		}
		
				
		/* build constraints for choice prompts */
		if (sym_is_choice(sym))
			add_choice_prompt_cond(sym);
		
		/* build constraints for dependencies (choice symbols) */
		if (sym_is_choice(sym))
			add_choice_dependencies(sym);
		
		/* build constraints for the choice options */
		// TODO
		if (sym_is_choice(sym))
			add_choice_constraints(sym);
		
// 		if (sym->name && strcmp(sym->name, "CRAMFS_MTD") == 0) {
// 			printf("PRINTING CRAMFS_MTD\n");
// 			print_symbol(sym);
// 			print_sym_constraint(sym);
// 		}
		
		/* build invisible constraints */
		struct property *prompt = sym_get_prompt(sym);
		if (prompt != NULL && prompt->visible.expr)
			add_invisible_constraints(sym, prompt);
		
	}
	
	/* build the constraints for select-variables */
	for_all_symbols(i, sym) {
		
		if (KCR_CMP) continue;
		
		if (!sym_is_boolean(sym)) continue;
		
		// TODO
		if (sym_is_choice(sym)) continue;
		
		if (!sym->rev_dep.expr) continue;
		
// 		assert(sym->rev_dep.expr != NULL);
		if (sym->list_sel_y == NULL) {
// 			printf("Is null for %s\n", sym_get_name(sym));
// 			print_sym_constraint(sym);
			//print_expr("Rev.dir:", sym->rev_dep.expr, 0);
			continue;
		}
        
		struct fexpr *sel_y = implies(sym->fexpr_sel_y, sym->fexpr_y);
		sym_add_constraint(sym, sel_y);
		
		struct fexpr *c1 = implies(sym->fexpr_sel_y, sym->list_sel_y);
		convert_fexpr_to_nnf(c1);
		sym_add_constraint(sym, c1);
		
		/* only continue for tristates */
			if (sym->type == S_BOOLEAN) continue;
        
		struct fexpr *sel_m = implies(sym->fexpr_sel_m, fexpr_or(sym->fexpr_m, sym->fexpr_y));
		sym_add_constraint(sym, sel_m);
        
		struct fexpr *c2 = implies(sym->fexpr_sel_m, sym->list_sel_m);
		convert_fexpr_to_nnf(c2);
		sym_add_constraint(sym, c2);
	}
	
	/* build constraints for non-booleans */
	for_all_symbols(i, sym) {
		
		if (!sym_is_nonboolean(sym)) continue;
		
		/* add known values from the range-attributes */
		if (sym->type == S_HEX || sym->type == S_INT)
			sym_add_nonbool_values_from_ranges(sym);
		
		/* build the range constraints for int/hex */
		if (sym->type == S_HEX || sym->type == S_INT)
			sym_add_range_constraints(sym);
		
		/* build constraints for dependencies for non-booleans */
// 		if (sym->dir_dep.expr)
// 			add_dependencies_nonbool(sym);
		
		/* build invisible constraints */
// 		struct property *prompt = sym_get_prompt(sym);
// 		if (prompt != NULL && prompt->visible.expr)
// 			add_invisible_constraints(sym, prompt);

		/* the symbol must have a value, if there is a prompt */
// 		if (sym_has_prompt(sym))
// 			sym_add_nonbool_prompt_constraint(sym);
		
		/* add current value to possible values */
		if (!sym->flags || !(sym->flags & SYMBOL_VALID))
			sym_calc_value(sym);
		const char *curr = sym_get_string_value(sym);
		if (strcmp(curr, "") != 0)
			sym_create_nonbool_fexpr(sym, (char *) curr);
		
		/* exactly one of the symbols must be true */
		sym_nonbool_at_least_1(sym);
		sym_nonbool_at_most_1(sym);
		
	}

	
// 	printf("done.\n");
}

/*
 * enforce tristate constraints
 * X and X_MODULE cannot both be true
 * also, the MODULES symbol must be set to yes/mod for tristates to be allowed 
 */
static void build_tristate_constraint_clause(struct symbol *sym)
{
	assert(sym->type == S_TRISTATE);
	char reason[CNF_REASON_LENGTH];
	strcpy(reason, "(#): enforce tristate constraints for symbol ");
	if (sym->name)
		strcat(reason, sym->name);

	struct fexpr *c = fexpr_or(fexpr_not(sym->fexpr_y), fexpr_not(sym->fexpr_m));
	sym_add_constraint(sym, c);

	/* enforce MODULES constraint */
	// TODO doublecheck that
	if (modules_sym->fexpr_y != NULL) {
		struct fexpr *c2 = implies(sym->fexpr_m, modules_sym->fexpr_y);
		sym_add_constraint(sym, c2);
	}
}

/*
 * build the select constraints
 */
static void add_selects_kcr(struct symbol *sym)
{
	/* parse reverse dependency */
	struct k_expr *ke = parse_expr(sym->rev_dep.expr, NULL);

	struct fexpr *fe_y = calculate_fexpr_y(ke);
	struct fexpr *sel_y = implies(fe_y, sym->fexpr_y);
	sym_add_constraint(sym, sel_y);

	struct fexpr *fe_both = calculate_fexpr_both(ke);
	struct fexpr *sel_both = implies(fe_both, sym_get_fexpr_both(sym));
	sym_add_constraint(sym, sel_both);
}

/*
 * build the select constraints simplified
 */
static void add_selects(struct symbol *sym)
{
	assert(sym_is_boolean(sym));
	struct property *p;
	for_all_properties(sym, p, P_SELECT) {
		struct symbol *selected = p->expr->left.sym;
				
		if (selected->type == S_UNKNOWN) continue;
		
		assert(selected->rev_dep.expr);
		
		struct fexpr *cond_y = const_true;
		struct fexpr *cond_both = const_true;
		if (p->visible.expr) {
			struct k_expr *ke = parse_expr(p->visible.expr, NULL);
			cond_y = calculate_fexpr_y(ke);
			cond_both = calculate_fexpr_both(ke);
		}
		
		struct fexpr *e1 = implies(fexpr_and(cond_y, sym->fexpr_y), selected->fexpr_sel_y);
        
		convert_fexpr_to_nnf(e1);
		sym_add_constraint(selected, e1);
// 		print_fexpr("e1:", e1, -1);
		
		/* imply that symbol is selected to y */
		if (selected->list_sel_y == NULL)
			selected->list_sel_y = fexpr_and(cond_y, sym->fexpr_y);
		else
			selected->list_sel_y = fexpr_or(selected->list_sel_y, fexpr_and(cond_y, sym->fexpr_y));
			
		assert(selected->list_sel_y != NULL);
		
		/* nothing more to do here */
		if (sym->type == S_BOOLEAN && selected->type == S_BOOLEAN) continue;
		

		struct fexpr *e2 = implies(fexpr_and(sym_get_fexpr_both(sym), cond_both), sym_get_fexpr_sel_both(selected));
        
		convert_fexpr_to_nnf(e2);
		sym_add_constraint(selected, e2);
// 		print_fexpr("e2:", e2, -1);
		
		/* imply that symbol is selected */
		if (selected->type == S_TRISTATE) {
			if (selected->list_sel_m == NULL)
				selected->list_sel_m = fexpr_and(cond_both, sym_get_fexpr_both(sym));
			else
				selected->list_sel_m = fexpr_or(selected->list_sel_m, fexpr_and(cond_both, sym_get_fexpr_both(sym)));
		} else {
			selected->list_sel_y = fexpr_or(selected->list_sel_y, fexpr_and(cond_both, sym_get_fexpr_both(sym)));
		}
	}
}

/*
 * build the dependency constraints for booleans
 */
static void add_dependencies_bool(struct symbol *sym)
{
	assert(sym_is_boolean(sym));
	assert(sym->dir_dep.expr);

// 	print_sym_name(sym);
// 	print_expr("dir_dep expr:", sym->dir_dep.expr, E_NONE);
	
	struct k_expr *ke_dirdep = parse_expr(sym->dir_dep.expr, NULL);
// 	struct k_expr *ke_revdep = sym->rev_dep.expr ? parse_expr(sym->rev_dep.expr, NULL) : get_const_false_as_kexpr();
	
// 	print_kexpr("kexpr:", ke_dirdep);
	
	struct fexpr *dep_both = calculate_fexpr_both(ke_dirdep);
// 	struct fexpr *sel_y = sym->rev_dep.expr ? sym->fexpr_sel_y : const_false;
// 	struct fexpr *sel_m = sym->rev_dep.expr ? sym->fexpr_sel_m : const_false;

// 	print_fexpr("fexpr dep_both:", dep_both, -1);
// 	print_fexpr("fexpr sel_y:", sel_y, -1);
	
	if (sym->type == S_TRISTATE) {
		struct fexpr *dep_y = calculate_fexpr_y(ke_dirdep);
// 		struct fexpr *sel_y = calculate_fexpr_y(ke_revdep);
		struct fexpr *sel_y = sym->rev_dep.expr ? sym->fexpr_sel_y : const_false;
		
		struct fexpr *fe_y = implies(sym->fexpr_y, fexpr_or(dep_y, sel_y));
// 		struct fexpr *fe_y = implies(sym->fexpr_y, dep_y);
		
		convert_fexpr_to_nnf(fe_y);
		sym_add_constraint(sym, fe_y);
		
		struct fexpr *fe_both = implies(sym->fexpr_m, fexpr_or(dep_both, sym_get_fexpr_sel_both(sym)));
// 		struct fexpr *fe_both = implies(sym->fexpr_m, dep_both);
		
		convert_fexpr_to_nnf(fe_both);
		sym_add_constraint(sym, fe_both);
	} else if (sym->type == S_BOOLEAN) {
		
// 		struct fexpr *fe_both = implies(sym->fexpr_y, fexpr_or(dep_both, sel_both));
// 		sym_add_constraint(sym, fe_both);
		
		
		if (!sym_is_choice_value(sym)) {
			struct fexpr *fe_both = implies(sym->fexpr_y, fexpr_or(dep_both, sym_get_fexpr_sel_both(sym)));
// 			struct fexpr *fe_both = implies(sym->fexpr_y, dep_both);
			
			convert_fexpr_to_nnf(fe_both);
			sym_add_constraint(sym, fe_both);
		} else {
			// TODO
			assert(ke_dirdep->type == KE_SYMBOL);
			struct symbol *ch = ke_dirdep->sym;
			struct fexpr *fe = implies(sym->fexpr_y, fexpr_and(ch->fexpr_y, fexpr_not(ch->fexpr_m)));
			
			convert_fexpr_to_nnf(fe);
			sym_add_constraint(sym, fe);
		}
	}
	//printf("\n");
}

/*
 * build the dependency constraints for booleans (KCR)
 */
static void add_dependencies_bool_kcr(struct symbol *sym)
{
	assert(sym_is_boolean(sym));
	assert(sym->dir_dep.expr);

// 	print_sym_name(sym);
// 	print_expr("dir_dep expr:", sym->dir_dep.expr, E_NONE);
	
	struct k_expr *ke_dirdep = parse_expr(sym->dir_dep.expr, NULL);
	struct k_expr *ke_revdep = sym->rev_dep.expr ? parse_expr(sym->rev_dep.expr, NULL) : get_const_false_as_kexpr();
	
// 	print_kexpr("kexpr:", ke_dirdep);
	
	struct fexpr *dep_both = calculate_fexpr_both(ke_dirdep);
	struct fexpr *sel_both = sym->rev_dep.expr ? calculate_fexpr_both(ke_revdep) : const_false;

// 	print_fexpr("fexpr dep_both:", dep_both, -1);
// 	print_fexpr("fexpr sel_both:", sel_both, -1);
	
	if (sym->type == S_TRISTATE) {
		struct fexpr *dep_y = calculate_fexpr_y(ke_dirdep);
		struct fexpr *sel_y = calculate_fexpr_y(ke_revdep);
		struct fexpr *fe_y = implies(sym->fexpr_y, fexpr_or(dep_y, sel_y));
		
		sym_add_constraint(sym, fe_y);
		
		struct fexpr *fe_both = implies(sym->fexpr_m, fexpr_or(dep_both, sel_both));
		sym_add_constraint(sym, fe_both);
	} else if (sym->type == S_BOOLEAN) {
		
// 		struct fexpr *fe_both = implies(sym->fexpr_y, fexpr_or(dep_both, sel_both));
// 		sym_add_constraint(sym, fe_both);
		
		
		if (!sym_is_choice_value(sym)) {
			struct fexpr *fe_both = implies(sym->fexpr_y, fexpr_or(dep_both, sel_both));
			sym_add_constraint(sym, fe_both);
		} else {
			// TODO
			assert(ke_dirdep->type == KE_SYMBOL);
			struct symbol *ch = ke_dirdep->sym;
			struct fexpr *fe = implies(sym->fexpr_y, fexpr_and(ch->fexpr_y, fexpr_not(ch->fexpr_m)));

			sym_add_constraint(sym, fe);
		}
	}
	//printf("\n");
}

/*
 * build the dependency constraints for non-booleans
 */
static void add_dependencies_nonbool(struct symbol *sym)
{
	assert(sym_is_nonboolean(sym));
	assert(sym->dir_dep.expr);
	
// 	print_sym_name(sym);
// 	print_expr("dir_dep expr:", sym->dir_dep.expr, E_NONE);
	
	struct k_expr *ke_dirdep = parse_expr(sym->dir_dep.expr, NULL);
	struct k_expr *ke_revdep = sym->rev_dep.expr ? parse_expr(sym->rev_dep.expr, NULL) : get_const_false_as_kexpr();
	
// 	print_kexpr("kexpr:", ke_dirdep);
	
	struct fexpr *dep_both = calculate_fexpr_both(ke_dirdep);
	struct fexpr *sel_both = sym->rev_dep.expr ? calculate_fexpr_both(ke_revdep) : const_false;
	
	// TODO check
	if (sel_both != const_false)
		perror("Non-boolean symbol has reverse dependencies.");

	struct fexpr *nb_vals = const_false;
	unsigned int i;
	/* can skip the first non-boolean value, since this is 'n' */
	for (i = 1; i < sym->fexpr_nonbool->arr->len; i++) {
		nb_vals = fexpr_or(nb_vals, g_array_index(sym->fexpr_nonbool->arr, struct fexpr *, i));
	}

	struct fexpr *fe_both = implies(nb_vals, fexpr_or(dep_both, sel_both));
	
	convert_fexpr_to_nnf(fe_both);
	sym_add_constraint(sym, fe_both);
}


/*
 * build the constraints for the choice prompt
 */
static void add_choice_prompt_cond(struct symbol* sym)
{
	assert(sym_is_boolean(sym));
	
	struct property *prompt = sym_get_prompt(sym);
	assert(prompt);
	
	struct k_expr *ke = prompt->visible.expr ? parse_expr(prompt->visible.expr, NULL) : get_const_true_as_kexpr();
	struct fexpr *promptCondition = calculate_fexpr_both(ke);
	
// 	print_expr("CHOICE_1 PROMPT:", prompt->visible.expr, E_NONE);
// 	print_fexpr("C_1 fexpr:", promptCondition, -1);
	
	struct fexpr *fe_both = sym_get_fexpr_both(sym);
	
	if (!sym_is_optional(sym)) {
		struct fexpr *req_cond = implies(promptCondition, fe_both);
// 		print_fexpr("PromptCond:", req_cond, -1);
		
// 		convert_fexpr_to_nnf(req_cond);
		sym_add_constraint(sym, req_cond);
	}
	
	struct fexpr *pr_cond = implies(fe_both, promptCondition);
	
// 	convert_fexpr_to_nnf(pr_cond);
	sym_add_constraint(sym, pr_cond);
}

/*
 * build constraints for dependencies (choice symbols)
 */
static void add_choice_dependencies(struct symbol *sym)
{
	assert(sym_is_choice(sym));
	
	struct property *prompt = sym_get_prompt(sym);
	assert(prompt);
	
	struct k_expr *ke_dirdep = prompt->visible.expr ? parse_expr(prompt->visible.expr, NULL) : get_const_true_as_kexpr();

// 	print_expr("dir_dep expr:", sym->dir_dep.expr, E_NONE);
	
	//struct k_expr *ke_dirdep = parse_expr(sym->dir_dep.expr, NULL);
	//struct k_expr *ke_revdep = sym->rev_dep.expr ? parse_expr(sym->rev_dep.expr, NULL) : get_const_false_as_kexpr();
	
	struct fexpr *dep_both = calculate_fexpr_both(ke_dirdep);
	//struct fexpr *sel_both = sym->rev_dep.expr ? calculate_fexpr_both(ke_revdep) : const_false;

// 	print_fexpr("fexpr dep_both:", dep_both, -1);
// 	print_fexpr("fexpr sel_both:", sel_both, -1);
	
	if (sym->type == S_TRISTATE) {
		struct fexpr *dep_y = calculate_fexpr_y(ke_dirdep);
		//struct fexpr *sel_y = calculate_fexpr_y(ke_revdep);
		//struct fexpr *fe_y = implies(sym->fexpr_y, fexpr_or(dep_y, sel_y));
		struct fexpr *fe_y = implies(sym->fexpr_y, dep_y);
		
// 		convert_fexpr_to_nnf(fe_y);
		sym_add_constraint(sym, fe_y);
		
		struct fexpr *fe_both = implies(sym->fexpr_m, dep_both);
		
// 		convert_fexpr_to_nnf(fe_both);
		sym_add_constraint(sym, fe_both);
	} else if (sym->type == S_BOOLEAN) {
		struct fexpr *fe_both = implies(sym->fexpr_y, dep_both);
		sym_add_constraint(sym, fe_both);
	}
}


/*
 * build constraints for the choice symbols
 */
static void add_choice_constraints(struct symbol *sym)
{
	assert(sym_is_boolean(sym));
	
	struct property *prompt = sym_get_prompt(sym);
	assert(prompt);
	
	unsigned int i, j;
	struct symbol *choice, *choice2;
	
	/* create list of all choice options */
	GArray *items = g_array_new(false, false, sizeof(struct symbol *));
	/* create list of choice options with a prompt */
	GArray *promptItems = g_array_new(false, false, sizeof(struct symbol *));
	
	struct property *prop;
	for_all_choices(sym, prop) {
		struct expr *expr;
		expr_list_for_each_sym(prop->expr, expr, choice) {
			g_array_append_val(items, choice);
			if (sym_get_prompt(choice) != NULL)
				g_array_append_val(promptItems, choice);
		}
	}
	
	/* if the choice is set to yes, at least one child must be set to yes */
	struct fexpr *c1 = NULL;
	for (i = 0; i < promptItems->len; i++) {
		choice = g_array_index(promptItems, struct symbol *, i);
		c1 = i == 0 ? choice->fexpr_y : fexpr_or(c1, choice->fexpr_y);
	}
	if (c1 != NULL)
		sym_add_constraint(sym, implies(sym->fexpr_y, c1));
	
	/* every choice option (even those without a prompt) implies the choice */
	for (i = 0; i < items->len; i++) {
		choice = g_array_index(items, struct symbol *, i);
		c1 = implies(sym_get_fexpr_both(choice), sym_get_fexpr_both(sym));
		sym_add_constraint(sym, c1);
	}

	/* choice options can only select mod, if the entire choice is mod */
	if (sym->type == S_TRISTATE) {
		for (i = 0; i < items->len; i++) {
			choice = g_array_index(items, struct symbol *, i);
			if (choice->type == S_TRISTATE) {
				c1 = implies(choice->fexpr_m, sym->fexpr_m);
				sym_add_constraint(sym, c1);
			}
		}
	}
	
	/* tristate options cannot be m, if the choice symbol is boolean */
	if (sym->type == S_BOOLEAN) {
		for (i = 0; i < items->len; i++) {
			choice = g_array_index(items, struct symbol *, i);
			if (choice->type == S_TRISTATE)
				sym_add_constraint(sym, fexpr_not(choice->fexpr_m));
		}
	}
	
	/* all choice options are mutually exclusive for yes */
	for (i = 0; i < promptItems->len; i++) {
		choice = g_array_index(promptItems, struct symbol *, i);
		for (j = i + 1; j < promptItems->len; j++) {
			choice2 = g_array_index(promptItems, struct symbol *, j);
			c1 = fexpr_or(fexpr_not(choice->fexpr_y), fexpr_not(choice2->fexpr_y));
			sym_add_constraint(sym, c1);
		}
	}
	
	/* if one choice option with a prompt is set to yes,
	 * then no other option may be set to mod */
	if (sym->type == S_TRISTATE) {
		for (i = 0; i < promptItems->len; i++) {
			choice = g_array_index(promptItems, struct symbol *, i);
		
			GArray *tmp_arr = g_array_new(false, false, sizeof(struct symbol *));
			for (j = 0; j < promptItems->len; j++) {
				choice2 = g_array_index(promptItems, struct symbol *, j);
				if (choice2->type == S_TRISTATE)
					g_array_append_val(tmp_arr, choice2);
			}
			if (tmp_arr->len == 0) continue;
			
			for (j = 0; j < tmp_arr->len; j++) {
				choice2 = g_array_index(tmp_arr, struct symbol *, j);
				c1 = j == 0 ? fexpr_not(choice2->fexpr_m) : fexpr_and(c1, fexpr_not(choice2->fexpr_m));
			}
			c1 = implies(choice->fexpr_y, c1);
			sym_add_constraint(sym, c1);
			
		}
	}
}

/*
 * build the constraints for invisible options such as defaults
 */
static void add_invisible_constraints(struct symbol *sym, struct property *prompt)
{
	assert(prompt);
// 	printf("\n");
// 	print_sym_name(sym);
// 	print_expr("Prompt condition:", prompt->visible.expr, E_NONE);
// 	print_expr("dir_dep:         ", sym->dir_dep.expr, 0);
	
	struct k_expr * ke_promptCond = parse_expr(prompt->visible.expr, NULL);
	struct fexpr *promptCondition_both = calculate_fexpr_both(ke_promptCond);
	struct fexpr *promptCondition_yes = calculate_fexpr_y(ke_promptCond);
	struct fexpr *nopromptCond = fexpr_not(promptCondition_both);

// 	convert_fexpr_to_nnf(nopromptCond);
// 	print_fexpr("nopromptCond:    ", nopromptCond, -1);
	
	GArray *defaults = get_defaults(sym);
	struct fexpr *default_y = get_default_y(defaults);
	struct fexpr *default_m = get_default_m(defaults);
	struct fexpr *default_both = fexpr_or(default_y, default_m);
	
// 	printf("Default map (len %d):\n", defaults->len);
// 	print_default_map(defaults);
// 	print_fexpr("Default_y:", default_y, -1);
// 	print_fexpr("Default_m:", default_m, -1);
// 	print_fexpr("Default_both:", default_both, -1);
	
	/* if invisible and on by default, then a symbol can only be deactivated by its dependencies */
	if (sym->type == S_TRISTATE) {
		struct fexpr *e1 = implies(nopromptCond, implies(default_y, sym->fexpr_y));
		convert_fexpr_to_nnf(e1);
		sym_add_constraint(sym, e1);
		
		struct fexpr *e2 = implies(nopromptCond, implies(default_m, sym_get_fexpr_both(sym)));
		convert_fexpr_to_nnf(e2);
		sym_add_constraint(sym, e2);
	} else if (sym->type == S_BOOLEAN) {
		struct fexpr *c = implies(default_both, sym->fexpr_y);
		
		// TODO tristate choice hack
		
		struct fexpr *c2 = implies(nopromptCond, c);
		convert_fexpr_to_nnf(c2);
		sym_add_constraint(sym, c2);
	} else {
		
	}
	
	/* tristate elements are only selectable as yes, if they are visible as yes */
	if (sym->type == S_TRISTATE) {
		struct fexpr *e1 = implies(promptCondition_both, implies(sym->fexpr_y, promptCondition_yes));
		
		convert_fexpr_to_nnf(e1);
		sym_add_constraint(sym, e1);
	}
	
}

/*
 * add the known values from the range-attributes
 */
static void sym_add_nonbool_values_from_ranges(struct symbol *sym)
{
	struct property *prop;
	for_all_properties(sym, prop, P_RANGE) {
		assert(prop != NULL);
		
		/* add the values to known values, if they don't exist yet */
		sym_create_nonbool_fexpr(sym, prop->expr->left.sym->name);
		sym_create_nonbool_fexpr(sym, prop->expr->right.sym->name);
	}
}

/*
 * build the range constraints for int/hex
 */
static void sym_add_range_constraints(struct symbol *sym)
{
	struct property *prop;
	struct fexpr *prevs, *propCond, *e;
	unsigned int i;
	GArray *prevCond = g_array_new(false, false, sizeof(struct fexpr *));
	for_all_properties(sym, prop, P_RANGE) {
		assert(prop != NULL);
		
		prevs = const_true;
		propCond = prop_get_condition(prop);

		if (prevCond->len == 0) {
			prevs = propCond;
		} else {
			for (i = 0; i < prevCond->len; i++) {
				e = g_array_index(prevCond, struct fexpr *, i);
				prevs = fexpr_and(fexpr_not(e), prevs);
			}
			prevs = fexpr_and(propCond, prevs);
		}
		g_array_append_val(prevCond, propCond);
		
// 		print_fexpr("propCond:", propCond, -1);
// 		print_fexpr("prevs:", prevs, -1);

		int base;
		long long range_min, range_max, tmp;
		
		switch (sym->type) {
		case S_INT:
			base = 10;
			break;
		case S_HEX:
			base = 16;
			break;
		default:
			return;
		}
		
		range_min = sym_get_range_val(prop->expr->left.sym, base);
		range_max = sym_get_range_val(prop->expr->right.sym, base);
		
		struct fexpr *nb_val;
		unsigned int i;
		/* can skip the first non-boolean value, since this is 'n' */
		for (i = 1; i < sym->fexpr_nonbool->arr->len; i++) {
			nb_val = g_array_index(sym->fexpr_nonbool->arr, struct fexpr *, i);
			tmp = strtoll(str_get(&nb_val->nb_val), NULL, base);
			
			/* known value is in range, nothing to do here */
			if (tmp >= range_min && tmp <= range_max) continue;
			
			struct fexpr *not_nb_val = fexpr_not(nb_val);
			if (tmp < range_min) {
				struct fexpr *c = implies(prevs, not_nb_val);
				sym_add_constraint(sym, c);
			}
			
			if (tmp > range_max) {
				struct fexpr *c = implies(prevs, not_nb_val);
				sym_add_constraint(sym, c);
			}
		}
	}
}

/*
 * build a constraint, s.t. at least 1 of the symbols for a non-boolean symbol is true
 */
static void sym_nonbool_at_least_1(struct symbol *sym)
{
	assert(sym_is_nonboolean(sym));
	
	struct fexpr *e = NULL;
	int i;
	for (i = 0; i < sym->fexpr_nonbool->arr->len; i++) {
		if (i == 0)
			e = g_array_index(sym->fexpr_nonbool->arr, struct fexpr *, i);
		else 
			e = fexpr_or(e, g_array_index(sym->fexpr_nonbool->arr, struct fexpr *, i));
		
	}
	sym_add_constraint(sym, e);
}

/*
 * build a constraint, s.t. at most 1 of the symbols for a non-boolean symbol can be true
 */
static void sym_nonbool_at_most_1(struct symbol *sym)
{
	assert(sym_is_nonboolean(sym));
	
	struct fexpr *e1, *e2;
	int i, j;
	for (i = 0; i < sym->fexpr_nonbool->arr->len; i++) {
		e1 = g_array_index(sym->fexpr_nonbool->arr, struct fexpr *, i);
		for (j = i + 1; j < sym->fexpr_nonbool->arr->len; j++) {
			e2 = g_array_index(sym->fexpr_nonbool->arr, struct fexpr *, j);
			struct fexpr *e = fexpr_or(fexpr_not(e1), fexpr_not(e2));
			sym_add_constraint(sym, e);
		}
	}
}

/*
 * build constraint for non-boolean symbols forcing a value when the symbol has a prompt
 */
static void sym_add_nonbool_prompt_constraint(struct symbol *sym)
{
	struct property *prompt = sym_get_prompt(sym);
	if (prompt == NULL) return;
	
	struct fexpr *promptCondition = prop_get_condition(prompt);
	struct fexpr *n = sym_get_nonbool_fexpr(sym, "n");
	assert(n != NULL);
	struct fexpr *c = implies(promptCondition, fexpr_not(n));
	
	sym_add_constraint(sym, c);
}


static struct default_map * create_default_map_entry(struct fexpr *val, struct fexpr *e)
{
	struct default_map *map = malloc(sizeof(struct default_map));
	map->val = val;
	map->e = e;
	
	return map;
}

static struct fexpr * findDefaultEntry(struct fexpr *val, GArray *defaults)
{
	unsigned int i;
	struct default_map *entry;
	for (i = 0; i < defaults->len; i++) {
		entry = g_array_index(defaults, struct default_map *, i);
		if (val == entry->val)
			return entry->e;
	}
	
	return const_false;
}

/* add a default value to the list */
static struct fexpr *covered;
static void updateDefaultList(struct fexpr *val, struct fexpr *newCond, GArray *defaults)
{
	struct fexpr *prevCond = findDefaultEntry(val, defaults);
	struct fexpr *cond = fexpr_or(prevCond, fexpr_and(newCond, fexpr_not(covered)));
	struct default_map *entry = create_default_map_entry(val, cond);
	g_array_append_val(defaults, entry);
	covered = fexpr_or(covered, newCond);
}

/*
 * return all defaults for a symbol
 */
static GArray * get_defaults(struct symbol *sym)
{
	struct property *p;
// 	struct default_map *map;
	GArray *defaults = g_array_new(false, false, sizeof(struct default_map *));
	covered = const_false;
	
	struct k_expr *ke_expr;
	struct fexpr *expr_yes;
	struct fexpr *expr_mod;
	struct fexpr *expr_both;
	
	for_all_defaults(sym, p) {
		ke_expr = p->visible.expr ? parse_expr(p->visible.expr, NULL) : get_const_true_as_kexpr();
		expr_yes = calculate_fexpr_y(ke_expr);
		expr_mod = calculate_fexpr_m(ke_expr);
		expr_both = calculate_fexpr_both(ke_expr);
		
		struct k_expr *ke_v = parse_expr(p->expr, NULL);

// 		print_expr("v/expr:", p->expr, E_NONE);
// 		printf("type: %d\n", p->expr->type);
		
		
// 		assert(p->visible.expr);
// 		print_expr("expr/visible.expr:", p->visible.expr, E_NONE);
// 		assert(p->visible.expr);
		
		/* if tristate and def.value = y */
		if (p->expr->type == E_SYMBOL && sym->type == S_TRISTATE && p->expr->left.sym == &symbol_yes) {
// 			printf("IS TRISTATECONSTANT SYMBOL_YES\n");
			
			updateDefaultList(symbol_yes_fexpr, expr_yes, defaults);
			
			updateDefaultList(symbol_mod_fexpr, expr_mod, defaults);
		}
		/* if def.value = n/m/y */
		else if (p->expr->type == E_SYMBOL && is_tristate_constant(p->expr->left.sym)) {
// 			printf("IS TRISTATECONSTANT\n");
			assert(sym_is_boolean(sym));
			struct fexpr *s = const_true;
			if (p->expr->left.sym == &symbol_yes)
				s = symbol_yes_fexpr;
			else if (p->expr->left.sym == &symbol_mod)
				s = symbol_mod_fexpr;
			else {
// 				perror("Default value of n.");
// 				print_sym_name(sym);
				continue;
			}
			
			assert(s != const_true);
			updateDefaultList(s, expr_both, defaults);
		}
		/* if def.value = non-boolean constant */
		else if (p->expr->type == E_SYMBOL && p->expr->left.sym->type == S_UNKNOWN) {
			// TODO
			continue;
// 			printf("IS NONBOOL CONSTANT\n");
			print_sym_name(sym);
			print_sym_name(p->expr->left.sym);
// 			assert(sym_is_nonboolean(sym));

			struct fexpr *s = sym_get_or_create_nonbool_fexpr(sym, p->expr->left.sym->name);
			
			assert(sym_is_nonboolean(sym));
			
			updateDefaultList(s, expr_both, defaults);
		} 
		/* any expression which evaluates to n/m/y for a tristate */
		else if (sym->type == S_TRISTATE) {
// 			printf("IS TRISTATE EXPRESSION\n");
			struct k_expr *ke = malloc(sizeof(struct k_expr));
			ke->parent = NULL;
			ke->type = KE_AND;
			ke->left = ke_expr;
			ke->right = ke_v;
			
			updateDefaultList(symbol_yes_fexpr, calculate_fexpr_y(ke), defaults);
			
			updateDefaultList(symbol_mod_fexpr, calculate_fexpr_m(ke), defaults);
		}
		/* if non-boolean && def.value = non-boolean symbol */
		else if (p->expr->type == E_SYMBOL && sym_is_nonboolean(sym) && sym_is_nonboolean(p->expr->left.sym)){
// 			printf("IS NONBOOL ITEM\n");
// 			print_sym_name(p->expr->left.sym);
			// TODO
		} 
		/* any expression which evaluates to n/m/y */
		else {
// 			printf("IS ELSE EXPRESSION\n");
			assert(sym->type == S_BOOLEAN);
			struct k_expr *ke = malloc(sizeof(struct k_expr));
			ke->parent = NULL;
			ke->type = KE_AND;
			ke->left = ke_expr;
			ke->right = ke_v;
			
			updateDefaultList(symbol_yes_fexpr, calculate_fexpr_both(ke), defaults);
		}
	}
	
	return defaults;
}

/*
 * return the default_map for "y", False if it doesn't exist
 */
static struct fexpr * get_default_y(GArray *arr)
{
	unsigned int i;
	struct default_map *entry;
	
	for (i = 0; i < arr->len; i++) {
		entry = g_array_index(arr, struct default_map *, i);
		if (entry->val->type == FE_SYMBOL && entry->val->sym == &symbol_yes)
			return entry->e;
	}
	
	return const_false;
}

/*
 * return the default map for "m", False if it doesn't exist
 */
static struct fexpr * get_default_m(GArray *arr)
{
	unsigned int i;
	struct default_map *entry;
	
	for (i = 0; i < arr->len; i++) {
		entry = g_array_index(arr, struct default_map *, i);
		if (entry->val->type == FE_SYMBOL && entry->val->sym == &symbol_mod)
			return entry->e;
	}
	
	return const_false;
}

/* 
 * get the value for the range
 */
static long long sym_get_range_val(struct symbol *sym, int base)
{
	sym_calc_value(sym);
	switch (sym->type) {
	case S_INT:
		base = 10;
		break;
	case S_HEX:
		base = 16;
		break;
	default:
		break;
	}
	return strtoll(sym->curr.val, NULL, base);
}
