/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 Patrick Franz <deltaone@debian.org>
 */

#ifndef CONFIGFIX_H
#define CONFIGFIX_H

/* make functions accessible from xconfig */
#ifdef __cplusplus
extern "C" {
#endif

/* include own definitions */
#include "cf_defs.h"
#include "cf_fixgen.h"

/* external functions */
struct sfix_list **run_satconf(struct symbol_dvalue **symbols, size_t n,
			       size_t *num_solutions, bool *trivial,
			       enum fixgen_exit_status *status);
struct sfl_list *run_satconf_list(struct sdv_list *symbols, bool *trivial,
				  enum fixgen_exit_status *status);
int apply_fix(struct sfix_list *fix);
int run_satconf_cli(const char *Kconfig_file);
void interrupt_fix_generation(void);
struct sfix_list *select_solution(struct sfl_list *solutions, int index);
int fix_symbol_count(struct sfix_list *fix);
struct symbol_fix *select_symbol(struct sfix_list *solution, int index);
void cf_sfix_list_free(struct sfix_list *list);

/* make functions accessible from xconfig */
#ifdef __cplusplus
}
#endif
#endif
