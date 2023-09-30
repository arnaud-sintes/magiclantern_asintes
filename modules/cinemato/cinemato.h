// maximum lens move speed value
#define LENS_MOVE_SPEED_COUNT 4

// speed of lens movement:
#define LENS_MOVE_SPEED__VERY_SLOW 0
#define LENS_MOVE_SPEED__SLOW 1
#define LENS_MOVE_SPEED__MEDIUM 2
#define LENS_MOVE_SPEED__FAST 3

// maximum length of the lens name:
#define LENS_NAME_MAX_LENGTH 32

// maximum number of focus position steps:
#define FOCUS_POSITION_MAX_STEPS 1024

// maxium number of step size:
#define STEP_SIZE_COUNT 3

// step sizes:
#define STEP_SIZE_1 0
#define STEP_SIZE_2 1
#define STEP_SIZE_3 2

// maximum 100 focus points:
#define FOCUS_POINT_MAX_COUNT 100


typedef struct
{
	// focus step in the normalized focus position list:
	unsigned focus_step;
	
	// focus distance in cm:
	unsigned focus_distance_cm;
	
	// speed index of the transition to this point:
	unsigned transition_speed;
}
focus_point;


typedef struct
{
	// lens name:
	char lens_name[ LENS_NAME_MAX_LENGTH ];
	
	// calibrated focus position step count:
	size_t focus_position_steps;
	 
	// normalized focus position for each step after calibration:
	unsigned focus_positions[ FOCUS_POSITION_MAX_STEPS ];
	
	// how much step reached per step size:
	size_t step_size_steps[ STEP_SIZE_COUNT ];
	
	// step size speeds (in steps per second):
	double step_size_speeds[ STEP_SIZE_COUNT ];
	
	// number of focus points:
	size_t focus_point_count;
	
	// focus point list:
	focus_point focus_points[ FOCUS_POINT_MAX_COUNT ];
}
data_store;


typedef struct
{
    // is task currently running?
    bool task_running;
	
	// is cinematographer hook activated?
	bool hook_activated;
	
	// are we in play or edit mode?
	bool play_mode;
	
	// are we calibrating the lens?
	bool lens_calibrating;
	
	// are we normalizing the focus position?
	bool focus_position_normalizing;
	
	// first known focus position:
	int first_focus_position;
	
	// focus position normalization:
	int focus_position_normalizer;
	
	// does the LV display activated?
	bool display_activated;
	
	// focus point index:
	size_t focus_point_index;
	
	// timepoint (ms) of a potential focus step edition:
	int edit_focus_step_timepoint_ms;
	
	// timepoint (ms) of a potential transition speed edition:
	int edit_transition_speed_timepoint_ms;
	
	// data store:
	data_store store;
}
data;


// global data values:
static data g_data = {
    .task_running = false,
	.hook_activated = false,
	.play_mode = false,
	.lens_calibrating = false,
	.focus_position_normalizing = false,
	.first_focus_position = 0,
	.focus_position_normalizer = 0,
	.display_activated = true,
	.focus_point_index = 0,
	.edit_focus_step_timepoint_ms = 0,
	.edit_transition_speed_timepoint_ms = 0,
	.store = {
		.focus_position_steps = 0,
		.focus_point_count = 0
	}
};


// maximum double conversion length:
#define MAX_DOUBLE_CONVERSION_LENGTH 16


// maximum number of overlay header values:
#define OVERLAY_HEADER_VALUE_COUNT 4

// overlay header possible values:
#define OVERLAY_HEADER_DEACTIVATED 0
#define OVERLAY_HEADER_EDIT 1
#define OVERLAY_HEADER_PLAY 2
#define OVERLAY_HEADER_WORKING 3

// maximum length of overlay header content:
#define OVERLAY_HEADER_MAX_LENGTH 5


// value representing an infinite focus distance:
#define	INFINITE_FOCUS_DISTANCE 65535


// very slow sleep duration in milliseconds:
#define VERY_SLOW_SLEEP_DURATION_MS 10


// maximum length of the focus distance string:
#define MAX_FOCUS_DISTANCE_LENGTH 8

// maximum lenght of the overlay trailer:
#define MAX_TRAILER_LENGTH 16

// highlight edition during 1s:
#define EDITION_HIGHLIGHT_MS 1000


// step number to apply to reach a position asap:
#define ASAP_STEP_NUMBER 100



// settings file name:
#define CINEMATO_SETTINGS_FILE "ML/SETTINGS/cinemato.cfg"

void save_data_store();
void load_data_store();



// maximum line length:
// "[edit] {{9999cm}} #99 /99 -> *99.999s*"
#define OVERLAY_MAX_LINE_LENGTH 40

// overlay positions in LV screen:
#define OVERLAY_POSITION_X 0 // sticked to left
#define OVERLAY_POSITION_Y 40 // 40px offset from LV screen top

void c_print( const char * _fmt, ... );

// printf doesn't seems to be able to dump float value, so here's a function that splits
// a float value to a "x.y" string, with 3 numbers after the dot:
char * format_float( const double _value, char * _buffer, const size_t _buffer_len );

const char * overlay_header( const int _overlay_header_type );

char * interpret_focus_distance( const unsigned _focus_distance, char * _buffer, const size_t _buffer_len );

// interpret a transition duration in seconds between two given steps, depending of the transition speed
double interpret_transition_duration( const unsigned _step_source, const unsigned _step_destination, const unsigned _transition_speed );

unsigned normalized_focus_position();

size_t closest_step_from_position( const unsigned _focus_position );

void overlay_print();

void overlay_task();

void set_focus_position_normalizer();

void lens_go_to( const unsigned _target_step, const unsigned _speed );

void dump_focus_position_range( const size_t _step_0, const size_t _step_1 );

void dump_focus_positions();

size_t evaluate_step_size( const unsigned _step_size );

double evaluate_step_size_speed( const bool _forward, const unsigned _step_size, const size_t _step_size_steps );

void calibrate_lens();

void toggle_mode( const bool _play_mode );

void play__go_to_next_focus_point();
void play__go_fast_to_first_focus_point();
void play__toggle_camera_display();

void edit__add_focus_point();
void edit__remove_focus_point();
void edit__go_to_next_focus_point();
void edit__go_to_previous_focus_point();
void edit__set_current_lens_information();
void edit__increase_transition_speed();
void edit__decrease_transition_speed();

unsigned int key_handler( const unsigned int _key );

unsigned int init();
unsigned int deinit();