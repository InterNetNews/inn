/*
June 14, 1999

Various bug fixes, code and documentation improvements since then
in 2002, 2003, 2014, 2016, 2021-2024.

Recover text articles from cyclic buffers
Articles start with  "\0Path:"
and end with "\r\n.\r\n"

Tested with INND 2.2 under AIX 4.2

rifkin@uconn.edu
*/
/*
(1) Pull 16 bytes at a time
(2) Last 7 bytes must be \000\000\000Path
(3) When found, print "\nPath";
(4) print subsequent bytes until \r\n.\r\n found
*/

#include "portable/system.h"

#define INFILE       1
#define FILEPREFIX   2
#define HEADER       3
#define STRING       4

/*  String buffer size  */
#define NBUFF        512

#define MAX_ART_SIZE 2200000

int WriteArticle(char *, int, char *, char *, char *, int);

static char ArtHead[7] = {0, 0, 0, 'P', 'a', 't', 'h'};
static char ArtTail[5] = {'\r', '\n', '.', '\r', '\n'};
static int LenTail = 5;

int
main(int argc, char *argv[])
{
    FILE *Infile;
    int NumTailCharFound;
    bool ReadingArticle = false;
    char buffer[32];
    char *obuffer = NULL;
    char *header = NULL;
    char *string = NULL;
    int osize = MAX_ART_SIZE;
    int opos = 0;
    int i;
    int fileno = 0;
    int artno = 0;

    /*  Check number of args  */
    if (argc < 3) {
        printf("Usage:\n");
        printf("  pullart <cycbuff> <fileprefix> [<header> <string>]\n\n");
        printf("  Read cycbuffer <cycbuff> and print all articles whose\n");
        printf("  article header field body <header> contains <string>.\n\n");
        printf("  Articles are written to files name <fileprefix>.nnnnnn\n");
        printf("  where nnnnnn is numbered sequentially from 0.\n\n");
        printf("  If <header> and <string> not specified, all articles\n");
        printf("  are written.\n\n");
        printf("Examples:\n");
        printf("  pullart /news3/cycbuff.3 alt.rec  Newsgroup: alt.rec\n");
        printf("  pullart /news3/cycbuff.3  all\n");
        printf("  pullart firstbuff  article Subject bluejay\n");
        exit(1);
    }

    /*  Allocate output buffer  */
    obuffer = (char *) calloc(osize + 1, sizeof(char));
    if (obuffer == NULL) {
        printf("Cannot allocate obuffer[]\n");
        return 1;
    }


    /*  Open input file  */
    Infile = fopen(argv[INFILE], "rb");
    if (Infile == NULL) {
        printf("Cannot open input file.\n");
        free(obuffer);
        return 1;
    }


    if (argc >= 4)
        header = argv[HEADER];
    if (argc >= 5)
        string = argv[STRING];
    if (header != NULL && *header == '\0') {
        header = NULL;
    }
    if (string != NULL && *string == '\0') {
        string = NULL;
    }

    /*test*/
    printf("filename <%s>\n", argv[INFILE]);
    printf("fileprefix <%s>\n", argv[FILEPREFIX]);
    printf("header <%s>\n", header != NULL ? header : NULL);
    printf("string <%s>\n", string != NULL ? string : NULL);


    /*  Skip first 0x38000 16byte buffers  */
    i = fseek(Infile, 0x38000L, SEEK_SET);

    /*  Read following 16 byte buffers  */
    ReadingArticle = false;
    NumTailCharFound = 0;
    artno = 0;
    while (0 != fread(buffer, 16, 1, Infile)) {

        /*  Found start of article, start writing to obuffer  */
        if (0 == memcmp(buffer + 9, ArtHead, 7)) {
            ReadingArticle = true;
            memcpy(obuffer, "Path", 4);
            opos = 4;
            continue;
        }

        /*  Currently reading article  */
        if (ReadingArticle) {
            for (i = 0; i < 16; i++) {

                /*  Article too big, drop it and move on  */
                if (opos >= osize) {
                    printf("article number %i bigger than buffer size %i.\n",
                           artno + 1, osize);
                    artno++;
                    ReadingArticle = false;
                    break;
                }

                /*  Add current character to output buffer, but remove \r  */
                if ('\r' != buffer[i])
                    obuffer[opos++] = buffer[i];

                /*  Check for article ending sequence  */
                if (buffer[i] == ArtTail[NumTailCharFound]) {
                    NumTailCharFound++;
                } else
                    NumTailCharFound = 0;

                /*  End found, write article, reset for next  */
                if (NumTailCharFound == LenTail) {
                    ReadingArticle = false;
                    NumTailCharFound = 0;

                    /*  Add trailing \0 to buffer  */
                    obuffer[opos + 1] = '\0';

                    fileno += WriteArticle(obuffer, opos, argv[FILEPREFIX],
                                           header, string, fileno);
                    artno++;
                    break;
                }
            }
        }
    }

    fclose(Infile);
    free(obuffer);

    return 0;
}


/*
Writes article stored in buff[] if it has a
"Newsgroups:" header field which contains *newsgroup
Write to a file named  fileprefix.fileno
*/
int
WriteArticle(char *buff, int n, char *fileprefix, char *headerin, char *string,
             int fileno)
{
    char *begptr;
    char *endptr;
    char *newsptr;
    char savechar;
    char header[NBUFF];
    char filename[NBUFF];
    FILE *outfile;


    /*  Prevent buffer overflow due to fileprefix too long  */
    if (strlen(fileprefix) > 384) {
        printf("program error: cannot have file prefix greater than 384 "
               "characters\n");
        exit(1);
    }

    /*
    Is header field here?  Search if header string requested, leave if not
    found
    */
    if (headerin != NULL) {
        /*  Find \nHEADER  */
        strlcpy(header, "\n", sizeof(header));
        strlcat(header, headerin, sizeof(header));

        begptr = strstr(buff, header);

        /*  return if Header name not found  */
        if (begptr == NULL) {
            return 0;
        }

        /*
        Header field found. What about string?
        Search if string requested, leave if not found
        */
        if (string != NULL) {
            /*  Find end of header field body  */
            begptr++;
            endptr = strchr(begptr, '\n');

            /*  Something is wrong, end of header field body not found, do not
             * write article
             */
            if (endptr == NULL)
                return 0;

            /*  Temporarily make string end a null char  */
            savechar = *endptr;
            *endptr = '\0';
            newsptr = strstr(begptr, string);

            /*  Requested newsgroup not found  */
            if (newsptr == NULL)
                return 0;

            /*  Restore character at end of header string  */
            *endptr = savechar;
        }
        /*  No string specified  */
    }
    /*  No header field specified  */

    /*  Open file, write buffer, close file  */
    snprintf(filename, sizeof(filename), "%s.%06i", fileprefix, fileno);

    outfile = fopen(filename, "wt");
    if (outfile == NULL) {
        printf("Cannot open file name %s\n", filename);
        exit(1);
    }

    while (n--)
        fprintf(outfile, "%c", *buff++);

    fclose(outfile);

    /*  Return number of files written  */
    return 1;
}
