#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <vstr.h>
#include <gmp.h>

static int mpz_cb(Vstr_base *base, size_t pos, Vstr_fmt_spec *spec) {
    // Get the argument and convert it to a string.
    void *mpz = VSTR_FMT_CB_ARG_PTR(spec, 0);
    char *buf = mpz_get_str(NULL, 10, mpz);

    // Create a reference and add it to the base, so that it will be freed
    // once processed.
    Vstr_ref *ref = vstr_ref_make_ptr(buf, vstr_ref_cb_free_ptr_ref);
    vstr_add_ref(base, pos, ref, 0, strlen(buf));
    vstr_ref_del(ref);

    return 1;
}

int main(int argc, char **argv) {
    // Initialize vstr library.
    if (!vstr_init()) {
        err(EXIT_FAILURE, "init");
    }

    // Create a vstr base object.
    Vstr_base *base = NULL;
    base = vstr_make_base(NULL);

    // Define format control character.
    vstr_cntl_conf(base->conf, VSTR_CNTL_CONF_SET_FMT_CHAR_ESC, '$');

    // Register control format MPZ type.
    vstr_fmt_add(base->conf, "<MPZ:%p>", mpz_cb, VSTR_TYPE_FMT_PTR_VOID, VSTR_TYPE_FMT_END);

    /* Now that we've configured the base object, we no longer directly use it.
     * Instead, we create additional base streams using the configuration we
     * just defined. This allows us to define custom formats once and expose
     * their functionality to any inherited streams. */

    // Create mpz_t.
    mpz_t n;
    mpz_init(n);
    mpz_set_str(n, "-1324134432432432321424322434132432412", 10);

    // Create a new stream, inheriting from the base configuration.
    Vstr_base *stream = vstr_make_base(base->conf);

    // Print the Vstr_base object. Can keep track of how many bytes have been written.
    int total_written = 0;
    total_written += vstr_add_fmt(stream, stream->len, "Hello $<MPZ:%p> world\n", (void*)n);
    mpz_mul(n, n, n);
    total_written += vstr_add_fmt(stream, stream->len, "World $<MPZ:%p> hello\n", (void*)n);

    // Flush our new vstr_base stream to output stream.
    while (stream->len) {
        vstr_sc_write_fd(stream, 1, stream->len, fileno(stdout), NULL);
    }

    // Free memory.
    mpz_clear(n);
    vstr_free_base(stream);
    vstr_free_base(base);

    // Deinitialize vstr library.
    vstr_exit();
    return EXIT_SUCCESS;
}
