#include <sys/types.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace stringmetric {

/*
 * A costfunc represents a cost scheme for Hirschberg's algorithm.
 * It takes two characters and returns the cost of transforming the
 * former to the latter.  Insertions and deletions are encoded as
 * transformations from and to null bytes.
 */
typedef int (*costfunc)(char, char);

/*
 * levenshtein is the common cost scheme for edit distance: matches cost
 * nothing, while mismatches, insertions and deletions cost one each.
 */
int levenshtein(char, char);

/*
 * The recursive step in the Needleman-Wunsch algorithm involves
 * choosing the cheapest operation out of three possibilities.  We
 * store the costs of the three operations into an array and use the
 * editop enum to index it.
 */

typedef enum {
  Del = 0,
  Sub = 1,
  Ins = 2,
} editop;

char* hirschberg(const char*, const char*, costfunc);
static char* hirschberg_recursive(char* c, const char* a, size_t m, const char* b, size_t n,
                                  costfunc f);
static char* nwalign(char*, const char*, size_t, const char*, size_t, costfunc);
static void nwlcost(int*, const char*, size_t, const char*, size_t, costfunc);
static void nwrcost(int*, const char*, size_t, const char*, size_t, costfunc);
static editop nwmin(int[3], char, char, costfunc);
static void memrev(void*, size_t);
static void* tryrealloc(void*, size_t);

} // namespace stringmetric