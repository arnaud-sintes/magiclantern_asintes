static char __module_string_a_name [] MODULE_STRINGS_SECTION = "Name";
static char __module_string_a_value[] MODULE_STRINGS_SECTION = "File Manager";
static char __module_string_b_name [] MODULE_STRINGS_SECTION = "Authors";
static char __module_string_b_value[] MODULE_STRINGS_SECTION = "ML devs";
static char __module_string_c_name [] MODULE_STRINGS_SECTION = "License";
static char __module_string_c_value[] MODULE_STRINGS_SECTION = "GPL";
static char __module_string_d_name [] MODULE_STRINGS_SECTION = "Summary";
static char __module_string_d_value[] MODULE_STRINGS_SECTION = "File manager (browse the files on the card)";
static char __module_string_e_name [] MODULE_STRINGS_SECTION = "Forum";
static char __module_string_e_value[] MODULE_STRINGS_SECTION = "http://www.magiclantern.fm/forum/index.php?topic=5522.0";
static char __module_string_f_name [] MODULE_STRINGS_SECTION = "Description";
static char __module_string_f_value[] MODULE_STRINGS_SECTION = 
    "Browse the card filesystem from ML menu.\n"
    "\n"
    "Features:\n"
    "\n"
    " *  Display file name, size and date\n"
    " *  Select files (individual or by extension)\n"
    " *  Copy and move files\n"
    " *  Delete files\n"
    " *  View files (text viewer built-in, other file types via custom\n"
    "    handlers)\n"
    "\n"
;
static char __module_string_g_name [] MODULE_STRINGS_SECTION = "Last update";
static char __module_string_g_value[] MODULE_STRINGS_SECTION = 
    "bcbabb3 on 2018-02-17 20:28:28 UTC by alex:\n"
    "Moved timer functions to timer.h (always included from dryos.h)\n"
;
static char __module_string_h_name [] MODULE_STRINGS_SECTION = "Build date";
static char __module_string_h_value[] MODULE_STRINGS_SECTION = "2023-02-02 22:07:09 UTC";
static char __module_string_i_name [] MODULE_STRINGS_SECTION = "Build user";
static char __module_string_i_value[] MODULE_STRINGS_SECTION = 
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
    MODULE_STRING(__module_string_i_name, __module_string_i_value) \
  MODULE_STRINGS_END()