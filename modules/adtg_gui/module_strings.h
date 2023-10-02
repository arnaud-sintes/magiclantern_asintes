static char __module_string_a_name [] MODULE_STRINGS_SECTION = "Name";
static char __module_string_a_value[] MODULE_STRINGS_SECTION = "ADTG register GUI";
static char __module_string_b_name [] MODULE_STRINGS_SECTION = "License";
static char __module_string_b_value[] MODULE_STRINGS_SECTION = "GPL";
static char __module_string_c_name [] MODULE_STRINGS_SECTION = "Author";
static char __module_string_c_value[] MODULE_STRINGS_SECTION = "a1ex";
static char __module_string_d_name [] MODULE_STRINGS_SECTION = "Credits";
static char __module_string_d_value[] MODULE_STRINGS_SECTION = "g3gg0 (adtg_log)";
static char __module_string_e_name [] MODULE_STRINGS_SECTION = "Summary";
static char __module_string_e_value[] MODULE_STRINGS_SECTION = "ADTG/CMOS register editing GUI (reverse engineering tool)";
static char __module_string_f_name [] MODULE_STRINGS_SECTION = "Forum";
static char __module_string_f_value[] MODULE_STRINGS_SECTION = "http://www.magiclantern.fm/forum/index.php?topic=6751.msg71720#msg71720";
static char __module_string_g_name [] MODULE_STRINGS_SECTION = "Description";
static char __module_string_g_value[] MODULE_STRINGS_SECTION = 
    "ADTG and CMOS register editing GUI.\n"
    "\n"
    " **Warning: this is not a toy; it can destroy your sensor. **\n"
    "\n"
    "This is a tool for reverse engineering the meaning of\n"
    "ADTG/CMOS registers (low-level sensor control).\n"
    "\n"
    "\n"
;
static char __module_string_h_name [] MODULE_STRINGS_SECTION = "Help page 1";
static char __module_string_h_value[] MODULE_STRINGS_SECTION = 
    "Usage 1/3\n"
    "\n"
    "To use it, you need to set CONFIG_GDB=y in Makefile.user. There\n"
    "are no binaries available (and shouldn't be, for safety reasons).\n"
    "\n"
    "All intercepted registers are displayed after Canon code touches\n"
    "them:\n"
    "\n"
    " *  [photo mode] first enable logging - simply open the ADTG\n"
    "    registers menu - then take a picture, for example, then look in\n"
    "    menu again\n"
    " *  [LiveView] some registers are updated continuously, but there\n"
    "    are a lot more that are updated when changing video modes or\n"
    "    when going in and out of LiveView (so, to see everything, first\n"
    "    open ADTG menu to enable logging, then go to LiveView, then\n"
    "    look in menu again)\n"
    "\n"
    "\n"
;
static char __module_string_i_name [] MODULE_STRINGS_SECTION = "Help page 2";
static char __module_string_i_value[] MODULE_STRINGS_SECTION = 
    "Usage 2/3\n"
    "\n"
    "For registers that we have some idea about what they do, it\n"
    "displays a short description:\n"
    "\n"
    " *  you can add help lines if you understand some more registers\n"
    "\n"
    "You can display diffs:\n"
    "\n"
    " *  e.g. enable logging, take a pic, select \"show modified registers\",\n"
    "    change ISO, take another pic, then look in the menu\n"
    "\n"
    "\n"
;
static char __module_string_j_name [] MODULE_STRINGS_SECTION = "Help page 3";
static char __module_string_j_value[] MODULE_STRINGS_SECTION = 
    "Usage 3/3\n"
    "\n"
    "You can override any register:\n"
    "\n"
    " *  if you don't have dual ISO yet on your camera, just change\n"
    "    CMOS[0] manually, then take pics ;)\n"
    " *  you can find some funky crop modes (e.g. if you change the line\n"
    "    skipping factor)\n"
    " *  now it's easier than ever to kill your sensor for science\n"
    "\n"
    "If in doubt, **take the battery out. Quickly!** (well, that's what I\n"
    "do)\n"
    "\n"
    "Tip: some registers use NRZI values (they are displayed with a N),\n"
    "others use normal values. If the value doesn't make sense (e.g.\n"
    "something affects brightness, but it seems kinda random, not\n"
    "gradual changes), try flipping the is_nrzi flag from known_regs.\n"
    "You can't do it from the menu yet.\n"
    "\n"
;
static char __module_string_k_name [] MODULE_STRINGS_SECTION = "Last update";
static char __module_string_k_value[] MODULE_STRINGS_SECTION = 
    "17bf504 on 2019-02-26 12:26:04 UTC by dudek53:\n"
    "add updated adtg_gui to the toolbox\n"
;
static char __module_string_l_name [] MODULE_STRINGS_SECTION = "Build date";
static char __module_string_l_value[] MODULE_STRINGS_SECTION = "2023-02-02 22:07:13 UTC";
static char __module_string_m_name [] MODULE_STRINGS_SECTION = "Build user";
static char __module_string_m_value[] MODULE_STRINGS_SECTION = 
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
    MODULE_STRING(__module_string_j_name, __module_string_j_value) \
    MODULE_STRING(__module_string_k_name, __module_string_k_value) \
    MODULE_STRING(__module_string_l_name, __module_string_l_value) \
    MODULE_STRING(__module_string_m_name, __module_string_m_value) \
  MODULE_STRINGS_END()