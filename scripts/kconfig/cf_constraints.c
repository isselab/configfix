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

#define KCR_CMP false
#define NPC_OPTIMISATION true

static void init_constraints(void);
static void get_constraints_bool(void);
static void get_constraints_select(void);
static void get_constraints_nonbool(void);

static void build_tristate_constraint_clause(struct symbol *sym);

static void add_selects_kcr(struct symbol *sym);
static void add_selects(struct symbol *sym);

static void add_dependencies_bool(struct symbol* sym);
static void add_dependencies_bool_kcr(struct symbol* sym);
static void add_dependencies_nonbool(struct symbol *sym);

static void add_choice_prompt_cond(struct symbol *sym);
static void add_choice_dependencies(struct symbol *sym);
static void add_choice_constraints(struct symbol *sym);
static void add_invisible_constraints(struct symbol *sym);
static void sym_nonbool_at_least_1(struct symbol *sym);
static void sym_nonbool_at_most_1(struct symbol *sym);
static void sym_add_nonbool_values_from_default_range(struct symbol *sym);
static void sym_add_range_constraints(struct symbol *sym);
static void sym_add_nonbool_prompt_constraint(struct symbol *sym);

static struct default_map * create_default_map_entry(struct fexpr *val, struct pexpr *e);
static struct defm_list * get_defaults(struct symbol *sym);
static struct pexpr * get_default_y(struct defm_list *list);
static struct pexpr * get_default_m(struct defm_list *list);
static struct pexpr *get_default_any(struct symbol *sym);
static long sym_get_range_val(struct symbol *sym, int base);

/* -------------------------------------- */

/*
 * build the constraints for each symbol
 */
void get_constraints(void)
{
	printd("Building constraints...");

	init_constraints();
	get_constraints_bool();
	get_constraints_select();
	get_constraints_nonbool();
}

/*
 * need to go through the constraints once to find all "known values"
 * for the non-Boolean symbols
 */
static void init_constraints(void)
{
	unsigned int i;
	struct symbol *sym;
	struct property *p;
	for_all_symbols(i, sym) {
		if (sym->type == S_UNKNOWN)
			continue;

		if (sym_is_boolean(sym)) {
			for_all_properties(sym, p, P_SELECT)
				expr_calculate_pexpr_both(p->visible.expr);

			for_all_properties(sym, p, P_IMPLY)
				expr_calculate_pexpr_both(p->visible.expr);
		}

		if (sym->dir_dep.expr)
			expr_calculate_pexpr_both(sym->dir_dep.expr);

		struct property *prompt = sym_get_prompt(sym);
		if (prompt != NULL && prompt->visible.expr) {
			expr_calculate_pexpr_both(prompt->visible.expr);
			get_defaults(sym);
		}

		if (sym_is_nonboolean(sym)) {
			for_all_defaults(sym, p) {
				if (p == NULL)
					continue;

				sym_create_nonbool_fexpr(sym, p->expr->left.sym->name);
			}
			for_all_properties(sym, p, P_RANGE) {
				if (p == NULL)
					continue;

				sym_create_nonbool_fexpr(sym, p->expr->left.sym->name);
				sym_create_nonbool_fexpr(sym, p->expr->right.sym->name);
			}
			const char *curr = sym_get_string_value(sym);
			if (strcmp(curr, "") != 0)
				sym_create_nonbool_fexpr(sym, (char *) curr);
		}

		if (sym->type == S_HEX || sym->type == S_INT)
			sym_add_nonbool_values_from_default_range(sym);
	}
}


/*
 *  build constraints for boolean symbols
 */
static void get_constraints_bool(void)
{
	unsigned int i;
	struct symbol *sym;
	for_all_symbols(i, sym) {

		if (!sym_is_boolean(sym))
			continue;

		/* build tristate constraints */
		if (sym->type == S_TRISTATE)
			build_tristate_constraint_clause(sym);

		/* build constraints for select statements
		 * need to treat choice symbols seperately */
		if (!KCR_CMP) {
			add_selects(sym);
		} else {
			if (sym->rev_dep.expr && !sym_is_choice(sym) && !sym_is_choice_value(sym))
				add_selects_kcr(sym);
		}

		/* build constraints for dependencies for booleans */
		if (sym->dir_dep.expr && !sym_is_choice(sym) && !sym_is_choice_value(sym)) {
			if (!KCR_CMP)
				add_dependencies_bool(sym);
			else
				add_dependencies_bool_kcr(sym);
		}

		/* build constraints for choice prompts */
		if (sym_is_choice(sym))
			add_choice_prompt_cond(sym);

		/* build constraints for dependencies (choice symbols and options) */
		if (sym_is_choice(sym) || sym_is_choice_value(sym))
			add_choice_dependencies(sym);

		/* build constraints for the choice groups */
		if (sym_is_choice(sym))
			add_choice_constraints(sym);

		/* build invisible constraints */
		add_invisible_constraints(sym);
	}
}

/*
* build the constraints for select-variables
* skip non-Booleans, choice symbols/options och symbols without rev_dir
*/
static void get_constraints_select(void)
{
	unsigned int i;
	struct symbol *sym;
	for_all_symbols(i, sym) {
		if (KCR_CMP)
			continue;

		if (!sym_is_boolean(sym))
			continue;

		if (sym_is_choice(sym) || sym_is_choice_value(sym))
			continue;

		if (!sym->rev_dep.expr)
			continue;

		if (sym->list_sel_y == NULL)
			continue;

		struct pexpr *sel_y = pexpr_implies(
					pexf(sym->fexpr_sel_y),
					pexf(sym->fexpr_y));
		sym_add_constraint(sym, sel_y);

		struct pexpr *c1 = pexpr_implies(
					pexf(sym->fexpr_sel_y),
					sym->list_sel_y);
		sym_add_constraint(sym, c1);

		/* only continue for tristates */
		if (sym->type == S_BOOLEAN)
			continue;

		struct pexpr *sel_m = pexpr_implies(
					pexf(sym->fexpr_sel_m),
					sym_get_fexpr_both(sym));
		sym_add_constraint(sym, sel_m);

		struct pexpr *c2 = pexpr_implies(
					pexf(sym->fexpr_sel_m),
					sym->list_sel_m);
		sym_add_constraint(sym, c2);
	}
}

/*
 * build constraints for non-booleans
 */
static void get_constraints_nonbool(void)
{
	unsigned int i;
	struct symbol *sym;
	for_all_symbols(i, sym) {

		if (!sym_is_nonboolean(sym))
			continue;

		/* the symbol must have a value, if there is a prompt */
		if (sym_has_prompt(sym))
			sym_add_nonbool_prompt_constraint(sym);

		/* build the range constraints for int/hex */
		if (sym->type == S_HEX || sym->type == S_INT)
			sym_add_range_constraints(sym);

		/* build constraints for dependencies for non-booleans */
		if (sym->dir_dep.expr)
			add_dependencies_nonbool(sym);

		/* build invisible constraints */
		add_invisible_constraints(sym);

		/* exactly one of the symbols must be true */
		sym_nonbool_at_least_1(sym);
		sym_nonbool_at_most_1(sym);
	}
}

/*
 * enforce tristate constraints
 */
static void build_tristate_constraint_clause(struct symbol *sym)
{
	if (sym->type != S_TRISTATE)
		return;

	struct pexpr *X = pexf(sym->fexpr_y), *X_m = pexf(sym->fexpr_m), *modules = pexf(modules_sym->fexpr_y);

	/* -X v -X_m */
	struct pexpr *c = pexpr_or(pexpr_not(X), pexpr_not(X_m));
	sym_add_constraint(sym, c);

	/* X_m -> MODULES */
	if (modules_sym->fexpr_y != NULL) {
		struct pexpr *c2 = pexpr_implies(X_m, modules);
		sym_add_constraint(sym, c2);
	}
}

/*
 * build the select constraints
 * - RDep(X) implies X
 */
static void add_selects_kcr(struct symbol *sym)
{
	struct pexpr *rdep_y = expr_calculate_pexpr_y(sym->rev_dep.expr);
	struct pexpr *c1 = pexpr_implies(rdep_y, pexf(sym->fexpr_y));
	sym_add_constraint(sym, c1);

	struct pexpr *rdep_both = expr_calculate_pexpr_both(sym->rev_dep.expr);
	struct pexpr *c2 = pexpr_implies(rdep_both, sym_get_fexpr_both(sym));
	sym_add_constraint(sym, c2);
}

/*
 * build the select constraints simplified
 * - RDep(X) implies X
 */
static void add_selects(struct symbol *sym)
{
	if (!sym_is_boolean(sym))
		return;

	struct property *p;
	for_all_properties(sym, p, P_SELECT) {
		struct symbol *selected = p->expr->left.sym;

		if (selected->type == S_UNKNOWN)
			continue;

		if (!selected->rev_dep.expr)
			continue;

		struct pexpr *cond_y = pexf(const_true);
		struct pexpr *cond_both = pexf(const_true);
		if (p->visible.expr) {
			cond_y = expr_calculate_pexpr_y(p->visible.expr);
			cond_both = expr_calculate_pexpr_both(p->visible.expr);
		}

		if (selected->type == S_BOOLEAN) {
			/* imply that symbol is selected to y */
			struct pexpr *e1 = pexpr_and(cond_both, sym_get_fexpr_both(sym));
			struct pexpr *c1 = pexpr_implies(e1, pexf(selected->fexpr_sel_y));
			sym_add_constraint(selected, c1);

			if (selected->list_sel_y == NULL)
				selected->list_sel_y = e1;
			else
				selected->list_sel_y = pexpr_or(selected->list_sel_y, e1);
		}

		if (selected->type == S_TRISTATE) {
			/* imply that symbol is selected to y */
			struct pexpr *e2 = pexpr_and(cond_y, pexf(sym->fexpr_y));
			struct pexpr *c2 = pexpr_implies(e2, pexf(selected->fexpr_sel_y));
			sym_add_constraint(selected, c2);

			if (selected->list_sel_y == NULL)
				selected->list_sel_y = e2;
			else
				selected->list_sel_y = pexpr_or(selected->list_sel_y, e2);

			/* imply that symbol is selected to m */
			struct pexpr *e3 = pexpr_and(cond_both, sym_get_fexpr_both(sym));
			struct pexpr *c3 = pexpr_implies(e3, pexf(selected->fexpr_sel_m));
			sym_add_constraint(selected, c3);

			if (selected->list_sel_m == NULL)
				selected->list_sel_m = e3;
			else
				selected->list_sel_m = pexpr_or(selected->list_sel_m, e3);
		}
	}
}

/*
 * build the dependency constraints for booleans
 *  - X implies Dep(X) or RDep(X)
 */
static void add_dependencies_bool(struct symbol *sym)
{
	if (!sym_is_boolean(sym) || !sym->dir_dep.expr)
		return;

	struct pexpr *dep_both = expr_calculate_pexpr_both(sym->dir_dep.expr);

	if (sym->type == S_TRISTATE) {
		struct pexpr *dep_y = expr_calculate_pexpr_y(sym->dir_dep.expr);
		struct pexpr *sel_y = sym->rev_dep.expr ? pexf(sym->fexpr_sel_y) : pexf(const_false);

		struct pexpr *c1 = pexpr_implies(pexf(sym->fexpr_y), pexpr_or(dep_y, sel_y));

		sym_add_constraint(sym, c1);

		struct pexpr *c2 = pexpr_implies(pexf(sym->fexpr_m), pexpr_or(dep_both, sym_get_fexpr_sel_both(sym)));

		sym_add_constraint(sym, c2);
	} else if (sym->type == S_BOOLEAN) {
		struct pexpr *c = pexpr_implies(pexf(sym->fexpr_y), pexpr_or(dep_both, sym_get_fexpr_sel_both(sym)));
		sym_add_constraint(sym, c);
	}
}

/*
 * build the dependency constraints for booleans (KCR)
 *  - X implies Dep(X) or RDep(X)
 */
static void add_dependencies_bool_kcr(struct symbol *sym)
{
	if (!sym_is_boolean(sym) || !sym->dir_dep.expr)
		return;

	struct pexpr *dep_both = expr_calculate_pexpr_both(sym->dir_dep.expr);

	struct pexpr *sel_both = sym->rev_dep.expr ? expr_calculate_pexpr_both(sym->rev_dep.expr) : pexf(const_false);

	if (sym->type == S_TRISTATE) {
		struct pexpr *dep_y = expr_calculate_pexpr_y(sym->dir_dep.expr);
		struct pexpr *sel_y = sym->rev_dep.expr ? expr_calculate_pexpr_y(sym->rev_dep.expr) : pexf(const_false);
		struct pexpr *c1 = pexpr_implies(pexf(sym->fexpr_y), pexpr_or(dep_y, sel_y));
		sym_add_constraint(sym, c1);

		struct pexpr *c2 = pexpr_implies(pexf(sym->fexpr_m), pexpr_or(dep_both, sel_both));
		sym_add_constraint(sym, c2);
	} else if (sym->type == S_BOOLEAN) {
		struct pexpr *c = pexpr_implies(pexf(sym->fexpr_y), pexpr_or(dep_both, sel_both));
		sym_add_constraint(sym, c);
	}
}

/*
 * build the dependency constraints for non-booleans
 * X_i implies Dep(X)
 */
static void add_dependencies_nonbool(struct symbol *sym)
{
	if (!sym_is_nonboolean(sym) || !sym->dir_dep.expr || sym->rev_dep.expr)
		return;

	struct pexpr *dep_both = expr_calculate_pexpr_both(sym->dir_dep.expr);

	struct pexpr *nb_vals = pexf(const_false);
	struct fexpr_node *node;
	/* can skip the first non-boolean value, since this is 'n' */
	fexpr_list_for_each(node, sym->nb_vals) {
		if (node->prev == NULL)
			continue;

		nb_vals = pexpr_or(nb_vals, pexf(node->elem));
	}

	struct pexpr *c = pexpr_implies(nb_vals, dep_both);
	sym_add_constraint(sym, c);
}

/*
 * build the constraints for the choice prompt
 */
static void add_choice_prompt_cond(struct symbol* sym)
{
	if (!sym_is_boolean(sym))
		return;

	struct property *prompt = sym_get_prompt(sym);
	if (prompt == NULL)
		return;

	struct pexpr *promptCondition = prompt->visible.expr ? expr_calculate_pexpr_both(prompt->visible.expr) : pexf(const_true);

	struct pexpr *fe_both = sym_get_fexpr_both(sym);

	if (!sym_is_optional(sym)) {
		struct pexpr *req_cond = pexpr_implies(promptCondition, fe_both);
		sym_add_constraint(sym, req_cond);
	}

	struct pexpr *pr_cond = pexpr_implies(fe_both, promptCondition);
	sym_add_constraint(sym, pr_cond);
}

/*
 * build constraints for dependencies (choice symbols and options)
 */
static void add_choice_dependencies(struct symbol *sym)
{
	if (!sym_is_choice(sym) || !sym_is_choice_value(sym))
		return;

	struct property *prompt = sym_get_prompt(sym);
	if (prompt == NULL)
		return;

	struct expr *to_parse;
	if (sym_is_choice(sym)) {
		if (!prompt->visible.expr)
			return;
		to_parse = prompt->visible.expr;
	} else {
		if (!sym->dir_dep.expr)
			return;
		to_parse = sym->dir_dep.expr;
	}

	struct pexpr *dep_both = expr_calculate_pexpr_both(to_parse);

	if (sym->type == S_TRISTATE) {
		struct pexpr *dep_y = expr_calculate_pexpr_y(to_parse);
		struct pexpr *c1 = pexpr_implies(pexf(sym->fexpr_y), dep_y);
		sym_add_constraint_eq(sym, c1);

		struct pexpr *c2 = pexpr_implies(pexf(sym->fexpr_m), dep_both);
		sym_add_constraint_eq(sym, c2);
	} else if (sym->type == S_BOOLEAN) {
		struct pexpr *c = pexpr_implies(pexf(sym->fexpr_y), dep_both);
		sym_add_constraint_eq(sym, c);
	}
}

/*
 * build constraints for the choice groups
 */
static void add_choice_constraints(struct symbol *sym)
{
	if (!sym_is_boolean(sym))
		return;

	struct property *prompt = sym_get_prompt(sym);
	if (prompt == NULL)
		return;

	struct symbol *choice, *choice2;
	struct sym_node *node, *node2;

	/* create list of all choice options */
	struct sym_list *items = sym_list_init();
	/* create list of choice options with a prompt */
	struct sym_list *promptItems = sym_list_init();

	struct property *prop;
	for_all_choices(sym, prop) {
		struct expr *expr;
		expr_list_for_each_sym(prop->expr, expr, choice) {
			sym_list_add(items, choice);
			if (sym_get_prompt(choice) != NULL)
				sym_list_add(promptItems, choice);
		}
	}

	/* if the choice is set to yes, at least one child must be set to yes */
	struct pexpr *c1 = NULL;
	sym_list_for_each(node, promptItems) {
		choice = node->elem;
		c1 = node->prev == NULL ? pexf(choice->fexpr_y) : pexpr_or(c1, pexf(choice->fexpr_y));
	}
	if (c1 != NULL) {
		struct pexpr *c2 = pexpr_implies(pexf(sym->fexpr_y), c1);
		sym_add_constraint(sym, c2);
	}

	/* every choice option (even those without a prompt) implies the choice */
	sym_list_for_each(node, items) {
		choice = node->elem;
		c1 = pexpr_implies(sym_get_fexpr_both(choice), sym_get_fexpr_both(sym));
		sym_add_constraint(sym, c1);
	}

	/* choice options can only select mod, if the entire choice is mod */
	if (sym->type == S_TRISTATE) {
		sym_list_for_each(node, items) {
			choice = node->elem;
			if (choice->type == S_TRISTATE) {
				c1 = pexpr_implies(pexf(choice->fexpr_m), pexf(sym->fexpr_m));
				sym_add_constraint(sym, c1);
			}
		}
	}

	/* tristate options cannot be m, if the choice symbol is boolean */
	if (sym->type == S_BOOLEAN) {
		sym_list_for_each(node, items) {
			choice = node->elem;
			if (choice->type == S_TRISTATE)
				sym_add_constraint(sym, pexpr_not(pexf(choice->fexpr_m)));
		}
	}

	/* all choice options are mutually exclusive for yes */
	sym_list_for_each(node, promptItems) {
		choice = node->elem;
		for (node2 = node->next; node2 != NULL; node2 = node2->next) {
			choice2 = node2->elem;
			c1 = pexpr_or(pexpr_not(pexf(choice->fexpr_y)), pexpr_not(pexf(choice2->fexpr_y)));
			sym_add_constraint(sym, c1);
		}
	}

	/* if one choice option with a prompt is set to yes,
	 * then no other option may be set to mod */
	if (sym->type == S_TRISTATE) {
		sym_list_for_each(node, promptItems) {
			choice = node->elem;

			struct sym_list *tmp = sym_list_init();
			for (node2 = node->next; node2 != NULL; node2 = node2->next) {
				choice2 = node2->elem;
				if (choice2->type == S_TRISTATE)
					sym_list_add(tmp, choice2);
			}
			if (tmp->size == 0) continue;

			sym_list_for_each(node2, tmp) {
				choice2 = node2->elem;
				if (node2->prev == NULL)
					c1 = pexpr_not(pexf(choice2->fexpr_m));
				else
					c1 = pexpr_and(c1, pexpr_not(pexf(choice2->fexpr_m)));
			}
			c1 = pexpr_implies(pexf(choice->fexpr_y), c1);
			sym_add_constraint(sym, c1);
		}
	}
}

/*
 * build the constraints for invisible options such as defaults
 */
static void add_invisible_constraints(struct symbol *sym)
{
	struct property *prompt = sym_get_prompt(sym);

	/* no constraints for the prompt, nothing to do here */
	if (prompt != NULL && !prompt->visible.expr)
		return;

	struct pexpr *promptCondition_both, *promptCondition_yes, *noPromptCond;
	if (prompt == NULL) {
		promptCondition_both = pexf(const_false);
		promptCondition_yes = pexf(const_false);
		noPromptCond = pexf(const_true);
	} else {
		promptCondition_both = pexf(const_false);
		promptCondition_yes = pexf(const_false);

		/* some symbols have multiple prompts */
		struct property *p;
		for_all_prompts(sym, p) {
			promptCondition_both = pexpr_or(promptCondition_both,
				expr_calculate_pexpr_both(p->visible.expr));
			promptCondition_yes = pexpr_or(promptCondition_yes,
				expr_calculate_pexpr_y(p->visible.expr));
		}
		noPromptCond = pexpr_not(promptCondition_both);
	}

	struct pexpr *npc;
	if (NPC_OPTIMISATION) {
		struct fexpr * npc_fe = fexpr_create(sat_variable_nr++, FE_NPC, "");
		if (sym_is_choice(sym))
			str_append(&npc_fe->name, "Choice_");

		str_append(&npc_fe->name, sym_get_name(sym));
		str_append(&npc_fe->name, "_NPC");
		sym->noPromptCond = npc_fe;
		fexpr_add_to_satmap(npc_fe);

		npc = pexf(npc_fe);

		struct pexpr *c = pexpr_implies(noPromptCond, npc);

		if (!sym_is_choice_value(sym) && !sym_is_choice(sym))
			sym_add_constraint(sym, c);
	} else {
		npc = noPromptCond;
	}

	struct defm_list *defaults = get_defaults(sym);
	struct pexpr *default_y = get_default_y(defaults);
	struct pexpr *default_m = get_default_m(defaults);
	struct pexpr *default_both = pexpr_or(default_y, default_m);

	/* tristate elements are only selectable as yes, if they are visible as yes */
	if (sym->type == S_TRISTATE) {
		struct pexpr *e1 = pexpr_implies(promptCondition_both, pexpr_implies(pexf(sym->fexpr_y), promptCondition_yes));
		sym_add_constraint(sym, e1);
	}

	/* if invisible and off by default, then a symbol can only be deactivated by its reverse dependencies */
	if (sym->type == S_TRISTATE) {
		struct pexpr *sel_y, *sel_m, *sel_both;
		if (sym->fexpr_sel_y != NULL) {
			sel_y = pexpr_implies(pexf(sym->fexpr_y), pexf(sym->fexpr_sel_y));
			sel_m = pexpr_implies(pexf(sym->fexpr_m), pexf(sym->fexpr_sel_m));
			sel_both = pexpr_implies(pexf(sym->fexpr_y), pexpr_or(pexf(sym->fexpr_sel_m), pexf(sym->fexpr_sel_y)));
		} else {
			sel_y = pexpr_not(pexf(sym->fexpr_y));
			sel_m = pexpr_not(pexf(sym->fexpr_m));
			sel_both = sel_y;
		}

		struct pexpr *c1 = pexpr_implies(pexpr_not(default_y), sel_y);
		struct pexpr *c2 = pexpr_implies(pexf(modules_sym->fexpr_y), c1);
		struct pexpr *c3 = pexpr_implies(npc, c2);
		sym_add_constraint(sym, c3);

		struct pexpr *d1 = pexpr_implies(pexpr_not(default_m), sel_m);
		struct pexpr *d2 = pexpr_implies(pexf(modules_sym->fexpr_y), d1);
		struct pexpr *d3 = pexpr_implies(npc, d2);
		sym_add_constraint(sym, d3);

		struct pexpr *e1 = pexpr_implies(pexpr_not(default_both), sel_both);
		struct pexpr *e2 = pexpr_implies(pexpr_not(pexf(modules_sym->fexpr_y)), e1);
		struct pexpr *e3 = pexpr_implies(npc, e2);
		sym_add_constraint(sym, e3);
	} else if (sym->type == S_BOOLEAN) {
		struct pexpr *sel_y;
		if (sym->fexpr_sel_y != NULL)
			sel_y = pexpr_implies(pexf(sym->fexpr_y), pexf(sym->fexpr_sel_y)); //sym->fexpr_sel_y;
		else
			sel_y = pexpr_not(pexf(sym->fexpr_y));

		struct pexpr *e1 = pexpr_implies(pexpr_not(default_both), sel_y);
		struct pexpr *e2 = pexpr_implies(npc, e1);

		sym_add_constraint_eq(sym, e2);
	} else {
		struct pexpr *default_any = get_default_any(sym);
		struct pexpr *e1 = pexf(const_true);
		struct fexpr_node *node;
		for (node = sym->nb_vals->head->next; node != NULL; node = node->next)
			e1 = pexpr_and(e1, pexpr_not(pexf(node->elem)));

		struct pexpr *e2 = pexpr_implies(pexpr_not(default_any), e1);
		struct pexpr *e3 = pexpr_implies(npc, e2);

		sym_add_constraint(sym, e3);
	}

	/* if invisible and on by default, then a symbol can only be deactivated by its dependencies */
	if (sym->type == S_TRISTATE) {
		if (defaults->size == 0)
			return;

		struct pexpr *e1 = pexpr_implies(npc, pexpr_implies(default_y, pexf(sym->fexpr_y)));
		sym_add_constraint(sym, e1);

		struct pexpr *e2 = pexpr_implies(npc, pexpr_implies(default_m, sym_get_fexpr_both(sym)));
		sym_add_constraint(sym, e2);
	} else if (sym->type == S_BOOLEAN) {
		if (defaults->size == 0)
			return;

		struct pexpr *c = pexpr_implies(default_both, pexf(sym->fexpr_y));

		// TODO tristate choice hack

		struct pexpr *c2 = pexpr_implies(npc, c);
		sym_add_constraint(sym, c2);
	} else {
		struct defm_node *node;
		struct pexpr *cond, *c;
		struct fexpr *f;
		defm_list_for_each(node, defaults) {
			f = node->elem->val;
			cond = node->elem->e;
			c = pexpr_implies(npc, pexpr_implies(cond, pexf(f)));
			sym_add_constraint(sym, c);
		}
	}
}

/*
 * add the known values from the default and range properties
 */
static void sym_add_nonbool_values_from_default_range(struct symbol *sym)
{
	struct property *p;

	for_all_defaults(sym, p) {
		if (p == NULL)
			continue;

		/* add the value to known values, if it doesn't exist yet */
		sym_create_nonbool_fexpr(sym, p->expr->left.sym->name);
	}

	for_all_properties(sym, p, P_RANGE) {
		if (p == NULL)
			continue;

		/* add the values to known values, if they don't exist yet */
		sym_create_nonbool_fexpr(sym, p->expr->left.sym->name);
		sym_create_nonbool_fexpr(sym, p->expr->right.sym->name);
	}
}

/*
 * build the range constraints for int/hex
 */
static void sym_add_range_constraints(struct symbol *sym)
{
	struct property *prop;
	struct pexpr *prevs, *propCond;
	struct pexpr_list *prevCond = pexpr_list_init();
	for_all_properties(sym, prop, P_RANGE) {
		if (prop == NULL)
			continue;

		prevs = pexf(const_true);
		propCond = prop_get_condition(prop);

		if (prevCond->size == 0) {
			prevs = propCond;
		} else {
			struct pexpr_node *node;
			pexpr_list_for_each(node, prevCond)
				prevs = pexpr_and(pexpr_not(node->elem), prevs);

			prevs = pexpr_and(propCond, prevs);
		}
		pexpr_list_add(prevCond, propCond);

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

		/* can skip the first non-boolean value, since this is 'n' */
		struct fexpr_node *node;
		fexpr_list_for_each(node, sym->nb_vals) {
			if (node->prev == NULL)
				continue;

			tmp = strtoll(str_get(&node->elem->nb_val), NULL, base);

			/* known value is in range, nothing to do here */
			if (tmp >= range_min && tmp <= range_max)
				continue;

			struct pexpr *not_nb_val = pexpr_not(pexf(node->elem));
			if (tmp < range_min) {
				struct pexpr *c = pexpr_implies(prevs, not_nb_val);
				sym_add_constraint(sym, c);
			}

			if (tmp > range_max) {
				struct pexpr *c = pexpr_implies(prevs, not_nb_val);
				sym_add_constraint(sym, c);
			}
		}
	}
}

/*
 * at least 1 of the known values for a non-boolean symbol must be true
 */
static void sym_nonbool_at_least_1(struct symbol *sym)
{
	if (!sym_is_nonboolean(sym))
		return;

	struct pexpr *e = NULL;
	struct fexpr_node *node;
	fexpr_list_for_each(node, sym->nb_vals) {
		if (node->prev == NULL)
			e = pexf(node->elem);
		else
			e = pexpr_or(e, pexf(node->elem));
	}
	sym_add_constraint(sym, e);
}

/*
 * at most 1 of the known values for a non-boolean symbol can be true
 */
static void sym_nonbool_at_most_1(struct symbol *sym)
{
	if (!sym_is_nonboolean(sym))
		return;

	struct pexpr *e1, *e2;
	struct fexpr_node *node1, *node2;
	fexpr_list_for_each(node1, sym->nb_vals) {
		e1 = pexf(node1->elem);
		for (node2 = node1->next; node2 != NULL; node2 = node2->next) {
			e2 = pexf(node2->elem);
			struct pexpr *e = pexpr_or(pexpr_not(e1), pexpr_not(e2));
			sym_add_constraint(sym, e);
		}
	}
}

/*
 * a visible prompt for a non-boolean implies a value for the symbol
 */
static void sym_add_nonbool_prompt_constraint(struct symbol *sym)
{
	struct property *prompt = sym_get_prompt(sym);
	if (prompt == NULL)
		return;

	struct pexpr *promptCondition = prop_get_condition(prompt);
	struct pexpr *n = pexf(sym_get_nonbool_fexpr(sym, "n"));

	if (n->type != PE_SYMBOL)
		return;
	if (n->left.fexpr == NULL)
		return;

	struct pexpr *c = pexpr_implies(promptCondition, pexpr_not(n));

	sym_add_constraint(sym, c);
}

static struct default_map * create_default_map_entry(struct fexpr *val, struct pexpr *e)
{
	struct default_map *map = malloc(sizeof(struct default_map));
	map->val = val;
	map->e = e;

	return map;
}

static struct pexpr * findDefaultEntry(struct fexpr *val, struct defm_list *defaults)
{
	struct defm_node *node;
	defm_list_for_each(node, defaults)
		if (val == node->elem->val)
			return node->elem->e;

	return pexf(const_false);
}

/* add a default value to the list */

/*
 * return all defaults for a symbol
 */
static struct pexpr *covered;
static bool is_tri_as_num(struct symbol *sym) {
	if (!sym->name)
		return false;

	return !strcmp(sym->name, "0") ||
		!strcmp(sym->name, "1") ||
		!strcmp(sym->name, "2");
}
static void add_to_default_map(struct defm_list *defaults, struct default_map *entry, struct symbol *sym)
{
	/* as this is a map, the entry must be replaced if it already exists */
	if (sym_is_boolean(sym)) {
		struct default_map *map;
		struct defm_node *node;
		defm_list_for_each(node, defaults) {
			map = node->elem;
			if (map->val->sym == entry->val->sym) {
				map->e = entry->e;
				return;
			}
		}
		defm_list_add(defaults, entry);
	} else {
		struct default_map *map;
		struct defm_node *node;
		defm_list_for_each(node, defaults) {
			map = node->elem;
			if (map->val->satval == entry->val->satval) {
				map->e = entry->e;
				return;
			}
		}
		defm_list_add(defaults, entry);
	}
}
static void updateDefaultList(struct fexpr *val, struct pexpr *newCond, struct defm_list *result, struct symbol *sym)
{
	struct pexpr *prevCond = findDefaultEntry(val, result);
	struct pexpr *cond = pexpr_or(prevCond, pexpr_and(newCond, pexpr_not(covered)));
	struct default_map *entry = create_default_map_entry(val, cond);
	add_to_default_map(result, entry, sym);
	covered = pexpr_or(covered, newCond);
}
static void add_defaults(struct prop_list *defaults, struct expr *ctx, struct defm_list *result, struct symbol *sym)
{
	struct prop_node *node;
	struct property *p;
	struct expr *expr;

	prop_list_for_each(node, defaults) {
		p = node->elem;
		if (p->visible.expr) {
			if (ctx == NULL)
				expr = p->visible.expr;
			else
				expr = expr_alloc_and(p->visible.expr, ctx);
		} else {
			if (ctx == NULL)
				expr = expr_alloc_symbol(&symbol_yes);
			else
				expr = expr_alloc_and(expr_alloc_symbol(&symbol_yes), ctx);
		}

		/* if tristate and def.value = y */
		if (p->expr->type == E_SYMBOL && sym->type == S_TRISTATE && p->expr->left.sym == &symbol_yes) {
			struct pexpr *expr_y = expr_calculate_pexpr_y(expr);
			struct pexpr *expr_m = expr_calculate_pexpr_m(expr);

			updateDefaultList(symbol_yes_fexpr, expr_y, result, sym);
			updateDefaultList(symbol_mod_fexpr, expr_m, result, sym);
		}
		/* if def.value = n/m/y */
		else if (p->expr->type == E_SYMBOL && sym_is_tristate_constant(p->expr->left.sym)) {
			struct fexpr *s;
			if (p->expr->left.sym == &symbol_yes)
				s = symbol_yes_fexpr;
			else if (p->expr->left.sym == &symbol_mod)
				s = symbol_mod_fexpr;
			else
				s = symbol_no_fexpr;

			updateDefaultList(s, expr_calculate_pexpr_both(expr), result, sym);
		}
		/* if def.value = n/m/y, but written as 0/1/2 for a boolean */
		else if (sym_is_boolean(sym) &&
			p->expr->type == E_SYMBOL &&
			p->expr->left.sym->type == S_UNKNOWN &&
			is_tri_as_num(p->expr->left.sym)) {

			struct fexpr *s;
			if (!strcmp(p->expr->left.sym->name, "0"))
				s = symbol_no_fexpr;
			else if (!strcmp(p->expr->left.sym->name, "1"))
				s = symbol_mod_fexpr;
			else
				s = symbol_yes_fexpr;

			updateDefaultList(s, expr_calculate_pexpr_both(expr), result, sym);
		}
		/* if def.value = non-boolean constant */
		else if (expr_is_nonbool_constant(p->expr)) {
			struct fexpr *s = sym_get_or_create_nonbool_fexpr(sym, p->expr->left.sym->name);
			updateDefaultList(s, expr_calculate_pexpr_both(expr), result, sym);
		}
		/* any expression which evaluates to n/m/y for a tristate */
		else if (sym->type == S_TRISTATE) {
			struct expr *e_tmp = expr_alloc_and(p->expr, expr);
			struct pexpr *expr_y = expr_calculate_pexpr_y(e_tmp);
			struct pexpr *expr_m = expr_calculate_pexpr_m(e_tmp);
			updateDefaultList(symbol_yes_fexpr, expr_y, result, sym);
			updateDefaultList(symbol_mod_fexpr, expr_m, result, sym);
		}
		/* if non-boolean && def.value = non-boolean symbol */
		else if (p->expr->type == E_SYMBOL && sym_is_nonboolean(sym) && sym_is_nonboolean(p->expr->left.sym)) {
			struct prop_list *nb_sym_defaults = prop_list_init();
			struct property *p_tmp;
			for_all_defaults(p->expr->left.sym, p_tmp)
				prop_list_add(nb_sym_defaults, p_tmp);

			add_defaults(nb_sym_defaults, expr, result, sym);
		}
		/* any expression which evaluates to n/m/y */
		else {
			struct expr *e_tmp = expr_alloc_and(p->expr, expr);
			struct pexpr *expr_both = expr_calculate_pexpr_both(e_tmp);
			updateDefaultList(symbol_yes_fexpr, expr_both, result, sym);
		}
	}
}
static struct defm_list * get_defaults(struct symbol *sym)
{
	struct defm_list *result = defm_list_init();
	covered = pexf(const_false);

	struct prop_list *defaults = prop_list_init();
	struct property *p;
	for_all_defaults(sym, p)
		prop_list_add(defaults, p);

	add_defaults(defaults, NULL, result, sym);

	return result;
}

/*
 * return the default_map for "y", False if it doesn't exist
 */
static struct pexpr * get_default_y(struct defm_list *list)
{
	struct default_map *entry;
	struct defm_node *node;

	defm_list_for_each(node, list) {
		entry = node->elem;
		if (entry->val->type == FE_SYMBOL && entry->val->sym == &symbol_yes)
			return entry->e;
	}

	return pexf(const_false);
}

/*
 * return the default map for "m", False if it doesn't exist
 */
static struct pexpr *get_default_m(struct defm_list *list)
{
	struct default_map *entry;
	struct defm_node *node;

	defm_list_for_each(node, list) {
		entry = node->elem;
		if (entry->val->type == FE_SYMBOL && entry->val->sym == &symbol_mod)
			return entry->e;
	}

	return pexf(const_false);
}

/*
 * return the constraint when _some_ default value will be applied
 */
static struct pexpr *get_default_any(struct symbol *sym)
{
	if (!sym_is_nonboolean(sym))
		return NULL;

	struct property *prop;
	struct expr *e;
	struct pexpr *p = pexf(const_false);
	for_all_defaults(sym, prop) {
		if (prop->visible.expr)
			e = prop->visible.expr;
		else
			e = expr_alloc_symbol(&symbol_yes);

		if (expr_can_evaluate_to_mod(e))
			p = pexpr_or(p, expr_calculate_pexpr_both(e));

		p = pexpr_or(p, expr_calculate_pexpr_y(e));
	}

	return p;
}

/*
 * get the value for the range
 */
static long sym_get_range_val(struct symbol *sym, int base)
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
	return strtol(sym->curr.val, NULL, base);
}

/*
 * count the number of all constraints
 */
unsigned int count_counstraints(void)
{
	unsigned int i, c = 0;
	struct symbol *sym;
	for_all_symbols(i, sym) {
		if (sym->type == S_UNKNOWN)
			continue;

		c += sym->constraints->size;
	}

	return c;
}

/*
 * add a constraint for a symbol
 */
void sym_add_constraint(struct symbol *sym, struct pexpr *constraint)
{
	if (!constraint)
		return;

	/* no need to add that */
	if (constraint->type == PE_SYMBOL && constraint->left.fexpr == const_true)
		return;

	/* this should never happen */
	if (constraint->type == PE_SYMBOL && constraint->left.fexpr == const_false)
		perror("Adding const_false.");

	pexpr_list_add(sym->constraints, constraint);

	if (!pexpr_is_nnf(constraint))
		pexpr_print("Not NNF:", constraint, -1);
}

/*
 * add a constraint for a symbol, but check for duplicate constraints
 */
void sym_add_constraint_eq(struct symbol *sym, struct pexpr *constraint)
{
	if (!constraint)
		return;

	/* no need to add that */
	if (constraint->type == PE_SYMBOL && constraint->left.fexpr == const_true)
		return;

	/* this should never happen */
	if (constraint->type == PE_SYMBOL && constraint->left.fexpr == const_false)
		perror("Adding const_false.");

	/* check the constraints for the same symbol */
	struct pexpr_node *node;
	pexpr_list_for_each(node, sym->constraints)
		if (pexpr_eq(constraint, node->elem))
			return;

	pexpr_list_add(sym->constraints, constraint);

	if (!pexpr_is_nnf(constraint))
		pexpr_print("Not NNF:", constraint, -1);
}
