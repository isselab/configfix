/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Patrick Franz <patfra71@gmail.com>
 */

#ifndef CF_SATUTILS_H
#define CF_SATUTILS_H

/* initialize PicoSAT */
PicoSAT * initialize_picosat(void);

/* construct the CNF-clauses from the constraints */
void construct_cnf_clauses(PicoSAT *pico);

/* add a clause to to PicoSAT */
void sat_add_clause(int num, ...);

/* add a clause from GArray to PicoSAT */
void sat_add_clause_garray(PicoSAT *pico, GArray *arr);

/* start PicoSAT */
void picosat_solve(PicoSAT *pico);

/* add assumption for a symbol to the SAT-solver */
void sym_add_assumption(PicoSAT *pico, struct symbol *sym);

/* add assumption for a boolean symbol to the SAT-solver */
void sym_add_assumption_tri(PicoSAT *pico, struct symbol *sym, tristate tri_val);

/* add assumptions for the symbols to be changed to the SAT solver */
void sym_add_assumption_sdv(PicoSAT *pico, GArray *arr);

#endif
