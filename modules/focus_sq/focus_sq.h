#ifndef __FOCUS_SQ_H__
#define __FOCUS_SQ_H__

#include <vector.h>

// maximum length of the lens name:
#define FSQ_LENS_NAME_MAX_LENGTH 32

// display buffer max content length:
#define FSQ_CONTENT_LENGTH 42

// maximum overlay line length:
// "[----] [99] /99 | [99.999m] | [9.999s]"
#define FSQ_OVERLAY_MAX_LINE_LENGTH 38

// maximum camera buttons we can affect:
#define FSQ_MAX_KEY_MAP 38

// constants
struct fsq_constants_t
{
    size_t max_focus_positions;                   // maximum focus positions on the lens
    int highlight_duration_ms;                    // duration (ms) of the highlighting after value edit
    unsigned int infinite_focus_distance_trigger; // focus distance trigger to display an infinite focus
    int overlay_position_x;                       // horizontal overlay positions in LV screen
    int overlay_position_y;                       // vertical overlay positions in LV screen
    char *const settings_file;                    // name of the configuration file
    unsigned asap_step_number;                    // step number to apply to reach a position asap
    double focus_point_increment_s;               // how much seconds did we increment (or decrement) the target duration value
};

// possible display states:
enum fsq_state
{
    fsq_inactive = 0,
    fsq_edit = 1,
    fsq_play = 2,
    fsq_checking_lens_limits = 3,
    fsq_calibrating_lens = 4
};

// information to be displayed in LV:
struct fsq_display_t
{
    char *const delimiters[2][5];       // generic list of delimiters used in highlights
    int state_edited_timepoint_ms;      // if not 0, the state was just edited
    size_t state;                       // current display state
    int transition_target_ms;           // if not 0, indicate the transition end timepoint in ms
    int sequence_edited_timepoint_ms;   // if not 0, the sequence was just edited
    size_t sequence_index;              // current/target sequence index
    size_t sequence_length;             // sequence length
    int focus_edited_timepoint_ms;      // if not 0, the focus position in the sequence was just edited
    unsigned normalized_focus_position; // normalized focus position as set in the sequence
    unsigned focus_distance_cm;         // focus position as set in the sequence
    int duration_edited_timepoint_ms;   // if not 0, the transition duration in the sequence was just edited
    double target_duration_s;           // target duration (second) as set in the sequence
    double duration_s;                  // related expected duration (second)
};

// single mode configuration:
struct fsq_step_mode_t
{
    size_t steps; // fixed step count taken in a single 'do' call
    double speed; // mode speed in steps per second
};

// possible mode indexes:
enum fsq_mode
{
    fsq_mode_3 = 0, // step_size = 3
    fsq_mode_2 = 1, // step_size = 2
    fsq_mode_1 = 2, // step_size = 1
};

// computed mode call count distribution:
struct fsq_distribution_t
{
    size_t mode_call_counts[3]; // distribution of mode call count
    double wait;                // how much we need to wait (in second), can be negative
    double duration_s;          // expected distribution duration (in second)
};

// single job structure:
struct fsq_job_t
{
    size_t remaining_call_counts; // how many remaining calls to do
    size_t count_cycle;           // the related count cycle of the job
};

// single focus point:
struct fsq_focus_point_t
{
    unsigned normalized_position;           // normalized focus position captured when setting the point
    unsigned distance_cm;                   // focus distance captured when setting the point
    double duration_s;                      // target duration (second)
    struct fsq_distribution_t distribution; // related distribution
};

// data to be stored on disc:
struct fsq_store_t
{
    char lens_name[FSQ_LENS_NAME_MAX_LENGTH]; // lens name
    vector /*<unsigned>*/ focus_positions;    // focus positions
    struct fsq_step_mode_t modes[3];          // modes setup
    vector /*<focus_point_t>*/ focus_points;  // the focus point sequence
};

// lens limits:
struct fsq_limits_t
{
    int first_focus_position;      // first known focus position
    int focus_position_normalizer; // focus position normalizer
};

// main focus sequencing data structure:
struct fsq_data_t
{
    struct fsq_constants_t constants; // constant values
    bool task_running;                // is task currently running?
    bool screen_on;                   // is the LV screen on or off? (screen saving)
    size_t index;                     // current index in the focus point sequence
    struct fsq_limits_t lens_limits;  // lens limits
    struct fsq_display_t display;     // display data
    struct fsq_store_t store;         // data store
};

// log something in the console:
void fsq_log(char *_fmt, ...);

// print a fixed sized bitmap with text data on LV screen:
void fsq_print_bmp(char *_fmt, ...);

// compute the specific edit/play "content" part, to be displayed in overlays:
char *fsq_print_overlay__edit_play_content(size_t _cycle);

// compute the "content" part to be displayed in overlays:
char *fsq_print_overlay__content(size_t _cycle);

// print the LV overlay with content:
// note: cycle value is a simple increment used for hightlights
void fsq_print_overlay(size_t _cycle);

// overlay drawing camera task:
void fsq_overlay_task();

// save configuration file:
void fsq_save_data_store();

// load configuraiton file:
void fsq_load_data_store();

// reach lens limit to compute first position and normalizer value:
void fsq_set_focus_position_normalizer();

// return the current focus position as normalized:
unsigned fsq_normalized_focus_position();

// compute the closest step for a given normalized position:
size_t fsq_closest_step_from_position(unsigned _focus_position);

// perform lens calibration:
void fsq_calibrate_lens();

// helper to dump the focus positions:
void fsq_dump_focus_position_range(size_t _step_0, size_t _step_1);

// dump the focus positions after calibration:
void fsq_dump_focus_positions();

// evaluate step size lens rotation steps depending of the mode:
void fsq_evaluate_step_size(size_t _mode);

// evaluate step size lens rotation speeds:
void fsq_evaluate_step_size_speed(bool _forward, size_t _mode);

// compute a distribution between two given focus positions:
struct fsq_distribution_t fsq_compute_distribution_between(unsigned _focus_position_1,
                                                           unsigned _focus_position_2, double _duration_s, int *_p_range);

// compute a distribution of the modes to cover a given range as close as possible as a given duration in second:
struct fsq_distribution_t fsq_distribute_modes(size_t _step_range, double _target_duration_s);

// compute the expected duration of a given duration:
double fsq_distribution_duration(struct fsq_distribution_t *const _p_distribution);

// play a given distribution:
void fsq_play_distribution(struct fsq_distribution_t *const _p_distribution, bool _forward);

// go to a position by playing a computed distribution:
void fsq_go_to();

// go to a position as fast as possible (live distribution computation):
void fsq_go_to_asap();

// toggle between focus sequencing module modes:
void fsq_action__toggle_mode();

// (de)activate the camera screen (screen/battery saver):
void fsq_action__toggle_camera_display();

// add a focus point in the sequence:
void fsq_action__edit__add_focus_point();

// update the lens information of the current point of the sequence:
void fsq_action__edit__set_current_lens_information();

// remove the current focus point in the sequence:
void fsq_action__edit__remove_focus_point();

// go to the next focus point in the sequence:
void fsq_action__edit__go_to_next_focus_point();

// go to the previous focus point in the sequence:
void fsq_action__edit__go_to_previous_focus_point();

// common transition speed update function of the current focus point in the sequence:
void fsq_action__edit__update_transition_speed(bool _increase);

// increase the transition speed of the current focus point in the sequence:
void fsq_action__edit__increase_transition_speed();

// decrease the transition speed of the current focus point in the sequence:
void fsq_action__edit__decrease_transition_speed();

// go to the next focus point in the sequence, taking in account the sequence settings:
void fsq_action__play__go_to_next_focus_point();

// go to the first point of the sequence as fast as possible:
void fsq_action__play__go_fast_to_first_focus_point();

// manage camera buttons depending of the configuration:
unsigned int fsq_key_handler(unsigned int _key);

// init focus sequencing module:
unsigned int fsq_init();

// de-init focus sequencing module:
unsigned int fsq_deinit();

#endif /*__FOCUS_SQ_H__*/