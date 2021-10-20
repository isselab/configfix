// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Patrick Franz <deltaone@debian.org>
 */

#ifndef CONFIGFIX_H
#define CONFIGFIX_H

/* make functions accessible from xconfig */
#ifdef __cplusplus
extern "C" {
#endif

/* include internal definitions */
#define LKC_DIRECT_LINK
#include "lkc.h"

/* include own definitions */
#include "cf_defs.h"

/* include other header files needed */
#include "picosat.h"
#include "cf_constraints.h"
#include "cf_expr.h"
#include "cf_rangefix.h"
#include "cf_satutils.h"
#include "cf_utils.h"

/* external functions */
struct sfl_list *run_satconf(struct sdv_list *symbols);
int apply_fix(struct sfix_list *fix);
int run_satconf_cli(const char *Kconfig_file);
void interrupt_rangefix(void);
struct sfix_list *select_solution(struct sfl_list *solutions, int index);
struct symbol_fix *select_symbol(struct sfix_list *solution, int index);

/* make functions accessible from xconfig */
#ifdef __cplusplus
}
#endif
#endif
