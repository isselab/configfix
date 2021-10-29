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

	printf("\nCreating constraints and CNF clauses...");
	/* measure time for constructing constraints and clauses */
	start = clock();

	/* parse Kconfig-file and read .config */
	init_config(argv[1]);

	/* initialize satmap and cnf_clauses */
	init_data();

	/* creating constants */
	create_constants();

	/* assign SAT variables & create sat_map */
	create_sat_variables();

	/* get the constraints */
	get_constraints();

	end = clock();
	time = ((double) (end - start)) / CLOCKS_PER_SEC;

	printd("done. (%.6f secs.)\n", time);

	/* start PicoSAT */
	PicoSAT *pico = picosat_init();
	picosat_enable_trace_generation(pico);
	printd("Building CNF-clauses...");
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
	printf("DIMACS-output has been written into %s\n", OUTFILE_DIMACS);

	return 0;
}

static void write_constraints_to_file(void)
{
	FILE *fd = fopen(OUTFILE_CONSTRAINTS, "w");
	unsigned int i;
	struct symbol *sym;

	for_all_symbols(i, sym) {
		if (sym->type == S_UNKNOWN) continue;

		struct pexpr_node *node;
		pexpr_list_for_each(node, sym->constraints) {
			struct gstr s = str_new();
			pexpr_as_char(node->elem, &s, 0);
			fprintf(fd, "%s\n", str_get(&s));
			str_free(&s);
		}
	}
	fclose(fd);
}

static void add_comment(FILE *fd, struct fexpr *e)
{
	fprintf(fd, "c %d %s\n", e->satval, str_get(&e->name));
}

static void write_dimacs_to_file(PicoSAT *pico)
{
	FILE *fd = fopen(OUTFILE_DIMACS, "w");

	unsigned int i;
	for (i = 1; i < sat_variable_nr; i++)
		add_comment(fd, &satmap[i]);

	picosat_print(pico, fd);
	fclose(fd);
}
