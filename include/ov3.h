#ifndef _OV3_H_
#define _OV3_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define OV3_READ  1
#define OV3_WRITE 2

BOOL OV3open(int cachesize, int mode);
BOOL OV3groupstats(char *group, int *lo, int *hi, int *count, int *flag);
BOOL OV3groupadd(char *group, char *flag);
BOOL OV3groupdel(char *group);
BOOL OV3add(TOKEN token, char *data, int len);
void *OV3opensearch(char *group, int low, int high);
BOOL OV3search(void *handle, int *artnum, char **data, int *len, TOKEN *token);
void OV3closesearch(void *handle);
BOOL OV3getartinfo(char *group, int artnum, char **data, int *len, TOKEN *token);
BOOL OV3expiregroup(char *group);
void OV3close(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _OV3_H_ */
#ifndef _OV3_H_
#define _OV3_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define OV3_READ  1
#define OV3_WRITE 2

BOOL OV3open(int cachesize, int mode);
BOOL OV3groupstats(char *group, int *lo, int *hi, int *count, int *flag);
BOOL OV3groupadd(char *group, char *flag);
BOOL OV3groupdel(char *group);
BOOL OV3add(TOKEN token, char *data, int len);
void *OV3opensearch(char *group, int low, int high);
BOOL OV3search(void *handle, int *artnum, char **data, int *len, TOKEN *token);
void OV3closesearch(void *handle);
BOOL OV3getartinfo(char *group, int artnum, char **data, int *len, TOKEN *token);
BOOL OV3expiregroup(char *group);
void OV3close(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _OV3_H_ */
#ifndef _OV3_H_
#define _OV3_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define OV3_READ  1
#define OV3_WRITE 2

BOOL OV3open(int cachesize, int mode);
BOOL OV3groupstats(char *group, int *lo, int *hi, int *count, int *flag);
BOOL OV3groupadd(char *group, char *flag);
BOOL OV3groupdel(char *group);
BOOL OV3add(TOKEN token, char *data, int len);
void *OV3opensearch(char *group, int low, int high);
BOOL OV3search(void *handle, int *artnum, char **data, int *len, TOKEN *token);
void OV3closesearch(void *handle);
BOOL OV3getartinfo(char *group, int artnum, char **data, int *len, TOKEN *token);
BOOL OV3expiregroup(char *group);
void OV3close(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _OV3_H_ */
