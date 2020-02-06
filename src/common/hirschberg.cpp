/*
 * hirschberg.c implements Hirschberg's algorithm for global string
 * alignment in C.  To test it, compile it with
 * `c99 -o hirschberg hirschberg.c` and then run
 * `./hirschberg <string1> <string2>`.  (hirschberg.c uses
 * variable-length arrays, so the 99 standard is necessary.)
 *
 * Copyright (c) 2015 Lari Rasku.  This code is released to the public
 * domain, or under CC0 if not applicable.
 */

#include <hirschberg.h>

using namespace stringmetric;

int stringmetric::levenshtein(char a, char b) { return a != b; }

/*
 * hirschberg calculates the global alignment of a and b with the
 * cost scheme f using Hirschberg's algorithm.  It returns the string
 * of the edit operations required to change a into b:
 *
 * 	+	insertion
 *	-	deletion
 *	=	match
 *	!	substitution
 *
 * The string should be free(3)'d when no longer needed.  On failure,
 * hirschberg returns NULL and sets errno to ENOMEM.  errno may be set
 * to ENOMEM even on a successful return if the area allocated for the
 * alignment string could not be shrunk.
 */
char* stringmetric::hirschberg(const char* a, const char* b, costfunc f) {
  char *c, *d;
  size_t m = strlen(a);
  size_t n = strlen(b);

  /*
   * The alignment string of a and b is at most as long as the
   * concatenation of the two (delete all of a + insert all of b).
   * This can overshoot the actual length of the alignment string
   * by quite a bit in many cases, so we try to shrink it with
   * tryrealloc at the end of the function.
   */
  c = (char*)malloc(m + n + 1);
  if (c == NULL) return NULL;
  if (m > n) {
    /*
     * hirschberg_recursive assumes that the first string
     * passed to it is the shorter one, so if a is not, we
     * flip it and b.  The resulting alignment is otherwise
     * equivalent to the non-flipped one, with the exception
     * that insertion and deletion operators need to be
     * flipped.
     */
    d = hirschberg_recursive(c, b, n, a, m, f);
    c = (char*)tryrealloc(c, d - c + 1);
    for (d = c; *d != '\0'; d++) switch (*d) {
      case '+':
        *d = '-';
        break;
      case '-':
        *d = '+';
        break;
      }
    return c;
  }
  d = hirschberg_recursive(c, a, m, b, n, f);
  return (char*)tryrealloc(c, d - c + 1);
}

/*
 * hirschberg_recursive is the recursive part of Hirschberg's algorithm.
 * The arguments are the same as hirschberg, with the exception that the
 * length m of a and the length n of b are now explicitly passed, and c
 * is a pointer to the buffer where the alignment string is to be
 * written.  hirschberg_recursive returns a pointer to the null byte
 * written after the last alignment character.
 */
static char* stringmetric::hirschberg_recursive(char* c, const char* a, size_t m, const char* b, size_t n,
                                  costfunc f) {
  if (n > 1) {
    size_t i, mmid, nmid;
    int lcost[m + 1], rcost[m + 1], tcost[m + 1];

    nmid = n / 2;
    nwlcost(lcost, a, m, b, nmid, f);
    nwrcost(rcost, a, m, b + nmid, n - nmid, f);

    mmid = 0;
    for (i = 0; i <= m; i++) {
      tcost[i] = lcost[i] + rcost[i];
      if (tcost[i] < tcost[mmid]) mmid = i;
    }

    c = hirschberg_recursive(c, a, mmid, b, nmid, f);
    c = hirschberg_recursive(c, a + mmid, m - mmid, b + nmid, n - nmid, f);
    return c;
  } else
    return nwalign(c, a, m, b, n, f);
}

/*
 * nwalign computes the Needleman-Wunsch alignment of a and b using the
 * cost scheme f and writes it into the buffer c.  It returns a pointer
 * to the null byte written after the last character in the alignment
 * string.
 *
 * This function uses O(mn) space.  hirschberg_recursive guarantees its
 * own O(m) space usage by only calling this when n <= 1.
 */
static char* stringmetric::nwalign(char* c, const char* a, size_t m, const char* b, size_t n, costfunc f) {
  char* d;
  size_t i, j;
  int s[m + 1][n + 1];

  s[0][0] = 0;
  for (i = 1; i <= m; i++) s[i][0] = s[i - 1][0] + f(a[i - 1], 0);
  for (j = 1; j <= n; j++) s[0][j] = s[0][j - 1] + f(0, b[j - 1]);
  for (j = 1; j <= n; j++)
    for (i = 1; i <= m; i++) {
      int cost[3] = {s[i - 1][j], s[i - 1][j - 1], s[i][j - 1]};
      s[i][j]     = cost[nwmin(cost, a[i - 1], b[j - 1], f)];
    }
  i = m;
  j = n;
  d = c;
  while (i > 0 && j > 0) {
    int cost[3] = {s[i - 1][j], s[i - 1][j - 1], s[i][j - 1]};
    switch (nwmin(cost, a[i - 1], b[j - 1], f)) {
    case Del:
      *d++ = '-';
      i--;
      break;
    case Sub:
      *d++ = a[i - 1] == b[j - 1] ? '=' : '!';
      i--;
      j--;
      break;
    case Ins:
      *d++ = '+';
      j--;
      break;
    }
  }
  for (; i > 0; i--) *d++ = '-';
  for (; j > 0; j--) *d++ = '+';
  *d = '\0';
  memrev(c, d - c);
  return d;
}

/*
 * nwlcost stores the last column of the Needleman-Wunsch alignment
 * cost matrix of a and b into s.
 */
static void stringmetric::nwlcost(int* s, const char* a, size_t m, const char* b, size_t n, costfunc f) {
  size_t i, j;
  int ss, tmp;

  s[0] = 0;
  for (i = 1; i <= m; i++) s[i] = s[i - 1] + f(a[i - 1], 0);
  for (j = 1; j <= n; j++) {
    ss = s[0];
    s[0] += f(0, b[j - 1]);
    for (i = 1; i <= m; i++) {
      int cost[3] = {s[i - 1], ss, s[i]};
      tmp         = cost[nwmin(cost, a[i - 1], b[j - 1], f)];
      ss          = s[i];
      s[i]        = tmp;
    }
  }
}

/*
 * nwrcost computes the reverse Needleman-Wunsch alignment of a and b,
 * that is, matching their suffixes rather than prefixes.  The last
 * column of this alignment cost matrix is stored into s.
 */
static void stringmetric::nwrcost(int* s, const char* a, size_t m, const char* b, size_t n, costfunc f) {
  ssize_t i, j;
  int ss, tmp;

  s[m] = 0;
  for (i = m - 1; i >= 0; i--) s[i] = s[i + 1] + f(a[i], 0);
  for (j = n - 1; j >= 0; j--) {
    ss = s[m];
    s[m] += f(0, b[j]);
    for (i = m - 1; i >= 0; i--) {
      int cost[3] = {s[i + 1], ss, s[i]};
      tmp         = cost[nwmin(cost, a[i], b[j], f)];
      ss          = s[i];
      s[i]        = tmp;
    }
  }
}

/*
 * nwmin returns the cheapest edit operation out of three possibilities
 * when the "current" characters are a and b, the cost scheme is f,
 * and the base costs of the Del, Sub, and Ins operations are recorded
 * in the cost array in that order.  The cost array is modified by
 * adding the edit costs for a and b to the appropriate cells.
 */
static editop stringmetric::nwmin(int cost[3], char a, char b, costfunc f) {
  editop i;

  cost[Del] += f(a, 0);
  cost[Sub] += f(a, b);
  cost[Ins] += f(0, b);
  i = cost[Del] < cost[Sub] ? Del : Sub;
  i = cost[i] < cost[Ins] ? i : Ins;
  return i;
}

/*
 * memrev reverses the size bytes pointed to by ptr.
 */
static void stringmetric::memrev(void* ptr, size_t size) {
  char* p = (char*)ptr;
  size_t i;
  char tmp;

  for (i = 0; i < size / 2; i++) {
    tmp             = p[i];
    p[i]            = p[size - 1 - i];
    p[size - 1 - i] = tmp;
  }
}

/*
 * tryrealloc tries to resize the area pointed to by ptr to size.  It
 * returns the resized area on success and ptr on failure.  On failure,
 * errno is set.
 */
static void* stringmetric::tryrealloc(void* ptr, size_t size) {
  void* b;

  b = realloc(ptr, size);
  return b != NULL ? b : ptr;
}