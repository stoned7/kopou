#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "kopou.h"


#define CONFIG_LINE_LENGTH_MAX 1024

static inline int config_yesnotoi(kstr_t str)
{
	if (!strcasecmp(str,"yes")) return 1;
	else if (!strcasecmp(str,"no")) return 0;
	else return -1;
}

static void set_config_from_str(kstr_t config);

void set_config_from_file(const kstr_t filename)
{
        char buf[CONFIG_LINE_LENGTH_MAX];
        kstr_t config = kstr_empty_new();

        FILE *configfile;
        configfile = fopen(filename, "r");
        if (configfile) {
                while (fgets(buf, sizeof(buf), configfile) != NULL) {
			config = kstr_cat_strn(&config, buf);
                }
                fclose(configfile);
		load_config_from_str(config);
        }
        kstr_del(config);
}

