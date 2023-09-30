#include <module.h>
#include <dryos.h>
#include <menu.h>
#include <config.h>
#include <lens.h>
#include <bmp.h>
#include <powersave.h>

#include "cinemato.h"


// TODO do we really want to named it "cinemato" because of the powersave feature?
// -> can we add this feature somewhere else?
// TODO add header file with all structs, defines and sorted function definition
// TODO the transition function question:
//		I have:
//			- n steps to cross with a d target duration in second
//			- "speed 1" doing n1 steps ( 1) in s1 ( 30) steps per second speed
//			- "speed 2" doing n2 steps ( 4) in s2 ( 75) steps per second speed
//			- "speed 3" doing n3 steps (28) in s3 (100) steps per second speed
//		How to do the best move (exact target point + closest duration) ?
// TODO timing estimation function
// TODO after each move, dump step deviation & duration deviation


void save_data_store()
{
    FIO_RemoveFile( CINEMATO_SETTINGS_FILE );
    FILE * p_file = FIO_CreateFile( CINEMATO_SETTINGS_FILE );
    FIO_WriteFile( p_file, g_data.store.lens_name, LENS_NAME_MAX_LENGTH * sizeof( char ) );
    FIO_WriteFile( p_file, &g_data.store.focus_position_steps, sizeof( size_t ) );
	FIO_WriteFile( p_file, g_data.store.focus_positions, g_data.store.focus_position_steps * sizeof( unsigned ) );
	FIO_WriteFile( p_file, g_data.store.step_size_steps, STEP_SIZE_COUNT * sizeof( size_t ) );
	FIO_WriteFile( p_file, g_data.store.step_size_speeds, STEP_SIZE_COUNT * sizeof( double ) );
    FIO_WriteFile( p_file, &g_data.store.focus_point_count, sizeof( size_t ) );
	FIO_WriteFile( p_file, g_data.store.focus_points, g_data.store.focus_point_count * sizeof( focus_point ) );
    FIO_CloseFile( p_file );
}


void load_data_store()
{
    FILE * p_file = FIO_OpenFile( CINEMATO_SETTINGS_FILE, O_RDONLY );
    if( p_file == 0 ) {
        return;
	}
	FIO_ReadFile( p_file, g_data.store.lens_name, LENS_NAME_MAX_LENGTH * sizeof( char ) );
    FIO_ReadFile( p_file, &g_data.store.focus_position_steps, sizeof( size_t ) );
	FIO_ReadFile( p_file, g_data.store.focus_positions, g_data.store.focus_position_steps * sizeof( unsigned ) );
	FIO_ReadFile( p_file, g_data.store.step_size_steps, STEP_SIZE_COUNT * sizeof( size_t ) );
	FIO_ReadFile( p_file, g_data.store.step_size_speeds, STEP_SIZE_COUNT * sizeof( double ) );
    FIO_ReadFile( p_file, &g_data.store.focus_point_count, sizeof( size_t ) );
	FIO_ReadFile( p_file, g_data.store.focus_points, g_data.store.focus_point_count * sizeof( focus_point ) );
    FIO_CloseFile( p_file );
}


void c_print( const char * _fmt, ... )
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


char * format_float( const double _value, char * _buffer, const size_t _buffer_len )
{
    const int left = ( int ) _value;
    const int right = ( ( int )( _value * 1000 ) ) - ( left * 1000 );
    snprintf( _buffer, _buffer_len - 1, "%d.%d", left, right );
    return _buffer;
}


const char * overlay_header( const int _overlay_header_type )
{
	// define possible headers:
	static const char * headers[ OVERLAY_HEADER_VALUE_COUNT ] = { "    ", "edit", "play", "----" };
	
	// animate working icon position:
	if( _overlay_header_type == OVERLAY_HEADER_WORKING ) {
		static int working_icon_position = 0;
		static const char * const working_labels[ OVERLAY_HEADER_MAX_LENGTH ] = { "Oo..", "oOo.", ".oOo", "..oO", "o..o" };
		headers[ OVERLAY_HEADER_WORKING ] = working_labels[ working_icon_position++ % OVERLAY_HEADER_MAX_LENGTH ];
	}
	
	// return header:
	ASSERT( _overlay_header_type < OVERLAY_HEADER_VALUE_COUNT );
	return headers[ _overlay_header_type ];
}


char * interpret_focus_distance( const unsigned _focus_distance, char * _buffer, const size_t _buffer_len )
{
	if( _focus_distance == INFINITE_FOCUS_DISTANCE ) {
		snprintf( _buffer, _buffer_len, "inf." ); // TODO inf character
	}
	else {
		snprintf( _buffer, _buffer_len, "%dcm", _focus_distance );
	}
	return _buffer;
}


double interpret_transition_duration( const unsigned _step_source, const unsigned _step_destination, const unsigned _transition_speed )
{
	ASSERT( _transition_speed < LENS_MOVE_SPEED_COUNT );
	
	// move speed to step size conversion:
	// LENS_MOVE_SPEED__VERY_SLOW, LENS_MOVE_SPEED__SLOW, LENS_MOVE_SPEED__MEDIUM, LENS_MOVE_SPEED__FAST
	static unsigned move_speed_to_step_size[ LENS_MOVE_SPEED_COUNT ] = { STEP_SIZE_1, STEP_SIZE_1, STEP_SIZE_2, STEP_SIZE_3 };
	
	// sleep duration per step, in second:
	const double sleep_duration_per_step_s = ( double ) ( ( _transition_speed == LENS_MOVE_SPEED__VERY_SLOW ) ? VERY_SLOW_SLEEP_DURATION_MS : 0 ) / ( double ) 1000;
	
	// steps to cross:
	const unsigned steps = ABS( ( int ) _step_destination - ( int ) _step_source );
	
	// compute total transition duration:
	return ( g_data.store.step_size_speeds[ move_speed_to_step_size[ _transition_speed ] ] + sleep_duration_per_step_s ) * steps;
}


unsigned normalized_focus_position()
{
	// keep only positive values starting at zero:
	return ( lens_info.focus_pos - g_data.first_focus_position ) * g_data.focus_position_normalizer;
}


size_t closest_step_from_position( const unsigned _focus_position )
{
	size_t step = 0;
	while( step < g_data.store.focus_position_steps && g_data.store.focus_positions[ step ] < _focus_position ) {
		step++;
	}
	// the match can be exact:
	if( g_data.store.focus_positions[ step ] == _focus_position )
		return step;
	// ...or not, then take the closest step before:
	return step - 1;
}


void overlay_print()
{
	/* TODO
	progress (6):   " .oO", ".oOo", "oOo.", "Oo. ", "o. .", ". .o"
	diff (2):       "{", "}"
	update (2x2):   " ", ">"  and " ", "<"
	transition (6): "==>", " ==", "  =", "   ", ">  ", "=> "

	[----] [99] /99 | [999cm] | [9.999s]                // max length


	[    ]												// disabled

	[....] checking lens limits...                      // (animated) lens limit
	[....] calibrating lens...                          // (animated) calibration

	[edit]   1  / 1 |   12cm  |                         // edit: steady
	[edit]   1  / 1 | { 89cm} |                         // edit: (animated) autofocus -> different step value
	[edit]   1  / 1 | > 89cm< |                         // edit: (animated) [SET] affect step value (update animation)
	[edit] > 2< / 2 |   89cm  |  3.458s                 // edit: (animated) [Q] insert new step
	[edit]   2  / 2 | {114cm} |  3.458s                 // edit: (animated) autofocus -> different step value
	[edit]   2  / 2 | >114cm< |  3.458s                 // edit: (animated) [SET] affect step value (update animation)
	[edit]   2  / 2 |  114cm  | >4.123s<                // edit: (animated) [UP|DOWN] increase/decrease transition duration
	[edit] ==> ( 1) | ( 72cm) | (0.100s)                // edit: (animated) fast transition after [TRASH] (previous), [LEFT|RIGHT]
	[edit]   1  / 2 |   12cm  |                         // edit: steady

	[play]   1  / 2 |   12cm  |                         // play: steady
	[play] ==> ( 2) | ( 48cm) | (4.123s)                // play: (animated) transition to next step [SET] or first step [Q]
	[play]   2  / 2 |  114cm  |                         // play: steady
	[play]  67  /99 |   inf.  |                         // play: steady (infinite focus length)
	*/
    // we're in the ML menus, bypass:
    if( gui_menu_shown() ) {
        return;
	}
		
	// hook currently deactivated:
	if( !g_data.hook_activated ) {
		
		// "[    ]"
		c_print( "[%s]", overlay_header( OVERLAY_HEADER_DEACTIVATED ) );
		return;
	}
	
	// currently normalizing focus position:
	if( g_data.focus_position_normalizing ) {
		
		// "[----] checking lens limits..."
		c_print( "[%s] %s", overlay_header( OVERLAY_HEADER_WORKING ), "checking lens limits..." );
		return;
	}
	
	// currently calibrating lens:
	if( g_data.lens_calibrating ) {
		
		// "[----] calibrating lens..."
		c_print( "[%s] %s", overlay_header( OVERLAY_HEADER_WORKING ), "calibrating lens..." );
		return;
	}
	
	// play mode overlays:
	if( g_data.play_mode ) {
		
		// TODO
		c_print( "[%s]", overlay_header( OVERLAY_HEADER_PLAY ) );
		return;
	}
	
	// edit mode overlays:		
	
	// TODO
	
	// check for focus step highlight expiration:
	if( g_data.edit_focus_step_timepoint_ms != 0 && ( get_ms_clock() - g_data.edit_focus_step_timepoint_ms ) > EDITION_HIGHLIGHT_MS ) {
		g_data.edit_focus_step_timepoint_ms = 0;
	}
	
	// check for transition speed highlight expiration:
	if( g_data.edit_transition_speed_timepoint_ms != 0 && ( get_ms_clock() - g_data.edit_transition_speed_timepoint_ms ) > EDITION_HIGHLIGHT_MS ) {
		g_data.edit_transition_speed_timepoint_ms = 0;
	}
	
	// get current focus point:
	focus_point * p_focus_point = &g_data.store.focus_points[ g_data.focus_point_index ];
	
	// get current focus step:
	const size_t current_step = closest_step_from_position( normalized_focus_position() );
	
	// does the current focus position match the one currently displayed?
	const bool matching_step = ( current_step == p_focus_point->focus_step );
	
	// hightlight style:
	static const char * const highlights[ 2 ] = { "\0", "*\0" };
	const char * const p_focus_step_highlight = highlights[ g_data.edit_focus_step_timepoint_ms == 0 ? 0 : 1 ];
	const char * const p_transition_speed_highlight = highlights[ g_data.edit_transition_speed_timepoint_ms == 0 ? 0 : 1 ];
	
	// bracket style depends of focus matching:
	static const char * const brackets_focused[ 2 ] = { "|\0", "|\0" };
	static const char * const brackets_unfocused[ 2 ] = { "{{\0", "}}\0" };
	const char * const * const p_brackets = matching_step ? brackets_focused : brackets_unfocused;
	
	// transition trailer to add when not on first focus point:
	char transition_trailer[ MAX_TRAILER_LENGTH ];
	if( g_data.focus_point_index == 0 ) {
		snprintf( transition_trailer, MAX_TRAILER_LENGTH, "" );
	}
	else {
		char duration_buffer[ MAX_DOUBLE_CONVERSION_LENGTH ];
		snprintf( transition_trailer, MAX_TRAILER_LENGTH, " -> %s%ss%s", p_transition_speed_highlight, format_float( interpret_transition_duration( g_data.store.focus_points[ g_data.focus_point_index - 1 ].focus_step, p_focus_point->focus_step, p_focus_point->transition_speed ), duration_buffer, MAX_DOUBLE_CONVERSION_LENGTH ), p_transition_speed_highlight );
	}
	
	// display also the realtime focus distance:
	char focus_distance[ MAX_FOCUS_DISTANCE_LENGTH ];	
	c_print( "[%s] %s%s%s%s%s %d /%d%s", overlay_header( OVERLAY_HEADER_EDIT ), p_brackets[ 0 ], p_focus_step_highlight, interpret_focus_distance( lens_info.focus_dist, focus_distance, MAX_FOCUS_DISTANCE_LENGTH ), p_focus_step_highlight, p_brackets[ 1 ], g_data.focus_point_index + 1, g_data.store.focus_point_count, transition_trailer );
}


void overlay_task()
{
	// load data store:
	load_data_store();
	
	// task is now running:
	g_data.task_running = true;
	
    // loop indefinitely:
    while( g_data.task_running ) {
		
		// print overlay:
        overlay_print();
        
        // breathe a little to let other tasks do their job properly:
        msleep( 100 );
    }
}


void set_focus_position_normalizer()
{
	// normalization in progress...
	g_data.focus_position_normalizing = true;
	
	// go to the very last position asap:
	while( lens_focus_ex( ASAP_STEP_NUMBER, 3, true, true, 0 ) ) {}
			
	// store last focus position:
	const int last_focus_position = lens_info.focus_pos;
	
	// go to the very first position asap:
	while( lens_focus_ex( ASAP_STEP_NUMBER, 3, false, true, 0 ) ) {}
	
	// set first focus position:
	g_data.first_focus_position = lens_info.focus_pos;
	
	// dump first & last focus positions:
	printf( "{c} first focus pos.: %d\n", g_data.first_focus_position );
	printf( "{c} last  focus pos.: %d\n", last_focus_position );
	
	// compute position normalizer regarding distance sign:
	const int focus_position_distance = last_focus_position - g_data.first_focus_position;
	g_data.focus_position_normalizer = focus_position_distance < 0 ? -1 : 1;
	printf( "{c} normalizer sign : %s\n", g_data.focus_position_normalizer > 0 ? "positive" : "negative" );
	
	// normalization finished.
	g_data.focus_position_normalizing = false;
}


void lens_go_to( const unsigned _target_step, const unsigned _speed )
{
	// TODO quite the same as evaluate_step_size_speed, but:
	// - no timing measure
	// - no wait_for_stabilized_focus_position
	// - direction is deduced from current vs target step
	// - careful, the distance is potentially lesser than 1x loop of step size
}


void dump_focus_position_range( const size_t _step_0, const size_t _step_1 )
{
	printf( "{c} focus positions [%d,%d]: ", _step_0, _step_1 );
	for( size_t step = _step_0; step <= _step_1; step++ ) {
		printf( "%d", g_data.store.focus_positions[ step ] );
		if( step != _step_1 ) {
			printf( "," );
		}
	}
	printf( "\n" );
}


void dump_focus_positions()
{	
	// dump positions range:
	const size_t step_range = 3;
	
	// dump first focus positions:
	const size_t step_0 = 0;
	const size_t step_1 = step_range - 1;
	dump_focus_position_range( step_0, step_1 );
	
	// dump last focus positions:
	const size_t step_3 = g_data.store.focus_position_steps - 1;
	const size_t step_2 = g_data.store.focus_position_steps - step_range;
	dump_focus_position_range( step_2, step_3 );
}


size_t evaluate_step_size( const unsigned _step_size )
{
	// get current focus position & compute closest step:
	const size_t step_0 = closest_step_from_position( normalized_focus_position() );
	
	// move lens, 8 iterations to average the output values:
	const size_t loop = 8;
	ASSERT( lens_focus_ex( loop, _step_size, false, true, 0 ) );
	
	// get new focus position & compute closest step:
	const size_t step_1 = closest_step_from_position( normalized_focus_position() );
	
	// compute corresponding step movement:
	const size_t step_move = ( step_0 - step_1 ) / loop;
	printf( "{c} steps for step size of %d: %d\n", _step_size, step_move );
	return step_move;
}


double evaluate_step_size_speed( const bool _forward, const unsigned _step_size, const size_t _step_size_steps )
{
	// get the current focus lens step from position:
	const size_t current_step = closest_step_from_position( normalized_focus_position() );
	
	// we want to cover the whole remaining steps to reach begin or end, depending if moving forward or backward:
	// (note: because the current step is the closest approximation, we need to take 1 step of margin when going forward)
	const size_t step_count = _forward ? ( g_data.store.focus_position_steps - current_step - 1 - 1 ) : current_step;
	
	// evaluate the number of loop we can do depending of the current step size steps:
	const size_t loop_count = step_count / _step_size_steps;
	
	// get the actual step count that will be reached regarding the step size:
	const size_t real_step_count = loop_count * _step_size_steps;
	
	// get timepoint before the move:
	const int t_before_ms = get_ms_clock();
	
	// do the move, without waiting for feedback:
	lens_focus_ex( loop_count, _step_size, _forward, false, 0 );
	
	// evaluate the current step size speed (in steps per second):
	const double step_size_speed = ( double ) ( real_step_count * 1000 ) / ( double ) ( get_ms_clock() - t_before_ms );
	
	// stabilize focus position before continuing:
	wait_for_stabilized_focus_position();
	
	// dump step size speed:
	char sps_buffer[ MAX_DOUBLE_CONVERSION_LENGTH ];
	printf( "{c} step size %d: %s steps per second\n", _step_size, format_float( step_size_speed, sps_buffer, MAX_DOUBLE_CONVERSION_LENGTH ) );
	
	// how many steps are remaining to reach the initial destination?
	const size_t step_count_remaining = step_count - real_step_count;
	
	// something remaining, do the move with a step size of 1 (without waiting for feedback):
	if( step_count_remaining != 0 ) {
		lens_focus_ex( step_count_remaining, 1, _forward, false, 0 );
	}
	
	// return computed step size speed:
	return step_size_speed;
}


void calibrate_lens()
{
	// calibration in progress...
	g_data.lens_calibrating = true;

	// for each atomically reachable lens step:
	g_data.store.focus_position_steps = 0;
	do {
		
		// store the normalized focus position and loop to next step:
		g_data.store.focus_positions[ g_data.store.focus_position_steps++ ] = normalized_focus_position();
	}
	// move forward:
	while( lens_focus_ex( 1, 1, true, true, 0 ) );
	
	// dump focus position steps:
	printf( "{c} focus position steps: %d\n", g_data.store.focus_position_steps );
	
	// dump information:
	dump_focus_positions();
	
	// remember lens name:
	memcpy( g_data.store.lens_name, lens_info.name, LENS_NAME_MAX_LENGTH );
	
	// evaluate how much steps does a step size of 3 and 2 (1 is obsviously 1):
	// note: we're going backward
	g_data.store.step_size_steps[ STEP_SIZE_3 ] = evaluate_step_size( 3 );
	g_data.store.step_size_steps[ STEP_SIZE_2 ] = evaluate_step_size( 2 );
	g_data.store.step_size_steps[ STEP_SIZE_1 ] = 1;
	
	// evaluate step size speeds:
	// note: we're finishing the backward move at step size 1, then we're moving forward at step size 2, then we're moving backward to home at step size 3
	g_data.store.step_size_speeds[ STEP_SIZE_1 ] = evaluate_step_size_speed( false, 1, g_data.store.step_size_steps[ STEP_SIZE_1 ] );
	g_data.store.step_size_speeds[ STEP_SIZE_2 ] = evaluate_step_size_speed( true, 2, g_data.store.step_size_steps[ STEP_SIZE_2 ] );
	g_data.store.step_size_speeds[ STEP_SIZE_3 ] = evaluate_step_size_speed( false, 3, g_data.store.step_size_steps[ STEP_SIZE_3 ] );
	
	// save calibration result in data store:
	save_data_store();
	
	// calibrated.
	g_data.lens_calibrating = false;
}


void toggle_mode( const bool _play_mode )
{
	g_data.play_mode = _play_mode;
	
	// we are switching to play mode, nothing to do:
	if( g_data.play_mode ) {
		return;
	}
	
	// we are switchting to edit mode:
	
	// perform focus position normalization if needed:
	if( g_data.focus_position_normalizer == 0 ) {
		set_focus_position_normalizer();
	}
	
	// perform lens calibration if needed:
	if( memcmp( g_data.store.lens_name, lens_info.name, LENS_NAME_MAX_LENGTH ) != 0 ) {
		calibrate_lens();
	}
	else {
		printf( "{c} lens was already calibrated.\n" );
	}
	
	// focus point list is not empty, that's fine:
	if( g_data.store.focus_point_count != 0 ) {
		return;
	}
	
	// there's currently no focus point in the list, add one:
	focus_point * p_focus_point = &g_data.store.focus_points[ g_data.focus_point_index ];
	p_focus_point->focus_step = closest_step_from_position( normalized_focus_position() );
	p_focus_point->focus_distance_cm = lens_info.focus_dist;
	p_focus_point->transition_speed = 0; // useless for 1st point
	
	// one more useful point:
	g_data.store.focus_point_count++;
}


void play__go_to_next_focus_point()
{
	// TODO
	// [SET] button runs transition from current to next focus point in the sequence,
	// go back using fast transition to the first focus point in the sequence when
	// reaching the end:
}


void play__go_fast_to_first_focus_point()
{
	// TODO
	// [Q] button runs fast transition to the first focus point in the sequence:
}


void play__toggle_camera_display()
{
	g_data.display_activated = !g_data.display_activated;
	if( !g_data.display_activated ) {
		display_off();
		return;
	}
	display_on();
}


void edit__add_focus_point()
{
	// TODO
	// [KEY_Q] button inserts and setup a new focus point in the sequence,
	// after the current point:
}


void edit__remove_focus_point()
{
	// TODO
	// [TRASH] button removes the current focus point then fast switch and setup the
	// closest available focus point in the sequence (right first, fallback to left):
	// note: won't work if only one focus point remains
}


void edit__go_to_next_focus_point()
{
	// TODO
	// [RIGHT] button fast switches and setup the next focus point in the sequence:
	// note: cycle to first point when currently on the last point
}


void edit__go_to_previous_focus_point()
{
	// TODO
	// [LEFT] button fast switches and setup the previous focus point in the sequence:
	// note: cycle to last point when currently on the first point
}


void edit__set_current_lens_information()
{	
	// update current point information:
	focus_point * p_focus_point = &g_data.store.focus_points[ g_data.focus_point_index ];
	p_focus_point->focus_step = closest_step_from_position( normalized_focus_position() );
	p_focus_point->focus_distance_cm = lens_info.focus_dist;
	
	// update store:
	save_data_store();
	
	// edit timepoint:
	g_data.edit_focus_step_timepoint_ms = get_ms_clock();
}


void edit__increase_transition_speed()
{
	// TODO
	// [UP] buttons increases the transition speed that will be used to go from the
	// previous to the current focus point in the sequence:
	// note: not available when we're on the first focus point of the sequence
	
	// g_data.edit_transition_speed_timepoint_ms = get_ms_clock();
}


void edit__decrease_transition_speed()
{
	// TODO
	// [DOWN] buttons decreases the transition speed that will be used to go from the
	// previous to the current focus point in the sequence:
	// note: not available when we're on the first focus point of the sequence
	
	// g_data.edit_transition_speed_timepoint_ms = get_ms_clock();
}


unsigned int key_handler( const unsigned int _key )
{
    // we're in the ML menus, bypass:
    if( gui_menu_shown() ) {
        return 1;
	}
	
	// task is not running, bypass:
    if( !g_data.task_running ) {
        return 1;
	}
    
    // [INFO] button (de)activates cinematographer hook:
    if( _key == MODULE_KEY_INFO ) {
        g_data.hook_activated = !g_data.hook_activated;
		
		// switch to edit mode by default:
		toggle_mode( false );
        return 0;
    }
	
	// specific hook are deactivated, no need to track other keys:
    if( !g_data.hook_activated ) {
        return 1;
	}
	
	// [PLAY] button toggles between edit and play mode
	if( _key == MODULE_KEY_PLAY ) {
		toggle_mode( !g_data.play_mode );
		return 0;
	}
	
	// play mode key mapping:
	if( g_data.play_mode ) {
		switch( _key ) {
			
			// [SET] button runs transition from current to next focus point in the sequence,
			// go back using fast transition to the first focus point in the sequence when
			// reaching the end:
			case MODULE_KEY_PRESS_SET:
				play__go_to_next_focus_point();
				return 0;
				
			// [Q] button runs fast transition to the first focus point in the sequence:
			case MODULE_KEY_Q:
				play__go_fast_to_first_focus_point();
				return 0;
				
			// [RATE] button (de)activates the camera display (battery saver):
			case MODULE_KEY_RATE:
				play__toggle_camera_display();
				return 0;
				
			default:;
		}
	}
	// edit mode key mapping:
	else {
		switch( _key ) {
				
			// [KEY_Q] button inserts and setup a new focus point in the sequence,
			// after the current point:
			case MODULE_KEY_Q:
				edit__add_focus_point();
				return 0;
				
			// [TRASH] button removes the current focus point then fast switch and setup the
			// closest available focus point in the sequence (right first, fallback to left):
			// note: won't work if only one focus point remains
			case MODULE_KEY_TRASH:
				edit__remove_focus_point();
				return 0;
				
			// [RIGHT] button fast switches and setup the next focus point in the sequence:
			// note: cycle to first point when currently on the last point
			case MODULE_KEY_PRESS_RIGHT:
				edit__go_to_next_focus_point();
				return 0;
				
			// [LEFT] button fast switches and setup the previous focus point in the sequence:
			// note: cycle to last point when currently on the first point
			case MODULE_KEY_PRESS_LEFT:
				edit__go_to_previous_focus_point();
				return 0;
			
			// [SET] button sets the current lens information (step offset & focus distance)
			// to the focus point currently under setup in the sequence:
			case MODULE_KEY_PRESS_SET:
				edit__set_current_lens_information();
				return 0;
				
			// [UP] buttons increases the transition speed that will be used to go from the
			// previous to the current focus point in the sequence:
			// note: not available when we're on the first focus point of the sequence
			case MODULE_KEY_PRESS_UP:
				edit__increase_transition_speed();
				return 0;
				
			// [DOWN] buttons decreases the transition speed that will be used to go from the
			// previous to the current focus point in the sequence:
			// note: not available when we're on the first focus point of the sequence
			case MODULE_KEY_PRESS_DOWN:
				edit__decrease_transition_speed();
				return 0;
				
			default:;
		}
	}
	
	// pass-through:
	return 1;
}


// menu definition:
struct menu_entry g_menu[] = { {
    .name    = "Cinematographer",
    .select  = run_in_separate_task, // DryOS task, running in parallel
    .priv    = overlay_task,
    .help    = "Start cinematographer mode with focus sequencing",
} };


unsigned int init()
{
	// default data init:
	memset( g_data.store.lens_name, 0, 32 );
	
    // add cinematographer mode to Movie menu:
    menu_add( "Movie", g_menu, COUNT( g_menu ) );
    return 0;
}


unsigned int deinit()
{
    return 0;
}


MODULE_INFO_START()
    MODULE_INIT( init )
    MODULE_DEINIT( deinit )
MODULE_INFO_END()

MODULE_CBRS_START()
    MODULE_CBR( CBR_KEYPRESS, key_handler, 0 )
MODULE_CBRS_END()