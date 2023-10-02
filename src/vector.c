#include "vector.h"


vector vector_create( const size_t _data_size )
{
    vector v = {
        .count = 0,
        .data_size = _data_size,
        .p_data = NULL
    };
    return v;
}


void vector_destroy( vector * _p_vector )
{
    if( _p_vector->p_data != NULL ) {
        free( _p_vector->p_data );
    }
    _p_vector->count = 0;
}


// [internal] swap the data content of a vector to a bigger or smaller vector
// _insertion   indicates if we're doing an insertion or a removal
// _position    position of the data insertion/removal in the vector
// _p_data      pointer to the data to insert (NULL if removal)
void _vector_swap( vector * _p_vector, const bool _insertion, const size_t _position, const void * const _p_data )
{
    // can insert before or at the tip of the vector:
    ASSERT( _position <= _p_vector->count );
    
    // allocate new vector:
    const size_t vector_count = _p_vector->count + ( _insertion ? 1 : -1 );
    uint8_t * p_data = malloc( vector_count * _p_vector->data_size );

    // copy left part of current vector in the new vector, if needed:
    if( _position > 0 ) {
        memcpy( p_data, _p_vector->p_data, _position * _p_vector->data_size );
    }

    // insert value in the new vector, if any:
    if( _p_data != NULL ) {
        memcpy( p_data + ( _position * _p_vector->data_size ), _p_data, _p_vector->data_size );
    }

    // copy right part of current vector in the new vector, if needed:
    const size_t right_count = _p_vector->count - _position;
    if( right_count != 0 ) {
        const size_t dst_position = _insertion ? _position + 1 : _position;
        const size_t src_position = _insertion ? _position : _position + 1;
        memcpy( p_data + ( dst_position * _p_vector->data_size ), _p_vector->p_data + ( src_position * _p_vector->data_size ), right_count * _p_vector->data_size );
    }

    // free old vector data:
    vector_destroy( _p_vector );

    // swap vector pointer and update count:
    _p_vector->p_data = p_data;
    _p_vector->count = vector_count;
}


void vector_insert( vector * _p_vector, const size_t _position, const void * const _p_data )
{
    // can insert before or at the tip of the vector:
    ASSERT( _position <= _p_vector->count );

    // generic vector swapping, inserting a cell:
    _vector_swap( _p_vector, true, _position, _p_data );
}


void vector_erase( vector * _p_vector, const size_t _position )
{
    // position must be within the vector range:
    ASSERT( _position < _p_vector->count );

    // only one value in the value: just destroy it:
    if( _p_vector->count == 1 ) {
        vector_destroy( _p_vector );
        return;
    }

    // generic vector swapping, removing a cell:
    _vector_swap( _p_vector, false, _position, NULL );
}


void vector_set( vector * _p_vector, const size_t _position, const void * const _p_data )
{
    // position must be within the vector range:
    ASSERT( _position < _p_vector->count );

    // set value in the vector:
    memcpy( _p_vector->p_data + ( _position * _p_vector->data_size ), _p_data, _p_vector->data_size );
}


void * vector_get( vector * _p_vector, const size_t _position )
{
    // position must be within the vector range:
    ASSERT( _position < _p_vector->count );

    // return pointer to value:
    return _p_vector->p_data + ( _position * _p_vector->data_size );
}


void vector_dump( vector * _p_vector )
{
    printf( "vector(%zd):", _p_vector->count );
    for( size_t i = 0; i < _p_vector->count; i++ ) {
        printf( "%d", *( ( int * ) vector_get( _p_vector, i ) ) );
        if( i < _p_vector->count - 1 ) {
            printf( "," );
        }   
    }
    printf( "\n" );
}