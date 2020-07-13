/**
 * Experimental SD UHS overclocking.
 */

#include <module.h>
#include <dryos.h>
#include <patch.h>
#include <console.h>
#include <config.h>

#include <lens.h>

/* camera-specific parameters */
static int valid = 0;
static uint32_t sd_enable_18V = 0;
static uint32_t sd_setup_mode = 0;
static uint32_t sd_setup_mode_in = 0;
static uint32_t sd_setup_mode_reg = 0xFFFFFFFF;
static uint32_t sd_set_function = 0;

static uint32_t uhs_regs[]     = { 0xC0400600, 0xC0400604,/*C0400608, C040060C*/0xC0400610, 0xC0400614, 0xC0400618, 0xC0400624, 0xC0400628, 0xC040061C, 0xC0400620 };   /* register addresses */
static uint32_t sdr50_700D[]   = {        0x3,        0x3,                             0x4, 0x1D000301,        0x0,      0x201,      0x201,      0x100,        0x4 };   /* SDR50 values from 700D (96MHz) */
static uint32_t sdr_160MHz[]   = {        0x2,        0x3,                             0x1, 0x1D000001,        0x0,      0x100,      0x100,      0x100,        0x1 };   /* overclocked values: 160MHz = 96*(4+1)/(2?+1) (found by brute-forcing) */
static uint32_t sdr_192MHz[]   = {        0x8,        0x3,                             0x4, 0x1D000301,        0x0,      0x201,      0x201,      0x100,        0x4 };
static uint32_t sdr_240MHz[]   = {        0x8,        0x3,                             0x3, 0x1D000301,        0x0,      0x201,      0x201,      0x100,        0x3 };
//static uint32_t sdr_160MHz[]   = {        0x3,        0x2,                             0x1, 0x1D000001,        0x0,      0x100,      0x101,      0x101,        0x1 }; /* possible also */
// 5D3 mode 0                                                                          17F  0x1D004101           0        403F        403F          7F          7F
// 5D3 mode 1 16MHz                         3           3          1          1         1D  0x1D001001           0       0xF0E       0xF0E          1D          1D
// 5D3 mode 2 24MHz                         3           3          1          1         13  0x1D000B01           0       0xA09       0xA09          13          13
// 5D3 mode 3 48MHz                         3           3          1          1          9  0x1D000601           0       0x504       0x504       0x100           9
// 700D mode 4 96MHz SDR50                  3           3          1          1          4  0x1D000301           0       0x201       0x201       0x100           4
// 5D3 mode 5 serial flash?                 3                                            7  0x1D000501           0       0x403       0x403       0x403           7
// 5D3 mode 6 (bench 5.5MB/s)               7                                           13  0x1D000B01           0       0xA09       0xA09          13          13

static uint32_t uhs_vals[COUNT(uhs_regs)];  /* current values */
static int sd_setup_mode_enable = 0;
static int turned_on = 0;
static CONFIG_INT("sd.sd_overclock", sd_overclock, 0);

/* start of the function */
static void sd_setup_mode_log(uint32_t* regs, uint32_t* stack, uint32_t pc)
{
    qprintf("sd_setup_mode(dev=%x)\n", regs[0]);
    
    /* this function is also used for other interfaces, such as serial flash */
    /* only enable overriding when called with dev=1 */
    sd_setup_mode_enable = (regs[0] == 1);
}

/* called right before the case switch in sd_setup_mode (not at the start of the function!) */
static void sd_setup_mode_in_log(uint32_t* regs, uint32_t* stack, uint32_t pc)
{
    qprintf("sd_setup_mode switch(mode=%x) en=%d\n", regs[sd_setup_mode_reg], sd_setup_mode_enable);
    
    if (sd_setup_mode_enable && regs[sd_setup_mode_reg] == 4)   /* SDR50? */
    {
        /* set our register overrides */
        for (int i = 0; i < COUNT(uhs_regs); i++)
        {
            MEM(uhs_regs[i]) = uhs_vals[i];
        }
        
        /* set some invalid mode to bypass the case switch
         * and keep our register values only */
        regs[sd_setup_mode_reg] = 0x13;
    }
}

static void sd_set_function_log(uint32_t* regs, uint32_t* stack, uint32_t pc)
{
    qprintf("sd_set_function(0x%x)\n", regs[0]);
    
    /* UHS-I SDR50? */
    if (regs[0] == 0xff0002)
    {
        /* force UHS-I SDR104 */
        regs[0] = 0xff0003;
    }
}

struct cf_device
{
    /* type b always reads from raw sectors */
    int (*read_block)(
                      struct cf_device * dev,
                      void * buf,
                      uintptr_t block,
                      size_t num_blocks
                      );
    
    int (*write_block)(
                       struct cf_device * dev,
                       const void * buf,
                       uintptr_t block,
                       size_t num_blocks
                       );
};

static void (*SD_ReConfiguration)() = 0;


static void sd_reset(struct cf_device * const dev)
{
    /* back to some safe values */
    memcpy(uhs_vals, sdr50_700D, sizeof(uhs_vals));
    
    /* clear error flag to allow activity after something went wrong */
    MEM((uintptr_t)dev + 80) = 0;
    
    /* re-initialize card */
    SD_ReConfiguration();
}

static void sd_overclock_task()
{
    if (valid)
    {
        NotifyBox(1000, "sd_uhs patching, wait...");
        msleep(1000);
    }
    if (!valid) msleep(2000);
    
    /* install the hack */
    memcpy(uhs_vals, sdr50_700D, sizeof(uhs_vals));
    if (sd_enable_18V)
    {
        patch_instruction(sd_enable_18V, 0xe3a00000, 0xe3a00001, "SD 1.8V");
    }
    patch_hook_function(sd_setup_mode, MEM(sd_setup_mode), sd_setup_mode_log, "SD UHS");
    patch_hook_function(sd_setup_mode_in, MEM(sd_setup_mode_in), sd_setup_mode_in_log, "SD UHS");
    
    /* power-cycle and reconfigure the SD card */
    SD_ReConfiguration();
    
    /* test some presets */
    
    /* enable SDR104 */
    patch_hook_function(sd_set_function, MEM(sd_set_function), sd_set_function_log, "SDR104");
    SD_ReConfiguration();
    
    if (sd_overclock == 1) memcpy(uhs_vals, sdr_160MHz, sizeof(uhs_vals));
    if (sd_overclock == 2) memcpy(uhs_vals, sdr_192MHz, sizeof(uhs_vals));
    if (sd_overclock == 3) memcpy(uhs_vals, sdr_240MHz, sizeof(uhs_vals));
    if (valid)
    {
        NotifyBox(1000, "patching done!");
        if (is_movie_mode()) menu_set_str_value_from_script("Movie", "RAW video", "ON", 1);
        msleep(100);
    }

}

static MENU_UPDATE_FUNC(sd_overclock_run)
{
    if (sd_overclock && !turned_on)
    {
        sd_overclock_task();
        turned_on = 1;
    }
}

static struct menu_entry sd_uhs_menu[] =
{
    {
        .name   = "SD overclock",
        .priv   = &sd_overclock,
        .max    = 1,
        .choices = CHOICES("OFF", "160MHz", "192MHz", "240MHz(feeling lucky)"),
        .help   = "Select a patch and restart camera. Disable with OFF and restart",
        .help2  = "Proven working with 95Mb/s and 170Mb/s cards",
    }
};

static struct menu_entry sd_uhs_menu1[] =
{
    {
        .name   = "SD overclock",
        .priv   = &sd_overclock,
        .max    = 1,
        .choices = CHOICES("OFF", "160MHz", "192MHz", "240MHz (feeling lucky)"),
        .help   = "Select a patch and restart camera. Disable with OFF and restart",
        .help2  = "Proven working with 95Mb/s and 170Mb/s cards",
    }
};

static unsigned int sd_uhs_init()
{
    //needed with manual lenses cause it stalls liveview.
    while (is_movie_mode() && !lv)
    {
        msleep(100);
    }
    
    if (is_camera("5D3", "1.1.3"))
    {
        /* sd_setup_mode:
         * sdSendCommand: CMD%d  Retry=... ->
         * sd_configure_device(1) (called after a function without args) ->
         * sd_setup_mode(dev) if dev is 1 or 2 ->
         * logging hook is placed in the middle of sd_setup_mode_in (not at start)
         */
        sd_setup_mode       = 0xFF47B4C0;   /* start of the function; not strictly needed on 5D3 */
        sd_setup_mode_in    = 0xFF47B4EC;   /* after loading sd_mode in R0, before the switch */
        sd_setup_mode_reg   = 0;            /* switch variable is in R0 */
        sd_set_function     = 0xFF6ADE34;   /* sdSetFunction */
        sd_enable_18V       = 0xFF47B4B8;   /* 5D3 only (Set 1.8V Signaling) */
        SD_ReConfiguration  = (void *) 0xFF6AFF1C;
        if (sd_overclock)
        {
            if (!gui_menu_shown() )
            {
                valid = 1;
                menu_set_str_value_from_script("Movie", "RAW video", "OFF", 1);
                msleep(800);
            }

            sd_overclock_task();
            turned_on = 1;
        }
    }
    
    if (is_camera("5D3", "1.2.3"))
    {
        sd_setup_mode       = 0xFF484474;
        sd_setup_mode_in    = 0xFF4844A0;
        sd_setup_mode_reg   = 0;
        sd_set_function     = 0xFF6B8FD0;
        sd_enable_18V       = 0xFF48446C;   /* 5D3 only (Set 1.8V Signaling) */
        SD_ReConfiguration  = (void *) 0xFF6BB0B8;
        if (sd_overclock)
        {
            if (!gui_menu_shown())
            {
                valid = 1;
                menu_set_str_value_from_script("Movie", "RAW video", "OFF", 1);
                msleep(800);
            }
            sd_overclock_task();
            turned_on = 1;
        }
    }
    
    /* stubs present? create the menu */
    if (is_dir("B:/"))
    {
        menu_add("Debug", sd_uhs_menu, COUNT(sd_uhs_menu));
        menu_add("Movie", sd_uhs_menu1, COUNT(sd_uhs_menu1));
    }
    
    return 0;
}

static unsigned int sd_uhs_deinit()
{
    return 0;
}

MODULE_INFO_START()
MODULE_INIT(sd_uhs_init)
MODULE_DEINIT(sd_uhs_deinit)
MODULE_INFO_END()

MODULE_CONFIGS_START()
MODULE_CONFIG(sd_overclock)
MODULE_CONFIGS_END()


