/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Patrick Franz <patfra71@gmail.com>
 */

#ifndef CF_RANGEFIX_H
#define CF_RANGEFIX_H

/* initialize RangeFix and return the diagnoses */
struct sfl_list * rangefix_run(PicoSAT *pico);

/* ask user which fix to apply */
struct sfix_list * choose_fix(struct sfl_list *diag);

/* print a single diagnosis of type symbol_fix */
void print_diagnosis_symbol(struct sfix_list *diag_sym);

#endif
