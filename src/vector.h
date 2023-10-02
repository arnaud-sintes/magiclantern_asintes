#ifndef __VECTOR_H__
#define __VECTOR_H__

#include <util.h>


// vector structure:
typedef struct
{
    size_t count;
    size_t data_size;
    uint8_t * p_data;
} vector;


// create a vector
// _data_size   is the size of the data to manipule (e.g.: sizeof(type) )
// return an empty vector object ready to be used
vector vector_create( const size_t _data_size );

// destroy properly a vector
void vector_destroy( vector * _p_vector );

// reserve a vector content for a subsequent deserialization
// _count       count of slot to reserve
void vector_reserve( vector * _p_vector, const size_t _count );

// insert a data in the vector, at a given position
// _position    position of the data insertion in the vector
//              NOTE: to queue data (insertion to end), use: vector_insert( &v, v.count, ... )
// _p_data      pointer to the data to insert
void vector_insert( vector * _p_vector, const size_t _position, const void * const _p_data );

// remove data from the vector, at a given position
// _position    position of the data removal in the vector
void vector_erase( vector * _p_vector, const size_t _position );

// set a data value in the vector, at a given position
// _position    position of the data value replacement
// _p_data      pointer to the data to set
void vector_set( vector * _p_vector, const size_t _position, const void * const _p_data );

// get a data value in the vector, at a given position
// _position    position of the data value to retrieve
// return a pointer to the data, requires a pointer cast/dereferencing: *( ( int * ) vector_get( _p_vector, 0 ) )
void * vector_get( vector * _p_vector, const size_t _position );

// helper to dump the current size and content of a vector
void vector_dump( vector * _p_vector );


#endif