static char __module_string_a_name [] MODULE_STRINGS_SECTION = "Name";
static char __module_string_a_value[] MODULE_STRINGS_SECTION = "Crop mode recording";
static char __module_string_b_name [] MODULE_STRINGS_SECTION = "Author";
static char __module_string_b_value[] MODULE_STRINGS_SECTION = "a1ex";
static char __module_string_c_name [] MODULE_STRINGS_SECTION = "License";
static char __module_string_c_value[] MODULE_STRINGS_SECTION = "GPL";
static char __module_string_d_name [] MODULE_STRINGS_SECTION = "Summary";
static char __module_string_d_value[] MODULE_STRINGS_SECTION = "Turn the 1080p and 720p video modes into 1:1 sensor crop modes";
static char __module_string_e_name [] MODULE_STRINGS_SECTION = "Description";
static char __module_string_e_value[] MODULE_STRINGS_SECTION = 
    "This alters the 1080p and 720p video modes, transforming them\n"
    "into 3x (1:1) crop modes, by tweaking the sensor registers.\n"
    "\n"
    "All other behaviors are the same as with Canon's 1080p/720p\n"
    "implementation: resolution, H.264, RAW, audio, HDMI, preview,\n"
    "overlays and so on.\n"
    "\n"
;
static char __module_string_f_name [] MODULE_STRINGS_SECTION = "Last update";
static char __module_string_f_value[] MODULE_STRINGS_SECTION = 
    "57cfa08 on 2021-04-01 08:16:41 UTC by Danne:\n"
    "crop_rec.c:(5D3 reducing 3.5 height by 10 to reduce corrupted fra...\n"
;
static char __module_string_g_name [] MODULE_STRINGS_SECTION = "Build date";
static char __module_string_g_value[] MODULE_STRINGS_SECTION = "2022-02-16 05:56:36 UTC";
static char __module_string_h_name [] MODULE_STRINGS_SECTION = "Build user";
static char __module_string_h_value[] MODULE_STRINGS_SECTION = 
    "daniel@Daniels-MacBook-Pro.local\n"
    "\n"
;

#define MODULE_STRINGS() \
  MODULE_STRINGS_START() \
    MODULE_STRING(__module_string_a_name, __module_string_a_value) \
    MODULE_STRING(__module_string_b_name, __module_string_b_value) \
    MODULE_STRING(__module_string_c_name, __module_string_c_value) \
    MODULE_STRING(__module_string_d_name, __module_string_d_value) \
    MODULE_STRING(__module_string_e_name, __module_string_e_value) \
    MODULE_STRING(__module_string_f_name, __module_string_f_value) \
    MODULE_STRING(__module_string_g_name, __module_string_g_value) \
    MODULE_STRING(__module_string_h_name, __module_string_h_value) \
  MODULE_STRINGS_END()
