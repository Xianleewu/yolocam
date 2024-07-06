#ifndef __RXI_INI_H__
#define __RXI_INI_H__

#ifdef __cplusplus
extern "C" {
#endif

#define INI_VERSION "0.1.1"

typedef struct ini_t ini_t;

extern ini_t*      rxi_ini_load(const char *filename);
extern void        rxi_ini_free(ini_t *ini);
extern const char* rxi_ini_get(ini_t *ini, const char *section, const char *key);
extern int         rxi_ini_sget(ini_t *ini, const char *section, const char *key, const char *scanfmt, void *dst);

#ifdef __cplusplus
}
#endif

#endif  /*__RXI_INI_H__*/
