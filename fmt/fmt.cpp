#include <iostream>
#include <regex>
#include <stdarg.h>
#include <fmt/format.h>
#include <fmt/core.h>
#include <fmt/args.h>
#include <gmp.h>
#include <gmpxx.h>

#include "fmt.hpp"
extern "C" {
#include "fmt.h"
}

extern "C" {

void fmt_print_nargs(const char *format, int nargs, ...) {
    va_list args;
    va_start(args, nargs);

    fmt::dynamic_format_arg_store<fmt::format_context> varargs;
    for (int n=0; n<nargs; n++) {
        custom_type arg{.ptr=va_arg(args, void*)};
        varargs.push_back(fmt::arg(std::to_string(n).c_str(), arg));
    }
    va_end(args);

    fmt::vprint(format, varargs);
}

} // extern "C"
