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

#include "kconfig-sat/satconf.h"

// static bool init_done = false;

static struct symbol * read_symbol_from_stdin(void);
static struct symbol_dvalue * sym_create_sdv(struct symbol *sym, char *input);
// static void run_satdvconf(struct symbol_dvalue *sdv);

/* -------------------------------------- */


int main(int argc, char *argv[])
{
	printf("\nHello satdvconfig!\n");
	
	/* parse Kconfig-file and read .config */
	init_config(argv[1]);
	
	GArray *diagnoses, *chosen_fix;
	GArray *symbols;
	
	while(1) {
		/* create the array */
		symbols = g_array_new(false, false, sizeof(struct symbol_dvalue *));
		
		/* ask for user input */
		struct symbol *sym = read_symbol_from_stdin();
		
		printf("Found symbol %s, type %s\n\n", sym->name, sym_type_name(sym->type));
		printf("Current value: %s\n", sym_get_string_value(sym));
		printf("Desired value: ");
		
		char input[100];
		fgets(input, 100, stdin);
		strtok(input, "\n");
		
		struct symbol_dvalue *sdv = sym_create_sdv(sym, input);
		g_array_append_val(symbols, sdv);
		
// 		run_satdvconf(sdv);
		diagnoses = run_satconf(symbols);
		chosen_fix = choose_fix(diagnoses);
		
		if (chosen_fix != NULL)
			apply_satfix(chosen_fix);
		
		/* clear the array */
		g_array_free(symbols, TRUE);
	}

	
	return EXIT_SUCCESS;
}

/*
 * read a symbol name from stdin
 */
static struct symbol * read_symbol_from_stdin(void)
{
	char input[100];
	
	struct symbol *sym = NULL;

	printf("\n");
	while (sym == NULL) {
		printf("Enter symbol name: ");
		fgets(input, 100, stdin);
		strtok(input, "\n");
		sym = sym_find(input);
	}
	
	return sym;
}

/*
 * create a symbol_dvalue struct containing the symbol and the desired value
 */
static struct symbol_dvalue * sym_create_sdv(struct symbol *sym, char *input)
{
	struct symbol_dvalue *sdv = malloc(sizeof(struct symbol_dvalue));
	sdv->sym = sym;
	sdv->type = sym_is_boolean(sym) ? SDV_BOOLEAN : SDV_NONBOOLEAN;
	
	if (sym_is_boolean(sym)) {
		if (strcmp(input, "y") == 0)
			sdv->tri = yes;
		else if (strcmp(input, "m") == 0)
			sdv->tri = mod;
		else if (strcmp(input, "n") == 0)
			sdv->tri = no;
		else
			perror("Not a valid tristate value.");
		
		/* sanitize input for booleans */
		if (sym->type == S_BOOLEAN && sdv->tri == mod)
			sdv->tri = yes;
	} else if (sym_is_nonboolean(sym)) {
		sdv->nb_val = str_new();
		str_append(&sdv->nb_val, input);
	}

	return sdv;
}

// static void run_satdvconf(struct symbol_dvalue *sdv)
// {
// 	if (!init_done) {
// 		printf("\n");
// 		printf("Init...");
// 		/* measure time for constructing constraints and clauses */
// 		clock_t start, end;
// 		double time;
// 		start = clock();
// 
// 		/* initialize satmap and cnf_clauses */
// 		init_data();
// 		
// 		/* creating constants */
// 		create_constants();
// 		
// 		/* assign SAT variables & create sat_map */
// 		assign_sat_variables();
// 		
// 		/* get the constraints */
// 		get_constraints();
// 		
// 		/* construct the CNF clauses */
// 		construct_cnf_clauses();
// 		
// 		end = clock();
// 		time = ((double) (end - start)) / CLOCKS_PER_SEC;
// 		
// 		printf("Generating constraints and clauses...done. (%.6f secs.)\n", time);
// 		
// 		init_done = true;
// 	}
// 	
// 	/* start PicoSAT */
// 	PicoSAT *pico = initialize_picosat();
// 	picosat_add_clauses(pico);
// 	
// 	/* add unit clauses for symbol */
// 	if (sym_get_type(sdv->sym) == S_BOOLEAN) {
// 		switch (sdv->tri) {
// 		case yes:
// 			picosat_add_arg(pico, sdv->sym->fexpr_y->satval, 0);
// 			break;
// 		case no:
// 			picosat_add_arg(pico, -(sdv->sym->fexpr_y->satval), 0);
// 			break;
// 		case mod:
// 			perror("Should not happen.\n");
// 		}
// 	} else if (sym_get_type(sdv->sym) == S_TRISTATE) {
// 		switch (sdv->tri) {
// 		case yes:
// 			picosat_add_arg(pico, sdv->sym->fexpr_y->satval, 0);
// 			picosat_add_arg(pico, -(sdv->sym->fexpr_m->satval), 0);
// 			break;
// 		case mod:
// 			picosat_add_arg(pico, -(sdv->sym->fexpr_y->satval), 0);
// 			picosat_add_arg(pico, sdv->sym->fexpr_m->satval, 0);
// 			break;
// 		case no:
// 			picosat_add_arg(pico, -(sdv->sym->fexpr_y->satval), 0);
// 			picosat_add_arg(pico, -(sdv->sym->fexpr_m->satval), 0);
// 		}
// 	}
// 
// 	picosat_solve(pico);
// 	free(pico);
// }
