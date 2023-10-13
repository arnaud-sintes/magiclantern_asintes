#include <module.h>
#include <dryos.h>
#include <menu.h>
#include <config.h>
#include <lens.h>
#include <bmp.h>
#include <powersave.h>
#include <util.h>   

#include "focus_sq.h"

// TODO the change between EDIT and PLAY modes is not obvious -> animate?
// TODO need to recompute some distribution timing on the next sequence point (c.f. TODO in code)
// TODO step deviation issues after movement seems quite big, why?
// - may be better to not save the distribution but always recalculate it based over the real current position at the beginning of the move?
// - maybe use an average step movement stored in double ?
// TODO got an ASSERTion one time during calibration around vector_get() -> cannot reproduce, but better dig it


// default data structure values:
static Data g_data = {
    .task_running = false,
    .index = 0,
    .lens_limits = {
        .first_focus_position = 0,
        .focus_position_normalizer = 0,
    },
    .display = {
        .state = STATE__INACTIVE,
        .transition_in_progress = false,
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


void Log( const char * _fmt, ... )
{
    va_list args;
    static char data[ 256 ];
    va_start( args, _fmt );
    vsnprintf( data, sizeof( data ) - 1, _fmt, args );
    va_end( args );
    printf( "%s", data );
}


void Print_bmp( const char * _fmt, ... )
{
    // unfold arguments regarding format:
    va_list args;
    static char data[ OVERLAY_MAX_LINE_LENGTH + 1 ];
    va_start( args, _fmt );
    const int dataLen = sizeof( data );
    const int written = vsnprintf( data, dataLen - 1, _fmt, args );
    va_end( args );
    
    // complement data with spaces regarding line width:
    memset( data + written, ' ', dataLen - 1 - written );
    data[ dataLen - 1 ] = 0;
    
    // draw label with data, white font over black background:
    static uint32_t fontspec = FONT( FONT_MONO_20, COLOR_WHITE, COLOR_BLACK );
    int x = OVERLAY_POSITION_X;
    int y = OVERLAY_POSITION_Y;
    bmp_puts( fontspec, &x, &y, data );
}


char * Print_overlay__edit_play_content( const size_t _cycle )
{
    // set up text content:
    static char content[ CONTENT_LENGTH + 1 ];
    static const char * const p_edit[ 2 ][ 4 ] = { { " ", ">", "{", "(" }, { " ", "<", "}", ")" } };
    const char * p_edit_left = 0;
    const char * p_edit_right = 0;

    // reset edit highlight timepoints, if needed:
    int * const p_edit_timepoints[ 3 ] = { &g_data.display.sequence_edited_timepoint_ms, &g_data.display.focus_edited_timepoint_ms, &g_data.display.duration_edited_timepoint_ms };
    const int current_clock_ms = get_ms_clock();
    for( int i = 0; i < 3; i ++ ) {
        int * const p_edit_timepoint = p_edit_timepoints[ i ];
        if( *p_edit_timepoint != 0 && ( current_clock_ms - *p_edit_timepoint ) > HIGHLIGTH_DURATION_MS ) {
            *p_edit_timepoint = 0;
        }
    }

    // sequence part:
    static char step[ CONTENT_LENGTH + 1 ];
    if( g_data.display.transition_in_progress ) {
        static const char * const p_transition[ 6 ] = { "==>", " ==", "  =", "   ", ">  ", "=> " };
        snprintf( step, CONTENT_LENGTH, "%s (% 2d)", p_transition[ _cycle % 6 ], g_data.display.sequence_index );
    }
    else {
        const size_t pos = g_data.display.sequence_edited_timepoint_ms != 0 ? _cycle % 2 : 0;
        p_edit_left = p_edit[ 0 ][ pos ];
        p_edit_right = p_edit[ 1 ][ pos ];
        snprintf( step, CONTENT_LENGTH, "%s% 2d%s /% 2d", p_edit_left, g_data.display.sequence_index, p_edit_right, g_data.display.sequence_length );
    }

    // focus part:
    static char focus[ CONTENT_LENGTH + 1 ];
    const bool position_is_different = g_data.display.state == STATE__EDIT && Normalized_focus_position() != g_data.display.normalized_focus_position;
    const size_t pos = g_data.display.transition_in_progress ? 3 : ( position_is_different ? ( _cycle % 2 ) << 1 : ( g_data.display.focus_edited_timepoint_ms != 0 ? _cycle % 2 : 0 ) );
    p_edit_left = p_edit[ 0 ][ pos ];
    p_edit_right = p_edit[ 1 ][ pos ];
    const unsigned focus_distance_cm = g_data.display.transition_in_progress || position_is_different ? lens_info.focus_dist : g_data.display.focus_distance_cm;
    static char focus_distance_buffer[ CONTENT_LENGTH + 1 ];
    if( focus_distance_cm == INFINITE_FOCUS_DISTANCE_TRIGGER ) {
        snprintf( focus_distance_buffer, CONTENT_LENGTH, " inf. " );
    }
    else {
        snprintf( focus_distance_buffer, CONTENT_LENGTH, "% 4dcm", focus_distance_cm );
    }
    snprintf( focus, CONTENT_LENGTH, "%s%s%s", p_edit_left, focus_distance_buffer, p_edit_right );

    // duration part:
    static char duration[ CONTENT_LENGTH + 1 ];
    if( !g_data.display.transition_in_progress && ( g_data.display.state == STATE__PLAY || ( g_data.display.state == STATE__EDIT && g_data.display.sequence_index == 1 ) ) ) {
        snprintf( duration, CONTENT_LENGTH, "" );
    }
    else {
        const size_t pos = g_data.display.transition_in_progress ? 3 : ( g_data.display.duration_edited_timepoint_ms != 0 ? _cycle % 2 : 0 );
        p_edit_left = p_edit[ 0 ][ pos ];
        p_edit_right = p_edit[ 1 ][ pos ];
        const double duration_s = g_data.display.duration_edited_timepoint_ms != 0 ? g_data.display.target_duration_s : g_data.display.duration_s;
        snprintf( duration, CONTENT_LENGTH, "%s%ss%s", p_edit_left, format_float( duration_s, 3 ), p_edit_right );
    }

    // content:
    snprintf( content, CONTENT_LENGTH, "%s | %s | %s", step, focus, duration );
    return content;
}


char * Print_overlay__content( const size_t _cycle )
{
    // edit or play:
    if( g_data.display.state == STATE__EDIT || g_data.display.state == STATE__PLAY ) {
        return Print_overlay__edit_play_content( _cycle );
    }

    // inactive state:
    static char content[ CONTENT_LENGTH + 1 ];
    if( g_data.display.state == STATE__INACTIVE ) {
        snprintf( content, CONTENT_LENGTH, "" );
        return content;
    }

    // deal with messages:
    static const char * const p_messages[ 2 ] = { "checking lens limits", "calibrating lens" };
    snprintf( content, CONTENT_LENGTH, "%s...", p_messages[ g_data.display.state - STATE__CHECKING_LENS_LIMITS ] );
    return content;
}


void Print_overlay( const size_t _cycle )
{
    // header:
    static const char * p_headers[ 4 ] = { "    ", "edit", "play", "----" };
    static const char * const p_header_progress[ 6 ] = { " .oO", ".oOo", "oOo.", "Oo. ", "o. .", ". .o" };
    p_headers[ 3 ] = p_header_progress[ _cycle % 6 ];
    const size_t header_index = g_data.display.state < 3 ? g_data.display.state : 3;

    // print:
    Print_bmp( "[%s] %s", p_headers[ header_index ], Print_overlay__content( _cycle ) );
}


void Overlay_task()
{
    // (de)activate task:
    g_data.task_running = !g_data.task_running;
    
    // loop indefinitely:
    size_t cycle = 0;
    while( g_data.task_running ) {
        
        // print overlay (only if not in menus):
        if( !gui_menu_shown() ) {
            Print_overlay( cycle++ );
        }
        
        // breathe a little to let other tasks do their job properly:
        msleep( 100 );
    }
}


void Save_data_store()
{
    FIO_RemoveFile( FOCUS_SQ_SETTINGS_FILE );
    FILE * p_file = FIO_CreateFile( FOCUS_SQ_SETTINGS_FILE );
    FIO_WriteFile( p_file, g_data.store.lens_name, sizeof( char ) * LENS_NAME_MAX_LENGTH );
    FIO_WriteFile( p_file, &g_data.store.focus_positions.count, sizeof( size_t ) );
    FIO_WriteFile( p_file, g_data.store.focus_positions.p_data, sizeof( unsigned ) * g_data.store.focus_positions.count );
    FIO_WriteFile( p_file, g_data.store.modes, sizeof( Mode ) * 3 );
    FIO_WriteFile( p_file, &g_data.store.focus_points.count, sizeof( size_t ) );
    FIO_WriteFile( p_file, g_data.store.focus_points.p_data, sizeof( Focus_point ) * g_data.store.focus_points.count );
    FIO_CloseFile( p_file );
}


void Load_data_store()
{
    FILE * p_file = FIO_OpenFile( FOCUS_SQ_SETTINGS_FILE, O_RDONLY );
    if( p_file == 0 ) {
        return;
    }
    FIO_ReadFile( p_file, g_data.store.lens_name, sizeof( char ) * LENS_NAME_MAX_LENGTH );
    FIO_ReadFile( p_file, &g_data.store.focus_positions.count, sizeof( size_t ) );
    vector_reserve( &g_data.store.focus_positions, g_data.store.focus_positions.count );
    FIO_ReadFile( p_file, g_data.store.focus_positions.p_data, sizeof( unsigned ) * g_data.store.focus_positions.count );
    FIO_ReadFile( p_file, g_data.store.modes, sizeof( Mode ) * 3 );
    FIO_ReadFile( p_file, &g_data.store.focus_points.count, sizeof( size_t ) );
    vector_reserve( &g_data.store.focus_points, g_data.store.focus_points.count );
    FIO_ReadFile( p_file, g_data.store.focus_points.p_data, sizeof( Focus_point ) * g_data.store.focus_points.count );
    FIO_CloseFile( p_file );
}


void Set_focus_position_normalizer()
{
    // normalization in progress...
    g_data.display.state = STATE__CHECKING_LENS_LIMITS;
    
    // go to the very last position asap:
    while( lens_focus_ex( ASAP_STEP_NUMBER, 3, true, true, 0 ) ) {}
            
    // store last focus position:
    const int last_focus_position = lens_info.focus_pos;
    
    // go to the very first position asap:
    while( lens_focus_ex( ASAP_STEP_NUMBER, 3, false, true, 0 ) ) {}
    
    // set first focus position:
    g_data.lens_limits.first_focus_position = lens_info.focus_pos;
    
    // dump first & last focus positions:
    Log( "{f} first focus pos.: %d\n", g_data.lens_limits.first_focus_position );
    Log( "{f} last  focus pos.: %d\n", last_focus_position );
    
    // compute position normalizer regarding distance sign:
    const int focus_position_distance = last_focus_position - g_data.lens_limits.first_focus_position;
    g_data.lens_limits.focus_position_normalizer = focus_position_distance < 0 ? -1 : 1;
    Log( "{f} normalizer sign : %s\n", g_data.lens_limits.focus_position_normalizer > 0 ? "positive" : "negative" );
    
    // normalization finished.
    g_data.display.state = STATE__EDIT;
}


unsigned Normalized_focus_position()
{
    // keep only positive values starting at zero:
    return ( lens_info.focus_pos - g_data.lens_limits.first_focus_position ) * g_data.lens_limits.focus_position_normalizer;
}


size_t Closest_step_from_position( const unsigned _focus_position )
{
    size_t step = 0;
    while( step < g_data.store.focus_positions.count && *( ( unsigned * ) vector_get( &g_data.store.focus_positions, step ) ) < _focus_position ) {
        step++;
    }
    // the match can be exact:
    if( *( ( unsigned * ) vector_get( &g_data.store.focus_positions, step ) ) == _focus_position )
        return step;
    // ...or not, then take the closest step before:
    return step - 1;
}


void Calibrate_lens()
{
    // calibration in progress...
    g_data.display.state = STATE__CALIBRATING_LENS;

    // avoid direct usage of the vector here:
    unsigned * focus_positions = malloc( MAX_FOCUS_POSITIONS * sizeof( unsigned ) );
    
    // for each atomically reachable lens step:
    size_t step_index = 0;
    do {
        
        // store the normalized focus position and loop to next step:
        ASSERT( step_index < MAX_FOCUS_POSITIONS );
        focus_positions[ step_index++ ] = Normalized_focus_position();
    }
    // move forward:
    while( lens_focus_ex( 1, 1, true, true, 0 ) );

    // transfer positions in the vector:
    vector_reserve( &g_data.store.focus_positions, step_index );
    memcpy( g_data.store.focus_positions.p_data, focus_positions, step_index * sizeof( unsigned ) );
    free( focus_positions );
    
    // dump focus position steps:
    Log( "{f} focus position steps: %d\n", g_data.store.focus_positions.count );
    
    // dump information:
    Dump_focus_positions();
    
    // remember lens name:
    memcpy( g_data.store.lens_name, lens_info.name, LENS_NAME_MAX_LENGTH );
    
    // evaluate how much steps does a step size of 3 and 2 (1 is obsviously 1):
    // note: we're going backward
    Evaluate_step_size( MODE_3 );
    Evaluate_step_size( MODE_2 );
    g_data.store.modes[ MODE_1 ].steps = 1;
    
    // evaluate step size speeds:
    // note: we're finishing the backward move at step size 1, then we're moving forward at step size 2, then we're moving backward to home at step size 3
    Evaluate_step_size_speed( false, MODE_1 );
    Evaluate_step_size_speed( true,  MODE_2 );
    Evaluate_step_size_speed( false, MODE_3 );
    
    // save calibration result in data store:
    Save_data_store();
    
    // calibrated.
    g_data.display.state = STATE__EDIT;
}


void Dump_focus_position_range( const size_t _step_0, const size_t _step_1 )
{
    Log( "{f} focus positions [%d,%d]: ", _step_0, _step_1 );
    for( size_t step = _step_0; step <= _step_1; step++ ) {
        Log( "%d", *( ( unsigned * ) vector_get( &g_data.store.focus_positions, step ) ) );
        if( step != _step_1 ) {
            Log( "," );
        }
    }
    Log( "\n" );
}


void Dump_focus_positions()
{    
    // dump positions range:
    const size_t step_range = 3;
    
    // dump first focus positions:
    const size_t step_0 = 0;
    const size_t step_1 = step_range - 1;
    Dump_focus_position_range( step_0, step_1 );
    
    // dump last focus positions:
    const size_t step_3 = g_data.store.focus_positions.count - 1;
    const size_t step_2 = g_data.store.focus_positions.count - step_range;
    Dump_focus_position_range( step_2, step_3 );
}


void Evaluate_step_size( const size_t _mode )
{
    // get current focus position & compute closest step:
    const size_t step_0 = Closest_step_from_position( Normalized_focus_position() );
    
    // move lens, 8 iterations to average the output values:
    const size_t loop = 8;
    const unsigned step_size = ( _mode == MODE_3 ) ? 3 : 2;
    ASSERT( lens_focus_ex( ( unsigned ) loop, step_size, false, true, 0 ) );
    
    // get new focus position & compute closest step:
    const size_t step_1 = Closest_step_from_position( Normalized_focus_position() );
    
    // compute corresponding step movement:
    const size_t step_size_steps = ( step_0 - step_1 ) / loop;
    Log( "{f} steps for step size of %d: %d\n", step_size, step_size_steps );
    g_data.store.modes[ _mode ].steps = step_size_steps;
}


void Evaluate_step_size_speed( const bool _forward, const size_t _mode )
{
    // get the current focus lens step from position:
    const size_t current_step = Closest_step_from_position( Normalized_focus_position() );
    
    // we want to cover the whole remaining steps to reach begin or end, depending if moving forward or backward:
    // (note: because the current step is the closest approximation, we need to take 1 step of margin when going forward)
    const size_t step_count = _forward ? ( g_data.store.focus_positions.count - current_step - 1 - 1 ) : current_step;
    
    // evaluate the number of loop we can do depending of the current step size steps:
    const size_t step_size_steps = g_data.store.modes[ _mode ].steps;
    const size_t loop_count = step_count / step_size_steps;
    
    // get the actual step count that will be reached regarding the step size:
    const size_t real_step_count = loop_count * step_size_steps;
    
    // get timepoint before the move:
    const int t_before_ms = get_ms_clock();
    
    // do the move, without waiting for feedback:
    const unsigned step_size = ( _mode == MODE_3 ) ? 3 : ( _mode == MODE_2 ) ? 2 : 1;
    lens_focus_ex( ( unsigned ) loop_count, ( unsigned ) step_size, _forward, false, 0 );
    
    // evaluate the current step size speed (in steps per second):
    const double step_size_speed = ( double ) ( real_step_count * 1000 ) / ( double ) ( get_ms_clock() - t_before_ms );
    
    // stabilize focus position before continuing:
    wait_for_stabilized_focus_position();
    
    // dump step size speed:
    Log( "{f} step size %d: %s steps per second\n", step_size, format_float( step_size_speed, 3 ) );
    g_data.store.modes[ _mode ].speed = step_size_speed;
    
    // how many steps are remaining to reach the initial destination?
    const size_t step_count_remaining = step_count - real_step_count;
    
    // something remaining, do the move with a step size of 1 (without waiting for feedback):
    if( step_count_remaining != 0 ) {
        lens_focus_ex( ( unsigned ) step_count_remaining, 1, _forward, false, 0 );
    }
}


Distribution Compute_distribution_between( const unsigned _focus_position_1, const unsigned _focus_position_2, const double _duration_s, int * _p_range )
{
    const int step_1 = ( int ) Closest_step_from_position( _focus_position_1 );
    const int step_2 = ( int ) Closest_step_from_position( _focus_position_2 );
    const int range = step_2 - step_1;
    if( _p_range != NULL ) {
        *_p_range = range;
    }
    return Distribute_modes( ABS( range ), _duration_s );
}


double Distribution_duration( const Distribution * const  _p_distribution )
{
    double duration_s = 0;
    for( int i = 0; i < 3; i++ ) {
        if( _p_distribution->mode_call_counts[ i ] != 0 ) {
            duration_s += ( _p_distribution->mode_call_counts[ i ] * g_data.store.modes[ i ].steps ) / g_data.store.modes[ i ].speed;
        }
    }
    return duration_s;
}


Distribution Distribute_modes( const size_t _step_range, const double _target_duration_s )
{
    Distribution distribution = { { 0, 0, 0 }, .wait = 0 };

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
        double best_distribution_duration = 100000;
        for( double delta = -0.2; delta < ( double ) 0.2; delta += ( double ) 0.005 ) { // 80 loops between +/-20% of the initial ratio

            // compute distribution with current ratio and delta:
            size_t count = ( size_t )( ( ratio + delta ) * _step_range ) / g_data.store.modes[ offset ].steps;

            // the first speed is the reference:
            size_t range = _step_range;
            int _offset = offset;
            Distribution current_distribution = { { 0, 0, 0 }, .wait = 0 };
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
            const double current_distribution_duration = ABS( _target_duration_s - Distribution_duration( &current_distribution ) );
            if( current_distribution_duration < best_distribution_duration ) {
                best_distribution_duration = current_distribution_duration;
                distribution = current_distribution;
            }
        }
    }
    
    // wait time:
    distribution.duration_s = Distribution_duration( &distribution );
    distribution.wait = _target_duration_s - distribution.duration_s;
    if( distribution.wait > 0 ) {
        distribution.duration_s += distribution.wait;
    }

    // return computed distribution:
    return distribution;
}


double Play_distribution( const Distribution * const _p_distribution, const bool _forward )
{
    // get timepoint before the move:
    const int t_before_ms = get_ms_clock();
    
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
    Job jobs[ 4 ] = { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } };
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

    // return the real duration of the distribution:
    return ( double ) ( get_ms_clock() - t_before_ms ) / ( double ) 1000.0;
}


void Go_to()
{
    // transition:
    g_data.display.transition_in_progress = true;

    // read focus point data:
    const unsigned current_focus_position = Normalized_focus_position();
    const Focus_point * const p_focus_point = vector_get( &g_data.store.focus_points, g_data.index );    

    // setup displays:
    g_data.display.sequence_index = g_data.index + 1;
    g_data.display.normalized_focus_position = p_focus_point->normalized_position;
    g_data.display.focus_distance_cm = p_focus_point->distance_cm;
    g_data.display.target_duration_s = p_focus_point->distribution.duration_s;
    g_data.display.duration_s = p_focus_point->distribution.duration_s;

    // play distribution:
    const double real_duration_s = Play_distribution( &p_focus_point->distribution, p_focus_point->normalized_position > current_focus_position );

    // wait for current stabilized focus position and get real focus position:
    wait_for_stabilized_focus_position();
    const unsigned real_focus_position = Normalized_focus_position();

    // dump deviations:
    Log( "{f} deviations: %d steps, %ss\n", ( int ) real_focus_position - ( int ) p_focus_point->normalized_position, format_float( real_duration_s - p_focus_point->distribution.duration_s, 3 ) );
    
    // transition finished:
    g_data.display.transition_in_progress = false;
}


void Go_to_asap()
{
    // transition:
    g_data.display.transition_in_progress = true;

    // wait for current stabilized focus position:
    wait_for_stabilized_focus_position();

    // read current focus position:
    const unsigned current_focus_position = Normalized_focus_position();

    // read focus point data:
    const Focus_point * const p_focus_point = vector_get( &g_data.store.focus_points, g_data.index );    
    
    // compute asap distribution:
    int range = 0;
    const Distribution distribution = Compute_distribution_between( current_focus_position, p_focus_point->normalized_position, 0, &range );

    // setup displays:
    g_data.display.sequence_index = g_data.index + 1;
    g_data.display.sequence_length = g_data.store.focus_points.count;
    g_data.display.normalized_focus_position = p_focus_point->normalized_position;
    g_data.display.focus_distance_cm = p_focus_point->distance_cm;
    g_data.display.target_duration_s = 0;
    g_data.display.duration_s = distribution.duration_s;

    // play distribution:
    Play_distribution( &distribution, range > 0 );
    
    // transition finished:
    g_data.display.transition_in_progress = false;
    g_data.display.target_duration_s = p_focus_point->duration_s;
    g_data.display.duration_s = p_focus_point->distribution.duration_s;
}


void Action__toggle_mode()
{
    // cycle between inactive, edit & play:
    g_data.display.state++;
    if( g_data.display.state > STATE__PLAY ) {
        g_data.display.state = STATE__INACTIVE;
    }

    // there's only something to do when switching to edit:
    if( g_data.display.state != STATE__EDIT ) {
        return;
    }
    
    // perform focus position normalization if needed:
    if( g_data.lens_limits.focus_position_normalizer == 0 ) {
        Set_focus_position_normalizer();
    }
    
    // perform lens calibration if needed:
    if( memcmp( g_data.store.lens_name, lens_info.name, LENS_NAME_MAX_LENGTH ) != 0 ) {
        Calibrate_lens();
    }
    else {
        Log( "{f} lens was already calibrated.\n" );
    }

    // focus point list is not empty, go to first/last known position asap:
    if( g_data.store.focus_points.count != 0 ) {
        Go_to_asap();
        return;
    }

    // empty list, add a first focus point:
    Action__edit__add_focus_point();
}


void Action__toggle_camera_display()
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


void Action__edit__add_focus_point()
{        
    // new focus point information:
    Focus_point focus_point = {
        .normalized_position = Normalized_focus_position(),
        .distance_cm = lens_info.focus_dist,
        .duration_s = 0
    };

    // compute new distribution if needed:
    if( g_data.store.focus_points.count > 0 ) {
        const Focus_point * const p_previous_focus_point = vector_get( &g_data.store.focus_points, g_data.index );
        focus_point.distribution = Compute_distribution_between( p_previous_focus_point->normalized_position, focus_point.normalized_position, focus_point.duration_s, NULL );

        // set the default target duration as close as possible as the best possible effort, using 100ms approximation:
        focus_point.duration_s = ( double ) ( ( int ) ( ( focus_point.distribution.duration_s + ( double ) FOCUS_POINT_INCREMENT_S ) * 10 ) ) / 10;

        // compute closest distribution with new target value:
        focus_point.distribution = Compute_distribution_between( p_previous_focus_point->normalized_position, focus_point.normalized_position, focus_point.duration_s, NULL );

        // increment index:
        g_data.index++;
    }

    // insert new focus point:
    vector_insert( &g_data.store.focus_points, g_data.index, &focus_point );

    // TODO if there's a point after the insertion, we need to update its distribution
        
    // save focus points:
    Save_data_store();

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


void Action__edit__set_current_lens_information()
{
    // get current focus point information:
    Focus_point * p_focus_point = vector_get( &g_data.store.focus_points, g_data.index );

    // update lens information:
    p_focus_point->normalized_position = Normalized_focus_position();
    p_focus_point->distance_cm = lens_info.focus_dist;

    // re-compute distribution if needed:
    if( g_data.index > 0 ) {
        const Focus_point * const p_previous_focus_point = vector_get( &g_data.store.focus_points, g_data.index - 1 );
        p_focus_point->distribution = Compute_distribution_between( p_previous_focus_point->normalized_position, p_focus_point->normalized_position, p_focus_point->duration_s, NULL );
    }

    // TODO if there's a point after the insertion, we need also to update its distribution
    
    // save focus points:
    Save_data_store();

    // setup displays:
    g_data.display.focus_edited_timepoint_ms = get_ms_clock();
    g_data.display.normalized_focus_position = p_focus_point->normalized_position;
    g_data.display.focus_distance_cm = p_focus_point->distance_cm;
    g_data.display.duration_edited_timepoint_ms = get_ms_clock();
    g_data.display.duration_s = p_focus_point->distribution.duration_s;
}


void Action__edit__remove_focus_point()
{
    // nothing to do if we're on the first point:
    if( g_data.index == 0 ) {
        return;
    }

    // remove point:
    vector_erase( &g_data.store.focus_points, g_data.index );

    // if needed, we have to recompute the related distribution of the point after the one just removed:
    if( g_data.index < g_data.store.focus_points.count ) {
        const Focus_point * const p_previous_focus_point = vector_get( &g_data.store.focus_points, g_data.index - 1 );
        Focus_point * p_focus_point = vector_get( &g_data.store.focus_points, g_data.index );
        p_focus_point->distribution = Compute_distribution_between( p_previous_focus_point->normalized_position, p_focus_point->normalized_position, p_focus_point->duration_s, NULL );
    }
    
    // save focus points:
    Save_data_store();

    // go to previous point:
    g_data.index--;

    // go to new position asap:
    Go_to_asap();
}


void Action__edit__go_to_next_focus_point()
{
    // increase index (or loop):
    if( g_data.index++ == g_data.store.focus_points.count - 1 ) {
        g_data.index = 0;
    }

    // go to new position asap:
    Go_to_asap();
}


void Action__edit__go_to_previous_focus_point()
{
    // decrease index (or loop):
    if( g_data.index-- == 0 ) {
        g_data.index = g_data.store.focus_points.count - 1;
    }

    // go to new position asap:
    Go_to_asap();
}


void Action__edit__update_transition_speed( const bool _increase )
{
    // nothing to do if we're on the first point:
    if( g_data.index == 0 ) {
        return;
    }

    // get current focus point information:
    Focus_point * p_focus_point = vector_get( &g_data.store.focus_points, g_data.index );

    // check for limits:
    if( ( _increase && p_focus_point->duration_s > ( double ) 9.8 ) || ( !_increase && p_focus_point->duration_s < ( double ) 0.1 ) ) {
        return;
    }

    // update transition duration:
    p_focus_point->duration_s += ( double ) FOCUS_POINT_INCREMENT_S * ( _increase ? 1 : -1 );

    // recompute distribution:
    const Focus_point * const p_previous_focus_point = vector_get( &g_data.store.focus_points, g_data.index - 1 );
    p_focus_point->distribution = Compute_distribution_between( p_previous_focus_point->normalized_position, p_focus_point->normalized_position, p_focus_point->duration_s, NULL );
    
    // save focus points:
    Save_data_store();

    // setup displays:
    g_data.display.duration_edited_timepoint_ms = get_ms_clock();
    g_data.display.target_duration_s = p_focus_point->duration_s;
    g_data.display.duration_s = p_focus_point->distribution.duration_s;
}


void Action__edit__increase_transition_speed()
{
    Action__edit__update_transition_speed( true );
}


void Action__edit__decrease_transition_speed()
{
    Action__edit__update_transition_speed( false );
}


void Action__play__go_to_next_focus_point()
{
    // check for limits:
    if( g_data.index == g_data.store.focus_points.count - 1 ) {
        return;
    }

    // increase index:
    g_data.index++;

    // go to position:
    Go_to();
}


void Action__play__go_fast_to_first_focus_point()
{
    // first focus point:
    g_data.index = 0;

    // go to new position asap:
    Go_to_asap();
}


unsigned int Key_handler( const unsigned int _key )
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
        Action__toggle_mode();
        return 0;
    }
    
    // no need to track keys when idle:
    if( g_data.display.state == STATE__INACTIVE ) {
        return 1;
    }

    // (de)activate the camera display (screen saver):
    if( _key == MODULE_KEY_RATE ) {
        Action__toggle_camera_display();
        return 0;
    }

    // edit & play keyboard mapping initialization:
    static void * functions[ 2 ][ MAX_KEY_MAP + 1 ] = { { ( void * ) -1 } };
    if( functions[ 0 ][ 0 ] == ( void * ) -1 ) {
        memset( functions, 0, 2 * MAX_KEY_MAP );

        // edit mode key mapping:
        functions[ STATE__EDIT - 1 ][ MODULE_KEY_Q ] = &Action__edit__add_focus_point;
        functions[ STATE__EDIT - 1 ][ MODULE_KEY_PRESS_SET ] = &Action__edit__set_current_lens_information;
        functions[ STATE__EDIT - 1 ][ MODULE_KEY_TRASH ] = &Action__edit__remove_focus_point;
        functions[ STATE__EDIT - 1 ][ MODULE_KEY_PRESS_RIGHT ] = &Action__edit__go_to_next_focus_point;
        functions[ STATE__EDIT - 1 ][ MODULE_KEY_PRESS_LEFT ] = &Action__edit__go_to_previous_focus_point;
        functions[ STATE__EDIT - 1 ][ MODULE_KEY_PRESS_UP ] = &Action__edit__increase_transition_speed;
        functions[ STATE__EDIT - 1 ][ MODULE_KEY_PRESS_DOWN ] = &Action__edit__decrease_transition_speed;
        
        // play mode key mapping:
        functions[ STATE__PLAY - 1 ][ MODULE_KEY_PRESS_SET ] = &Action__play__go_to_next_focus_point;
        functions[ STATE__PLAY - 1 ][ MODULE_KEY_Q ] = &Action__play__go_fast_to_first_focus_point;
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
    .priv    = Overlay_task,
    .help    = "Start focus sequencing",
} };


unsigned int Init()
{
    // init lens name:
    memset( g_data.store.lens_name, 0, LENS_NAME_MAX_LENGTH );

    // init vectors:
    g_data.store.focus_positions = vector_create( sizeof( unsigned ) );
    g_data.store.focus_points = vector_create( sizeof( Focus_point ) );

    // load data:
    Load_data_store();
    
    // add focus sequencing mode to Focus menu:
    menu_add( "Focus", g_menu, COUNT( g_menu ) );
    return 0;
}


unsigned int Deinit()
{
    // stop task:
    g_data.task_running = false;

    return 0;
}


MODULE_INFO_START()
    MODULE_INIT( Init )
    MODULE_DEINIT( Deinit )
MODULE_INFO_END()

MODULE_CBRS_START()
    MODULE_CBR( CBR_KEYPRESS, Key_handler, 0 )
MODULE_CBRS_END()