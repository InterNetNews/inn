/* sasl_config.h
   Copyright (C) 2000 Kenichi Okada <okada@opaopa.org>

   Author: Kenichi Okada <okada@opaopa.org>
   Created: 2000-03-04
*/

#ifndef SASL_CONFIG_H
#define SASL_CONFIG_H

extern void sasl_config_read(void);
extern const char *sasl_config_getstring(const char *key, const char *def);
extern int sasl_config_getint(const char *key, int def);
extern int sasl_config_getswitch(const char *key, int def);
extern const char *sasl_config_partitiondir(const char *partition);

#endif /* SASL_SASL_CONFIG_H */
