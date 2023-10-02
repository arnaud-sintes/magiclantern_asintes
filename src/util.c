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


int abs_( const int _value )
{
    return _value< 0  ? -_value : _value;
}


int floor_( const double _value )
{
    int value = ( int ) _value;
    return _value >= 0 ? value : value - 1;
}


int ceil_( const double _value )
{
    int value = ( int ) _value;
    return _value > 0 ? value + 1 : value;
}


int round_( const double _value )
{
    return _value >= 0 ? floor_( _value + 0.5 ) : ceil_( _value - 0.5 );
}


char * format_float_ex( const double _value, const int _digits, char * _buffer, const size_t _buffer_len )
{
    ASSERT( _digits > 1 && _digits < 9 );
    const int left = ( int ) _value;
    static const int factors[ 8 ] = { 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000 };
    const int factor = factors[ _digits - 1 ];
    const int right = abs_( round_( _value * factor ) ) - abs_( left * factor );
    snprintf( _buffer, _buffer_len - 1, "%d.%d", left, right );
    return _buffer;
}


char * format_float( const double _value, const int _digits )
{
    static char buffer[ 64 ];
    return format_float_ex( _value, _digits, buffer, 63 );
}