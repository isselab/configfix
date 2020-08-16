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

#ifdef CONFIGFIX_TEST
bool apply_fix_bool(GArray *diag);
// iterate fixes in a diagnosis with and without index
#define for_all_fixes(diag, fix) \
	int fix_idx; for (fix_idx = 0; fix_idx < diag->len; fix_idx++) for (fix = g_array_index(diag, struct symbol_fix *, fix_idx);fix;fix=NULL)
#define for_every_fix(diag, i, fix) \
	for (i = 0; i < diag->len; i++) for (fix = g_array_index(diag, struct symbol_fix *, i);fix;fix=NULL)
#endif

#endif
