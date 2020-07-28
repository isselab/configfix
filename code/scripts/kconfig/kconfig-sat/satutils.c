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

static void run_unsat_problem(PicoSAT *pico);
static void add_select_constraints(PicoSAT *pico, struct symbol *sym);
static void sym_desired_value(void);

/* -------------------------------------- */

/*
 * initialize PicoSAT
 */
PicoSAT * initialize_picosat(void)
{
	// TODO
	printf("\nInitializing PicoSAT...");
	PicoSAT *pico = picosat_init();
	picosat_enable_trace_generation(pico);
	printf("done.\n");
	
	return pico;
}

/*
 * add a clause to PicoSAT
 * First argument is the SAT solver
 */
void sat_add_clause(int num, ...)
{
	if (num <= 1) return;
	
	va_list valist;
	int i, *id, *lit;
	PicoSAT *pico;
	GArray *arr = g_array_new(false, false, sizeof(int *));
	
	/* initialize valist for num number of arguments */
	va_start(valist, num);
	
	pico = va_arg(valist, PicoSAT *);
	
	/* access all the arguments assigned to valist */
	for (i = 1; i < num; i++) {
		lit = malloc(sizeof(int));
		*lit = va_arg(valist, int);
		picosat_add(pico, *lit);
		g_array_append_val(arr, lit);
	}
	id = malloc(sizeof(int));
	*id = picosat_add(pico, 0);
// 	*id = g_hash_table_size(cnf_clauses);
	
	/* add clause to hashmap */
	g_hash_table_insert(cnf_clauses_map, id, arr);
	
// 	printf("Clause added, id %d\n", id);
	
	/* clean memory reserved for valist */
	va_end(valist);
}

/* 
 * add a clause from a GArray to PicoSAT 
 */
void sat_add_clause_garray(PicoSAT *pico, GArray *arr)
{
	int i, *id, *lit;

	for (i = 0; i < arr->len; i++) {
		lit = g_array_index(arr, int *, i);
		picosat_add(pico, *lit);
	}
	id = malloc(sizeof(int));
	*id = picosat_add(pico, 0);
	
	/* add clause to hashmap if not done yet */
	if (!g_hash_table_contains(cnf_clauses_map, id))
		g_hash_table_insert(cnf_clauses_map, id, arr);
}

/*
 * add clauses to the PicoSAT
 */
// void picosat_add_clauses(PicoSAT *pico)
// {
// 	printf("Adding clauses...");
// 	
// 	int tmp_clauses = nr_of_clauses;
// 	
// 	clock_t start, end;
// 	double time;
// 	start = clock();
// 
// 	/* add CNF-clauses to PicoSAT
// 	 * ignore tautologies */
// 	struct cnf_clause *cl;
// 	unsigned int i, j;
// 	for (i = 0; i < cnf_clauses->len; i++) {
// 		cl = g_array_index(cnf_clauses, struct cnf_clause *, i);
// 		if (cnf_is_tautology(cl)) {
// 			tmp_clauses--;
// 			continue;
// 		}
// 		
// 		struct cnf_literal *lit;
// 		for (j = 0; j < cl->lits->len; j++) {
// 			lit = g_array_index(cl->lits, struct cnf_literal *, j);
// 			picosat_add(pico, lit->val);
// 			if (j == cl->lits->len - 1)
// 				picosat_add(pico, 0);
// 		}
// 	}
// 	
// 	end = clock();
// 	time = ((double) (end - start)) / CLOCKS_PER_SEC;
// 	printf("%d clauses added. (%.6f secs.)\n", tmp_clauses, time);
// 
// 	assert(tmp_clauses == picosat_added_original_clauses(pico));
// }

/*
 * start PicoSAT
 */
void picosat_solve(PicoSAT *pico)
{
	printf("Solving SAT-problem...");
	
	/* add assumptions */
	struct symbol *sym;
	unsigned int i;
	for_all_symbols(i, sym) {
		if (sym->type == S_UNKNOWN) continue;
		
		sym_add_assumption(pico, sym);
	}
	
	clock_t start, end;
	double time;
	start = clock();
	
	int res = picosat_sat(pico, -1);
	
	end = clock();
	time = ((double) (end - start)) / CLOCKS_PER_SEC;
	printf("done. (%.6f secs.)\n\n", time);
	
	if (res == PICOSAT_SATISFIABLE) {
		printf("===> PROBLEM IS SATISFIABLE <===\n");
		
// 		struct symbol *sym;
// 		bool found = false;
// 		
// 		/* check, if a symbol has been selected, but has unmet dependencies */
// 		for_all_symbols(i, sym) {
// 			if (sym->dir_dep.tri < sym->rev_dep.tri) {
// 				found = true;
// 				sym_warn_unmet_dep(sym);
// 				add_select_constraints(pico, sym);
// 				printf("Symbol %s has unmet dependencies.\n", sym->name);
// 			}
// 		}
// 		if (!found) return;
// 		printf("\n");
// 		
// 		/* add assumptions again */
// 		for_all_symbols(i, sym)
// 			sym_add_assumption(pico, sym);
		
// 		int res = picosat_sat(pico, -1);
// 		
// 		/* problem should be unsatisfiable now */
// 		if (res == PICOSAT_UNSATISFIABLE) {
// 			run_unsat_problem(pico);
// 		} else {
// 			printf("SOMETHING IS A MISS. PROBLEM IS NOT UNSATISFIABLE.\n");
// 		}
		
// 		printf("All CNFs:\n");
// 		print_all_cnf_clauses( cnf_clauses );
		
	} else if (res == PICOSAT_UNSATISFIABLE) {
		printf("===> PROBLEM IS UNSATISFIABLE <===\n");
		
		/* print unsat core */
		printf("\nPrinting unsatisfiable core:\n");
		struct fexpr *e;
		for (i = 1; i < sat_variable_nr; i++) {
			if (picosat_failed_assumption(pico, i)) {
				int *index = malloc(sizeof(*index));
				*index = i;
				e = g_hash_table_lookup(satmap, index);
				printf("%s <%d>\n", str_get(&e->name), e->assumption);
			}
			
		}
// 		printf("\n");
		
		return;
		printf("\n");
		run_unsat_problem(pico);
	}
	else
		printf("Unknown if satisfiable.\n");
}

static void run_unsat_problem(PicoSAT *pico)
{
	/* get the diagnoses from RangeFix */
	GArray *diagnoses = rangefix_init(pico);
	
	/* ask user for solution to apply */
	GArray *fix = choose_fix(diagnoses);
	
	/* user chose no action, so exit */
	if (fix == NULL) return;
	
	/* apply the fix */ 
	apply_fix(fix);
}

/*
 * add the sharpened dependency constraints in order to make it unsatisfiable 
 */
static void add_select_constraints(PicoSAT *pico, struct symbol *sym)
{
// 	assert(sym->dir_dep.expr);
// 	
// 	struct k_expr *ke_dirdep = parse_expr(sym->dir_dep.expr, NULL);
// 	struct fexpr *dep_both = calculate_fexpr_both(ke_dirdep);
// 	int tmp_clauses = nr_of_clauses;
// 	
// 	if (sym->type == S_TRISTATE) {
// 		struct fexpr *dep_y = calculate_fexpr_y(ke_dirdep);
// 		struct fexpr *fe_y = implies(sym->fexpr_y, dep_y);
// 		convert_fexpr_to_nnf(fe_y);
// 		convert_fexpr_to_cnf(fe_y);
// 		unfold_cnf_clause(fe_y);
// 		
// 		struct fexpr *fe_both = implies(sym->fexpr_m, dep_both);
// 		convert_fexpr_to_nnf(fe_both);
// 		convert_fexpr_to_cnf(fe_both);
// 		unfold_cnf_clause(fe_both);
// 	} else if (sym->type == S_BOOLEAN) {
// 		struct fexpr *fe_both = implies(sym->fexpr_y, dep_both);
// 		convert_fexpr_to_nnf(fe_both);
// 		convert_fexpr_to_cnf(fe_both);
// 		unfold_cnf_clause(fe_both);
// 	}
// 	
// 	/* add newly created CNF-clauses to PicoSAT, ignore tautologies */
// 	struct cnf_clause *cl;
// 	unsigned int i, j;
// 	for (i = tmp_clauses; i < cnf_clauses->len; i++) {
// 		cl = g_array_index(cnf_clauses, struct cnf_clause *, i);
// 		if (cnf_is_tautology(cl)) {
// 			nr_of_clauses--;
// 			continue;
// 		}
// 		
// 		struct cnf_literal *lit;
// 		for (j = 0; j < cl->lits->len; j++) {
// 			lit = g_array_index(cl->lits, struct cnf_literal *, j);
// 			picosat_add(pico, lit->val);
// 			if (j == cl->lits->len - 1)
// 				picosat_add(pico, 0);
// 		}
// 	}
// 	printf("old %d, new %d\n", tmp_clauses, nr_of_clauses);
}

static void sym_desired_value(void)
{
	int choice;
	printf("\n> ");
	scanf("%d", &choice);
	
	/* no changes wanted */
	if (choice == 0) return;
}

/*
 * add assumption for a symbol to the SAT-solver
 */
void sym_add_assumption(PicoSAT *pico, struct symbol *sym)
{
	/*
	* TODO
	* Decide if we want the value from .config or the actual value,
	* which might differ because of prompt conditions.
	*/

	if (sym_is_boolean(sym)) {
// 		int tri_val = sym->def[S_DEF_USER].tri;
		int tri_val = sym_get_tristate_value(sym);
		
		sym_add_assumption_tri(pico, sym, tri_val);
		
		return;
	}
	
	if (sym_is_nonboolean(sym)) {
		struct fexpr *e = g_array_index(sym->fexpr_nonbool->arr, struct fexpr *, 0);
		unsigned int i;
		
		/* symbol does not have a value */
		if (!sym_has_value(sym)) {
			/* set value for sym=n */
			picosat_assume(pico, e->satval);
			e->assumption = true;
			
			/* set value for all other fexpr */
			for (i = 1; i < sym->fexpr_nonbool->arr->len; i++) {
				e = g_array_index(sym->fexpr_nonbool->arr, struct fexpr *, i);
				picosat_assume(pico, -(e->satval));
				e->assumption = false;
			}
			
			return;
		}
		
		/* symbol does have a value set */
		
		/* set value for sym=n */
		picosat_assume(pico, -(e->satval));
		e->assumption = false;
		
		const char *string_val = sym_get_string_value(sym);
		
		/* set value for all other fexpr */
		for (i = 1; i < sym->fexpr_nonbool->arr->len; i++) {
			e = g_array_index(sym->fexpr_nonbool->arr, struct fexpr *, i);
			if (strcmp(str_get(&e->nb_val), string_val) == 0) {
				picosat_assume(pico, e->satval);
				e->assumption = true;
			} else {
				picosat_assume(pico, -(e->satval));
				e->assumption = false;
			}
		}
	}
}

/*
 * add assumption for a boolean symbol to the SAT-solver
 */
void sym_add_assumption_tri(PicoSAT *pico, struct symbol *sym, tristate tri_val)
{
	if (sym->type == S_BOOLEAN) {
		int a = sym->fexpr_y->satval;
		switch (tri_val) {
		case no:
			picosat_assume(pico, -a);
			sym->fexpr_y->assumption = false;
			break;
		case mod:
			perror("Should not happen. Boolean symbol is set to mod.\n");
			break;
		case yes:
			picosat_assume(pico, a);
			sym->fexpr_y->assumption = true;
			break;
		}
	}
	if (sym->type == S_TRISTATE) {
		int a = sym->fexpr_y->satval;
		int a_m = sym->fexpr_m->satval;
		switch (tri_val) {
		case no:
			picosat_assume(pico, -a);
			picosat_assume(pico, -a_m);
			sym->fexpr_y->assumption = false;
			sym->fexpr_m->assumption = false;
			break;
		case mod:
			picosat_assume(pico, -a);
			picosat_assume(pico, a_m);
			sym->fexpr_y->assumption = false;
			sym->fexpr_m->assumption = true;
			break;
		case yes:
			picosat_assume(pico, a);
			picosat_assume(pico, -a_m);
			sym->fexpr_y->assumption = true;
			sym->fexpr_m->assumption = false;
			break;
		}
	}
}

/* 
 * add assumptions for the symbols to be changed to the SAT solver
 */
void sym_add_assumption_sdv(PicoSAT *pico, GArray *arr)
{
	unsigned int i;
	struct symbol_dvalue *sdv;
	for (i = 0; i < arr->len; i++) {
		sdv = g_array_index(arr, struct symbol_dvalue *, i);
		
		int lit_y = sdv->sym->fexpr_y->satval;
		
		if (sdv->sym->type == S_BOOLEAN) {
			switch (sdv->tri) {
			case yes:
				picosat_assume(pico, lit_y);
				break;
			case no:
				picosat_assume(pico, -lit_y);
				break;
			case mod:
				perror("Should not happen.\n");
			}
		} else if (sdv->sym->type == S_TRISTATE) {
			int lit_m = sdv->sym->fexpr_m->satval;
			switch (sdv->tri) {
			case yes:
				picosat_assume(pico, lit_y);
				picosat_assume(pico, -lit_m);
				break;
			case mod:
				picosat_assume(pico, -lit_y);
				picosat_assume(pico, lit_m);
				break;
			case no:
				picosat_assume(pico, -lit_y);
				picosat_assume(pico, -lit_m);
			}
		}
	}
}
