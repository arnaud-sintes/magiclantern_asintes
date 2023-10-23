#include <dryos.h>
#include <property.h>
#include <util.h>
#include "math.h"

/* helper functions for atomic in-/decrasing variables */
void util_atomic_inc(uint32_t *value)
{
    uint32_t old_int = cli();
    (*value)++;
    sei(old_int);
}

void util_atomic_dec(uint32_t *value)
{
    uint32_t old_int = cli();
    (*value)--;
    sei(old_int);
}

/* simple binary search */
/* crit returns negative if the tested value is too high, positive if too low, 0 if perfect */
int bin_search(int lo, int hi, CritFunc crit)
{
    ASSERT(crit);
    if (lo >= hi-1) return lo;
    int m = (lo+hi)/2;
    int c = crit(m);
    if (c == 0) return m;
    if (c > 0) return bin_search(m, hi, crit);
    return bin_search(lo, m, crit);
}

char *format_float_ex(double _value, int _digits, char *_buffer, size_t _buffer_len)
{
    ASSERT(_digits > 0 && _digits < 9);
    static unsigned factors[8] = {10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000};
    static char *const signs[2] = {"", "-"};
    unsigned factor = factors[_digits - 1];
    unsigned integer_value = (unsigned)round(ABS(_value) * factor);
    unsigned left = integer_value / factor;
    unsigned right = integer_value - (left * factor);
    unsigned right_digits = (right == 0) ? 1 : (unsigned)floor(log10(right)) + 1;
    static char right_buffer[8];
    unsigned offset = _digits - right_digits;
    memset(right_buffer, '0', offset);
    snprintf(&right_buffer[offset], 8, "%d", right);
    snprintf(_buffer, _buffer_len - 1, "%s%d.%s", signs[_value < 0 ? 1 : 0], left, right_buffer);
    return _buffer;
}