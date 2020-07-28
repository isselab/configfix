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

#define MAX_DIAGNOSES 3
#define MAX_SECONDS 30
#define SLICE_PROBLEM false
#define PRINT_UNSAT_CORE true
#define PRINT_DIAGNOSES false
#define PRINT_DIAGNOSIS_FOUND true
#define MINIMISE_DIAGNOSES false
#define MINIMISE_UNSAT_CORE true


static GArray *diagnoses, *diagnoses_symbol;

static GArray * generate_diagnoses(PicoSAT *pico);
static GArray * generate_diagnoses_sliced(PicoSAT *pico);

static void add_fexpr_to_constraint_set(GArray *C);
static void set_assumptions(PicoSAT *pico, GArray *c);
static void set_assumptions_mapped(PicoSAT *pico, GArray *c, GHashTable *satvarmap);
static void fexpr_add_assumption(PicoSAT *pico, struct fexpr *e, int satval);
static GArray * get_unsat_core_soft(PicoSAT *pico);
static GArray * get_unsat_core_soft_mapped(PicoSAT *pico, GHashTable *satvarmap);
static void extract_unsat_core_hard(PicoSAT *pico, PicoSAT *pico_cur, GHashTable *clauses_added, GHashTable *satvarmap);
static GArray * minimise_unsat_core(PicoSAT *pico, GArray *C);
static GArray * minimise_unsat_core_mapped(PicoSAT *pico, GArray *C, GHashTable *satvarmap);
static bool diagnosis_satisfies_entire_problem(PicoSAT *pico, GArray *c);
static int * g_hash_table_find_key(GHashTable *satvarmap, int *keyvalue);
static GArray * map_oldclause_to_newclause(GHashTable *satvarmap, GArray *oldc);

static GArray * get_difference(GArray *C, GArray *E0);
static bool has_intersection(GArray *e, GArray *X);
static GArray * get_union(GArray *A, GArray *B);
static GArray * clone_array(GArray *arr);
static bool is_subset_of(GArray *A, GArray *B);
static void print_array(char *title, GArray *arr);
static void print_unsat_core(GArray *arr);
static bool diagnosis_contains_fexpr(GArray *diagnosis, struct fexpr *e);
static bool diagnosis_contains_symbol(GArray *diagnosis, struct symbol *sym);

static void print_diagnoses(GArray *diag);
static void print_diagnoses_symbol(GArray *diag_sym);

static GArray * convert_diagnoses(GArray *diagnoses);
static GArray * convert_diagnosis(GArray *diagnosis);
static struct symbol_fix * symbol_fix_create(struct fexpr *e, enum symbolfix_type type, GArray *diagnosis);
static GArray * minimise_diagnoses(PicoSAT *pico, GArray *diagnoses);

static tristate calculate_new_tri_val(struct fexpr *e, GArray *diagnosis);
static const char * calculate_new_string_value(struct fexpr *e, GArray *diagnosis);

/* count assumptions, only used for debugging */
static unsigned int nr_of_assumptions = 0, nr_of_assumptions_true = 0;

/* -------------------------------------- */

GArray * rangefix_init(PicoSAT *pico)
{
	printf("Starting RangeFix...\n");
	printf("Generating diagnoses...");
	clock_t start, end;
	double time;
	start = clock();
	
	/* generate the diagnoses */
	if (SLICE_PROBLEM)
		diagnoses = generate_diagnoses_sliced(pico);
	else
		diagnoses = generate_diagnoses(pico);

	end = clock();
	time = ((double) (end - start)) / CLOCKS_PER_SEC;
	printf("done. (%.6f secs.)\n", time);
	
// 	printf("\n");
	
	if (PRINT_DIAGNOSES) {
		printf("Diagnoses (only for debugging):\n");
		print_diagnoses(diagnoses);
		printf("\n");
	}
	
	/* convert diagnoses of fexpr to diagnoses of symbols */
	
	
	if (MINIMISE_DIAGNOSES)
		diagnoses_symbol = minimise_diagnoses(pico, diagnoses);
	else
		diagnoses_symbol = convert_diagnoses(diagnoses);
	
	printf("\n");
	
	return diagnoses_symbol;
}

/*
 * generate the diagnoses
 */
static GArray * generate_diagnoses(PicoSAT *pico)
{
	GArray *C = g_array_new(false, false, sizeof(struct fexpr *));
	GArray *E = g_array_new(false, false, sizeof(GArray *));
	GArray *R = g_array_new(false, false, sizeof(GArray *));
	GArray *X, *e, *x_set, *E1, *E_R_Union, *E2;
	struct fexpr *x;
	unsigned int i, j, k, diagnosis_index;
	
	/* create constraint set C */
	add_fexpr_to_constraint_set(C);
	
	if (PRINT_UNSAT_CORE)
		printf("\n");

	/* init E with an empty diagnosis */
	GArray *empty_diagnosis = g_array_new(false, false, sizeof(struct fexpr *));
	g_array_append_val(E, empty_diagnosis);
	
	/* start the clock */
	clock_t start_t, end_t;
	double time_t;
	start_t = clock();
	
	//num = (rand() % (upper – lower + 1)) + lower
	while (E->len > 0) {
// 		printf("start while, E.length: %d\n", E->len);
		/* get random diagnosis */
		diagnosis_index = rand() % (E->len);
		GArray *E0 = g_array_index(E, GArray *, diagnosis_index);
// 		print_array("Select partial diagnosis", E0);
		
		/* calculate C\E0 */
		GArray *c = get_difference(C, E0);
// 		print_array("Soft constraints", c);
// 		getchar();
		
		/* set assumptions */
		nr_of_assumptions = 0;
		nr_of_assumptions_true = 0;
		set_assumptions(pico, c);
// 		printf("Asuumptions: %d (true: %d)\n", nr_of_assumptions, nr_of_assumptions_true);
		
// 		clock_t start_t2, end_t2;
// 		double time_t2;
// 		start_t2 = clock();
		
		int res = picosat_sat(pico, -1);
		
// 		end_t2 = clock();
// 		time_t2 = ((double) (end_t2 - start_t2)) / CLOCKS_PER_SEC;
// 		printf("PicoSAT time : %.6f secs.\n", time_t2);
		
// 		int res = picosat_sat(pico, -1);
		if (res == PICOSAT_SATISFIABLE) {
// 			printf("SATISFIABLE\n");
// 			getchar();
			if (PRINT_DIAGNOSIS_FOUND)
				print_array("DIAGNOSIS FOUND", E0);
			
			E = g_array_remove_index(E, diagnosis_index);
			if (E0->len > 0)
				R = g_array_append_val(R, E0);
			else
				g_array_free(E0, false);
			
			g_array_free(c, false);
			
			if (R->len >= MAX_DIAGNOSES)
				goto DIAGNOSES_FOUND;
			
			continue;
			
		} else if (res == PICOSAT_UNSATISFIABLE) {
// 			printf("UNSATISFIABLE\n");
// 			getchar();
		} else if (res == PICOSAT_UNKNOWN) {
			printf("UNKNOWN\n");
		} else {
			perror("Doh brother.");
		}
		
		/* check elapsed time */
		end_t = clock();
		time_t = ((double) (end_t - start_t)) / CLOCKS_PER_SEC;
		if (time_t > (double) MAX_SECONDS)
			goto DIAGNOSES_FOUND;
		
		/* get unsat core from SAT solver */
		X = get_unsat_core_soft(pico);
		
		/* minimise the unsat core */
		if (MINIMISE_UNSAT_CORE)
			X = minimise_unsat_core(pico, X);
		
		if (PRINT_UNSAT_CORE)
			print_unsat_core(X);
		
		for (i = 0; i < E->len; i++) {
			/* get partial diagnosis */
			e = g_array_index(E, GArray *, i);
// 			print_array("Look at partial diagnosis", e);
			
			/* check, if there is an intersection between e and X
			 * if there is, go to the next partial diagnosis */
			if (has_intersection(e, X)) {
// 				printf("Intersection with core.\n");
				continue;
			}
			
			/* for each fexpr in the core */
			for (j = 0; j < X->len; j++) {
				x = g_array_index(X, struct fexpr *, j);
// 				printf("x = %s\n", str_get(&x->name));

				/* create {x} */
				x_set = g_array_new(false, false, sizeof(struct fexpr *));
				g_array_append_val(x_set, x);
// 				print_array("{x}", x_set);
				
				/* create E' = e ∪ {x} */
				E1 = get_union(e, x_set);
// 				print_array("E1", E1);
				
				/* create (E\e) ∪ R */
				E_R_Union = clone_array(E);
				g_array_remove_index(E_R_Union, i);
				E_R_Union = get_union(E_R_Union, R);
				
				bool E2_subset_of_E1 = false;
				
				/* E" ∈ (E\e) ∪ R */
				for (k = 0; k < E_R_Union->len; k++) {
					E2 = g_array_index(E_R_Union, GArray *, k);
// 					print_array("E2", E2);
// 					getchar();
					
					/* E" ⊆ E' ? */
					if (is_subset_of(E2, E1)) {
						E2_subset_of_E1 = true;
// 						print_array("E\"", E2);
// 						print_array("E'", E1);
						break;
					}
				}
				
				g_array_free(E_R_Union, false);
				
				/* ∄ E" ⊆ E' */
				if (!E2_subset_of_E1) {
					E = g_array_append_val(E, E1);
// 					print_array("Add partial diagnosis", E1);
				} else {
					g_array_free(E1, false);
				}
			}
			
// 			print_array("Remove partial diagnosis", e);
			g_array_free(e, false);
			g_array_remove_index(E, i);
			i--;

		}
		g_array_free(X, false);
		g_array_free(c, false);
		
// 		printf("E.length: %d, i = %d\n", E->len, i);
		
	}

DIAGNOSES_FOUND:
	g_array_free(C, false);
	g_array_free(E, false);
	
	return R;
}

// void print_clause(GArray *arr)
// {
// 	int i, *lit;
// 	struct fexpr *e;
// 	printf("Core clause: ");
// 	for (i = 0; i < arr->len; i++) {
// 		lit = g_array_index(arr, int *, i);
// 		if (*lit < 0) printf("-");
// 		*lit = abs(*lit);
// 		e = (struct fexpr *) g_hash_table_lookup(satmap, lit);
// 		printf("%s ", str_get(&e->name));
// 	}
// 	printf("\n");
// }

/*
 * generate the diagnoses
 */
static GArray * generate_diagnoses_sliced(PicoSAT *pico)
{
	GArray *C = g_array_new(false, false, sizeof(struct fexpr *));
	GArray *E = g_array_new(false, false, sizeof(GArray *));
	GArray *R = g_array_new(false, false, sizeof(GArray *));
	GArray *X, *e, *x_set, *E1, *E_R_Union, *E2;
	struct fexpr *x;
	unsigned int i, j, k, diagnosis_index;
	
	/* SAT solver */
	PicoSAT *pico_cur = picosat_init();
	picosat_enable_trace_generation(pico_cur);
	
	/* HashSet to keep track which clauses have been added */
	GHashTable *clauses_added = g_hash_table_new_full(
		g_int_hash, g_int_equal, //< This is an integer hash.
		free, //< Call "free" on the key (made with "malloc").
		NULL //< Call "free" on the value (made with "strdup").
	);
	
	/* HashMap to map old sat variables to new satvariables */
	GHashTable *satvarmap = g_hash_table_new_full(
		g_int_hash, g_int_equal, //< This is an integer hash.
		NULL, //< Call "free" on the key (made with "malloc").
		free //< Call "free" on the value (made with "strdup").
	);
	
	/* create constraint set C */
	add_fexpr_to_constraint_set(C);
	
	if (PRINT_UNSAT_CORE)
		printf("\n");

	/* init E with an empty diagnosis */
	GArray *empty_diagnosis = g_array_new(false, false, sizeof(struct fexpr *));
	g_array_append_val(E, empty_diagnosis);
	
	/* start the clock */
	clock_t start_t, end_t;
	double time_t;
	start_t = clock();
	
	/* extract unsat core and add it to the SAT solver */
	extract_unsat_core_hard(pico, pico_cur, clauses_added, satvarmap);
	
	//num = (rand() % (upper – lower + 1)) + lower
	while (E->len > 0) {
// 		printf("start while, E.length: %d\n", E->len);
		/* get random diagnosis */
		diagnosis_index = rand() % (E->len);
		GArray *E0 = g_array_index(E, GArray *, diagnosis_index);
// 		print_array("Select partial diagnosis", E0);
		
		/* calculate C\E0 */
		GArray *c = get_difference(C, E0);
// 		print_array("Soft constraints", c);
		
	CONTINUE_ALT1:
		/* set assumptions */
		nr_of_assumptions = 0;
		nr_of_assumptions_true = 0;
		set_assumptions_mapped(pico_cur, c, satvarmap);
// 		printf("Asuumptions: %d (true: %d)\n", nr_of_assumptions, nr_of_assumptions_true);
		
// 		clock_t start_t2, end_t2;
// 		double time_t2;
// 		start_t2 = clock();
		
		int res = picosat_sat(pico_cur, -1);
		
// 		end_t2 = clock();
// 		time_t2 = ((double) (end_t2 - start_t2)) / CLOCKS_PER_SEC;
// 		printf("CNF-clauses  : %d\n", picosat_added_original_clauses(pico_cur));
// 		printf("SAT-variables: %d\n", picosat_variables(pico_cur));
// 		printf("PicoSAT time : %.6f secs.\n", time_t2);
		
		
		if (res == PICOSAT_SATISFIABLE) {
// 			printf("SATISFIABLE\n");
			
			/* Does diagnosis solve entire problem? */
			if (!diagnosis_satisfies_entire_problem(pico, c)) {
				extract_unsat_core_hard(pico, pico_cur, clauses_added, satvarmap);
				
				goto CONTINUE_ALT1;
			}
			
			if (PRINT_DIAGNOSIS_FOUND)
				print_array("DIAGNOSIS FOUND", E0);
			
			E = g_array_remove_index(E, diagnosis_index);
			if (E0->len > 0)
				R = g_array_append_val(R, E0);
			else
				g_array_free(E0, false);
			
			g_array_free(c, false);
			
			if (R->len >= MAX_DIAGNOSES)
				goto DIAGNOSES_FOUND;
			
			continue;
			
		} else if (res == PICOSAT_UNSATISFIABLE) {
// 			printf("UNSATISFIABLE\n");
		} else if (res == PICOSAT_UNKNOWN) {
			printf("UNKNOWN\n");
		} else {
			perror("Doh brother.");
		}
		
		/* check elapsed time */
		end_t = clock();
		time_t = ((double) (end_t - start_t)) / CLOCKS_PER_SEC;
		if (time_t > (double) MAX_SECONDS)
			goto DIAGNOSES_FOUND;
		
		/* get unsat core from SAT solver */
		X = get_unsat_core_soft_mapped(pico_cur, satvarmap);
		
		/* minimise the unsat core */
		if (MINIMISE_UNSAT_CORE)
			X = minimise_unsat_core_mapped(pico_cur, X, satvarmap);
		
		if (PRINT_UNSAT_CORE)
			print_unsat_core(X);
		
		for (i = 0; i < E->len; i++) {
			/* get partial diagnosis */
			e = g_array_index(E, GArray *, i);
// 			print_array("Look at partial diagnosis", e);
			
			/* check, if there is an intersection between e and X
			 * if there is, go to the next partial diagnosis */
			if (has_intersection(e, X)) {
// 				printf("Intersection with core.\n");
				continue;
			}
			
			/* for each fexpr in the core */
			for (j = 0; j < X->len; j++) {
				x = g_array_index(X, struct fexpr *, j);
// 				printf("x = %s\n", str_get(&x->name));

				/* create {x} */
				x_set = g_array_new(false, false, sizeof(struct fexpr *));
				g_array_append_val(x_set, x);
// 				print_array("{x}", x_set);
				
				/* create E' = e ∪ {x} */
				E1 = get_union(e, x_set);
// 				print_array("E1", E1);
				
				/* create (E\e) ∪ R */
				E_R_Union = clone_array(E);
				g_array_remove_index(E_R_Union, i);
				E_R_Union = get_union(E_R_Union, R);
				
				bool E2_subset_of_E1 = false;
				
				/* E" ∈ (E\e) ∪ R */
				for (k = 0; k < E_R_Union->len; k++) {
					E2 = g_array_index(E_R_Union, GArray *, k);
// 					print_array("E2", E2);
// 					getchar();
					
					/* E" ⊆ E' ? */
					if (is_subset_of(E2, E1)) {
						E2_subset_of_E1 = true;
// 						print_array("E\"", E2);
// 						print_array("E'", E1);
						break;
					}
				}
				
				g_array_free(E_R_Union, false);
				
				/* ∄ E" ⊆ E' */
				if (!E2_subset_of_E1) {
					E = g_array_append_val(E, E1);
// 					print_array("Add partial diagnosis", E1);
				} else {
					g_array_free(E1, false);
				}
			}
			
// 			print_array("Remove partial diagnosis", e);
			g_array_free(e, false);
			g_array_remove_index(E, i);
			i--;

		}
		g_array_free(X, false);
		g_array_free(c, false);
		
// 		printf("E.length: %d, i = %d\n", E->len, i);
		
	}

DIAGNOSES_FOUND:
	g_array_free(C, false);
	g_array_free(E, false);
	
// 	g_hash_table_destroy(clauses_added);
	g_hash_table_remove_all(clauses_added);
// 	g_hash_table_destroy(satvarmap);
	g_hash_table_remove_all(satvarmap);
	
	picosat_reset(pico_cur);
	
	return R;
}

/*
 * add the fexpr to the constraint set C 
 */
static void add_fexpr_to_constraint_set(GArray *C)
{
	unsigned int i, nr_sym = 0, nr_fexpr = 0;
	struct symbol *sym;
	for_all_symbols(i, sym) {
		/* must be a proper symbol */
		if (sym->type == S_UNKNOWN) continue;
		
		/* don't need the conflict symbols
		 * they are handled seperately */
		if (sym_is_sdv(sdv_arr, sym)) continue;
		
		/* must have a prompt and a name */
		if (!sym->name || !sym_has_prompt(sym)) continue;
		
		nr_sym++;

		if (sym->type == S_BOOLEAN) {
			g_array_append_val(C, sym->fexpr_y);
			nr_fexpr++;
		}
		else if (sym->type == S_TRISTATE) {
			g_array_append_val(C, sym->fexpr_y);
			g_array_append_val(C, sym->fexpr_m);
			nr_fexpr += 2;
		}
		else if (sym->type == S_INT || sym->type == S_HEX || sym->type == S_STRING) {
			unsigned int i;
			struct fexpr *e;
			for (i = 0; i < sym->fexpr_nonbool->arr->len; i++) {
				e = g_array_index(sym->fexpr_nonbool->arr, struct fexpr *, i);
				g_array_append_val(C, e);
				nr_fexpr++;
			}
		}
		else {
			perror("Error adding variables to constraint set C.");
		}
	}
// 	printf("Added %d symbols, %d variables.\n", nr_sym, nr_fexpr);
}

/*
 * check whether the fexpr symbolises the no-value-set fexpr for a non-boolean symbol
 */
static bool fexpr_is_novalue(struct fexpr *e)
{
	assert(sym_is_nonboolean(e->sym));

	return e == g_array_index(e->sym->fexpr_nonbool->arr, struct fexpr *, 0);
}

static void set_assumptions_sdv(PicoSAT *pico, GArray *arr)
{
	unsigned int i;
	struct symbol_dvalue *sdv;
	struct symbol *sym;
	
	for (i = 0; i < arr->len; i++) {
		sdv = g_array_index(arr, struct symbol_dvalue *, i);
		sym = sdv->sym;
		
		int lit_y = sym->fexpr_y->satval;
		
		if (sdv->sym->type == S_BOOLEAN) {
			switch (sdv->tri) {
			case yes:
				picosat_assume(pico, lit_y);
				sym->fexpr_y->assumption = true;
				nr_of_assumptions_true++;
				break;
			case no:
				picosat_assume(pico, -lit_y);
				sym->fexpr_y->assumption = false;
				break;
			case mod:
				perror("Should not happen.\n");
			}
			nr_of_assumptions++;
		} else if (sdv->sym->type == S_TRISTATE) {
			int lit_m = sdv->sym->fexpr_m->satval;
			switch (sdv->tri) {
			case yes:
				picosat_assume(pico, lit_y);
				sym->fexpr_y->assumption = true;
				picosat_assume(pico, -lit_m);
				sym->fexpr_m->assumption = false;
				nr_of_assumptions_true++;
				break;
			case mod:
				picosat_assume(pico, -lit_y);
				sym->fexpr_y->assumption = false;
				picosat_assume(pico, lit_m);
				sym->fexpr_m->assumption = true;
				nr_of_assumptions_true++;
				break;
			case no:
				picosat_assume(pico, -lit_y);
				sym->fexpr_y->assumption = false;
				picosat_assume(pico, -lit_m);
				sym->fexpr_y->assumption = false;
			}
			nr_of_assumptions += 2;
		}
	}
}

static void set_assumptions_sdv_mapped(PicoSAT *pico, GArray *arr, GHashTable* satvarmap)
{
	unsigned int i;
	struct symbol_dvalue *sdv;
	struct symbol *sym;
	
	for (i = 0; i < arr->len; i++) {
		sdv = g_array_index(arr, struct symbol_dvalue *, i);
		sym = sdv->sym;
		
		int tmp_y = sym->fexpr_y->satval;
		int lit_y;
		if (!g_hash_table_contains(satvarmap, &tmp_y)) {
			int *newval = malloc(sizeof(int));
			*newval = g_hash_table_size(satvarmap) + 1;
			int *oldval = malloc(sizeof(int));
			*oldval = tmp_y;
			g_hash_table_insert(satvarmap, oldval, newval);
			lit_y = *newval;
		} else {
			int *tmp = g_hash_table_lookup(satvarmap, &tmp_y);
			lit_y = *tmp;
		}
		
		if (sdv->sym->type == S_BOOLEAN) {
			switch (sdv->tri) {
			case yes:
				picosat_assume(pico, lit_y);
				sym->fexpr_y->assumption = true;
				nr_of_assumptions_true++;
				break;
			case no:
				picosat_assume(pico, -lit_y);
				sym->fexpr_y->assumption = false;
				break;
			case mod:
				perror("Should not happen.\n");
			}
			nr_of_assumptions++;
		} else if (sdv->sym->type == S_TRISTATE) {
			int tmp_m = sym->fexpr_m->satval;
			int *tmpval2;
			if (!g_hash_table_contains(satvarmap, &tmp_m)) {
				int *newval2 = malloc(sizeof(int));
				*newval2 = g_hash_table_size(satvarmap) + 1;
				int *oldval2 = malloc(sizeof(int));
				*oldval2 = tmp_m;
				g_hash_table_insert(satvarmap, oldval2, newval2);
				tmpval2 = newval2;
			} else {
				tmpval2 = g_hash_table_lookup(satvarmap, &tmp_m);
			}
			int lit_m = *tmpval2;
			switch (sdv->tri) {
			case yes:
				picosat_assume(pico, lit_y);
				sym->fexpr_y->assumption = true;
				picosat_assume(pico, -lit_m);
				sym->fexpr_m->assumption = false;
				nr_of_assumptions_true++;
				break;
			case mod:
				picosat_assume(pico, -lit_y);
				sym->fexpr_y->assumption = false;
				picosat_assume(pico, lit_m);
				sym->fexpr_m->assumption = true;
				nr_of_assumptions_true++;
				break;
			case no:
				picosat_assume(pico, -lit_y);
				sym->fexpr_y->assumption = false;
				picosat_assume(pico, -lit_m);
				sym->fexpr_y->assumption = false;
			}
			nr_of_assumptions += 2;
		}
	}
}

/*
 * set the assumptions for the next run of Picosat
 */
static void set_assumptions(PicoSAT *pico, GArray *c)
{
	struct fexpr *e;
	unsigned int i;
	for (i = 0; i < c->len; i++) {
		e = g_array_index(c, struct fexpr *, i);
		fexpr_add_assumption(pico, e, e->satval);
	}
	
	/* set assumptions for the conflict-symbols */
	set_assumptions_sdv(pico, sdv_arr);
}

/*
 * set the assumptions for the next run of Picosat_cur
 * but check whether the fexpr needs to be added first
 */
static void set_assumptions_mapped(PicoSAT* pico, GArray* c, GHashTable* satvarmap)
{
	struct fexpr *e;
	unsigned int i;
	int *satval;
	for (i = 0; i < c->len; i++) {
		e = g_array_index(c, struct fexpr *, i);
		if (g_hash_table_contains(satvarmap, &e->satval)) {
			satval = g_hash_table_lookup(satvarmap, &e->satval);
			fexpr_add_assumption(pico, e, *satval);
// 			printf("Adding assumption for %s (%d)\n", str_get(&e->name), *satval);
		}
		
	}
	
	/* set assumptions for the conflict-symbols */
	set_assumptions_sdv_mapped(pico, sdv_arr, satvarmap);
}

/*
 * set the assumtption for a fexpr for the next run of Picosat
 */
static void fexpr_add_assumption(PicoSAT *pico, struct fexpr *e, int satval)
{
	struct symbol *sym = e->sym;
	
	if (sym->type == S_BOOLEAN) {
		int tri_val = sym->def[S_DEF_USER].tri;
		tri_val = sym_get_tristate_value(sym);
	
		if (tri_val == yes) {
			picosat_assume(pico, satval);
			e->assumption = true;
			nr_of_assumptions_true++;
		} else {
			picosat_assume(pico, -satval);
			e->assumption = false;
		}
		nr_of_assumptions++;
	}
	
	if (sym->type == S_TRISTATE) {
		int tri_val = sym->def[S_DEF_USER].tri;
		tri_val = sym_get_tristate_value(sym);
		
		if (e->tri == yes) {
			if (tri_val == yes) {
				picosat_assume(pico, satval);
				e->assumption = true;
				nr_of_assumptions_true++;
			} else {
				picosat_assume(pico, -satval);
				e->assumption = false;
			}
		} else if (e->tri == mod) {
			if (tri_val == mod) {
				picosat_assume(pico, satval);
				e->assumption = true;
				nr_of_assumptions_true++;
			} else {
				picosat_assume(pico, -satval);
				e->assumption = false;
			}
		}
		nr_of_assumptions++;
	}

	if (sym->type == S_INT || sym->type == S_HEX || sym->type == S_STRING) {
// 		const char *string_val = sym_get_string_value(sym);
		
		/* check, if e symbolises the no-value-set fexpr */
		if (fexpr_is_novalue(e)) {
			if (sym_has_value(e->sym)) {
				picosat_assume(pico, -satval);
				e->assumption = false;
			} else {
				picosat_assume(pico, satval);
				e->assumption = true;
				nr_of_assumptions_true++;
			}
			nr_of_assumptions++;
			return;
		}
		
		if (sym_has_value(e->sym) && strcmp(str_get(&e->nb_val), sym_get_string_value(sym)) == 0) {
			picosat_assume(pico, satval);
			e->assumption = true;
			nr_of_assumptions_true++;
		} else {
			picosat_assume(pico, -satval);
			e->assumption = false;
		}
		nr_of_assumptions++;
	}
}

/*
 * get the unsatisfiable soft constraints from the last run of Picosat
 */
static GArray * get_unsat_core_soft(PicoSAT *pico)
{
	GArray *ret = g_array_new(false, false, sizeof(struct fexpr *));
	struct fexpr *e;
	
	int *lit = malloc(sizeof(int));
	const int *i = picosat_failed_assumptions(pico);
	*lit = abs(*i++);
	
	while (*lit != 0) {
		e = g_hash_table_lookup(satmap, lit);
		
		if (!sym_is_sdv(sdv_arr, e->sym))
			g_array_append_val(ret, e);
		
		*lit = abs(*i++);
	}
	
	return ret;
}

/*
 * get the unsatisfiable soft constraints from the last run of Picosat
 * (used for the smaller problem)
 */
static GArray * get_unsat_core_soft_mapped(PicoSAT *pico, GHashTable *satvarmap)
{
	GArray *ret = g_array_new(false, false, sizeof(struct fexpr *));
	struct fexpr *e;
	
	int *lit = malloc(sizeof(int));
	int *mapped;
	const int *i = picosat_failed_assumptions(pico);
	*lit = abs(*i++);
	
	while (*lit != 0) {
		mapped = g_hash_table_find_key(satvarmap, lit);
		e = g_hash_table_lookup(satmap, mapped);
		
		if (!sym_is_sdv(sdv_arr, e->sym))
			g_array_append_val(ret, e);

		*lit = abs(*i++);
	}

	return ret;
}

/*
 * extract the unsatisfiable hard constraints from the entire problem 
 * and add the clauses to the second SAT solver
 */
static void extract_unsat_core_hard(PicoSAT *pico, PicoSAT *pico_cur, GHashTable *clauses_added, GHashTable *satvarmap)
{
	int i;
	GArray *clause, *newclause;
	
	for (i = 0; i < picosat_added_original_clauses(pico); i++) {
		/* clause was part of an unsat core before */
		if (g_hash_table_contains(clauses_added, &i)) continue;
		
		/* clause is not part of the unsat core */
		if (picosat_coreclause(pico, i) == 0) continue; 
		
		/* get clause */
		clause = (GArray *) g_hash_table_lookup(cnf_clauses_map, &i);
		newclause = map_oldclause_to_newclause(satvarmap, clause);
		
		/* add clause to pico_cur/clauses_added if not done yet */
		sat_add_clause_garray(pico_cur, newclause);
		int *j = malloc(sizeof(int));
		*j = i;
		g_hash_table_insert(clauses_added, j, j);
	}
}

/*
 * minimise the unsat core C
 */
static GArray * minimise_unsat_core(PicoSAT *pico, GArray *C)
{
	/* no need to check further */
	if (C->len == 1) return C;
	
	GArray * c_set;
	unsigned int i;
	struct fexpr * c;
	
	for (i = 0; i < C->len; i++) {
		c = g_array_index(C, struct fexpr *, i);
		
		/* create C\c */
		c_set = g_array_new(false, false, sizeof(struct fexpr *));
		g_array_append_val(c_set, c);
		GArray *t = get_difference(C, c_set);
		
		/* invoke PicoSAT */
		set_assumptions(pico, t);
		
		int res = picosat_sat(pico, -1);
		
		if (res == PICOSAT_UNSATISFIABLE) {
			g_array_remove_index(C, i);
			i--;
		}
		
		g_array_free(c_set, false);
		g_array_free(t, false);
	}
	
	return C;
}

/*
 * minimise the unsat core C
 */
static GArray * minimise_unsat_core_mapped(PicoSAT *pico, GArray *C, GHashTable *satvarmap)
{
	/* no need to check further */
	if (C->len == 1) return C;
	
	GArray * c_set;
	unsigned int i;
	struct fexpr * c;
	
	for (i = 0; i < C->len; i++) {
		c = g_array_index(C, struct fexpr *, i);
		
		/* create C\c */
		c_set = g_array_new(false, false, sizeof(struct fexpr *));
		g_array_append_val(c_set, c);
		GArray *t = get_difference(C, c_set);
		
		/* invoke PicoSAT */
		set_assumptions_mapped(pico, t, satvarmap);
		
		int res = picosat_sat(pico, -1);
		
		if (res == PICOSAT_UNSATISFIABLE) {
			g_array_remove_index(C, i);
			i--;
		}
		
		g_array_free(c_set, false);
		g_array_free(t, false);
	}
	
	return C;
}

/*
 * check whether a diagnosis that solves the smaller problem
 * actually solves the entire problem
 */
static bool diagnosis_satisfies_entire_problem(PicoSAT *pico, GArray *c)
{
	set_assumptions(pico, c);
	
	int res = picosat_sat(pico, -1);
	
	if (res == PICOSAT_SATISFIABLE)
		return true;
	else if (res == PICOSAT_UNSATISFIABLE)
		return false;
	else
		perror("Cannot satisfy entire problem.");
	
	return false;
}

/*
 * find a key in satvarmap given a value
 */
static int * g_hash_table_find_key(GHashTable *satvarmap, int *keyvalue)
{
	GHashTableIter iter;
	gpointer key, value;

	g_hash_table_iter_init(&iter, satvarmap);
	while (g_hash_table_iter_next(&iter, &key, &value))
	{
		if (*((int *) value) == *keyvalue) 
			return (int *) key;
	}
	
	return NULL;
}

/*
 * convert a clause from pico to a clause for pico_cur
 */
static GArray * map_oldclause_to_newclause(GHashTable *satvarmap, GArray *oldc)
{
	GArray *newc = g_array_new(false, false, sizeof(int *));
	int i, *newval, *oldval, *newabsval, *oldabsval;
	for (i = 0; i < oldc->len; i++) {
		oldval = g_array_index(oldc, int *, i);
		if (*oldval < 0) {
			oldabsval = malloc(sizeof(int));
			*oldabsval = abs(*oldval);
		} else {
			oldabsval = oldval;
		}
		if (!g_hash_table_contains(satvarmap, oldabsval)) {
			newabsval = malloc(sizeof(int));
			*newabsval = g_hash_table_size(satvarmap) + 1;
			g_hash_table_insert(satvarmap, oldabsval, newabsval);
		} else {
			newabsval = g_hash_table_lookup(satvarmap, oldabsval);
		}

		newval = malloc(sizeof(int));
		*newval = *oldval < 0 ? *newabsval * -1 : *newabsval;
		g_array_append_val(newc, newval);
	}
	
	return newc;
}

/*
 * Calculate C\E0
 */
static GArray * get_difference(GArray *C, GArray *E0)
{
	GArray *ret = g_array_new(false, false, sizeof(struct fexpr *));
	struct fexpr *e1, *e2;
	unsigned int i, j;
	bool found;
	
	for (i = 0; i < C->len; i++) {
		e1 = g_array_index(C, struct fexpr *, i);
		found = false;
		for (j = 0; j < E0->len; j++) {
			e2 = g_array_index(E0, struct fexpr *, j);
			if (e1 == e2) {
				found = true;
				break;
			}
		}
		if (!found)
			g_array_append_val(ret, e1);
	}
	
	return ret;
}

/*
 * check, if there is an intersection between e and X
 */
static bool has_intersection(GArray *e, GArray *X)
{
	struct fexpr *e1, *e2;
	unsigned int i, j;
	
	for (i = 0; i < e->len; i++) {
		e1 = g_array_index(e, struct fexpr *, i);
		for (j = 0; j < X->len; j++) {
			e2 = g_array_index(X, struct fexpr *, j);
			if (e1 == e2)
				return true;
		}
	}
	return false;
}

/*
 * get the union of 2 GArrays with fexpr
 */
static GArray * get_union(GArray *A, GArray *B)
{
	GArray *ret = g_array_new(false, false, sizeof(void *));
	struct fexpr *e1, *e2;
	unsigned int i, j;
	bool found = false;
	
	for (i = 0; i < A->len; i++) {
		e1 = g_array_index(A, void *, i);
		g_array_append_val(ret, e1);
	}
	
	for (i = 0; i < B->len; i++) {
		e1 = g_array_index(B, void *, i);
		for (j = 0; j < A->len; j++) {
			e2 = g_array_index(A, void *, j);
			if (e1 == e2) {
				found = true;
				break;
			}
		}
		if (!found)
			g_array_append_val(ret, e1);
	}
	
	return ret;
}

/*
 * clone a single GArray
 */
static GArray * clone_array(GArray *arr)
{
	GArray *ret = g_array_new(false, false, sizeof(void *));
	struct fexpr *e;
	unsigned int i;

	for (i = 0; i < arr->len; i++) {
		e = g_array_index(arr, void *, i);
		g_array_append_val(ret, e);
	}
	
	return ret;
}

/*
 * check, if A is a subset of B
 */
static bool is_subset_of(GArray *A, GArray *B)
{
	struct fexpr *e1, *e2;
	unsigned int i, j;
	bool found;
	
	for (i = 0; i < A->len; i++) {
		e1 = g_array_index(A, struct fexpr *, i);
		found = false;
		for (j = 0; j < B->len; j++) {
			e2 = g_array_index(B, struct fexpr *, j);
			if (e1 == e2) {
				found = true;
				break;
			}
		}
		if (!found)
			return false;
	}
	
	return true;
}

/*
 * print a GArray with fexpr
 */
static void print_array(char *title, GArray *arr)
{
	struct fexpr *e;
	unsigned int i;
	printf("%s: [", title);
	
	for (i = 0; i < arr->len; i++) {
		e = g_array_index(arr, struct fexpr *, i);
		printf("%s", str_get(&e->name));
		if (i != arr->len - 1)
			printf(", ");
	}
	
	printf("]\n");
}

/*
 * print an unsat core
 */
static void print_unsat_core(GArray *arr)
{
	struct fexpr *e;
	unsigned int i;
	printf("Unsat core: [");
	
	for (i = 0; i < arr->len; i++) {
		e = g_array_index(arr, struct fexpr *, i);
		printf("%s", str_get(&e->name));
		printf(" <%s>", e->assumption ? "T" : "F");
		if (i != arr->len - 1)
			printf(", ");
	}
	
	printf("]\n");
}


/*
 * check if a diagnosis contains a fexpr
 */
static bool diagnosis_contains_fexpr(GArray *diagnosis, struct fexpr *e)
{
	struct fexpr *e2;
	unsigned int i;
	for (i = 0; i < diagnosis->len; i++) {
		e2 = g_array_index(diagnosis, struct fexpr *, i);
		if (e == e2)
			return true;
	}
	
	return false;
}

/*
 * check if a diagnosis contains a symbol
 */
static bool diagnosis_contains_symbol(GArray *diagnosis, struct symbol *sym)
{
	struct symbol_fix *sym_fix;
	unsigned int i;
	
	for (i = 0; i < diagnosis->len; i++) {
		sym_fix = g_array_index(diagnosis, struct symbol_fix *, i);
		if (sym == sym_fix->sym)
			return true;
	}
	
	return false;
}

/*
 * print the diagnoses of type fexpr
 */
static void print_diagnoses(GArray *diag)
{
	GArray *arr;
	struct fexpr *e;
	unsigned int i, j;

	for (i = 0; i < diag->len; i++) {
		arr = g_array_index(diag, GArray *, i);
		printf("%d: [", i+1);
		for (j = 0; j < arr->len; j++) {
			e = g_array_index(arr, struct fexpr *, j);
			char *new_val = e->assumption ? "false" : "true";
			printf("%s => %s", str_get(&e->name), new_val);
			if (j != arr->len - 1)
				printf(", ");
		}
		printf("]\n");
	}
}

/*
 * print a single diagnosis of type symbol_fix
 */
void print_diagnosis_symbol(GArray *diag_sym)
{
	struct symbol_fix *fix;
	unsigned int i;
	
	printf("[");
	for (i = 0; i < diag_sym->len; i++) {
		fix = g_array_index(diag_sym, struct symbol_fix *, i);
		
		if (fix->type == SF_BOOLEAN)
			printf("%s => %s", fix->sym->name, tristate_get_char(fix->tri));
		else if (fix->type == SF_NONBOOLEAN)
			printf("%s => %s", fix->sym->name, str_get(&fix->nb_val));
		else
			perror("NB not yet implemented.");
		
		if (i != diag_sym->len - 1)
			printf(", ");
	}
	printf("]\n");
}

/*
 * print the diagnoses of type symbol_fix
 */
static void print_diagnoses_symbol(GArray *diag_sym)
{
	GArray *arr;
	unsigned int i;

	for (i = 0; i < diag_sym->len; i++) {
		arr = g_array_index(diag_sym, GArray *, i);
		printf("%d: ", i+1);
		print_diagnosis_symbol(arr);
	}
}

/*
 * convert a single diagnosis of fexpr into a diagnosis of symbols
 */
static GArray * convert_diagnosis(GArray *diagnosis)
{
	GArray *diagnosis_symbol = g_array_new(false, false, sizeof(struct symbol_fix *));
	unsigned int i;
	struct fexpr *e;
	struct symbol_fix *fix;
	struct symbol_dvalue *sdv;
	
	/* set the values for the conflict symbols */
	for (i = 0; i < sdv_arr->len; i++) {
		sdv = g_array_index(sdv_arr, struct symbol_dvalue *, i);
		fix = malloc(sizeof(struct symbol_fix));
		fix->sym = sdv->sym;
		fix->type = SF_BOOLEAN;
		fix->tri = sdv->tri;
		g_array_append_val(diagnosis_symbol, fix);
	}
	
	for (i = 0; i < diagnosis->len; i++) {
		e = g_array_index(diagnosis, struct fexpr *, i);
		
		/* diagnosis already contains symbol, so continue */
		if (diagnosis_contains_symbol(diagnosis_symbol, e->sym)) continue;
		
		// TODO for disallowed
		enum symbolfix_type type;
		if (sym_is_boolean(e->sym))
			type = SF_BOOLEAN;
		else if (sym_is_nonboolean(e->sym))
			type = SF_NONBOOLEAN;
		else
			type = SF_DISALLOWED;
		fix = symbol_fix_create(e, type, diagnosis);
		
		g_array_append_val(diagnosis_symbol, fix);
	}
	
	return diagnosis_symbol;
}


/*
 * convert the diagnoses of fexpr into diagnoses of symbols
 * it is easier to handle symbols when applying fixes
 */
static GArray * convert_diagnoses(GArray *diag_arr)
{
	diagnoses_symbol = g_array_new(false, false, sizeof(GArray *));
	
	GArray *diagnosis, *diagnosis_symbol;
	unsigned int i;
	
	for (i = 0; i < diag_arr->len; i++) {
		diagnosis = g_array_index(diag_arr, GArray *, i);
		diagnosis_symbol = convert_diagnosis(diagnosis);
		g_array_append_val(diagnoses_symbol, diagnosis_symbol);
	}
	
	return diagnoses_symbol;
}

/* 
 * create a symbol_fix given a fexpr
 */
static struct symbol_fix * symbol_fix_create(struct fexpr *e, enum symbolfix_type type, GArray *diagnosis)
{
	struct symbol_fix *fix = malloc(sizeof(struct symbol_fix));
	fix->sym = e->sym;
	fix->type = type;
	
	switch(type) {
	case SF_BOOLEAN:
		fix->tri = calculate_new_tri_val(e, diagnosis);
		break;
	case SF_NONBOOLEAN:
		fix->nb_val = str_new();
		str_append(&fix->nb_val, calculate_new_string_value(e, diagnosis));
		break;
	default:
		perror("Illegal symbolfix_type.\n");
	}

	return fix;
}

/*
 * remove symbols from the diagnosis, which will be set automatically:
 * 1. symbol gets selected
 * 2. choice symbol gets enabled/disabled automatically
 * 3. symbol uses a default value
 */
static GArray * minimise_diagnoses(PicoSAT *pico, GArray *diagnoses)
{
	clock_t start, end;
	double time;
	
	printf("Minimising diagnoses...");
	
	start = clock();
	
	unsigned int i, j;
	GArray *d, *diagnosis_symbol;
	GArray *diagnoses_symbol = g_array_new(false, false, sizeof(GArray *));
	struct fexpr *e;
	int satval, deref = 0;
	struct symbol_fix *fix;

	/* create soft constraint set C */
	GArray *C = g_array_new(false, false, sizeof(struct fexpr *));
	add_fexpr_to_constraint_set(C);
	
	
	for (i = 0; i < diagnoses->len; i++) {
		d = g_array_index(diagnoses, GArray *, i);
		
		/* set assumptions for those symbols that don't need to be changed */
		set_assumptions(pico, get_difference(C, d));

		/* flip the assumptions from the diagnosis */
		for (j = 0; j < d->len; j++) {
			e = g_array_index(d, struct fexpr *, j);
			
			satval = e->assumption ? -(e->satval) : e->satval;	
			picosat_assume(pico, satval);
		}
		
		int res = picosat_sat(pico, -1);
		if (res != PICOSAT_SATISFIABLE)
			perror("Diagnosis not satisfiable (minimise).");
		
		diagnosis_symbol = convert_diagnosis(d);
		
		/* check if symbol gets selected */
		for (j = 0; j < diagnosis_symbol->len; j++) {
			fix = g_array_index(diagnosis_symbol, struct symbol_fix *, j);
			
			/* symbol is never selected, continue */
			if (!fix->sym->fexpr_sel_y) continue;
			
			/* check, if the symbol was selected anyway */
			if (fix->sym->type == S_BOOLEAN && fix->tri == yes) {
				deref = picosat_deref(pico, fix->sym->fexpr_sel_y->satval);
			} else if (fix->sym->type == S_TRISTATE && fix->tri == yes) {
				deref = picosat_deref(pico, fix->sym->fexpr_sel_y->satval);
			} else if (fix->sym->type == S_TRISTATE && fix->tri == mod) {
				deref = picosat_deref(pico, fix->sym->fexpr_sel_m->satval);
			}
			
			if (deref == 1)
				g_array_remove_index(diagnosis_symbol, j--);
			
			deref = 0;
		}
		
		g_array_prepend_val(diagnoses_symbol, diagnosis_symbol);
	}
	
	end = clock();
	time = ((double) (end - start)) / CLOCKS_PER_SEC;
	
	printf("done. (%.6f secs.)\n", time);

	return diagnoses_symbol;
}

/*
 * list the diagnoses and let user choose a diagnosis to be applied
 */
GArray * choose_fix(GArray *diag)
{
	printf("=== GENERATED DIAGNOSES ===\n");
	printf("0: No changes wanted\n");
	print_diagnoses_symbol(diag);
	
	int choice;
	printf("\n> Choose option: ");
	scanf("%d", &choice);
	
	/* no changes wanted */
	if (choice == 0) return NULL;
	
	return g_array_index(diag, GArray *, choice - 1);
}

/*
 * apply the fixes from a diagnosis
 */
void apply_fix(GArray *diag)
{
	struct symbol_fix *fix;
	unsigned int i, no_symbols_set = 0, iterations = 0;
	GArray *tmp = g_array_copy(diag);

	
	printf("\nTrying to apply fixes now...\n");
	
	while (no_symbols_set < diag->len) {
		if (iterations > diag->len * 2) {
			printf("\nCould not apply all values :-(.\n");
			return;
		}
		
		for (i = 0; i < tmp->len; i++) {
			fix = g_array_index(tmp, struct symbol_fix *, i);
			
			/* update symbol's current value */
			sym_calc_value(fix->sym);
			
			/* value already set? */
			if (fix->type == SF_BOOLEAN) {
				if (fix->tri == sym_get_tristate_value(fix->sym)) {
					g_array_remove_index(tmp, i--);
					no_symbols_set++;
					continue;
				}
			} else if (fix->type == SF_NONBOOLEAN) {
				if (strcmp(str_get(&fix->nb_val),sym_get_string_value(fix->sym)) == 0) {
					g_array_remove_index(tmp, i--);
					no_symbols_set++;
					continue;
				}
			} else {
				perror("Error applying fix. Value set for disallowed.");
			}

				
			/* could not set value, try next */
			if (fix->type == SF_BOOLEAN) {
				if (!sym_set_tristate_value(fix->sym, fix->tri)) {
// 					printf("Could not set value for %s.\n", sym_get_name(fix->sym));
					continue;
				}
			} else if (fix->type == SF_NONBOOLEAN) {
				if (!sym_set_string_value(fix->sym, str_get(&fix->nb_val))) {
					continue;
				}
			} else {
				perror("Error applying fix. Value set for disallowed.");
			}

			
			/* could set value, remove from tmp */
			if (fix->type == SF_BOOLEAN) {
				printf("%s set to %s.\n", sym_get_name(fix->sym), tristate_get_char(fix->tri));
			} else if (fix->type == SF_NONBOOLEAN) {
				printf("%s set to %s.\n", sym_get_name(fix->sym), str_get(&fix->nb_val));
			}
			
			g_array_remove_index(tmp, i--);
			no_symbols_set++;
		}
		iterations++;
	}

	printf("\nFixes successfully applied.\n");
}

/*
 * apply the fixes from a diagnosis
 */
// void apply_fix2(GArray *diag)
// {
// 	struct symbol_fix *fix;
// 	struct symbol *sym;
// 	unsigned int i;
// 	char *file_sat = ".config_sat";
// 	GHashTable *map = g_hash_table_new_full(
// 		g_str_hash,
// 		g_str_equal,
// 		NULL,
// 		free
//   	);
// 	
// 	printf("\nTrying to apply fixes now...\n");
// 
// 	/* store the current configuration in a hashmap */
// 	printf("Backing up old values...");
// 	for_all_symbols(i, sym) {
// 		if (sym->type == S_UNKNOWN) continue;
// 		
// 		if (g_hash_table_lookup(map, sym_get_name(sym)) != NULL)
// 			printf("Double key found: %s\n", sym_get_name(sym));
// 		
// 		g_hash_table_insert(map, strdup(sym_get_name(sym)), strdup(sym_get_string_value(sym)));
// 	}
// 	printf("done.\n");
// 	
// 	/* 
// 	 * changes will be applied as a "transaction"
// 	 * we allow illegal values temporarily
// 	 */
// 	printf("Setting new values...");
// 	for (i = 0; i < diag->len; i++) {
// 		fix = g_array_index(diag, struct symbol_fix *, i);
// 		
// 		if (fix->type == SF_BOOLEAN) {
// 			sym_set_tristate_value_mod(fix->sym, fix->tri);
// 		} else if (fix->type == SF_NONBOOLEAN) {
// 			sym_set_string_value(fix->sym, str_get(&fix->nb_val));
// 		}
// 	}
// 	printf("done.\n");
// 	
// 	/* Just updating the symbols' does not suffice.
// 	 * The values for everything else needs to be updated as well.
// 	 * Recalculations must be performed.
// 	 * One way of doing it is to save the current configuration and then
// 	 * simply reload it, thereby recalculating everything.
// 	 */
// 	
// 	/* store the changes... */
// 	printf("Reloading configuration...\n");
// 	conf_write(file_sat);
// 	
// 	/* ...and reload the configuration */
// 	conf_read(file_sat);
// 	printf("done.\n");
// 	
// 	/* check, that all values are within range */
// 	printf("Checking range of all symbols...");
// 	for (i = 0; i < diag->len; i++) {
// 		fix = g_array_index(diag, struct symbol_fix *, i);
// 		
// 		if (fix->type == SF_BOOLEAN) {
// 			if (!sym_tristate_within_range(fix->sym, fix->tri))
// 				printf("Symbol %s not within range\n", sym_get_name(fix->sym));
// 		} else if (fix->type == SF_NONBOOLEAN) {
// 			if (!sym_string_within_range(fix->sym, str_get(&fix->nb_val)))
// 				printf("Symbol %s not within range\n", sym_get_name(fix->sym));
// 		}	
// 	}
// 	printf("done.\n");
// 	
// 	/* 
// 	 * check, that only the proposed changes have changed
// 	 * exception for symbols without a prompt
// 	 */
// 	printf("Checking, that only symbols from the diagnosis have changed...");
// 	for_all_symbols(i, sym) {
// 		char *oldval = g_hash_table_lookup(map, sym_get_name(sym));
// 		char *newval = strdup(sym_get_string_value(sym));
// 		
// 		/* 
// 		 * if the symbol is in the diagnosis, it must change
// 		 * if the symbol is not in the diagnosis, the value does not change
// 		 */
// 		if (diagnosis_contains_symbol(diag, sym)) {
// 			if (strcmp(oldval, newval) == 0)
// 				printf("%s should have changed.\n", sym_get_name(sym));
// 		} else {
// 			if (strcmp(oldval, newval) != 0)
// 				printf("%s should not change.\n", sym_get_name(sym));
// 		}
// 	}
// 	printf("done.\n");
// 	
// 	/* roll back to original configuration in case of problems */
// 	TODO
// 	
// 	/* clean up */
// 	if (remove(file_sat))
// 		printf("Error: could not remove temporary files.\n");
// 
// 
// 	printf("Applying fixes done.\n");
// }

/*
 * calculate the new value for a boolean symbol given a diagnosis and an fexpr
 */
static tristate calculate_new_tri_val(struct fexpr *e, GArray *diagnosis)
{
	assert(sym_is_boolean(e->sym));
	
	/* return the opposite of the last assumption for booleans */
	if (e->sym->type == S_BOOLEAN)
		return e->assumption ? no : yes;
	
	/* new values for tristate must be deduced from the diagnosis */
	if (e->sym->type == S_TRISTATE) {
		/* fexpr_y */
		if (e->tri == yes) {
			if (e->assumption == true)
				/* 
				 * if diagnosis contains fexpr_m, fexpr_m was false
				 * => new value is mod
				 */
				return diagnosis_contains_fexpr(diagnosis, e->sym->fexpr_m) ? mod : no;
			else if (e->assumption == false)
				/*
				 * if fexpr_y is set to true, the new value must be yes
				 */
				return yes;
		}
		/* fexpr_m */
		if (e->tri == mod) {
			if (e->assumption == true)
				/*
				 * if diagnosis contains fexpr_y, fexpr_y was false
				 * => new value is yes
				 */
				return diagnosis_contains_fexpr(diagnosis, e->sym->fexpr_m) ? yes : no;
			else if (e->assumption == false)
				/*
				 * if diagnosis contains fexpr_m, the new value must be mod
				 */
				return mod;
		}
		perror("Should not get here.\n");
	}
	
	perror("Error calculating new tristate value.\n");
	return no;
}

/*
 * calculate the new value for a non-boolean symbol given a diagnosis and an fexpr
 */
static const char * calculate_new_string_value(struct fexpr *e, GArray *diagnosis)
{
	assert(sym_is_nonboolean(e->sym));

	/* if assumption was false before, this is the new value because only 1 variable can be true */
	if (e->assumption == false)
		return str_get(&e->nb_val);
	
	struct fexpr *e2;
	unsigned int i;
	
	/* a diagnosis always contains 2 variables for the same symbol
	* one is set to true, the other to false
	* otherwise you'd set 2 variables to true, which is not allowed */
	for (i = 0; i < diagnosis->len; i++) {
		e2 = g_array_index(diagnosis, struct fexpr *, i);
		
		/* not interested in other symbols or the same fexpr */
		if (e->sym != e2->sym || e == e2) continue;
		
		return str_get(&e2->nb_val);
	}

	perror("Error calculating new string value.\n");
	return "";
}
