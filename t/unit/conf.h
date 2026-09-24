/*
 * Minimal mock of ProFTPD's conf.h, just enough to compile mod_clamav.c in
 * an isolated unit test. Not meant for a real build.
 */
#ifndef MOCK_CONF_H
#define MOCK_CONF_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/un.h>

#define CURRENT_CONF 0

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

typedef unsigned long pr_off_t;
typedef int MODRET;

#define PR_LOG_DEBUG 0
#define PR_LOG_INFO 1
#define PR_LOG_NOTICE 2
#define PR_LOG_WARNING 3
#define PR_LOG_ERR 4
#define PR_LOG_CRIT 5
#define DEBUG4 4

#define C_STOR "STOR"
#define C_APPE "APPE"
#define C_STOU "STOU"

#define R_550 550

#define CONF_ROOT 1
#define CONF_LIMIT 2
#define CONF_VIRTUAL 4
#define CONF_GLOBAL 8
#define CONF_DIR 16
#define CONF_PARAM 32
#define CF_MERGEDOWN 1

#define PR_LU "lu"

typedef struct {
  char *name;
  void *argv[8];
  int argc;
  void *pool;
  int flags;
} config_rec;

typedef struct {
  char *argv[8];
  int argc;
  void *pool;
  void *tmp_pool;
} cmd_rec;

typedef struct {
  void *fh_pool;
  char *fh_path;
  int fh_fd;
} pr_fh_t;

typedef struct {
  int (*close)(pr_fh_t *, int);
} pr_fs_t;

typedef struct {
  char *chroot_path;
  const char *curr_cmd;
  void *pool;
} pr_session_t;

extern pr_session_t session;

typedef struct {
  const char *name;
  MODRET (*handler)(cmd_rec *);
  void *next;
} conftable;

typedef struct {
  void *a;
  void *b;
  int c;
  const char *name;
  conftable *conftab;
  void *d;
  void *e;
  void *f;
  int (*sess_init)(void);
  const char *version;
} module;

config_rec *find_config(int, int, const char *, int);
void *get_param_ptr(int, const char *, int);
int get_boolean(cmd_rec *, int);
config_rec *add_config_param(const char *, int, void *);
config_rec *add_config_param_str(const char *, int, void *);

void *pcalloc(void *, size_t);
char *pstrdup(void *, const char *);
char *pdircat(void *, ...);
char *pstrcat(void *, ...);

void pr_log_pri(int, const char *, ...);
void pr_log_debug(int, const char *, ...);
void pr_trace_msg(const char *, int, const char *, ...);

int pr_signals_handle(void);

int pr_fsio_unlink(const char *);
int pr_fsio_fstat(pr_fh_t *, struct stat *);
void pr_fs_clear_cache(void);
char *pr_fs_getcwd(void);

void pr_session_end(int);
const char *pr_session_get_protocol(int);
void pr_event_generate(const char *, ...);
int pr_event_register(module *, const char *, void *, void *);
int pr_response_add_err(int, const char *, ...);
pr_fs_t *pr_register_fs(void *, const char *, const char *);

#define CHECK_ARGS(cmd, n) do { } while (0)
#define CHECK_CONF(cmd, flags) do { } while (0)
#define CONF_ERROR(cmd, msg) do { return 0; } while (0)
#define PR_HANDLED(cmd) 1

#endif /* MOCK_CONF_H */
