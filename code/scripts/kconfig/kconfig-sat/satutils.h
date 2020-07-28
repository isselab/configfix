#ifndef SATUTILS_H
#define SATUTILS_H

/* initialize PicoSAT */
PicoSAT * initialize_picosat(void);

/* add a clause to to PicoSAT */
void sat_add_clause(int num, ...);

/* add a clause from GArray to PicoSAT */
void sat_add_clause_garray(PicoSAT *pico, GArray *arr);

/* add clauses to the PicoSAT */
// void picosat_add_clauses(PicoSAT *pico);

/* start PicoSAT */
void picosat_solve(PicoSAT *pico);


/* add assumption for a symbol to the SAT-solver */
void sym_add_assumption(PicoSAT *pico, struct symbol *sym);

/* add assumption for a boolean symbol to the SAT-solver */
void sym_add_assumption_tri(PicoSAT *pico, struct symbol *sym, tristate tri_val);

/* add assumptions for the symbols to be changed to the SAT solver */
void sym_add_assumption_sdv(PicoSAT *pico, GArray *arr);

#endif
