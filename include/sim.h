/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/
#ifndef _SIM_H
#define _SIM_H

typedef struct _sim *sim_t;

/* SIM session */
_public sim_t sim_new(cci_t cc);
_public int sim_sms_save(sim_t sim, const char *fn);
_public int sim_sms_restore(sim_t sim, const char *fn);
_public void sim_free(sim_t sim);

#endif /* _GSM_H */
