#ifndef RANGEFIX_H
#define RANGEFIX_H

/* initialize RangeFix and return the diagnoses */
GArray * rangefix_init(PicoSAT *pico);

/* ask user which fix to apply */
GArray * choose_fix(GArray *diag);

/* apply the fix */
void apply_fix(GArray *diag);

/* print a single diagnosis of type symbol_fix */
void print_diagnosis_symbol(GArray *diag_sym);

#endif
