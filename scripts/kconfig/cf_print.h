/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Patrick Franz <patfra71@gmail.com>
 */

#ifndef CF_PRINT_H
#define CF_PRINT_H

#define OUTFILE_FEXPR "./scripts/kconfig/cf_constraints.out"

/* print all symbols */
void print_all_symbols(void);

/* print a symbol's name */
void print_sym_name(struct symbol *sym);

/* print an expr */
void print_expr(char *tag, struct expr *e, int prevtoken);

/* print some debug info about the tree structure of k_expr */
void debug_print_kexpr(struct k_expr *e);

/* print a kexpr */
void print_kexpr(char *tag, struct k_expr *e);

/* write a kexpr into a string */
void kexpr_as_char(struct k_expr *e, struct gstr *s);

/* print all constraints for a symbol */
void print_sym_constraint(struct symbol *sym);

/* print the satmap */
// void print_satmap(gpointer key, gpointer value, gpointer userData);

/* print a default map */
void print_default_map(GArray *map); 

/* write all CNF-clauses into a file in DIMACS-format */
void write_cnf_to_file(GArray *cnf_clauses, int sat_variable_nr, int nr_of_clauses);

/* write all constraints into a file for testing purposes */
void write_constraints_to_file_kcr(void);

#endif
