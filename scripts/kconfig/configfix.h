/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Patrick Franz <patfra71@gmail.com>
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


/* include glib */
#include <glib.h>


/* include own definitions */
#include "cf_defs.h"


/* include other header files needed */
#include "picosat.h"
#include "cf_constraints.h"
#include "cf_fexpr.h"
#include "cf_print.h"
#include "cf_rangefix.h"
#include "cf_satutils.h"
#include "cf_utils.h"


/* external functions */
GArray * run_satconf(GArray *arr);

int apply_fix(GArray *diag);

int run_satconf_cli(const char *Kconfig_file);


/* make functions accessible from xconfig */
#ifdef __cplusplus
}
#endif

#endif
