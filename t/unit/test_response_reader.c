/*
 * Unit test for mod_clamav's ClamAV response reader and result parser.
 *
 * Exercises clamav_read_response_line() and clamav_response_is_ok() against
 * well-formed and malformed scanner replies, including the embedded-NUL
 * cases that must be rejected rather than silently accepted as clean.
 *
 * Build and run (AddressSanitizer + UndefinedBehaviorSanitizer):
 *
 *   cc -std=c11 -g -Wall -fsanitize=address,undefined \
 *      -I t/unit -o t/unit/test_response_reader t/unit/test_response_reader.c
 *   ./t/unit/test_response_reader
 */
#include "conf.h"
#include "privs.h"

/* Globals / functions the module references but that this test does not
 * exercise. Provided so mod_clamav.c links in isolation. */
pr_session_t session;

int pr_signals_handle(void) { return 0; }
config_rec *find_config(int a, int b, const char *c, int d) {
  (void) a; (void) b; (void) c; (void) d; return NULL;
}
void *get_param_ptr(int a, const char *b, int c) {
  (void) a; (void) b; (void) c; return NULL;
}
int get_boolean(cmd_rec *c, int i) { (void) c; (void) i; return 0; }
config_rec *add_config_param(const char *a, int b, void *c) {
  (void) a; (void) b; (void) c; return NULL;
}
config_rec *add_config_param_str(const char *a, int b, void *c) {
  (void) a; (void) b; (void) c; return NULL;
}
void *pcalloc(void *p, size_t n) { (void) p; return calloc(1, n); }
char *pstrdup(void *p, const char *s) { (void) p; return strdup(s); }
char *pdircat(void *p, ...) { (void) p; return strdup("/"); }
char *pstrcat(void *p, ...) { (void) p; return strdup(""); }
void pr_log_pri(int p, const char *f, ...) { (void) p; (void) f; }
void pr_log_debug(int p, const char *f, ...) { (void) p; (void) f; }
void pr_trace_msg(const char *c, int p, const char *f, ...) {
  (void) c; (void) p; (void) f;
}
int pr_fsio_unlink(const char *p) { (void) p; return 0; }
int pr_fsio_fstat(pr_fh_t *fh, struct stat *st) { (void) fh; (void) st; return 0; }
void pr_fs_clear_cache(void) { }
char *pr_fs_getcwd(void) { return strdup(""); }
void pr_session_end(int s) { (void) s; }
const char *pr_session_get_protocol(int s) { (void) s; return "FTP"; }
void pr_event_generate(const char *n, ...) { (void) n; }
int pr_event_register(module *m, const char *n, void *cb, void *ud) {
  (void) m; (void) n; (void) cb; (void) ud; return 0;
}
int pr_response_add_err(int c, const char *f, ...) { (void) c; (void) f; return 0; }
pr_fs_t *pr_register_fs(void *p, const char *a, const char *b) {
  (void) p; (void) a; (void) b;
  static pr_fs_t fs;
  return &fs;
}

/* Pull in the real module source so the static reader/parser live in this
 * translation unit and can be called directly. */
#include "mod_clamav.c"

static int failures = 0;

static void check(const char *name, int cond) {
  if (!cond) {
    fprintf(stderr, "FAIL: %s\n", name);
    failures++;
  } else {
    fprintf(stderr, "ok:   %s\n", name);
  }
}

/* Feed `len` bytes of `data` to the reader. Returns the malloc'ed line (or
 * NULL) and reports the reader's errno via *out_errno. */
static char *feed(const unsigned char *data, size_t len, int *complete,
    int *out_errno) {
  FILE *f = fmemopen((void *) data, (size_t) len, "r");
  char *r;
  int e;

  if (f == NULL) {
    return NULL;
  }
  r = clamav_read_response_line(f, complete);
  e = errno;
  fclose(f);
  if (out_errno != NULL) {
    *out_errno = e;
  }
  return r;
}

int main(void) {
  int complete = FALSE, e = 0;
  char *r;

  /* Clean response. */
  r = feed((const unsigned char *) "stream: OK\n", 11, &complete, &e);
  check("clean: line returned", r != NULL);
  check("clean: complete", complete == TRUE);
  check("clean: exact text", r != NULL && strcmp(r, "stream: OK\n") == 0);
  check("clean: is_ok", r != NULL && clamav_response_is_ok(r) == TRUE);
  free(r);

  /* Infected response. */
  r = feed((const unsigned char *) "stream: Eicar-Signature FOUND\n", 30,
      &complete, &e);
  check("infected: line returned", r != NULL);
  check("infected: complete", complete == TRUE);
  check("infected: not ok", r != NULL && clamav_response_is_ok(r) == FALSE);
  check("infected: FOUND marker", r != NULL && strstr(r, "FOUND\n") != NULL);
  free(r);

  /* Truncated response (no terminating newline). */
  r = feed((const unsigned char *) "stream: Eicar FOUND", 19, &complete, &e);
  check("truncated: line returned", r != NULL);
  check("truncated: incomplete", complete == FALSE);
  check("truncated: not ok", r != NULL && clamav_response_is_ok(r) == FALSE);
  free(r);

  /* Unrecognized complete response. */
  r = feed((const unsigned char *) "stream: WEIRD\n", 14, &complete, &e);
  check("unknown: line returned", r != NULL);
  check("unknown: complete", complete == TRUE);
  check("unknown: not ok", r != NULL && clamav_response_is_ok(r) == FALSE);
  free(r);

  /* Leading NUL byte. */
  {
    static const unsigned char lead_nul[] = { '\0', '\n' };
    r = feed(lead_nul, sizeof(lead_nul), &complete, &e);
    check("leading NUL: rejected", r == NULL);
    check("leading NUL: EPROTO", e == EPROTO);
    free(r);
  }

  /* Regression: NUL after a nonempty prefix, with a following "OK" line.
   * Must be rejected, not spliced into "stream: OK\n". */
  {
    static const unsigned char embedded_nul[] = {
      's', 't', 'r', 'e', 'a', 'm', ':', ' ',
      '\0',
      'E', 'i', 'c', 'a', 'r', ' ', 'F', 'O', 'U', 'N', 'D', '\n',
      'O', 'K', '\n'
    };
    r = feed(embedded_nul, sizeof(embedded_nul), &complete, &e);
    check("embedded NUL: rejected", r == NULL);
    check("embedded NUL: EPROTO", e == EPROTO);
    check("embedded NUL: not accepted as clean",
        r == NULL || clamav_response_is_ok(r) == FALSE);
    free(r);
  }

  /* Long response (path longer than the initial 4096 buffer). */
  {
    size_t pathlen = 5000;
    char *longline = malloc(pathlen + 8);
    size_t i;
    if (longline == NULL) {
      return 1;
    }
    for (i = 0; i < pathlen; i++) {
      longline[i] = 'a';
    }
    memcpy(longline + pathlen, ": OK\n", 5);
    r = feed((const unsigned char *) longline, pathlen + 5, &complete, &e);
    check("long: line returned", r != NULL);
    check("long: complete", complete == TRUE);
    check("long: full length", r != NULL && strlen(r) == pathlen + 5);
    check("long: is_ok", r != NULL && clamav_response_is_ok(r) == TRUE);
    free(r);
    free(longline);
  }

  if (failures == 0) {
    fprintf(stderr, "\nAll reader tests passed.\n");
    return 0;
  }
  fprintf(stderr, "\n%d reader test(s) FAILED.\n", failures);
  return 1;
}
