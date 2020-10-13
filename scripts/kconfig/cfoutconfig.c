/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Patrick Franz <patfra71@gmail.com>
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

#include "configfix.h"

#define OUTFILE_CONSTRAINTS "./scripts/kconfig/cfout_constraints.txt"
#define OUTFILE_DIMACS "./scripts/kconfig/cfout_constraints.dimacs"

static void write_constraints_to_file(void);
static void write_dimacs_to_file(PicoSAT *pico);

/* -------------------------------------- */

int main(int argc, char *argv[])
{
	clock_t start, end;
	double time;
	
// 	printf("\nHello configfix!\n\n");
	
	printf("\nInit...");
	/* measure time for constructing constraints and clauses */
	start = clock();
	
	/* parse Kconfig-file and read .config */
	init_config(argv[1]);

	/* initialize satmap and cnf_clauses */
	init_data();

	/* creating constants */
	create_constants();

	/* assign SAT variables & create sat_map */
	assign_sat_variables();

	/* get the constraints */
	get_constraints();

	end = clock();
	time = ((double) (end - start)) / CLOCKS_PER_SEC;
	
	printf("done. (%.6f secs.)\n", time);
	
	/* start PicoSAT */
	PicoSAT *pico = picosat_init();
	picosat_enable_trace_generation(pico);
	printf("Building CNF-clauses...");
	start = clock();
	
	/* construct the CNF clauses */
	construct_cnf_clauses(pico);
	
	end = clock();
	time = ((double) (end - start)) / CLOCKS_PER_SEC;
	printf("done. (%.6f secs.)\n", time);
	
	printf("\n");
	
	/* write constraints into file */
	start = clock();
	printf("Writing constraints...");
	write_constraints_to_file();
	end = clock();
	time = ((double) (end - start)) / CLOCKS_PER_SEC;
	printf("done. (%.6f secs.)\n", time);
	
	/* write SAT problem in DIMACS into file */
	start = clock();
	printf("Writing SAT problem in DIMACS...");
	write_dimacs_to_file(pico);
	end = clock();
	time = ((double) (end - start)) / CLOCKS_PER_SEC;
	printf("done. (%.6f secs.)\n", time);
	
	printf("\nConstraints have been written into %s\n", OUTFILE_CONSTRAINTS);
	printf("SAT problem has been written in %s\n", OUTFILE_DIMACS);
	
	return 0;
}

static void write_constraints_to_file(void)
{
	FILE *fd = fopen(OUTFILE_CONSTRAINTS, "w");
	unsigned int i, j;
	struct symbol *sym;
	struct fexpr *e;
	
	for_all_symbols(i, sym) {
		if (sym->type == S_UNKNOWN) continue;
		
		for (j = 0; j < sym->constraints->arr->len; j++) {
			e = g_array_index(sym->constraints->arr, struct fexpr *, j);
			struct gstr s = str_new();
			fexpr_as_char_short(e, &s, -1);
			fprintf(fd, "%s\n", str_get(&s));
			str_free(&s);
		}
	}
	fclose(fd);
}

static void add_comments(gpointer key, gpointer value, gpointer fd)
{
	struct fexpr *e = (struct fexpr *) value;
	
	if (
		e->type == FE_TMPSATVAR || 
		e->type == FE_SELECT ||
		e->type == FE_CHOICE ||
		e->type == FE_FALSE ||
		e->type == FE_TRUE
	) return;
	
	fprintf((FILE *) fd, "c %d %s\n", *((int *) key), str_get(&e->name));
}

static void write_dimacs_to_file(PicoSAT *pico)
{
	FILE *fd = fopen(OUTFILE_DIMACS, "w");
	g_hash_table_foreach(satmap, add_comments, fd);
	picosat_print(pico, fd);
	fclose(fd);
}
