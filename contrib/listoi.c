/* display an overview.index file */

#include <stdio.h>
#include <sys/types.h>
#include "configdata.h"
#include "clibrary.h"
#include "innd.h"

/*
**  Print a usage message and exit.
*/
STATIC NORETURN
Usage(void)
{
        (void)fprintf(stderr, "Usage: listoi newsgroup\n");
        exit(1);
}

main(int ac, char *av[]) {

        FILE *fi;
        char packed[OVERINDEXPACKSIZE];
        char *p, *g;
        OVERINDEX index;

        if (ac!=2)
                Usage();
        g=av[1];
        
        /* convert the `.' in the group to `/' */
        while ((p=strchr(g, '.'))!=NULL)
                *p='/';

        /* Set defaults. */
        if (ReadInnConf() < 0) exit(1);

        p = NEW(char, strlen(innconf->pathoverview) + strlen(g) +
            strlen(innconf->overviewname) + 32);
        sprintf(p, "%s/%s/%s.index", innconf->pathoverview, g,
            innconf->overviewname);

        if ((fi = fopen(p, "r")) == NULL) {
                perror("open overview file");
                free(p);
                exit(1);
        }
        free(p);
        while (fread(&packed, OVERINDEXPACKSIZE, 1, fi) == 1) {
                UnpackOverIndex(packed, &index);
                printf("%d %s\n", index.artnum, HashToText(index.hash));
        }
        fclose(fi);

}

