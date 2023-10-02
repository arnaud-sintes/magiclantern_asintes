#ifndef __UTIL_H__
#define __UTIL_H__



/**
 * @brief decrease variable at given address with interrupts disabled
 * @param value pointer to value to decrease
 */
void util_atomic_dec(uint32_t *value);

/**
 * @brief increase variable at given address with interrupts disabled
 * @param value pointer to value to increase
 */
void util_atomic_inc(uint32_t *value);

/* macros from: http://gareus.org/wiki/embedding_resources_in_executables */
/**
 * @brief macros to access resource data that has been added using objcopy or ld
 */
#define EXTLD(NAME) \
  extern const unsigned char _binary_ ## NAME ## _start[]; \
  extern const unsigned char _binary_ ## NAME ## _end[]
#define LDVAR(NAME) _binary_ ## NAME ## _start
#define LDLEN(NAME) ((_binary_ ## NAME ## _end) - (_binary_ ## NAME ## _start))


/* simple binary search */
/* crit returns negative if the tested value is too high, positive if too low, 0 if perfect */
typedef int (*CritFunc)(int);
int bin_search(int lo, int hi, CritFunc crit);


// return the absolute integer value of a given integer value
int abs_( const int _value );

// return the floored integer value of a given float value
int floor_( const double _value );

// return the ceiled integer value of a given float value
int ceil_( const double _value );

// return the rounded integer value of a given float value
int round_( const double _value );

// format a float value as a text (potentially rounded)
// _value       the float value to convert
// _precision   the expected number of digits after the decimal point
// _buffer      the buffer to use to dump the text
// _buffer_len  the buffer lenght
// return a string pointer
char * format_float_ex( const double _value, const int _digits, char * _buffer, const size_t _buffer_len );

// format a float value as a text, using internal buffer
char * format_float( const double _value, const int _digits );


#endif
