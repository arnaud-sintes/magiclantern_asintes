#include <dryos.h>
#include <property.h>
#include <util.h>

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


char * format_float( const double _value, const int _digits, char * _buffer, const size_t _buffer_len )
{
    ASSERT( _digits > 1 && _digits < 9 );
    const int left = ( int ) _value;
    int factor = 1;
    for( int i = 0; i < _digits; i++ ) {
        factor *= 10;
    }
    // TODO better use round( _value * factor ) in the future, but round() is not implemented
    const int right = ABS( ( int )( _value * factor ) ) - ABS( left * factor );
    snprintf( _buffer, _buffer_len - 1, "%d.%d", left, right );
    return _buffer;
}