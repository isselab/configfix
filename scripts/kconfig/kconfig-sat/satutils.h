#ifndef SATUTILS_H
#define SATUTILS_H

/* initialize PicoSAT */
PicoSAT * initialize_picosat(void);

/* add clauses to the PicoSAT */
void picosat_add_clauses(PicoSAT *pico);

/* start PicoSAT */
void picosat_solve(PicoSAT *pico);


/* add assumption for a symbol to the SAT-solver */
void sym_add_assumption(PicoSAT *pico, struct symbol *sym);

/* add assumption for a boolean symbol to the SAT-solver */
void sym_add_assumption_tri(PicoSAT *pico, struct symbol *sym, tristate tri_val);

#endif
