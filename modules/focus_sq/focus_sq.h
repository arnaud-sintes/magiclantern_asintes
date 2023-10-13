#ifndef __FOCUS_SQ_H__
#define __FOCUS_SQ_H__

#include <vector.h>


// possible display states:
#define STATE__INACTIVE 0
#define STATE__EDIT 1
#define STATE__PLAY 2
#define STATE__CHECKING_LENS_LIMITS 3
#define STATE__CALIBRATING_LENS 4


// information to be displayed in LV:
typedef struct
{
    const char * const delimiters[ 2 ][ 5 ];
    int state_edited_timepoint_ms;          // if not 0, the state was just edited
    size_t state;                           // current display state
    bool transition_in_progress;            // is a lens focus transition currently in progress?
    int sequence_edited_timepoint_ms;       // if not 0, the sequence was just edited
    size_t sequence_index;                  // current/target sequence index
    size_t sequence_length;                 // sequence length
    int focus_edited_timepoint_ms;          // if not 0, the focus position in the sequence was just edited
    unsigned normalized_focus_position;     // normalized focus position as set in the sequence
    unsigned focus_distance_cm;             // focus position as set in the sequence
    int duration_edited_timepoint_ms;       // if not 0, the transition duration in the sequence was just edited
    double target_duration_s;               // target duration (second) as set in the sequence
    double duration_s;                      // related expected duration (second)
}
Display;


// single mode configuration:
typedef struct
{
    size_t steps;                           // fixed step count taken in a single 'do' call
    double speed;                           // mode speed in steps per second
}
Mode;


// possible mode indexes:
#define MODE_3  0                           // step_size = 3
#define MODE_2  1                           // step_size = 2
#define MODE_1  2                           // step_size = 1


// computed mode call count distribution:
typedef struct
{
    size_t mode_call_counts[ 3 ];           // distribution of mode call count
    double wait;                            // how much we need to wait (in second), can be negative
    double duration_s;                      // expected distribution duration (in second)
}
Distribution;


// single job structure:
typedef struct {
    size_t remaining_call_counts;           // how many remaining calls to do
    size_t count_cycle;                     // the related count cycle of the job
}
Job;


// single focus point:
typedef struct {
    unsigned normalized_position;           // normalized focus position captured when setting the point
    unsigned distance_cm;                   // focus distance captured when setting the point
    double duration_s;                      // target duration (second)
    Distribution distribution;              // related distribution
}
Focus_point;


// maximum length of the lens name:
#define LENS_NAME_MAX_LENGTH 32

// maximum focus positions on the lens:
#define MAX_FOCUS_POSITIONS 1024


// data to be stored on disc:
typedef struct
{
    char lens_name[ LENS_NAME_MAX_LENGTH ]; // lens name
    vector /*<unsigned>*/ focus_positions;  // focus positions
    Mode modes[ 3 ];                        // modes setup
    vector /*<Focus_point>*/ focus_points;  // the focus point sequence
}
Store;


// lens limits:
typedef struct
{
    int first_focus_position;               // first known focus position
    int focus_position_normalizer;          // focus position normalizer
}
Limits;


// main data structure:
typedef struct
{
    bool task_running;                      // is task currently running?
    bool screen_on;                         // is the LV screen on or off? (screen saving)
    size_t index;                           // current index in the focus point sequence
    Limits lens_limits;                     // lens limits
    Display display;                        // display data
    Store store;                            // data store
}
Data;


// log something in the console:
void Log( const char * _fmt, ... );


// display buffer max content length:
#define CONTENT_LENGTH 31


// maximum overlay line length:
// "[----] [99] /99 | [9999cm] | [9.999s]"
#define OVERLAY_MAX_LINE_LENGTH 38

// print a fixed sized bitmap with text data on LV screen:
void Print_bmp( const char * _fmt, ... );


// duration (ms) of the highlighting after value edit:
#define HIGHLIGTH_DURATION_MS 1000

// focus distance trigger to display an infinite focus:
#define INFINITE_FOCUS_DISTANCE_TRIGGER 65535

// overlay positions in LV screen:
#define OVERLAY_POSITION_X 0 // sticked to left
#define OVERLAY_POSITION_Y 40 // 40px offset from LV screen top

// compute the specific edit/play "content" part, to be displayed in overlays:
char * Print_overlay__edit_play_content( const size_t _cycle );

// compute the "content" part to be displayed in overlays:
char * Print_overlay__content( const size_t _cycle );

// print the LV overlay with content:
// note: cycle value is a simple increment used for hightlights
void Print_overlay( const size_t _cycle );

// overlay drawing camera task:
void Overlay_task();


// name of the configuration file:
#define FOCUS_SQ_SETTINGS_FILE "ML/SETTINGS/focus_sq.cfg"

// save configuration file:
void Save_data_store();

// load configuraiton file:
void Load_data_store();


// step number to apply to reach a position asap:
#define ASAP_STEP_NUMBER 100

// reach lens limit to compute first position and normalizer value:
void Set_focus_position_normalizer();

// return the current focus position as normalized:
unsigned Normalized_focus_position();

// compute the closest step for a given normalized position:
size_t Closest_step_from_position( const unsigned _focus_position );


// perform lens calibration:
void Calibrate_lens();

// helper to dump the focus positions:
void Dump_focus_position_range( const size_t _step_0, const size_t _step_1 );

// dump the focus positions after calibration:
void Dump_focus_positions();

// evaluate step size lens rotation steps depending of the mode:
void Evaluate_step_size( const size_t _mode );

// evaluate step size lens rotation speeds:
void Evaluate_step_size_speed( const bool _forward, const size_t _mode );


// currently the wait_for_stabilized_focus_position() took 200ms:
#define STABILIZATION_DURATION 0.2

// compute a distribution between two given focus positions:
Distribution Compute_distribution_between( const unsigned _focus_position_1, const unsigned _focus_position_2, const double _duration_s, int * _p_range );

// compute a distribution of the modes to cover a given range as close as possible as a given duration in second:
Distribution Distribute_modes( const size_t _step_range, const double _target_duration_s );

// compute the expected duration of a given duration:
double Distribution_duration( const Distribution * const  _p_distribution );

// play a given distribution:
void Play_distribution( const Distribution * const _p_distribution, const bool _forward );


// go to a position by playing a computed distribution:
void Go_to();

// go to a position as fast as possible (live distribution computation):
void Go_to_asap();


// toggle between focus sequencing module modes:
void Action__toggle_mode();

// (de)activate the camera screen (screen/battery saver):
void Action__toggle_camera_display();


// how much seconds did we increment (or decrement) the target duration value:
#define FOCUS_POINT_INCREMENT_S 0.1

// add a focus point in the sequence:
void Action__edit__add_focus_point();

// update the lens information of the current point of the sequence:
void Action__edit__set_current_lens_information();

// remove the current focus point in the sequence:
void Action__edit__remove_focus_point();

// go to the next focus point in the sequence:
void Action__edit__go_to_next_focus_point();

// go to the previous focus point in the sequence:
void Action__edit__go_to_previous_focus_point();

// common transition speed update function of the current focus point in the sequence:
void Action__edit__update_transition_speed( const bool _increase );

// increase the transition speed of the current focus point in the sequence:
void Action__edit__increase_transition_speed();

// decrease the transition speed of the current focus point in the sequence:
void Action__edit__decrease_transition_speed();


// go to the next focus point in the sequence, taking in account the sequence settings:
void Action__play__go_to_next_focus_point();

// go to the first point of the sequence as fast as possible:
void Action__play__go_fast_to_first_focus_point();


// maximum camera buttons we can affect:
#define MAX_KEY_MAP 38

// manage camera buttons depending of the configuration:
unsigned int Key_handler( const unsigned int _key );

// init focus sequencing module:
unsigned int Init();

// de-init focus sequencing module:
unsigned int Deinit();


#endif /*__FOCUS_SQ_H__*/