#include <module.h>
#include <dryos.h>
#include <menu.h>
#include <config.h>
#include <lens.h>
#include <bmp.h>
#include <powersave.h>
#include <util.h>   

#include "focus_sq.h"


// TODO user documentation


// default data structure values:
static struct fsq_data_t g_data = {
    .task_running = false,
    .screen_on = true,
    .index = 0,
    .lens_limits = {
        .first_focus_position = 0,
        .focus_position_normalizer = 0,
    },
    .display = {
        .delimiters = { { " ", ">", "{", "(", "[" }, { " ", "<", "}", ")", "]" } },
        .state_edited_timepoint_ms = 0,
        .state = fsq_inactive,
        .transition_target_ms = 0,
        .sequence_edited_timepoint_ms = 0,
        .sequence_index = 0,
        .sequence_length = 0,
        .focus_edited_timepoint_ms = 0,
        .normalized_focus_position = 0,
        .focus_distance_cm = 0,
        .duration_edited_timepoint_ms = 0,
        .target_duration_s = 0,
        .duration_s = 0,
    }
};


void fsq_log( const char * const _fmt, ... )
{
    va_list args;
    static char data[ 256 ];
    va_start( args, _fmt );
    vsnprintf( data, sizeof( data ) - 1, _fmt, args );
    va_end( args );
    printf( "%s", data );
}


void fsq_print_bmp( const char * const _fmt, ... )
{
    // unfold arguments regarding format:
    va_list args;
    static char data[ FSQ_OVERLAY_MAX_LINE_LENGTH + 1 ];
    va_start( args, _fmt );
    const int dataLen = sizeof( data );
    const int written = vsnprintf( data, dataLen - 1, _fmt, args );
    va_end( args );
    
    // complement data with spaces regarding line width:
    memset( data + written, ' ', dataLen - 1 - written );
    data[ dataLen - 1 ] = 0;
    
    // draw label with data, white font over black background:
    static uint32_t fontspec = FONT( FONT_MONO_20, COLOR_WHITE, COLOR_BLACK );
    int x = FSQ_OVERLAY_POSITION_X;
    int y = FSQ_OVERLAY_POSITION_Y;
    bmp_puts( fontspec, &x, &y, data );
}


char * fsq_print_overlay__edit_play_content( const size_t _cycle )
{
    // set up text content:
    static char content[ FSQ_CONTENT_LENGTH + 1 ];
    const char * p_edit_left = 0;
    const char * p_edit_right = 0;

    // sequence part:
    static char step[ FSQ_CONTENT_LENGTH + 1 ];
    if( g_data.display.transition_target_ms != 0 ) {
        static const char * const p_transition[ 6 ] = { "==>", " ==", "  =", "   ", ">  ", "=> " };
        snprintf( step, FSQ_CONTENT_LENGTH, "%s (% 2d)", p_transition[ _cycle % 6 ], g_data.display.sequence_index );
    }
    else {
        const size_t pos = ( g_data.display.sequence_edited_timepoint_ms != 0 ) ? _cycle % 2 : 0;
        p_edit_left = g_data.display.delimiters[ 0 ][ pos ];
        p_edit_right = g_data.display.delimiters[ 1 ][ pos ];
        snprintf( step, FSQ_CONTENT_LENGTH, "%s% 2d%s /% 2d", p_edit_left, g_data.display.sequence_index,
            p_edit_right, g_data.display.sequence_length );
    }

    // focus part:
    static char focus[ FSQ_CONTENT_LENGTH + 1 ];
    const bool position_is_different = ( g_data.display.state == fsq_edit )
        && ( g_data.display.focus_distance_cm != lens_info.focus_dist );
    size_t pos = 3;
    if( g_data.display.transition_target_ms == 0 ) {
        if( position_is_different ) {
            pos = ( _cycle % 2 ) << 1;
        }
        else {
            pos = ( g_data.display.focus_edited_timepoint_ms != 0 ) ? _cycle % 2 : 0;
        }
    }
    p_edit_left = g_data.display.delimiters[ 0 ][ pos ];
    p_edit_right = g_data.display.delimiters[ 1 ][ pos ];
    const unsigned focus_distance_cm = ( ( g_data.display.transition_target_ms != 0 ) || position_is_different )
        ? lens_info.focus_dist
        : g_data.display.focus_distance_cm;
    static char focus_distance_buffer[ FSQ_CONTENT_LENGTH + 1 ];
    if( focus_distance_cm == FSQ_INFINITE_FOCUS_DISTANCE_TRIGGER ) {
        snprintf( focus_distance_buffer, FSQ_CONTENT_LENGTH, " inf. " );
    }
    else {
        // centimeter display:
        if( focus_distance_cm < 100 ) {
            snprintf( focus_distance_buffer, FSQ_CONTENT_LENGTH, "% 4dcm", focus_distance_cm );
        }
        // or meter display:
        else {
            static char distance_buffer[ FSQ_CONTENT_LENGTH + 1 ];
            const double focus_distance_m = ( double ) focus_distance_cm / ( double ) 100;
            snprintf( focus_distance_buffer, FSQ_CONTENT_LENGTH, "%sm",
                format_float_ex( focus_distance_m, 3, distance_buffer, FSQ_CONTENT_LENGTH ) );
        }
    }
    snprintf( focus, FSQ_CONTENT_LENGTH, "%s%s%s", p_edit_left, focus_distance_buffer, p_edit_right );

    // duration part:
    static char duration[ FSQ_CONTENT_LENGTH + 1 ];
     if( ( g_data.display.transition_target_ms == 0 ) &&
        ( g_data.display.state == fsq_play ||
            ( g_data.display.state == fsq_edit && g_data.display.sequence_index == 1 ) ) ) {
        snprintf( duration, FSQ_CONTENT_LENGTH, "" );
    }
    else {
        size_t pos = 3;
        if( ( g_data.display.transition_target_ms == 0 ) ) {
            pos = ( g_data.display.duration_edited_timepoint_ms != 0 ) ? _cycle % 2 : 0;
        }
        p_edit_left = g_data.display.delimiters[ 0 ][ pos ];
        p_edit_right = g_data.display.delimiters[ 1 ][ pos ];
        double duration_s = ( g_data.display.duration_edited_timepoint_ms != 0 )
            ? g_data.display.target_duration_s
            : g_data.display.duration_s;
        // we're in a transition, display decreasing duration:
        if( g_data.display.transition_target_ms != 0 ) {
            const int current_clock_ms = get_ms_clock();
            if( current_clock_ms > g_data.display.transition_target_ms ) {
                g_data.display.transition_target_ms = 0;
            }
            else {
                duration_s = ( ( double ) ( g_data.display.transition_target_ms - current_clock_ms ) )
                    / ( double ) 1000;
            }
        }            
        static char duration_buffer[ FSQ_CONTENT_LENGTH + 1 ];
        snprintf( duration, FSQ_CONTENT_LENGTH, "%s%ss%s", p_edit_left,
            format_float_ex( duration_s, 3, duration_buffer, FSQ_CONTENT_LENGTH ), p_edit_right );
    }

    // content:
    snprintf( content, FSQ_CONTENT_LENGTH, "%s | %s | %s", step, focus, duration );
    return content;
}


char * fsq_print_overlay__content( const size_t _cycle )
{
    // edit or play:
    if( g_data.display.state == fsq_edit || g_data.display.state == fsq_play ) {
        return fsq_print_overlay__edit_play_content( _cycle );
    }

    // inactive state:
    static char content[ FSQ_CONTENT_LENGTH + 1 ];
    if( g_data.display.state == fsq_inactive ) {
        snprintf( content, FSQ_CONTENT_LENGTH, "" );
        return content;
    }

    // deal with messages:
    static const char * const p_messages[ 2 ] = { "checking lens limits", "calibrating lens" };
    snprintf( content, FSQ_CONTENT_LENGTH, "%s...", p_messages[ g_data.display.state - fsq_checking_lens_limits ] );
    return content;
}


void fsq_print_overlay( const size_t _cycle )
{
    // reset edit highlight timepoints, if needed:
    int * const p_edit_timepoints[ 4 ] = {
        &g_data.display.state_edited_timepoint_ms,
        &g_data.display.sequence_edited_timepoint_ms,
        &g_data.display.focus_edited_timepoint_ms,
        &g_data.display.duration_edited_timepoint_ms
    };
    const int current_clock_ms = get_ms_clock();
    for( int i = 0; i < 4; i ++ ) {
        int * const p_edit_timepoint = p_edit_timepoints[ i ];
        if( *p_edit_timepoint != 0 && ( current_clock_ms - *p_edit_timepoint ) > FSQ_HIGHLIGTH_DURATION_MS ) {
            *p_edit_timepoint = 0;
        }
    }

    // header:
    static const char * p_headers[ 4 ] = { "    ", "edit", "play", "----" };
    static const char * const p_header_progress[ 6 ] = { " .oO", ".oOo", "oOo.", "Oo. ", "o. .", ". .o" };
    p_headers[ 3 ] = p_header_progress[ _cycle % 6 ];
    const size_t header_index = ( g_data.display.state < 3 ) ? g_data.display.state : 3;

    // print:
    const size_t pos = ( g_data.display.state_edited_timepoint_ms != 0 ) ? ( _cycle % 2 ) << 2 : 4;
    const char * const p_edit_left = g_data.display.delimiters[ 0 ][ pos ];
    const char * const p_edit_right = g_data.display.delimiters[ 1 ][ pos ];
    fsq_print_bmp( "%s%s%s %s", p_edit_left, p_headers[ header_index ], p_edit_right,
        fsq_print_overlay__content( _cycle ) );
}


void fsq_overlay_task()
{
    // (de)activate task:
    g_data.task_running = !g_data.task_running;
    
    // loop indefinitely:
    size_t cycle = 0;
    while( g_data.task_running ) {
        
        // print overlay (only if not in menus):
        if( !gui_menu_shown() ) {
            fsq_print_overlay( cycle++ );
        }
        
        // breathe a little to let other tasks do their job properly:
        msleep( 100 );
    }
}


void fsq_save_data_store()
{
    FIO_RemoveFile( FSQ_FOCUS_SQ_SETTINGS_FILE );
    FILE * p_file = FIO_CreateFile( FSQ_FOCUS_SQ_SETTINGS_FILE );
    FIO_WriteFile( p_file, g_data.store.lens_name, sizeof( char ) * FSQ_LENS_NAME_MAX_LENGTH );
    FIO_WriteFile( p_file, &g_data.store.focus_positions.count, sizeof( size_t ) );
    FIO_WriteFile( p_file, g_data.store.focus_positions.p_data,
        sizeof( unsigned ) * g_data.store.focus_positions.count );
    FIO_WriteFile( p_file, g_data.store.modes, sizeof( struct fsq_step_mode_t ) * 3 );
    FIO_WriteFile( p_file, &g_data.store.focus_points.count, sizeof( size_t ) );
    FIO_WriteFile( p_file, g_data.store.focus_points.p_data,
        sizeof( struct fsq_focus_point_t ) * g_data.store.focus_points.count );
    FIO_CloseFile( p_file );
}


void fsq_load_data_store()
{
    FILE * p_file = FIO_OpenFile( FSQ_FOCUS_SQ_SETTINGS_FILE, O_RDONLY );
    if( p_file == 0 ) {
        return;
    }
    FIO_ReadFile( p_file, g_data.store.lens_name, sizeof( char ) * FSQ_LENS_NAME_MAX_LENGTH );
    FIO_ReadFile( p_file, &g_data.store.focus_positions.count, sizeof( size_t ) );
    vector_reserve( &g_data.store.focus_positions, g_data.store.focus_positions.count );
    FIO_ReadFile( p_file, g_data.store.focus_positions.p_data,
        sizeof( unsigned ) * g_data.store.focus_positions.count );
    FIO_ReadFile( p_file, g_data.store.modes, sizeof( struct fsq_step_mode_t ) * 3 );
    FIO_ReadFile( p_file, &g_data.store.focus_points.count, sizeof( size_t ) );
    vector_reserve( &g_data.store.focus_points, g_data.store.focus_points.count );
    FIO_ReadFile( p_file, g_data.store.focus_points.p_data,
        sizeof( struct fsq_focus_point_t ) * g_data.store.focus_points.count );
    FIO_CloseFile( p_file );
}


void fsq_set_focus_position_normalizer()
{
    // normalization in progress...
    g_data.display.state = fsq_checking_lens_limits;
    
    // go to the very last position asap:
    while( lens_focus_ex( FSQ_ASAP_STEP_NUMBER, 3, true, true, 0 ) ) {}
            
    // store last focus position:
    const int last_focus_position = lens_info.focus_pos;
    
    // go to the very first position asap:
    while( lens_focus_ex( FSQ_ASAP_STEP_NUMBER, 3, false, true, 0 ) ) {}
    
    // set first focus position:
    g_data.lens_limits.first_focus_position = lens_info.focus_pos;
    
    // dump first & last focus positions:
    fsq_log( "{f} first focus pos.: %d\n", g_data.lens_limits.first_focus_position );
    fsq_log( "{f} last  focus pos.: %d\n", last_focus_position );
    
    // compute position normalizer regarding distance sign:
    const int focus_position_distance = last_focus_position - g_data.lens_limits.first_focus_position;
    g_data.lens_limits.focus_position_normalizer = ( focus_position_distance < 0 ) ? -1 : 1;
    fsq_log( "{f} normalizer sign : %s\n", ( g_data.lens_limits.focus_position_normalizer > 0 )
        ? "positive"
        : "negative" );
    
    // normalization finished.
    g_data.display.state = fsq_edit;
}


unsigned fsq_normalized_focus_position()
{
    // keep only positive values starting at zero:
    return ( lens_info.focus_pos - g_data.lens_limits.first_focus_position )
        * g_data.lens_limits.focus_position_normalizer;
}


size_t fsq_closest_step_from_position( const unsigned _focus_position )
{
    // search for an exact or greater match:
    for( size_t step = 0; step < g_data.store.focus_positions.count; step++ ) {
        const unsigned focus_position = *( ( unsigned * ) vector_get( &g_data.store.focus_positions, step ) );

        // lesser than, continue:
        if( focus_position < _focus_position ) {
            continue;
        }

        // perfect match:
        if( focus_position == _focus_position ) {
            return step;
        }

        // greater match, return the closest step before:
        return ( step == 0 ) ? 0 : step - 1;
    }
    
    // reached limit, return last possible step:
    return g_data.store.focus_positions.count - 1;
}


void fsq_calibrate_lens()
{
    // calibration in progress...
    g_data.display.state = ( size_t ) fsq_calibrate_lens;

    // avoid direct usage of the vector here:
    unsigned * focus_positions = malloc( FSQ_MAX_FOCUS_POSITIONS * sizeof( unsigned ) );
    
    // for each atomically reachable lens step:
    size_t step_index = 0;
    do {
        
        // store the normalized focus position and loop to next step:
        ASSERT( step_index < FSQ_MAX_FOCUS_POSITIONS );
        focus_positions[ step_index++ ] = fsq_normalized_focus_position();
    }
    // move forward:
    while( lens_focus_ex( 1, 1, true, true, 0 ) );

    // transfer positions in the vector:
    vector_reserve( &g_data.store.focus_positions, step_index );
    memcpy( g_data.store.focus_positions.p_data, focus_positions, step_index * sizeof( unsigned ) );
    free( focus_positions );
    
    // dump focus position steps:
    fsq_log( "{f} focus position steps: %d\n", g_data.store.focus_positions.count );
    
    // dump information:
    fsq_dump_focus_positions();
    
    // remember lens name:
    memcpy( g_data.store.lens_name, lens_info.name, FSQ_LENS_NAME_MAX_LENGTH );
    
    // evaluate how much steps does a step size of 3 and 2 (1 is obsviously 1):
    // note: we're going backward
    fsq_evaluate_step_size( fsq_mode_3 );
    fsq_evaluate_step_size( fsq_mode_2 );
    g_data.store.modes[ fsq_mode_1 ].steps = 1;
    
    // evaluate step size speeds:
    // note: we're finishing the backward move at step size 1, then we're moving forward at step size 2,
    // then we're moving backward to home at step size 3
    fsq_evaluate_step_size_speed( false, fsq_mode_1 );
    fsq_evaluate_step_size_speed( true, fsq_mode_2 );
    fsq_evaluate_step_size_speed( false, fsq_mode_3 );
    
    // save calibration result in data store:
    fsq_save_data_store();
    
    // calibrated.
    g_data.display.state = fsq_edit;
}


void fsq_dump_focus_position_range( const size_t _step_0, const size_t _step_1 )
{
    fsq_log( "{f} focus positions [%d,%d]: ", _step_0, _step_1 );
    for( size_t step = _step_0; step <= _step_1; step++ ) {
        fsq_log( "%d", *( ( unsigned * ) vector_get( &g_data.store.focus_positions, step ) ) );
        if( step != _step_1 ) {
            fsq_log( "," );
        }
    }
    fsq_log( "\n" );
}


void fsq_dump_focus_positions()
{    
    // dump positions range:
    const size_t step_range = 3;
    
    // dump first focus positions:
    const size_t step_0 = 0;
    const size_t step_1 = step_range - 1;
    fsq_dump_focus_position_range( step_0, step_1 );
    
    // dump last focus positions:
    const size_t step_3 = g_data.store.focus_positions.count - 1;
    const size_t step_2 = g_data.store.focus_positions.count - step_range;
    fsq_dump_focus_position_range( step_2, step_3 );
}


void fsq_evaluate_step_size( const size_t _mode )
{
    // get current focus position & compute closest step:
    const size_t step_0 = fsq_closest_step_from_position( fsq_normalized_focus_position() );
    
    // move lens, 8 iterations to average the output values:
    const size_t loop = 8;
    const unsigned step_size = ( _mode == fsq_mode_3 ) ? 3 : 2;
    ASSERT( lens_focus_ex( ( unsigned ) loop, step_size, false, true, 0 ) );
    
    // get new focus position & compute closest step:
    const size_t step_1 = fsq_closest_step_from_position( fsq_normalized_focus_position() );
    
    // compute corresponding step movement:
    const size_t step_size_steps = ( step_0 - step_1 ) / loop;
    fsq_log( "{f} steps for step size of %d: %d\n", step_size, step_size_steps );
    g_data.store.modes[ _mode ].steps = step_size_steps;
}


void fsq_evaluate_step_size_speed( const bool _forward, const size_t _mode )
{
    // get the current focus lens step from position:
    const size_t current_step = fsq_closest_step_from_position( fsq_normalized_focus_position() );
    
    // we want to cover the whole remaining steps to reach begin or end, depending if moving forward or backward:
    // note: because the current step is the closest approximation, we need to take 1 step of margin when going forward
    const size_t step_count = _forward
        ? ( g_data.store.focus_positions.count - current_step - 1 - 1 )
        : current_step;
    
    // evaluate the number of loop we can do depending of the current step size steps:
    const size_t step_size_steps = g_data.store.modes[ _mode ].steps;
    const size_t loop_count = step_count / step_size_steps;
    
    // get the actual step count that will be reached regarding the step size:
    const size_t real_step_count = loop_count * step_size_steps;
    
    // get timepoint before the move:
    const int t_before_ms = get_ms_clock();
    
    // do the move, without waiting for feedback:
    unsigned step_size = 3;
    if( _mode != fsq_mode_3 ) {
        step_size = ( _mode == fsq_mode_2 ) ? 2 : 1;
    }
    lens_focus_ex( ( unsigned ) loop_count, ( unsigned ) step_size, _forward, false, 0 );
    
    // evaluate the current step size speed (in steps per second):
    const double step_size_speed = ( double ) ( real_step_count * 1000 ) / ( double ) ( get_ms_clock() - t_before_ms );
    
    // stabilize focus position before continuing:
    wait_for_stabilized_focus_position();
    
    // dump step size speed:
    static char speed_buffer[ FSQ_CONTENT_LENGTH + 1 ];
    fsq_log( "{f} step size %d: %s steps per second\n", step_size, format_float_ex( step_size_speed, 3, speed_buffer,
        FSQ_CONTENT_LENGTH ) );
    g_data.store.modes[ _mode ].speed = step_size_speed;
    
    // how many steps are remaining to reach the initial destination?
    const size_t step_count_remaining = step_count - real_step_count;
    
    // something remaining, do the move with a step size of 1 (without waiting for feedback):
    if( step_count_remaining != 0 ) {
        lens_focus_ex( ( unsigned ) step_count_remaining, 1, _forward, false, 0 );
    }
}


struct fsq_distribution_t fsq_compute_distribution_between( const unsigned _focus_position_1,
    const unsigned _focus_position_2, const double _duration_s, int * _p_range )
{
    const int step_1 = ( int ) fsq_closest_step_from_position( _focus_position_1 );
    const int step_2 = ( int ) fsq_closest_step_from_position( _focus_position_2 );
    const int range = step_2 - step_1;
    if( _p_range != NULL ) {
        *_p_range = range;
    }
    return fsq_distribute_modes( ABS( range ), _duration_s );
}


double fsq_distribution_duration( const struct fsq_distribution_t * const  _p_distribution )
{
    double duration_s = 0;
    for( int i = 0; i < 3; i++ ) {
        if( _p_distribution->mode_call_counts[ i ] != 0 ) {
            duration_s += ( _p_distribution->mode_call_counts[ i ] * g_data.store.modes[ i ].steps )
                / g_data.store.modes[ i ].speed;
        }
    }
    return duration_s;
}


struct fsq_distribution_t fsq_distribute_modes( const size_t _step_range, const double _target_duration_s )
{
    struct fsq_distribution_t distribution = { { 0, 0, 0 }, .wait = 0 };

    // compute possible durations depending of modes:
    double durations[ 3 ];
    for( int i = 0; i < 3; i++ ) {
        durations[ i ] = _step_range / g_data.store.modes[ i ].speed;
    }

    // quickest effort:
    if( durations[ 0 ] > _target_duration_s ) {
        // compute mode calls to reach the exact target point:
        size_t range = 0;
        for( int i = 0; i < 3 && range < _step_range; i++ ) {
            distribution.mode_call_counts[ i ] = ( _step_range - range ) / g_data.store.modes[ i ].steps;
            range += distribution.mode_call_counts[ i ] * g_data.store.modes[ i ].steps;
        }
    }
    // slowest:
    else
    if( durations[ 2 ] < _target_duration_s ) {
        distribution.mode_call_counts[ 2 ] = _step_range;
    }
    // regular distribution:
    else {
        // search in-between position:
        int offset = 0;
        while( offset < 2 && _target_duration_s > durations[ offset + 1 ] ) {
            offset++;
        }

        // compute in-between duration ratio to reduce the dichotomic search range as much as possible:
        const double ratio = 1 - ( ( _target_duration_s - durations[ offset ] ) / durations[ offset + 1 ] );

        // dichotomic best distribution search:
        // (perform 80 loops between +/-20% of the initial ratio)
        double best_distribution_duration = 100000;
        for( double delta = -0.2; delta < ( double ) 0.2; delta += ( double ) 0.005 ) {

            // compute distribution with current ratio and delta:
            size_t count = ( size_t )( ( ratio + delta ) * _step_range ) / g_data.store.modes[ offset ].steps;

            // the first speed is the reference:
            size_t range = _step_range;
            int _offset = offset;
            struct fsq_distribution_t current_distribution = { { 0, 0, 0 }, .wait = 0 };
            int mode_call_counts_index = offset;
            current_distribution.mode_call_counts[ mode_call_counts_index++ ] = count;

            // iterate over slower speeds until reaching the destination:
            while( _offset < 3 && count != 0 ) {
                range -= count * g_data.store.modes[ _offset ].steps;
                _offset++;
                count = range / g_data.store.modes[ _offset ].steps;
                if( count != 0 ) {
                    current_distribution.mode_call_counts[ mode_call_counts_index++ ] = count;
                }
            }

            // is it the best match regarding the duration? (the smallest the closest to the target)
            const double current_distribution_duration = ABS( _target_duration_s -
                fsq_distribution_duration( &current_distribution ) );
            if( current_distribution_duration < best_distribution_duration ) {
                best_distribution_duration = current_distribution_duration;
                distribution = current_distribution;
            }
        }
    }
    
    // wait time:
    distribution.duration_s = fsq_distribution_duration( &distribution );
    distribution.wait = _target_duration_s - distribution.duration_s;
    if( distribution.wait > 0 ) {
        distribution.duration_s += distribution.wait;
    }

    // return computed distribution:
    return distribution;
}


void fsq_play_distribution( const struct fsq_distribution_t * const _p_distribution, const bool _forward )
{
    // what's the greatest count value in the distribution?
    size_t max_count = 0;
    for( int i = 0; i < 3; i++ ) {
        if( _p_distribution->mode_call_counts[ i ] > max_count ) {
            max_count = _p_distribution->mode_call_counts[ i ];
        }
    }

    // prepare jobs:
    // NOTE: 1-3 jobs are for lens_focus_ex calls with step_size 1, 2, 3
    // 4th job is for msleep( sleep_duration_ms ) calls
    struct fsq_job_t jobs[ 4 ] = { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } };
    for( int i = 0; i < 3; i++ ) {
        if( _p_distribution->mode_call_counts[ i ] != 0 ) {
            jobs[ i ].remaining_call_counts = _p_distribution->mode_call_counts[ i ];
            jobs[ i ].count_cycle = max_count / _p_distribution->mode_call_counts[ i ];
        }
    }

    // if sleep is positive, distribute the sleep statements:
    size_t sleep_duration_ms = 5;
    if( _p_distribution->wait > 0 ) {
        jobs[ 3 ].remaining_call_counts = max_count;
        // while the sleep count is greater than maximum count, increase the sleep time:
        while( jobs[ 3 ].remaining_call_counts >= max_count ) {
            sleep_duration_ms *= 2; // try 10ms, 20ms, 40ms, 80ms...
            jobs[ 3 ].remaining_call_counts = ( size_t )( _p_distribution->wait * 1000 / sleep_duration_ms );
        }
        jobs[ 3 ].count_cycle = max_count / jobs[ 3 ].remaining_call_counts;
    }

    // do smoothly distributed job calls until reaching destination:
    bool reached = false;
    size_t cycle = 0;
    while( !reached ) {
        reached = true;

        // for each job:
        for( int i = 0; i < 4; i++ ) {

            // nothing left to do here:
            if( jobs[ i ].remaining_call_counts == 0 ) {
                continue;
            }

            // something left, don't escape yet:
            reached = false;

            // not yet the right cycle to get a smooth value distribution:
            if( cycle % jobs[ i ].count_cycle != jobs[ i ].count_cycle - 1 ) {
                continue;
            }

            // do the job itself:
            if( i < 3 ) {
                // convert job index to step_size
                lens_focus_ex( 1, ( 2 - i ) + 1, _forward, false, 0 );
            }
            else {
                msleep( ( int ) sleep_duration_ms );
            }
            jobs[ i ].remaining_call_counts--;
        }

        // cycle increment:
        cycle++;
    }
}


void fsq_go_to()
{
    // read focus point data:
    const struct fsq_focus_point_t * const p_focus_point = vector_get( &g_data.store.focus_points, g_data.index );    

    // transition:
    g_data.display.transition_target_ms = get_ms_clock() + ( p_focus_point->distribution.duration_s * 1000 );

    // setup displays:
    g_data.display.sequence_index = g_data.index + 1;
    g_data.display.normalized_focus_position = p_focus_point->normalized_position;
    g_data.display.focus_distance_cm = p_focus_point->distance_cm;
    g_data.display.target_duration_s = p_focus_point->distribution.duration_s;
    g_data.display.duration_s = p_focus_point->distribution.duration_s;
    
    // get timepoint before the move:
    const int t_before_ms = get_ms_clock();

    // play distribution:
    fsq_play_distribution( &p_focus_point->distribution,
        p_focus_point->normalized_position > fsq_normalized_focus_position() );

    // real duration of the distribution:
    const double real_duration_s = ( double ) ( get_ms_clock() - t_before_ms ) / ( double ) 1000.0;

    // wait for current stabilized focus position:
    wait_for_stabilized_focus_position();

    // get target and real focus step values:
    const int target_step = ( int ) fsq_closest_step_from_position( p_focus_point->normalized_position );
    const unsigned real_focus_position = fsq_normalized_focus_position();
    const int real_step = ( int ) fsq_closest_step_from_position( real_focus_position );
    unsigned corrected_real_focus_position = real_focus_position;

    // correct position only if needed:
    if( real_step != target_step ) {

        // finish the move to reach exactly the focus point:
        const int range = target_step - real_step;
        lens_focus_ex( ABS( range ), 1, range > 0, false, 0 );

        // wait for current stabilized focus position:
        wait_for_stabilized_focus_position();

        // get corrected real focus position:
        corrected_real_focus_position = fsq_normalized_focus_position();
    }

    // final position deviation:
    const int deviation_after = ( int ) p_focus_point->normalized_position - ( int ) corrected_real_focus_position;

    // dump deviation without correction:
    static char deviation_buffer[ FSQ_CONTENT_LENGTH + 1 ];
    const char * const p_duration_deviation = format_float_ex( real_duration_s - p_focus_point->distribution.duration_s,
        3, deviation_buffer, FSQ_CONTENT_LENGTH );
    if( real_step == target_step ) {
        fsq_log( "{f} deviations: %d, %ss\n", deviation_after, p_duration_deviation );
    }
    // dump corrected deviation:
    else {
        const int deviation_before = ( int ) p_focus_point->normalized_position - ( int ) real_focus_position;
        fsq_log( "{f} deviations: (%d) %d corrected, %ss\n", deviation_before, deviation_after, p_duration_deviation );
    }
}


void fsq_go_to_asap()
{
    // read current focus position:
    const unsigned current_focus_position = fsq_normalized_focus_position();

    // read focus point data:
    const struct fsq_focus_point_t * const p_focus_point = vector_get( &g_data.store.focus_points, g_data.index );    

    // transition:
    g_data.display.transition_target_ms = get_ms_clock() + ( p_focus_point->distribution.duration_s * 1000 );
    
    // compute asap distribution:
    int range = 0;
    const struct fsq_distribution_t distribution = fsq_compute_distribution_between( current_focus_position,
        p_focus_point->normalized_position, 0, &range );

    // setup displays:
    g_data.display.sequence_index = g_data.index + 1;
    g_data.display.sequence_length = g_data.store.focus_points.count;
    g_data.display.normalized_focus_position = p_focus_point->normalized_position;
    g_data.display.focus_distance_cm = p_focus_point->distance_cm;
    g_data.display.target_duration_s = 0;
    g_data.display.duration_s = distribution.duration_s;

    // play distribution:
    fsq_play_distribution( &distribution, range > 0 );

    // wait for current stabilized focus position:
    wait_for_stabilized_focus_position();

    // get real focus step values:
    const int target_step = ( int ) fsq_closest_step_from_position( p_focus_point->normalized_position );
    const int real_step = ( int ) fsq_closest_step_from_position( fsq_normalized_focus_position() );

    // correct position only if needed:
    if( real_step != target_step ) {

        // finish the move to reach exactly the focus point:
        const int range = target_step - real_step;
        lens_focus_ex( ABS( range ), 1, range > 0, false, 0 );

        // wait for current stabilized focus position:
        wait_for_stabilized_focus_position();
    }
    
    // transition finished:
    g_data.display.target_duration_s = p_focus_point->duration_s;
    g_data.display.duration_s = p_focus_point->distribution.duration_s;
}


void fsq_action__toggle_mode()
{
    // cycle between inactive, edit & play:
    g_data.display.state_edited_timepoint_ms = get_ms_clock();
    g_data.display.state++;
    if( g_data.display.state > fsq_play ) {
        g_data.display.state = fsq_inactive;
    }

    // there's only something to do when switching to edit:
    if( g_data.display.state != fsq_edit ) {
        return;
    }
    
    // perform focus position normalization if needed:
    if( g_data.lens_limits.focus_position_normalizer == 0 ) {
        fsq_set_focus_position_normalizer();
    }
    
    // perform lens calibration if needed:
    if( memcmp( g_data.store.lens_name, lens_info.name, FSQ_LENS_NAME_MAX_LENGTH ) != 0 ) {
        fsq_calibrate_lens();
    }
    else {
        fsq_log( "{f} lens was already calibrated.\n" );
    }

    // focus point list is not empty, go to first/last known position asap:
    if( g_data.store.focus_points.count != 0 ) {
        fsq_go_to_asap();
        return;
    }

    // empty list, add a first focus point:
    fsq_action__edit__add_focus_point();
}


void fsq_action__toggle_camera_display()
{
    // toggle screen:
    g_data.screen_on = !g_data.screen_on;

    // display on/off:
    if( g_data.screen_on ) {
        display_on();
    }
    else {
        display_off();
    }
}


void fsq_action__edit__add_focus_point()
{        
    // new focus point information:
    struct fsq_focus_point_t focus_point = {
        .normalized_position = fsq_normalized_focus_position(),
        .distance_cm = lens_info.focus_dist,
        .duration_s = 0
    };

    // compute new distribution if needed:
    if( g_data.store.focus_points.count > 0 ) {
        const struct fsq_focus_point_t * const p_previous_focus_point =
            vector_get( &g_data.store.focus_points, g_data.index );
        focus_point.distribution = fsq_compute_distribution_between( p_previous_focus_point->normalized_position,
            focus_point.normalized_position, focus_point.duration_s, NULL );

        // set the default target duration as close as possible as the best possible effort, using 100ms approximation:
        focus_point.duration_s = ( double ) ( ( int ) ( ( focus_point.distribution.duration_s +
            ( double ) FSQ_FOCUS_POINT_INCREMENT_S ) * 10 ) ) / 10;

        // compute closest distribution with new target value:
        focus_point.distribution = fsq_compute_distribution_between( p_previous_focus_point->normalized_position,
            focus_point.normalized_position, focus_point.duration_s, NULL );

        // increment index:
        g_data.index++;
    }

    // insert new focus point:
    vector_insert( &g_data.store.focus_points, g_data.index, &focus_point );

    // if needed, we have to recompute the related distribution of the point after the one being added:
    if( g_data.index < g_data.store.focus_points.count - 1 ) {
        struct fsq_focus_point_t * p_next_focus_point = vector_get( &g_data.store.focus_points, g_data.index + 1 );
        p_next_focus_point->distribution = fsq_compute_distribution_between( focus_point.normalized_position,
            p_next_focus_point->normalized_position, p_next_focus_point->duration_s, NULL );
    }
        
    // save focus points:
    fsq_save_data_store();

    // setup displays:
    g_data.display.sequence_edited_timepoint_ms = get_ms_clock();
    g_data.display.sequence_index = g_data.index + 1;
    g_data.display.sequence_length = g_data.store.focus_points.count;
    g_data.display.focus_edited_timepoint_ms = get_ms_clock();
    g_data.display.normalized_focus_position = focus_point.normalized_position;
    g_data.display.focus_distance_cm = focus_point.distance_cm;
    g_data.display.duration_edited_timepoint_ms = get_ms_clock();
    g_data.display.target_duration_s = focus_point.duration_s;
    g_data.display.duration_s = focus_point.distribution.duration_s;
}


void fsq_action__edit__set_current_lens_information()
{
    // get current focus point information:
    struct fsq_focus_point_t * p_focus_point = vector_get( &g_data.store.focus_points, g_data.index );

    // update lens information:
    p_focus_point->normalized_position = fsq_normalized_focus_position();
    p_focus_point->distance_cm = lens_info.focus_dist;

    // re-compute distribution if needed:
    if( g_data.index > 0 ) {
        const struct fsq_focus_point_t * const p_previous_focus_point =
            vector_get( &g_data.store.focus_points,g_data.index - 1 );
        p_focus_point->distribution = fsq_compute_distribution_between( p_previous_focus_point->normalized_position,
            p_focus_point->normalized_position, p_focus_point->duration_s, NULL );
    }

    // if needed, we have to recompute the related distribution of the point after the one being updated:
    if( g_data.index < g_data.store.focus_points.count - 1 ) {
        struct fsq_focus_point_t * const p_next_focus_point =
            vector_get( &g_data.store.focus_points, g_data.index + 1 );
        p_next_focus_point->distribution = fsq_compute_distribution_between( p_focus_point->normalized_position,
            p_next_focus_point->normalized_position, p_next_focus_point->duration_s, NULL );
    }
    
    // save focus points:
    fsq_save_data_store();

    // setup displays:
    g_data.display.focus_edited_timepoint_ms = get_ms_clock();
    g_data.display.normalized_focus_position = p_focus_point->normalized_position;
    g_data.display.focus_distance_cm = p_focus_point->distance_cm;
    g_data.display.duration_edited_timepoint_ms = get_ms_clock();
    g_data.display.duration_s = p_focus_point->distribution.duration_s;
}


void fsq_action__edit__remove_focus_point()
{
    // nothing to do if only one point remains:
    if( g_data.store.focus_points.count == 1 ) {
        return;
    }

    // remove current point:
    vector_erase( &g_data.store.focus_points, g_data.index );

    // if it was the latest point of the list, go to previous point:
    if( g_data.index == g_data.store.focus_points.count ) {
        g_data.index--;
    }

    // get current focus point for potential subsequent distribution recomputation:
    struct fsq_focus_point_t * const p_focus_point = vector_get( &g_data.store.focus_points, g_data.index );

    // we're not on the first point, we need to recompute the distribution:
    if( g_data.index != 0 ) {
        struct fsq_focus_point_t * const p_previous_focus_point =
            vector_get( &g_data.store.focus_points, g_data.index - 1 );
        p_focus_point->distribution = fsq_compute_distribution_between( p_previous_focus_point->normalized_position,
            p_focus_point->normalized_position, p_focus_point->duration_s, NULL );
    }

    // if needed, we have to recompute the related distribution of the point after the one just removed:
    if( g_data.index < g_data.store.focus_points.count - 1 ) {
        struct fsq_focus_point_t * const p_next_focus_point =
            vector_get( &g_data.store.focus_points, g_data.index + 1 );
        p_next_focus_point->distribution = fsq_compute_distribution_between( p_focus_point->normalized_position,
            p_next_focus_point->normalized_position, p_next_focus_point->duration_s, NULL );
    }
    
    // save focus points:
    fsq_save_data_store();

    // go to new position asap:
    fsq_go_to_asap();
}


void fsq_action__edit__go_to_next_focus_point()
{
    // increase index (or loop to beginning):
    if( g_data.index++ == g_data.store.focus_points.count - 1 ) {
        g_data.index = 0;
    }

    // go to new position asap:
    fsq_go_to_asap();
}


void fsq_action__edit__go_to_previous_focus_point()
{
    // decrease index (or loop to end):
    if( g_data.index-- == 0 ) {
        g_data.index = g_data.store.focus_points.count - 1;
    }

    // go to new position asap:
    fsq_go_to_asap();
}


void fsq_action__edit__update_transition_speed( const bool _increase )
{
    // nothing to do if we're on the first point:
    if( g_data.index == 0 ) {
        return;
    }

    // get current focus point information:
    struct fsq_focus_point_t * p_focus_point = vector_get( &g_data.store.focus_points, g_data.index );

    // check for limits:
    if( ( _increase && p_focus_point->duration_s > ( double ) 9.8 ) ||
        ( !_increase && p_focus_point->duration_s < ( double ) 0.1 ) ) {
        return;
    }

    // update transition duration:
    p_focus_point->duration_s += ( double ) FSQ_FOCUS_POINT_INCREMENT_S * ( _increase ? 1 : -1 );

    // recompute distribution:
    const struct fsq_focus_point_t * const p_previous_focus_point =
        vector_get( &g_data.store.focus_points, g_data.index - 1 );
    p_focus_point->distribution = fsq_compute_distribution_between( p_previous_focus_point->normalized_position,
        p_focus_point->normalized_position, p_focus_point->duration_s, NULL );
    
    // save focus points:
    fsq_save_data_store();

    // setup displays:
    g_data.display.duration_edited_timepoint_ms = get_ms_clock();
    g_data.display.target_duration_s = p_focus_point->duration_s;
    g_data.display.duration_s = p_focus_point->distribution.duration_s;
}


void fsq_action__edit__increase_transition_speed()
{
    fsq_action__edit__update_transition_speed( true );
}


void fsq_action__edit__decrease_transition_speed()
{
    fsq_action__edit__update_transition_speed( false );
}


void fsq_action__play__go_to_next_focus_point()
{
    // increase index (or loop):
    if( g_data.index == g_data.store.focus_points.count - 1 ) {
        return;
    }

    // increase index:
    g_data.index++;

    // go to position:
    fsq_go_to();
}


void fsq_action__play__go_fast_to_first_focus_point()
{
    // first focus point:
    g_data.index = 0;

    // go to new position asap:
    fsq_go_to_asap();
}


unsigned int fsq_key_handler( const unsigned int _key )
{
    // we're in the ML menus, bypass:
    if( gui_menu_shown() ) {
        return 1;
    }
    
    // task is not running, bypass:
    if( !g_data.task_running ) {
        return 1;
    }
    
    // [INFO] button toggle between modes:
    if( _key == MODULE_KEY_INFO ) {
        fsq_action__toggle_mode();
        return 0;
    }
    
    // no need to track keys when idle:
    if( g_data.display.state == fsq_inactive ) {
        return 1;
    }

    // (de)activate the camera display (screen saver):
    if( _key == MODULE_KEY_RATE ) {
        fsq_action__toggle_camera_display();
        return 0;
    }

    // edit & play keyboard mapping initialization:
    static void * functions[ 2 ][ FSQ_MAX_KEY_MAP + 1 ] = { { ( void * ) -1 } };
    if( functions[ 0 ][ 0 ] == ( void * ) -1 ) {
        memset( functions, 0, 2 * FSQ_MAX_KEY_MAP );

        // edit mode key mapping:
        functions[ fsq_edit - 1 ][ MODULE_KEY_Q ] = &fsq_action__edit__add_focus_point;
        functions[ fsq_edit - 1 ][ MODULE_KEY_PRESS_SET ] = &fsq_action__edit__set_current_lens_information;
        functions[ fsq_edit - 1 ][ MODULE_KEY_TRASH ] = &fsq_action__edit__remove_focus_point;
        functions[ fsq_edit - 1 ][ MODULE_KEY_PRESS_RIGHT ] = &fsq_action__edit__go_to_next_focus_point;
        functions[ fsq_edit - 1 ][ MODULE_KEY_PRESS_LEFT ] = &fsq_action__edit__go_to_previous_focus_point;
        functions[ fsq_edit - 1 ][ MODULE_KEY_PRESS_UP ] = &fsq_action__edit__increase_transition_speed;
        functions[ fsq_edit - 1 ][ MODULE_KEY_PRESS_DOWN ] = &fsq_action__edit__decrease_transition_speed;
        
        // play mode key mapping:
        functions[ fsq_play - 1 ][ MODULE_KEY_PRESS_SET ] = &fsq_action__play__go_to_next_focus_point;
        functions[ fsq_play - 1 ][ MODULE_KEY_Q ] = &fsq_action__play__go_fast_to_first_focus_point;
    }

    // get the function associated to the key:
    const void * const p_function = functions[ g_data.display.state - 1 ][ _key ];
    if( p_function != NULL ) {
        ( ( void ( * )() ) p_function )();
        return 0;
    }

    // pass-through:
    return 1;
}


// menu definition:
struct menu_entry g_menu[] = { {
    .name    = "Sequence",
    .select  = run_in_separate_task, // DryOS task, running in parallel
    .priv    = fsq_overlay_task,
    .help    = "Start focus sequencing",
} };


unsigned int fsq_init()
{
    // init lens name:
    memset( g_data.store.lens_name, 0, FSQ_LENS_NAME_MAX_LENGTH );

    // init vectors:
    g_data.store.focus_positions = vector_create( sizeof( unsigned ) );
    g_data.store.focus_points = vector_create( sizeof( struct fsq_focus_point_t ) );

    // load data:
    fsq_load_data_store();
    
    // add focus sequencing mode to Focus menu:
    menu_add( "Focus", g_menu, COUNT( g_menu ) );
    return 0;
}


unsigned int fsq_deinit()
{
    // stop task:
    g_data.task_running = false;

    return 0;
}


MODULE_INFO_START()
    MODULE_INIT( fsq_init )
    MODULE_DEINIT( fsq_deinit )
MODULE_INFO_END()

MODULE_CBRS_START()
    MODULE_CBR( CBR_KEYPRESS, fsq_key_handler, 0 )
MODULE_CBRS_END()