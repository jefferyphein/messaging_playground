#include <stdio.h>
#include <stdlib.h>
#include <gmp.h>
#include "fmt.h"

int main(int argc, char **argv) {
    mpz_t m, n;
    mpz_init(n);
    mpz_init(m);
    mpz_set_ui(n, 1234567890);
    mpz_mul(n, n, n);
    mpz_set(m, n);
    mpz_mul(n, n, n);
    mpz_mul(n, n, n);

    fmt_print("{1:mpz_t} {0:mpz_t:x}\n", n, m);

    mpz_clear(n);
    mpz_clear(m);
    return EXIT_SUCCESS;
}
