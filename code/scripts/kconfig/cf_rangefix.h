/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Patrick Franz <patfra71@gmail.com>
 */

#ifndef CF_RANGEFIX_H
#define CF_RANGEFIX_H

/* initialize RangeFix and return the diagnoses */
GArray * rangefix_run(PicoSAT *pico);

/* ask user which fix to apply */
GArray * choose_fix(GArray *diag);

/* apply the fix */
int apply_fix(GArray *diag);

/* print a single diagnosis of type symbol_fix */
void print_diagnosis_symbol(GArray *diag_sym);

#endif
