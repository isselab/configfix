// SPDX-License-Identifier: GPL-2.0
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

static struct symbol * read_symbol_from_stdin(void);
static struct symbol_dvalue * sym_create_sdv(struct symbol *sym, char *input);
static void handle_fixes(struct sfl_list *diag);

/* -------------------------------------- */

int main(int argc, char *argv[])
{
	CFDEBUG = true;

	if (argc > 1 && !strcmp(argv[1], "-s")) {
		printd("\nHello configfix!\n\n");

		run_satconf_cli(argv[2]);
		return EXIT_SUCCESS;
	}

	printd("\nCLI for configfix!\n");

	init_config(argv[1]);

	struct sfl_list *diagnoses;
	struct sdv_list *symbols;

	while(1) {
		/* create the array */
		symbols = sdv_list_init();

		/* ask for user input */
		struct symbol *sym = read_symbol_from_stdin();

		printd("Found symbol %s, type %s\n\n", sym->name, sym_type_name(sym->type));
		printd("Current value: %s\n", sym_get_string_value(sym));
		printd("Desired value: ");

		char input[100];
		fgets(input, 100, stdin);
		strtok(input, "\n");

		struct symbol_dvalue *sdv = sym_create_sdv(sym, input);
		sdv_list_add(symbols, sdv);

		diagnoses = run_satconf(symbols);
		handle_fixes(diagnoses);
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

	printd("\n");
	while (sym == NULL) {
		printd("Enter symbol name: ");
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

/*
 * print the diagnoses of type symbol_fix
 */
static void print_diagnoses_symbol(struct sfl_list *diag_sym)
{
	struct sfl_node *arr;
	unsigned int i = 1;

	sfl_list_for_each(arr, diag_sym) {
		printd(" %d: ", i++);
		print_diagnosis_symbol(arr->elem);
	}
}

static void apply_all_adiagnoses(struct sfl_list *diag) {
	printd("Applying all diagnoses now...\n");

	unsigned int counter = 1;
	struct sfl_node *node;
	sfl_list_for_each(node, diag) {
		printd("\nDiagnosis %d:\n", counter++);
		apply_fix(node->elem);

		printd("\nResetting config.\n");
		conf_read(NULL);
	}
}

/*
 * print all void print_fixes()
 */
static void handle_fixes(struct sfl_list *diag)
{
	printd("=== GENERATED DIAGNOSES ===\n");
	printd("-1: No changes wanted\n");
	printd(" 0: Apply all diagnoses\n");
	print_diagnoses_symbol(diag);

	int choice;
	printd("\n> Choose option: ");
	scanf("%d", &choice);

	if (choice == -1 || choice > diag->size)
		return;

	if (choice == 0) {
		apply_all_adiagnoses(diag);
		return;
	}

	unsigned int counter;
	struct sfl_node *node = diag->head;
	for (counter = 1; counter < choice; counter++)
		node = node->next;

	apply_fix(node->elem);

	printd("\nResetting config.\n");
	conf_read(NULL);
}
