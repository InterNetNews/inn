/* sasl_config.h
   Copyright (C) 2000 Kenichi Okada <okada@opaopa.org>

   Author: Kenichi Okada <okada@opaopa.org>
   Created: 2000-03-04
*/

#ifndef SASL_CONFIG_H
#define SASL_CONFIG_H

#ifndef P
#ifdef __STDC__
#define P(x) x
#else
#define P(x) ()
#endif
#endif

extern void sasl_config_read P((void));
extern const char *sasl_config_getstring P((const char *key, const char *def));
extern int sasl_config_getint P((const char *key, int def));
extern int sasl_config_getswitch P((const char *key, int def));
extern const char *sasl_config_partitiondir P((const char *partition));

#endif /* SASL_SASL_CONFIG_H */
