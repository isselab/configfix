#ifndef SATCONF_H
#define SATCONF_H

/* make functions accessible from xconfig */
#ifdef __cplusplus
extern "C" {
#endif

/* include internal definitions */
#define LKC_DIRECT_LINK
#include "../lkc.h"


/* include glib */
#include <glib.h>


/* include own definitions */
#include "defs.h"


/* include other header files needed */
#include "picosat.h"
#include "cnf.h"
#include "constraints.h"
#include "fexpr.h"
#include "print.h"
#include "rangefix.h"
#include "satutils.h"
#include "utils.h"


/* external functions */
GArray * run_satconf(GArray *arr);

int apply_satfix(GArray *fix);

int run_satconf_cli(const char *Kconfig_file);


/* make functions accessible from xconfig */
#ifdef __cplusplus
}
#endif

#endif
