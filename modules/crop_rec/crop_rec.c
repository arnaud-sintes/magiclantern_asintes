#include <dryos.h>
#include <module.h>
#include <config.h>
#include <menu.h>
#include <beep.h>
#include <property.h>
#include <patch.h>
#include <bmp.h>
#include <lvinfo.h>
#include <powersave.h>
#include <raw.h>
#include <fps.h>
#include <shoot.h>
#include <lens.h>

#undef CROP_DEBUG

#ifdef CROP_DEBUG
#define dbg_printf(fmt,...) { printf(fmt, ## __VA_ARGS__); }
#else
#define dbg_printf(fmt,...) {}
#endif

static int is_digic4 = 0;
static int is_digic5 = 0;
static int is_5D3 = 0;

//Not static so we can get info in mlv_lite regarding crop rec on or off
CONFIG_INT("crop.preset", crop_preset_index, 1);
static CONFIG_INT("crop.shutter_range", shutter_range, 0);
static CONFIG_INT("crop.ratios", ratios, 1);
static CONFIG_INT("crop.x3crop", x3crop, 0);
static CONFIG_INT("crop.zoomaid", zoomaid, 0);
static CONFIG_INT("crop.set_25fps", set_25fps, 0);
static CONFIG_INT("crop.framestop", framestop, 0);
static CONFIG_INT("crop.frameburst", frameburst, 0);
static CONFIG_INT("crop.isoaverage", isoaverage, 0);
static CONFIG_INT("crop.isoclimb", isoclimb, 0);
static CONFIG_INT("crop.presets", presets, 0);

CONFIG_INT("crop.isoauto", isoauto, 0); /* to be read in mlv_lite.c */
CONFIG_INT("crop.bitdepth", bitdepth, 4); /* non static or it can´t be read in mlv_lite.c. See definitions bottom of raw.h */
#define OUTPUT_8BIT (bitdepth == 1)
#define OUTPUT_9BIT (bitdepth == 2)
#define OUTPUT_10BIT (bitdepth == 3)
#define OUTPUT_12BIT (bitdepth == 4)

enum crop_preset {
    CROP_PRESET_OFF = 0,
    CROP_PRESET_3X,
    CROP_PRESET_33K,
    CROP_PRESET_3X_TALL,
    CROP_PRESET_3K,
    CROP_PRESET_4K_HFPS,
    CROP_PRESET_UHD,
    CROP_PRESET_FULLRES_LV,
    CROP_PRESET_3x3_1X,
    CROP_PRESET_3x3_1X_45p,
    CROP_PRESET_3x3_1X_48p,
    CROP_PRESET_3x3_1X_50p,
    CROP_PRESET_3x3_1X_60p,
    CROP_PRESET_3x1,
    CROP_PRESET_40_FPS,
    CROP_PRESET_CENTER_Z,
    CROP_PRESET_mv1080_mv720p,
    CROP_PRESET_1x3,
    CROP_PRESET_anamorph_fullres,
    NUM_CROP_PRESETS
};

/* presets are not enabled right away (we need to go to play mode and back)
 * so we keep two variables: what's selected in menu and what's actually used.
 * note: the menu choices are camera-dependent */
static enum crop_preset crop_preset = 0;

/* must be assigned in crop_rec_init */
static enum crop_preset * crop_presets = 0;

/* current menu selection (*/
#define CROP_PRESET_MENU crop_presets[crop_preset_index]

/* menu choices for 5D3 */
static enum crop_preset crop_presets_5d3[] = {
    CROP_PRESET_OFF,
    CROP_PRESET_1x3,
    CROP_PRESET_3K,
    CROP_PRESET_33K,
    CROP_PRESET_3X,
    CROP_PRESET_CENTER_Z,
    CROP_PRESET_3x3_1X_45p,
    CROP_PRESET_3x3_1X_48p,
    CROP_PRESET_3x3_1X_50p,
    CROP_PRESET_3x3_1X_60p,
    CROP_PRESET_UHD,
    CROP_PRESET_4K_HFPS,
    CROP_PRESET_FULLRES_LV,
    CROP_PRESET_anamorph_fullres,
    CROP_PRESET_mv1080_mv720p,
    //CROP_PRESET_3X_TALL,
    //CROP_PRESET_1x3,
    //CROP_PRESET_3x1,
    //CROP_PRESET_40_FPS,
};

static const char * crop_choices_5d3[] = {
    "OFF",
    "anamorphic",
    "3K 1:1",
    "3.3K 1:1 x5",
    "1920 1:1",
    "3.5K 1:1 centered x5",
    "1080p 45fps",
    "1050p 48fps",
    "1920 50fps",
    "1920 60fps",
    "UHD 1:1",
    "4K 1:1 half-fps",
    "Full-res LiveView",
    "anamorphic fullres",
    "mv1080p 38fps",
    //"1920 1:1 tall",
    //"1x3 binning",
    //"3x1 binning",      /* doesn't work well */
    //"40 fps",
};

static const char crop_choices_help_5d3[] =
"Change 1080p and 720p movie modes into crop modes (select one)";

static const char crop_choices_help2_5d3[] =
"\n"
"1x3 binning anamorphic\n"
"1:1 3K crop (3072x1920 @ 24p, square raw pixels, preview broken)\n"
"1:1 3.3K crop (3264x1836 @ 24p, x5 cropped realtime preview)\n"
"1:1 sensor readout (square raw pixels, 3x crop, good preview in 1080p)\n"
"1:1 readout in x5 zoom mode (centered raw, high res, cropped preview)\n"
"1920x1080 @ 45p, 3x3 binning (50/60 FPS in Canon menu)\n"
"1920x1050 @ 48p, 3x3 binning (50/60 FPS in Canon menu)\n"
"1920x960 @ 50p, 3x3 binning (50/60 FPS in Canon menu)\n"
"1920x804 @ 60p, 3x3 binning (50/60 FPS in Canon menu)\n"
"1:1 4K UHD crop (3840x1600 @ 24p, square raw pixels, preview broken)\n"
"1:1 4K crop (4096x3072 @ 12.5 fps, half frame rate, preview broken)\n"
"Full resolution LiveView (5796x3870 @ 7.4 fps, 5784x3864, preview broken)\n"
"anamorphic fullres 1920x3760 (extreme anamorphic)\n"
"mv1080 regular real time preview\n"
"1x3 binning: read all lines, bin every 3 columns (extreme anamorphic)\n"
"1:1 crop, higher vertical resolution (1920x1920 @ 24p, cropped preview)\n"
//"3x1 binning: bin every 3 lines, read all columns (extreme anamorphic)\n"
"FPS override test\n";

/* camera-specific parameters */
static uint32_t CMOS_WRITE      = 0;
static uint32_t MEM_CMOS_WRITE  = 0;
static uint32_t ADTG_WRITE      = 0;
static uint32_t MEM_ADTG_WRITE  = 0;
static uint32_t ENGIO_WRITE     = 0;
static uint32_t MEM_ENGIO_WRITE = 0;

/* from SENSOR_TIMING_TABLE (fps-engio.c) or FPS override submenu */
static int fps_main_clock = 0;
static int default_timerA[11]; /* 1080p  1080p  1080p   720p   720p   zoom   crop   crop   crop   crop   crop */
static int default_timerB[11]; /*   24p    25p    30p    50p    60p     x5    24p    25p    30p    50p    60p */
static int default_fps_1k[11] = { 23976, 25000, 29970, 50000, 59940, 29970, 23976, 25000, 29970, 50000, 59940 };

/* video modes */
/* note: zoom mode is identified by checking registers directly */

static int is_1080p()
{
    /* note: on 5D2 and 5D3 (maybe also 6D, not sure),
     * sensor configuration in photo mode is identical to 1080p.
     * other cameras may be different */
    return !is_movie_mode() || video_mode_resolution == 0;
}

static int is_720p()
{
    return is_movie_mode() && video_mode_resolution == 1;
}

static int is_supported_mode()
{
    if (!lv) return 0;
    
    /* no more crashes when selecing photo mode */
    if (!is_movie_mode() || CROP_PRESET_MENU == CROP_PRESET_OFF) return 0;
    
    /* workaround getting below cams working with focus aid */
    static int last_hs_aid = 0;
    if (!get_halfshutter_pressed()) last_hs_aid = get_ms_clock();
    if (get_ms_clock() - last_hs_aid > 300 && get_halfshutter_pressed() && is_5D3 && !RECORDING && CROP_PRESET_MENU != CROP_PRESET_33K && (zoomaid == 0x1 || zoomaid == 0x2)) return 0;
    
    switch (crop_preset)
    {
            /* note: zoom check is also covered by check_cmos_vidmode */
            /* (we need to apply CMOS settings before PROP_LV_DISPSIZE fires) */
        case CROP_PRESET_CENTER_Z:
        case CROP_PRESET_33K:
            return 1;
            break;
            
        default:
            return is_1080p() || is_720p();
    }
}

static int32_t  target_yres = 0;
// static int32_t  delta_adtg0 = 0;
// static int32_t  delta_adtg1 = 0;
static int32_t  delta_head3 = 0;
static int32_t  delta_head4 = 0;
static int32_t  reg_713c = 0;
static int32_t  reg_7150 = 0;
static int32_t  reg_6014 = 0;
static int32_t  reg_6008 = 0;
static int32_t  reg_800c = 0;
static int32_t  reg_8000 = 0;
static int32_t  reg_8183 = 0;
static int32_t  reg_8184 = 0;
static int32_t  reg_timing1 = 0;
static int32_t  reg_timing2 = 0;
static int32_t  reg_timing3 = 0;
static int32_t  reg_timing4 = 0;
static int32_t  reg_timing5 = 0;
static int32_t  reg_timing6 = 0;
static int32_t  reg_6824 = 0;
static int32_t  reg_6800_height = 0;
static int32_t  reg_6800_width = 0;
static int32_t  reg_6804_height = 0;
static int32_t  reg_6804_width = 0;
static int32_t  reg_83d4 = 0;
static int32_t  reg_83dc = 0;
static int32_t  reg_8024 = 0;
static uint32_t cmos1_lo = 0, cmos1_hi = 0;
static uint32_t cmos0 = 0;
static uint32_t cmos1 = 0;
static uint32_t cmos2 = 0;
static uint32_t cmos3 = 0;
static uint32_t cmos4 = 0;
static uint32_t cmos5 = 0;
static uint32_t cmos6 = 0;
static uint32_t cmos7 = 0;
static uint32_t cmos8 = 0;
static uint32_t cmos9 = 0;
static int32_t  reg_skip_left = 0;
static int32_t  reg_skip_right = 0;
static int32_t  reg_skip_top = 0;
static int32_t  reg_skip_bottom = 0;
static int32_t  reg_bl = 0;
static int32_t  reg_gain = 0;
static int crop_patch = 0;
static int crop_patch2 = 0;
static int isopatch = 0;
static int isopatchoff = 1;
static int iso1 = 1;
static int row1 = 0;
static int row2 = 0;
static int row3 = 0;
static int row4 = 0;
static int row5 = 0;
static int row6 = 0;
static int isohigh = 0;
static int movcount = 0;
//static int preset1 = 1;
//static int preset2 = 1;
//static int preset3 = 1;

/* helper to allow indexing various properties of Canon's video modes */
static inline int get_video_mode_index()
{
    if (lv_dispsize > 1)
    {
        return 5;
    }
    
    if (!is_movie_mode())
    {
        /* FIXME: some cameras may use 50p or 60p */
        return 2;
    }
    
    if (video_mode_crop)
    {
        /* some cameras may have various crop modes, hopefully at most one per FPS */
        return
        (video_mode_fps == 24) ?  6 :
        (video_mode_fps == 25) ?  7 :
        (video_mode_fps == 30) ?  8 :
        (video_mode_fps == 50) ?  9 :
        /* (video_mode_fps == 60) */ 10 ;
    }
    
    /* regular video modes */
    return
    (video_mode_fps == 24) ?  0 :
    (video_mode_fps == 25) ?  1 :
    (video_mode_fps == 30) ?  2 :
    (video_mode_fps == 50) ?  3 :
    /* (video_mode_fps == 60) */ 4 ;
}

/* optical black area sizes */
/* not sure how to adjust them from registers, so... hardcode them here */
static inline void FAST calc_skip_offsets(int * p_skip_left, int * p_skip_right, int * p_skip_top, int * p_skip_bottom)
{
    /* start from LiveView values */
    int skip_left       = 146;
    int skip_right      = 2;
    int skip_top        = 28;
    int skip_bottom     = 0;
    
    switch (crop_preset)
    {
        case CROP_PRESET_FULLRES_LV:
            /* photo mode values */
            skip_left       = 138;
            skip_right      = 2;
            skip_top        = 60;   /* fixme: this is different, why? */
            break;
            
        case CROP_PRESET_3K:
        case CROP_PRESET_UHD:
        case CROP_PRESET_4K_HFPS:
            skip_right      = 0;    /* required for 3840 - tight fit */
            /* fall-through */
            
        case CROP_PRESET_3X_TALL:
            skip_top        = 30;
            break;
            
        case CROP_PRESET_1x3:
                skip_top        = 60;
            if (ratios == 0x1)
            {
                skip_left       = 206;
                skip_right      = 62;
                skip_top        = 60;
                skip_bottom     = 30;
            }
            if (ratios == 0x1 && set_25fps == 0x1)
            {
                skip_left       = 248;
                skip_right      = 110;
                skip_top        = 60;
                skip_bottom     = 42;
            }
            if (ratios == 0x2)
            {
                skip_left       = 206;
                skip_right      = 62;
                skip_top        = 60;
            }
            if (ratios == 0x2 && set_25fps == 0x1)
            {
                skip_left       = 248;
                skip_right      = 110;
                skip_top        = 60;
            }
            if (ratios == 0x3)
            {
                skip_left       = 426;
                skip_right      = 282;
                skip_top        = 60;
            }
            if (ratios == 0x3 && set_25fps == 0x1)
            {
                skip_left       = 512;
                skip_right      = 270;
                skip_top        = 60;
            }
            break;
            
        case CROP_PRESET_3X:
        case CROP_PRESET_anamorph_fullres:
            skip_top        = 60;
            break;
            
        case CROP_PRESET_3x3_1X:
        case CROP_PRESET_3x3_1X_50p:
        case CROP_PRESET_3x3_1X_60p:
        case CROP_PRESET_3x3_1X_48p:
        case CROP_PRESET_3x3_1X_45p:
            if (is_720p()) skip_top = 0;
            break;
    }
    
    if (p_skip_left)   *p_skip_left    = skip_left + reg_skip_left;
    if (p_skip_right)  *p_skip_right   = skip_right + reg_skip_right;
    if (p_skip_top)    *p_skip_top     = skip_top + reg_skip_top;
    if (p_skip_bottom) *p_skip_bottom  = skip_bottom + reg_skip_bottom;
}

/* to be in sync with 0xC0F06800 */
static int get_top_bar_adjustment()
{
    switch (crop_preset)
    {
        case CROP_PRESET_FULLRES_LV:
            return 0;                   /* 0x10018: photo mode value, unchanged */
        case CROP_PRESET_3x3_1X:
        case CROP_PRESET_3x3_1X_50p:
        case CROP_PRESET_3x3_1X_60p:
        case CROP_PRESET_3x3_1X_48p:
        case CROP_PRESET_3x3_1X_45p:
            if (is_720p()) return 28;   /* 0x1D0017 from 0x10017 */
            /* fall through */
        default:
            return 30;                  /* 0x1F0017 from 0x10017 */
    }
}

/* Vertical resolution from current unmodified video mode */
/* (active area only, as seen by mlv_lite) */
static inline int get_default_yres()
{
    return
    (video_mode_fps <= 30) ? 1290 : 672;
}

/* skip_top from unmodified video mode (raw.c, LiveView skip offsets) */
static inline int get_default_skip_top()
{
    return
    (video_mode_fps <= 30) ? 28 : 20;
}

/* max resolution for each video mode (trial and error) */
/* it's usually possible to push the numbers a few pixels further,
 * at the risk of corrupted frames */
static int max_resolutions[NUM_CROP_PRESETS][6] = {
    /*   24p   25p   30p   50p   60p   x5 */
    [CROP_PRESET_3X_TALL]       = { 1920, 1728, 1536,  960,  800, 1320 },
    [CROP_PRESET_3x3_1X]        = { 1290, 1290, 1290,  960,  800, 1320 },
    [CROP_PRESET_3x3_1X_50p]    = { 1290, 1290, 1290,  960,  960, 1320 },
    [CROP_PRESET_3x3_1X_60p]    = { 1290, 1290, 1290,  800,  800, 1320 },
    [CROP_PRESET_3x3_1X_48p]    = { 1290, 1290, 1290, 1040, 1040, 1320 },
    [CROP_PRESET_3x3_1X_45p]    = { 1290, 1290, 1290, 1080, 1080, 1320 },
    [CROP_PRESET_3K]            = { 1920, 1728, 1504,  760,  680, 1320 },
    [CROP_PRESET_UHD]           = { 1536, 1472, 1120,  640,  540, 1320 },
    [CROP_PRESET_4K_HFPS]       = { 3072, 3072, 2500, 1440, 1200, 1320 },
    [CROP_PRESET_FULLRES_LV]    = { 3870, 3870, 3870, 3870, 3870, 1320 },
};

/* 5D3 vertical resolution increments over default configuration */
/* note that first scanline may be moved down by 30 px (see reg_override_top_bar) */
static inline int FAST calc_yres_delta()
{
    int desired_yres = (target_yres) ? target_yres
    : max_resolutions[crop_preset][get_video_mode_index()];
    
    if (desired_yres)
    {
        /* user override */
        int skip_top;
        calc_skip_offsets(0, 0, &skip_top, 0);
        int default_yres = get_default_yres();
        int default_skip_top = get_default_skip_top();
        int top_adj = get_top_bar_adjustment();
        return desired_yres - default_yres + skip_top - default_skip_top + top_adj;
    }
    
    ASSERT(0);
    return 0;
}

#define YRES_DELTA calc_yres_delta()


static int cmos_vidmode_ok = 0;

/* return value:
 *  1: registers checked and appear OK (1080p/720p video mode)
 *  0: registers checked and they are not OK (other video mode)
 * -1: registers not checked
 */
static int FAST check_cmos_vidmode(uint16_t* data_buf)
{
    int ok = 1;
    int found = 1;
    while (*data_buf != 0xFFFF)
    {
        int reg = (*data_buf) >> 12;
        int value = (*data_buf) & 0xFFF;
        
        if (is_5D3)
        {
            if (reg == 1)
            {
                found = 1;
                
                switch (crop_preset)
                {
                    case CROP_PRESET_CENTER_Z:
                    case CROP_PRESET_33K:
                    {
                        /* detecting the zoom mode is tricky;
                         * let's just exclude 1080p and 720p for now ... */
                        if (value == 0x800 ||
                            value == 0xBC2)
                        {
                            ok = 0;
                        }
                        break;
                    }
                        
                    default:
                    {
                        if (value != 0x800 &&   /* not 1080p? */
                            value != 0xBC2)     /* not 720p? */
                        {
                            ok = 0;
                        }
                        break;
                    }
                }
            }
        }
        
        data_buf++;
    }
    
    if (found) return ok;
    
    return -1;
}

/* pack two 6-bit values into a 12-bit one */
#define PACK12(lo,hi) ((((lo) & 0x3F) | ((hi) << 6)) & 0xFFF)

/* pack two 16-bit values into a 32-bit one */
#define PACK32(lo,hi) (((uint32_t)(lo) & 0xFFFF) | ((uint32_t)(hi) << 16))

/* pack two 16-bit values into a 32-bit one */
#define PACK32(lo,hi) (((uint32_t)(lo) & 0xFFFF) | ((uint32_t)(hi) << 16))

static void FAST cmos_hook(uint32_t* regs, uint32_t* stack, uint32_t pc)
{
    /* make sure we are in 1080p/720p mode */
    if (!is_supported_mode())
    {
        /* looks like checking properties works fine for detecting
         * changes in video mode, but not for detecting the zoom change */
        return;
    }
    
    /* also check CMOS registers; in zoom mode, we get different values
     * and this check is instant (no delays).
     *
     * on 5D3, the 640x480 acts like 1080p during standby,
     * so properties are our only option for that one.
     */
    
    uint16_t* data_buf = (uint16_t*) regs[0];
    int ret = check_cmos_vidmode(data_buf);
    
    if (ret >= 0)
    {
        cmos_vidmode_ok = ret;
    }
    
    if (ret != 1)
    {
        return;
    }
    
    int cmos_new[10] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
    
    if (is_5D3)
    {
        switch (crop_preset)
        {
                /* 1:1 (3x) */
            case CROP_PRESET_3X:
                /* start/stop scanning line, very large increments */
                /* note: these are two values, 6 bit each, trial and error */
                cmos_new[1] = (is_720p())
                ? PACK12(13,10)     /* 720p,  almost centered */
                : PACK12(11,11);    /* 1080p, almost centered */
                
                cmos_new[2] = 0x10E;    /* read every column, centered crop */
                cmos_new[6] = 0x170;    /* pink highlights without this */
                break;
                
            case CROP_PRESET_3X_TALL:
                cmos_new[1] =           /* vertical centering (trial and error) */
                (video_mode_fps == 24) ? PACK12(8,13)  :
                (video_mode_fps == 25) ? PACK12(8,12)  :
                (video_mode_fps == 30) ? PACK12(9,11)  :
                (video_mode_fps == 50) ? PACK12(12,10) :
                (video_mode_fps == 60) ? PACK12(13,10) :
                (uint32_t) -1 ;
                cmos_new[2] = 0x10E;    /* horizontal centering (trial and error) */
                cmos_new[6] = 0x170;    /* pink highlights without this */
                break;
                
                /* 3x3 binning in 720p */
                /* 1080p it's already 3x3, don't change it */
            case CROP_PRESET_3x3_1X:
            case CROP_PRESET_3x3_1X_50p:
            case CROP_PRESET_3x3_1X_60p:
            case CROP_PRESET_3x3_1X_48p:
            case CROP_PRESET_3x3_1X_45p:
                if (is_720p())
                {
                    /* start/stop scanning line, very large increments */
                    cmos_new[1] =
                    ((crop_preset == CROP_PRESET_3x3_1X_48p || crop_preset == CROP_PRESET_3x3_1X_45p) && (x3crop == 0x0 && !crop_patch)) ? PACK12(3,15) :
                    ((x3crop == 0x1 || crop_patch) && crop_preset == CROP_PRESET_3x3_1X_50p)                  	 ? PACK12(14,11) :
                    ((x3crop == 0x1 || crop_patch) && crop_preset == CROP_PRESET_3x3_1X_60p)                  	 ? PACK12(15,11) :
                    ((x3crop == 0x1 || crop_patch) && (crop_preset == CROP_PRESET_3x3_1X_48p || crop_preset == CROP_PRESET_3x3_1X_45p)) ? PACK12(11,10) :
                    (video_mode_fps == 50 || crop_preset == CROP_PRESET_3x3_1X_50p)                  ? PACK12(4,14) :
                    (video_mode_fps == 60 || crop_preset == CROP_PRESET_3x3_1X_60p)                  ? PACK12(6,14) :
                    (uint32_t) -1 ;
                    
                    cmos_new[6] = 0x370;    /* pink highlights without this */
                    if (x3crop == 0x1 || crop_patch)
                    {
                        cmos_new[2] = 0x10E;    /* read every column, centered crop */
                        cmos_new[6] = 0x170;    /* pink highlights without this */
                    }
                    
                }
                break;
                
            case CROP_PRESET_3K:
                cmos_new[1] =           /* vertical centering (trial and error) */
                (video_mode_fps == 24) ? PACK12(8,12)  :
                (video_mode_fps == 25) ? PACK12(8,12)  :
                (video_mode_fps == 30) ? PACK12(9,11)  :
                (video_mode_fps == 50) ? PACK12(13,10) :
                (video_mode_fps == 60) ? PACK12(14,10) :    /* 13,10 has better centering, but overflows */
                (uint32_t) -1 ;
                cmos_new[2] = 0x0BE;    /* horizontal centering (trial and error) */
                cmos_new[6] = 0x170;    /* pink highlights without this */
                break;
                
            case CROP_PRESET_UHD:
                cmos_new[1] =
                (video_mode_fps == 24) ? PACK12(10,12)  :
                (video_mode_fps == 25) ? PACK12(10,12)  :
                (video_mode_fps == 30) ? PACK12(12,12)  :
                (video_mode_fps == 50) ? PACK12(14,10) :
                (video_mode_fps == 60) ? PACK12(13,9) : /* 60fps not vertically centered */
                (uint32_t) -1 ;
                cmos_new[2] = 0x08E;    /* horizontal centering (trial and error) */
                cmos_new[6] = 0x170;    /* pink highlights without this */
                break;
                
            case CROP_PRESET_4K_HFPS:
                cmos_new[1] =
                (video_mode_fps == 24) ? PACK12(4,15)  :
                (video_mode_fps == 25) ? PACK12(4,15)  :
                (video_mode_fps == 30) ? PACK12(6,14)  :
                (video_mode_fps == 50) ? PACK12(10,11) :
                (video_mode_fps == 60) ? PACK12(12,11) :
                (uint32_t) -1 ;
                cmos_new[2] = 0x07E;    /* horizontal centering (trial and error) */
                cmos_new[6] = 0x170;    /* pink highlights without this */
                break;
                
            case CROP_PRESET_FULLRES_LV:
                cmos_new[1] = 0x800;    /* from photo mode */
                cmos_new[2] = 0x00E;    /* 8 in photo mode; E enables shutter speed control from ADTG 805E */
                cmos_new[6] = 0x170;    /* pink highlights without this */
                break;
                
                /* 1x3 binning (read every line, bin every 3 columns) */
            case CROP_PRESET_1x3:
                /* start/stop scanning line, very large increments */
                cmos_new[1] =
                (set_25fps != 0x1) ? PACK12(7,14)  :
                (set_25fps == 0x1) ? PACK12(8,14)  :
                (uint32_t) -1 ;
                
                cmos_new[6] = 0x170;    /* pink highlights without this */
                break;
                
                /* 1x3 binning (read every line, bin every 3 columns) */
            case CROP_PRESET_anamorph_fullres:
                /* start/stop scanning line, very large increments */
                cmos_new[1] = 0x800;
                cmos_new[6] = 0x170;    /* pink highlights without this */
                break;
                
                /* 3x1 binning (bin every 3 lines, read every column) */
            case CROP_PRESET_3x1:
                cmos_new[2] = 0x10E;    /* read every column, centered crop */
                break;
                
                /* raw buffer centered in zoom mode */
            case CROP_PRESET_CENTER_Z:
                cmos_new[1] = PACK12(9,12); /* vertical (first|last) */
                cmos_new[2] = 0x09E;            /* horizontal offset (mask 0xFF0) */
                break;
            
            case CROP_PRESET_33K:
                cmos_new[1] = PACK12(9,13); /* vertical (first|last) */
                cmos_new[2] = 0xb2;
                break;
                
        }
    }
    
    /* restrict max auto iso to 400/800/1600 instead of skyrocketing to 6400 */
    if ((isoauto == 0x1 || isoauto == 0x2 || isoauto == 0x3) && lens_info.raw_iso == 0x0)
    {
        /* dummy reg */
        if (isoauto == 0x1 && lens_info.raw_iso_auto > 0x54) EngDrvOutLV(0xC0F0b12c, 0x7);
        if (isoauto == 0x2 && lens_info.raw_iso_auto > 0x5c) EngDrvOutLV(0xC0F0b12c, 0x8);
        if (isoauto == 0x3 && lens_info.raw_iso_auto > 0x63) EngDrvOutLV(0xC0F0b12c, 0x9);
        
        if (is_5D3 && isoauto == 0x1 && lens_info.raw_iso_auto > 0x54) cmos_new[0] = 0x223; // stick to iso 400
        if (is_5D3 && isoauto == 0x2 && lens_info.raw_iso_auto > 0x5c) cmos_new[0] = 0x333; // stick to iso 800
        if (is_5D3 && isoauto == 0x3 && lens_info.raw_iso_auto > 0x63) cmos_new[0] = 0x443; // stick to iso 1600
    }
    
    if ((isoauto == 0x1 || isoauto == 0x2 || isoauto == 0x3) && lens_info.raw_iso == 0x0)
    {
        if (isoauto == 0x1 && lens_info.raw_iso_auto < 0x54) EngDrvOutLV(0xC0F0b12c, 0x0);
        if (isoauto == 0x2 && lens_info.raw_iso_auto < 0x5c) EngDrvOutLV(0xC0F0b12c, 0x0);
        if (isoauto == 0x3 && lens_info.raw_iso_auto < 0x63) EngDrvOutLV(0xC0F0b12c, 0x0);
    }
    
    
    /* fast access to iso with press down button */
    if (is_5D3 && isoclimb != 0x0 && lens_info.raw_iso != 0x0)
    {
        
        /* check if masc selected isoauto in canon menu ;) */
        if (lens_info.raw_iso == 0x0)
        {
            NotifyBox(3000, "Turn off autoiso if using iso climb");
        }
        
        isopatch = 1;
        if (isoclimb == 0x2)
        {
            EngDrvOutLV(0xC0F0b12c, 0x12);
            if (is_5D3) cmos_new[0] = 0x113;
        }
        else if (isoclimb == 0x3)
        {
            EngDrvOutLV(0xC0F0b12c, 0x13);
            if (is_5D3) cmos_new[0] = 0x223;
        }
        else if (isoclimb == 0x4)
        {
            EngDrvOutLV(0xC0F0b12c, 0x14);
            if (is_5D3) cmos_new[0] = 0x333;
        }
        else if (isoclimb == 0x5)
        {
            EngDrvOutLV(0xC0F0b12c, 0x15);
            if (is_5D3) cmos_new[0] = 0x443;
        }
        else if (isoclimb == 0x6)
        {
            EngDrvOutLV(0xC0F0b12c, 0x16);
            if (is_5D3) cmos_new[0] = 0x553;
        }
        else if (isoclimb == 0x7)
        {
            EngDrvOutLV(0xC0F0b12c, 0x17);
            if (is_5D3) cmos_new[0] = 0xdd3;
        }
        if (isoclimb == 0x1)
        {
            EngDrvOutLV(0xC0F0b12c, 0x11);
            if (is_5D3) cmos_new[0] = 0x3;
        }
    }
    
    
    /* menu overrides */
    if (cmos1_lo || cmos1_hi)
    {
        cmos_new[1] = PACK12(cmos1_lo,cmos1_hi);
    }
    
    if (cmos2)
    {
        cmos_new[2] = cmos2;
    }
    
    if (cmos3)
    {
        cmos_new[3] = cmos3;
    }
    
    if (cmos4)
    {
        cmos_new[4] = cmos4;
    }
    
    if (cmos5)
    {
        cmos_new[5] = cmos5;
    }
    
    if (cmos6)
    {
        cmos_new[6] = cmos6;
    }
    
    if (cmos7)
    {
        cmos_new[7] = cmos7;
    }
    
    if (cmos8)
    {
        cmos_new[8] = cmos8;
    }
    
    if (cmos9)
    {
        cmos_new[9] = cmos9;
    }
    
    /* framestop 2 frames */
    if(RECORDING && framestop == 1 && movcount++ >= 18)
    {
        module_send_keypress(MODULE_KEY_REC);
    }
    else
    {
        movcount++;
    }
    
    /* framestop 3 frames */
    if(RECORDING && framestop == 2 && movcount++ >= 28)
    {
        module_send_keypress(MODULE_KEY_REC);
    }
    else
    {
        movcount++;
    }
    
    /* iso averaging. Merge your mlv in Mlv App using the tmix filter */
    if ((isoaverage == 1 || isoaverage == 2) && lv_dispsize != 10 && !frameburst)
    {
        /* iso reg */
        if (RECORDING) cmos_new[0] = cmos0;
        if (iso1)
        {
            cmos0 = 0x3;
            if (isoaverage == 2) cmos0 = 0x113;
        }
        
        if (cmos0 == 0x223 || cmos0 == 0x333)
        {
            cmos0 = 0x3;
            if (isoaverage == 2) cmos0 = 0x113;
            iso1 = 1;
            row1 = 1;
            if (row6 && movcount++ >= 100)
            {
                module_send_keypress(MODULE_KEY_REC);
            }
            else
            {
                movcount++;
            }
            if (row1 && row2 && row3 && row4 && row5)
            {
                row6 = 1;
            }
            if (row1 && row2 && row3 && row4)
            {
                row5 = 1;
            }
            if (row1 && row2 && row3)
            {
                row4 = 1;
            }
            if (row1 && row2)
            {
                row3 = 1;
            }
            if (row1 && !row2)
            {
                row2 = 1;
            }
            
        }
        else if (cmos0 == 0x443 || cmos0 == 0x553)
        {
            cmos0 = 0x223;
            if (isoaverage == 2) cmos0 = 0x333;
        }
        else if ((cmos0 == 0x3 || cmos0 == 0x113) && !iso1)
        {
            cmos0 = 0x443;
            if (isoaverage == 2) cmos0 = 0x553;
        }
        
        iso1 = 0;
    }
    
    /* iso averaging. Merge your mlv in Mlv App using the tmix filter */
    if ((isoaverage == 3 || isoaverage == 4) && lv_dispsize != 10)
    {
        /* iso reg */
        if (RECORDING) cmos_new[0] = cmos0;
        if (iso1)
        {
            cmos0 = 0x3;
            if (isoaverage == 2) cmos0 = 0x113;
        }
        
        if ((cmos0 == 0x223 || cmos0 == 0x333) && !isohigh)
        {
            cmos0 = 0x3;
            if (isoaverage == 2) cmos0 = 0x113;
            iso1 = 1;
            row1 = 1;
            if (row6 && movcount++ >= 150)
            {
                module_send_keypress(MODULE_KEY_REC);
            }
            else
            {
                movcount++;
            }
            if (row1 && row2 && row3 && row4 && row5)
            {
                row6 = 1;
            }
            if (row1 && row2 && row3 && row4)
            {
                row5 = 1;
            }
            if (row1 && row2 && row3)
            {
                row4 = 1;
            }
            if (row1 && row2)
            {
                row3 = 1;
            }
            if (row1 && !row2)
            {
                row2 = 1;
            }
            
        }
        else if ((cmos0 == 0x223 || cmos0 == 0x333) && isohigh)
        {
            cmos0 = 0x223;
            if (isoaverage == 2) cmos0 = 0x333;
            isohigh = 0;
        }
        else if ((cmos0 == 0x443 || cmos0 == 0x553) && isohigh)
        {
            cmos0 = 0x223;
            if (isoaverage == 2) cmos0 = 0x333;
        }
        else if ((cmos0 == 0x443 || cmos0 == 0x553) && !isohigh)
        {
            cmos0 = 0x443;
            if (isoaverage == 2) cmos0 = 0x553;
            isohigh = 1;
        }
        else if ((cmos0 == 0x3 || cmos0 == 0x113) && !iso1)
        {
            cmos0 = 0x443;
            if (isoaverage == 2) cmos0 = 0x553;
        }
        
        iso1 = 0;
    }
    
    if (frameburst && lv_dispsize != 10 && RECORDING)
    {
        /* speicify amount of frames */
        if(movcount++ >= frameburst*8)
        {
            module_send_keypress(MODULE_KEY_REC);
        }
        else
        {
            movcount++;
        }
        
    }
    
    /* copy data into a buffer, to make the override temporary */
    /* that means: as soon as we stop executing the hooks, values are back to normal */
    static uint16_t copy[512];
    uint16_t* copy_end = &copy[COUNT(copy)];
    uint16_t* copy_ptr = copy;
    
    while (*data_buf != 0xFFFF)
    {
        *copy_ptr = *data_buf;
        
        int reg = (*data_buf) >> 12;
        if (cmos_new[reg] != -1)
        {
            *copy_ptr = (reg << 12) | cmos_new[reg];
            dbg_printf("CMOS[%x] = %x\n", reg, cmos_new[reg]);
        }
        
        data_buf++;
        copy_ptr++;
        if (copy_ptr > copy_end) while(1);
    }
    *copy_ptr = 0xFFFF;
    
    /* pass our modified register list to cmos_write */
    regs[0] = (uint32_t) copy;
}

static uint32_t nrzi_encode( uint32_t in_val )
{
    uint32_t out_val = 0;
    uint32_t old_bit = 0;
    for (int num = 0; num < 31; num++)
    {
        uint32_t bit = in_val & 1<<(30-num) ? 1 : 0;
        if (bit != old_bit)
            out_val |= (1 << (30-num));
        old_bit = bit;
    }
    return out_val;
}

static uint32_t nrzi_decode( uint32_t in_val )
{
    uint32_t val = 0;
    if (in_val & 0x8000)
        val |= 0x8000;
    for (int num = 0; num < 31; num++)
    {
        uint32_t old_bit = (val & 1<<(30-num+1)) >> 1;
        val |= old_bit ^ (in_val & 1<<(30-num));
    }
    return val;
}

static int FAST adtg_lookup(uint32_t* data_buf, int reg_needle)
{
    while(*data_buf != 0xFFFFFFFF)
    {
        int reg = (*data_buf) >> 16;
        if (reg == reg_needle)
        {
            return *(uint16_t*)data_buf;
        }
    }
    return -1;
}

/* adapted from fps_override_shutter_blanking in fps-engio.c */
static int adjust_shutter_blanking(int old)
{
    /* sensor duty cycle: range 0 ... timer B */
    int current_blanking = nrzi_decode(old);
    
    int video_mode = get_video_mode_index();
    
    /* what value Canon firmware assumes for timer B? */
    int fps_timer_b_orig = default_timerB[video_mode];
    
    int current_exposure = fps_timer_b_orig - current_blanking;
    
    /* wrong assumptions? */
    if (current_exposure < 0)
    {
        return old;
    }
    
    int default_fps = default_fps_1k[video_mode];
    int current_fps = fps_get_current_x1000();
    
    dbg_printf("FPS %d->%d\n", default_fps, current_fps);
    
    float frame_duration_orig = 1000.0 / default_fps;
    float frame_duration_current = 1000.0 / current_fps;
    
    float orig_shutter = frame_duration_orig * current_exposure / fps_timer_b_orig;
    
    float new_shutter =
    (shutter_range == 0) ?
    ({
        /* original shutter speed from the altered video mode */
        orig_shutter;
    }) :
    ({
        /* map the available range of 1/4000...1/30 (24-30p) or 1/4000...1/60 (50-60p)
         * from minimum allowed (1/15000 with full-res LV) to 1/fps */
        int max_fps_shutter = (video_mode_fps <= 30) ? 33333 : 64000;
        int default_fps_adj = 1e9 / (1e9 / max_fps_shutter - 250);
        (orig_shutter - 250e-6) * default_fps_adj / current_fps;
    });
    
    /* what value is actually used for timer B? (possibly after our overrides) */
    int fps_timer_b = (shamem_read(0xC0F06014) & 0xFFFF) + 1;
    
    dbg_printf("Timer B %d->%d\n", fps_timer_b_orig, fps_timer_b);
    
    int new_exposure = new_shutter * fps_timer_b / frame_duration_current;
    int new_blanking = COERCE(fps_timer_b - new_exposure, 10, fps_timer_b - 2);
    
    dbg_printf("Exposure %d->%d (timer B units)\n", current_exposure, new_exposure);
    
#ifdef CROP_DEBUG
    float chk_shutter = frame_duration_current * new_exposure / fps_timer_b;
    dbg_printf("Shutter %d->%d us\n", (int)(orig_shutter*1e6), (int)(chk_shutter*1e6));
#endif
    
    dbg_printf("Blanking %d->%d\n", current_blanking, new_blanking);
    
    return nrzi_encode(new_blanking);
}

extern void fps_override_shutter_blanking();

static void FAST adtg_hook(uint32_t* regs, uint32_t* stack, uint32_t pc)
{
    if (!is_supported_mode() || !cmos_vidmode_ok)
    {
        /* don't patch other video modes */
        return;
    }
    
    if (!is_720p())
    {
        if (crop_preset == CROP_PRESET_3x3_1X ||
            crop_preset == CROP_PRESET_3x3_1X_50p ||
            crop_preset == CROP_PRESET_3x3_1X_60p ||
            crop_preset == CROP_PRESET_3x3_1X_45p ||
            crop_preset == CROP_PRESET_3x3_1X_48p)
        {
            /* these presets only have effect in 720p mode */
            return;
        }
    }
    
    /* This hook is called from the DebugMsg's in adtg_write,
     * so if we change the register list address, it won't be able to override them.
     * Workaround: let's call it here. */
    fps_override_shutter_blanking();
    
    uint32_t cs = regs[0];
    uint32_t *data_buf = (uint32_t *) regs[1];
    int dst = cs & 0xF;
    
    /* copy data into a buffer, to make the override temporary */
    /* that means: as soon as we stop executing the hooks, values are back to normal */
    static uint32_t copy[512];
    uint32_t* copy_end = &copy[COUNT(copy)];
    uint32_t* copy_ptr = copy;
    
    struct adtg_new
    {
        int dst;
        int reg;
        int val;
    };
    
    /* expand this as required */
    struct adtg_new adtg_new[30] = {{0}};
    
    const int blanking_reg_zoom   = (is_5D3) ? 0x805E : 0x805F;
    const int blanking_reg_nozoom = (is_5D3) ? 0x8060 : 0x8061;
    const int blanking_reg = (lv_dispsize == 1) ? blanking_reg_zoom : blanking_reg_nozoom;
    
    /* scan for shutter blanking and make both zoom and non-zoom value equal */
    /* (the values are different when using FPS override with ADTG shutter override) */
    /* (fixme: might be better to handle this in ML core?) */
    int shutter_blanking = 0;
    for (uint32_t * buf = data_buf; *buf != 0xFFFFFFFF; buf++)
    {
        int reg = (*buf) >> 16;
        if (reg == blanking_reg)
        {
            int val = (*buf) & 0xFFFF;
            shutter_blanking = val;
        }
    }
    
    /* some modes may need adjustments to maintain exposure */
    if (shutter_blanking)
    {
        if (is_5D3)
        {
            /* FIXME: remove this kind of hardcoded conditions */
            if (((crop_preset == CROP_PRESET_CENTER_Z || crop_preset == CROP_PRESET_33K) && lv_dispsize != 1) ||
                ((crop_preset != CROP_PRESET_CENTER_Z && crop_preset != CROP_PRESET_33K) && lv_dispsize == 1))
            {
                shutter_blanking = adjust_shutter_blanking(shutter_blanking);
            }
        }
    }
    
    /* should probably be made generic */
    if (is_5D3)
    {
        /* all modes may want to override shutter speed */
        /* ADTG[0x8060]: shutter blanking for 3x3 mode  */
        /* ADTG[0x805E]: shutter blanking for zoom mode  */
        adtg_new[0] = (struct adtg_new) {6, blanking_reg_zoom, shutter_blanking};
        adtg_new[1] = (struct adtg_new) {6, blanking_reg_nozoom, shutter_blanking};
        
        /* Correct analog gain pushed autoiso wise otherwise. Only 14bit while recording or below applies */
        if (isoauto != 0x0 && !bitdepth && !is_5D3)
        {
            
            //if (lens_info.raw_iso_auto > 0x54)
            //{
            ///* correct black level/tint when previewing */
            //EngDrvOutLV(0xc0f37aec, 0x73ca + reg_bl);
            //EngDrvOutLV(0xc0f37af8, 0x73ca + reg_bl);
            //EngDrvOutLV(0xc0f37b04, 0x73ca + reg_bl);
            //EngDrvOutLV(0xc0f37ae0, 0x73ca + reg_bl);
            //}
            
            /* true iso 400 */
            if (isoauto == 0x1 && lens_info.raw_iso_auto > 0x54)
            {
                adtg_new[13] = (struct adtg_new) {6, 0x8882, 0x438 + reg_gain};
                adtg_new[14] = (struct adtg_new) {6, 0x8884, 0x439 + reg_gain};
                adtg_new[15] = (struct adtg_new) {6, 0x8886, 0x439 + reg_gain};
                adtg_new[16] = (struct adtg_new) {6, 0x8888, 0x439 + reg_gain};
            }
            /* true iso 800 */
            if (isoauto == 0x2 && lens_info.raw_iso_auto > 0x5c)
            {
                adtg_new[13] = (struct adtg_new) {6, 0x8882, 0x421 + reg_gain};
                adtg_new[14] = (struct adtg_new) {6, 0x8884, 0x420 + reg_gain};
                adtg_new[15] = (struct adtg_new) {6, 0x8886, 0x421 + reg_gain};
                adtg_new[16] = (struct adtg_new) {6, 0x8888, 0x422 + reg_gain};
            }
            /* true iso 1600 */
            if (isoauto == 0x3 && lens_info.raw_iso_auto > 0x63)
            {
                adtg_new[13] = (struct adtg_new) {6, 0x8882, 0x457 + reg_gain};
                adtg_new[14] = (struct adtg_new) {6, 0x8884, 0x456 + reg_gain};
                adtg_new[15] = (struct adtg_new) {6, 0x8886, 0x457 + reg_gain};
                adtg_new[16] = (struct adtg_new) {6, 0x8888, 0x457 + reg_gain};
            }
            
        }
        
        /* only apply bit reducing while recording, not while idle */
        if (RECORDING && is_5D3)
        {
            if (OUTPUT_8BIT)
            {
                /* 8bit roundtrip only not applied here with following set ups */
                adtg_new[13] = (struct adtg_new) {6, 0x8882, 12 + reg_gain};
                adtg_new[14] = (struct adtg_new) {6, 0x8884, 12 + reg_gain};
                adtg_new[15] = (struct adtg_new) {6, 0x8886, 12 + reg_gain};
                adtg_new[16] = (struct adtg_new) {6, 0x8888, 12 + reg_gain};
            }
            
            if (OUTPUT_9BIT)
            {
                /* 9bit roundtrip only not applied here with following set ups */
                adtg_new[13] = (struct adtg_new) {6, 0x8882, 30 + reg_gain};
                adtg_new[14] = (struct adtg_new) {6, 0x8884, 30 + reg_gain};
                adtg_new[15] = (struct adtg_new) {6, 0x8886, 30 + reg_gain};
                adtg_new[16] = (struct adtg_new) {6, 0x8888, 30 + reg_gain};
            }
            
            if (OUTPUT_10BIT)
            {
                /* 10bit roundtrip only not applied here with following set ups */
                adtg_new[13] = (struct adtg_new) {6, 0x8882, 60 + reg_gain};
                adtg_new[14] = (struct adtg_new) {6, 0x8884, 60 + reg_gain};
                adtg_new[15] = (struct adtg_new) {6, 0x8886, 60 + reg_gain};
                adtg_new[16] = (struct adtg_new) {6, 0x8888, 60 + reg_gain};
            }
            
            if (OUTPUT_12BIT)
            {
                /* 12bit roundtrip only not applied here with following set ups */
                adtg_new[13] = (struct adtg_new) {6, 0x8882, 250 + reg_gain};
                adtg_new[14] = (struct adtg_new) {6, 0x8884, 250 + reg_gain};
                adtg_new[15] = (struct adtg_new) {6, 0x8886, 250 + reg_gain};
                adtg_new[16] = (struct adtg_new) {6, 0x8888, 250 + reg_gain};
            }
            
        }
        
    }
    
    /* hopefully generic; to be tested later */
    if (1)
    {
        switch (crop_preset)
        {
                /* all 1:1 modes (3x, 3K, 4K...) */
            case CROP_PRESET_3X:
            case CROP_PRESET_3X_TALL:
            case CROP_PRESET_3K:
            case CROP_PRESET_UHD:
            case CROP_PRESET_4K_HFPS:
            case CROP_PRESET_FULLRES_LV:
                /* ADTG2/4[0x8000] = 5 (set in one call) */
                /* ADTG2[0x8806] = 0x6088 on 5D3 (artifacts without it) */
                adtg_new[2] = (struct adtg_new) {6, 0x8000, 5};
                if (is_5D3) {
                    /* this register is model-specific */
                    adtg_new[3] = (struct adtg_new) {2, 0x8806, 0x6088};
                }
                break;
                
                /* 3x3 binning in 720p (in 1080p it's already 3x3) */
            case CROP_PRESET_3x3_1X:
            case CROP_PRESET_3x3_1X_50p:
            case CROP_PRESET_3x3_1X_60p:
            case CROP_PRESET_3x3_1X_48p:
            case CROP_PRESET_3x3_1X_45p:
                /* ADTG2/4[0x800C] = 2: vertical binning factor = 3 */
                adtg_new[2] = (struct adtg_new) {6, 0x800C, 2 + reg_800c};
                if ((x3crop == 0x1 || crop_patch) && is_5D3)
                {
                    adtg_new[3] = (struct adtg_new) {6, 0x8000, 5 + reg_8000};
                    adtg_new[17] = (struct adtg_new) {2, 0x8806, 0x6088};
                }
                break;
                
            case CROP_PRESET_1x3:
            case CROP_PRESET_anamorph_fullres:
                /* ADTG2/4[0x800C] = 0: read every line */
                adtg_new[2] = (struct adtg_new) {6, 0x800C, 0 + reg_800c};
                // adtg_new[3] = (struct adtg_new) {6, 0x8000, 6 + reg_8000};
                break;
                
                /* 3x1 binning (bin every 3 lines, read every column) */
                /* doesn't work well, figure out why */
            case CROP_PRESET_3x1:
                /* ADTG2/4[0x800C] = 2: vertical binning factor = 3 */
                /* ADTG2[0x8806] = 0x6088 on 5D3 (artifacts worse without it) */
                adtg_new[2] = (struct adtg_new) {6, 0x800C, 2 + reg_800c};
                if (is_5D3) {
                    /* this register is model-specific */
                    adtg_new[3] = (struct adtg_new) {2, 0x8806, 0x6088};
                }
                break;
        }
    }
    
    /* these should work on all presets, on all DIGIC 5 models and also on recent DIGIC 4 */
    if (1)
    {
        /* assuming FPS timer B was overridden before this */
        int fps_timer_b = (shamem_read(0xC0F06014) & (0xFFFF + reg_timing5));
        int readout_end = shamem_read(is_digic4 ? 0xC0F06088 : 0xC0F06804) >> 16;
        
        /* PowerSaveTiming registers */
        /* after readout is finished, we can turn off the sensor until the next frame */
        /* we could also set these to 0; it will work, but the sensor will run a bit hotter */
        /* to be tested to find out exactly how much */
        adtg_new[4]  = (struct adtg_new) {6, 0x8172, nrzi_encode(readout_end + 1 + reg_timing1) }; /* PowerSaveTiming ON (6D/700D) */
        adtg_new[5]  = (struct adtg_new) {6, 0x8178, nrzi_encode(readout_end + 1 + reg_timing1) }; /* PowerSaveTiming ON (5D3/6D/700D) */
        adtg_new[6]  = (struct adtg_new) {6, 0x8196, nrzi_encode(readout_end + 1 + reg_timing1) }; /* PowerSaveTiming ON (5D3) */
        
        adtg_new[7]  = (struct adtg_new) {6, 0x8173, nrzi_encode(fps_timer_b - 5 + reg_timing3) }; /* PowerSaveTiming OFF (6D/700D) */
        adtg_new[8]  = (struct adtg_new) {6, 0x8179, nrzi_encode(fps_timer_b - 5 + reg_timing2) }; /* PowerSaveTiming OFF (5D3/6D/700D) */
        adtg_new[9]  = (struct adtg_new) {6, 0x8197, nrzi_encode(fps_timer_b - 5 + reg_timing2) }; /* PowerSaveTiming OFF (5D3) */
        
        adtg_new[10] = (struct adtg_new) {6, 0x82B6, nrzi_encode(readout_end - 1 + reg_timing6) }; /* PowerSaveTiming ON? (700D); 2 units below the "ON" timing from above */
        
        /* ReadOutTiming registers */
        /* these shouldn't be 0, as they affect the image */
        adtg_new[11] = (struct adtg_new) {6, 0x82F8, nrzi_encode(readout_end + 1 + reg_timing4) }; /* ReadOutTiming */
        adtg_new[12] = (struct adtg_new) {6, 0x82F9, nrzi_encode(fps_timer_b - 1 + reg_timing4) }; /* ReadOutTiming end? */
    }
    
    while(*data_buf != 0xFFFFFFFF)
    {
        *copy_ptr = *data_buf;
        int reg = (*data_buf) >> 16;
        for (int i = 0; i < COUNT(adtg_new); i++)
        {
            if ((reg == adtg_new[i].reg) && (dst & adtg_new[i].dst))
            {
                int new_value = adtg_new[i].val;
                dbg_printf("ADTG%x[%x] = %x\n", dst, reg, new_value);
                *(uint16_t*)copy_ptr = new_value;
                
                if (reg == blanking_reg_zoom || reg == blanking_reg_nozoom)
                {
                    /* also override in original data structure */
                    /* to be picked up on the screen indicators */
                    *(uint16_t*)data_buf = new_value;
                }
            }
        }
        data_buf++;
        copy_ptr++;
        if (copy_ptr >= copy_end) while(1);
    }
    *copy_ptr = 0xFFFFFFFF;
    
    /* pass our modified register list to adtg_write */
    regs[1] = (uint32_t) copy;
}

/* changing bits */
static inline uint32_t reg_override_bits(uint32_t reg, uint32_t old_val)
{

    /* only apply bit reducing while recording, not while idle */
    if (RECORDING && is_5D3 && bitdepth)
    {
        /* compensates for black level issues with analog gain. Used for both 10 and 12 bit */
        if (CROP_PRESET_MENU != CROP_PRESET_CENTER_Z && CROP_PRESET_MENU != CROP_PRESET_33K) *(volatile uint32_t*)0xC0F08560 = 0x7f6;
        
        if (OUTPUT_8BIT)
        {
            switch (reg)
            {
                /* correct liveview brightness */
                case 0xC0F42744: return 0x6060606;
            }
        }
        
        if (OUTPUT_9BIT)
        {
            switch (reg)
            {
                /* correct liveview brightness */
                case 0xC0F42744: return 0x5050505;
            }
        }
        
        if (OUTPUT_10BIT)
        {
            switch (reg)
            {
                /* correct liveview brightness */
                case 0xC0F42744: return 0x4040404;
            }
        }
        
        if (OUTPUT_12BIT)
        {
            switch (reg)
            {
                /* correct liveview brightness */
                case 0xC0F42744: return 0x2020202;
            }
        }
    }
    
    
    /* used to check raw.c and lens.c displaying correct white level */
    if (!RECORDING && is_5D3)
    {
        if (!bitdepth)
        {
            EngDrvOutLV(0xc0f0815c, 0x2);
        }
        
        if (OUTPUT_8BIT)
        {
            EngDrvOutLV(0xc0f0815c, 0x3);
        }
        
        if (OUTPUT_9BIT)
        {
            EngDrvOutLV(0xc0f0815c, 0x4);
        }
        
        if (OUTPUT_10BIT)
        {
            EngDrvOutLV(0xc0f0815c, 0x5);
        }
        
        if (OUTPUT_12BIT)
        {
            EngDrvOutLV(0xc0f0815c, 0x6);
        }
    }
    
    if (is_5D3 && !RECORDING)
    {
        /* resets frameburst counter */
        movcount = 0;
        if (isoaverage)
        {
            row6 = 0;
            row5 = 0;
            row4 = 0;
            row3 = 0;
            row2 = 0;
            row1 = 0;
            iso1 = 1;
            isohigh = 0;
        }
    }
    
    return 0;
}

/* this is used to cover the black bar at the top of the image in 1:1 modes */
/* (used in most other presets) */
static inline uint32_t reg_override_top_bar(uint32_t reg, uint32_t old_val)
{
    switch (reg)
    {
            /* raw start line/column */
            /* move start line down by 30 pixels */
            /* not sure where this offset comes from */
        case 0xC0F06800:
            return 0x1F0017;
    }
    
    return 0;
}

/* these are required for increasing vertical resolution */
/* (used in most other presets) */
static inline uint32_t reg_override_HEAD34(uint32_t reg, uint32_t old_val)
{
    switch (reg)
    {
            /* HEAD3 timer */
        case 0xC0F0713C:
            return old_val + YRES_DELTA + delta_head3;
            
            /* HEAD4 timer */
        case 0xC0F07150:
            return old_val + YRES_DELTA + delta_head4;
    }
    
    return reg_override_bits(reg, old_val);
}

static inline uint32_t reg_override_common(uint32_t reg, uint32_t old_val)
{
    uint32_t a = reg_override_top_bar(reg, old_val);
    if (a) return a;
    
    uint32_t b = reg_override_HEAD34(reg, old_val);
    if (b) return b;
    
    return reg_override_bits(reg, old_val);
}

static inline uint32_t reg_override_fps(uint32_t reg, uint32_t timerA, uint32_t timerB, uint32_t old_val)
{
    /* hardware register requires timer-1 */
    timerA--;
    timerB--;
    
    /* only override FPS registers if the old value is what we expect
     * otherwise we may be in some different video mode for a short time
     * this race condition is enough to lock up LiveView in some cases
     * e.g. 5D3 3x3 50/60p when going from photo mode to video mode
     */
    
    switch (reg)
    {
        case 0xC0F06824:
        case 0xC0F06828:
        case 0xC0F0682C:
        case 0xC0F06830:
        case 0xC0F06010:
        {
            uint32_t expected = default_timerA[get_video_mode_index()] - 1;
            
            if (old_val == expected || old_val == expected + 1)
            {
                return timerA;
            }
            
            break;
        }
            
        case 0xC0F06008:
        case 0xC0F0600C:
        {
            uint32_t expected = default_timerA[get_video_mode_index()] - 1;
            expected |= (expected << 16);
            
            if (old_val == expected || old_val == expected + 0x00010001)
            {
                return timerA | (timerA << 16);
            }
            
            break;
        }
            
        case 0xC0F06014:
        {
            uint32_t expected = default_timerB[get_video_mode_index()] - 1;
            
            if (old_val == expected || old_val == expected + 1)
            {
                return timerB;
            }
            
            break;
        }
    }
    
    return 0;
}

static inline uint32_t reg_override_3X_tall(uint32_t reg, uint32_t old_val)
{
    /* change FPS timers to increase vertical resolution */
    if (video_mode_fps >= 50)
    {
        int timerA = 400;
        
        int timerB =
        (video_mode_fps == 50) ? 1200 :
        (video_mode_fps == 60) ? 1001 :
        -1 ;
        
        int a = reg_override_fps(reg, timerA, timerB, old_val);
        if (a) return a;
    }
    
    /* fine-tuning head timers appears to help
     * pushing the resolution a tiny bit further */
    int head_adj =
    (video_mode_fps == 50) ? -30 :
    (video_mode_fps == 60) ? -20 :
    0 ;
    
    switch (reg)
    {
            /* raw resolution (end line/column) */
        case 0xC0F06804:
            return old_val + (YRES_DELTA << 16);
            
            /* HEAD3 timer */
        case 0xC0F0713C:
            return old_val + YRES_DELTA + delta_head3 + head_adj;
            
            /* HEAD4 timer */
        case 0xC0F07150:
            return old_val + YRES_DELTA + delta_head4 + head_adj;
    }
    
    return reg_override_common(reg, old_val);
}

static inline uint32_t reg_override_3x3_50p(uint32_t reg, uint32_t old_val)
{
    if (!is_720p())
    {
        /* 1080p not patched in 3x3 */
        return 0;
    }
    
    int timerA = 400;
    int timerB = 1200;
    
    /*  reduce to 30fps temporary to be able to go back from x10 zoom without freezes */
    if (lv_dispsize != 1) *(volatile uint32_t*)0xC0F06014 = 0x613;
    
    int a = reg_override_fps(reg, timerA, timerB, old_val);
    if (a) return a;
    
    switch (reg)
    {
            /* for some reason, top bar disappears with the common overrides */
            /* very tight fit - every pixel counts here */
        case 0xC0F06800:
            return 0x1D0017;
            
            /* raw resolution (end line/column) */
        case 0xC0F06804:
            return 0x3de011b + reg_6804_width + (reg_6804_height << 16);
            
            /* HEAD3 timer */
        case 0xC0F0713C:
            return 0x3de + reg_713c;
            
            /* HEAD4 timer */
        case 0xC0F07150:
            return 0x3d2 + reg_7150;
    }
    
    return reg_override_common(reg, old_val);
}

static inline uint32_t reg_override_3x3_60p(uint32_t reg, uint32_t old_val)
{
    if (!is_720p())
    {
        /* 1080p not patched in 3x3 */
        return 0;
    }
    
    int timerA = 400;
    int timerB = 1001;
    
    /*  reduce to 30fps temporary to be able to go back from x10 zoom without freezes */
    if (lv_dispsize != 1) *(volatile uint32_t*)0xC0F06014 = 0x613;
    
    int a = reg_override_fps(reg, timerA, timerB, old_val);
    if (a) return a;
    
    switch (reg)
    {
            /* for some reason, top bar disappears with the common overrides */
            /* very tight fit - every pixel counts here */
        case 0xC0F06800:
            return 0x1D0017;
            
            /* raw resolution (end line/column) */
        case 0xC0F06804:
            return 0x343011b + reg_6804_width + (reg_6804_height << 16);
            
            /* HEAD3 timer */
        case 0xC0F0713C:
            return 0x32b + reg_713c;
            
            /* HEAD4 timer */
        case 0xC0F07150:
            return 0x2e5 + reg_7150;
    }
    
    return reg_override_common(reg, old_val);
}

static inline uint32_t reg_override_3x3_48p(uint32_t reg, uint32_t old_val)
{
    if (!is_720p())
    {
        /* 1080p not patched in 3x3 */
        return 0;
    }
    
    /* change FPS timers to increase vertical resolution */
    if (video_mode_fps >= 50)
    {
        int timerA = 400;
        int timerB = 1250;
        
        /*  reduce to 30fps temporary to be able to go back from x10 zoom without freezes */
        if (lv_dispsize != 1) *(volatile uint32_t*)0xC0F06014 = 0x613;
        
        int a = reg_override_fps(reg, timerA, timerB, old_val);
        if (a) return a;
    }
    
    switch (reg)
    {
            /* for some reason, top bar disappears with the common overrides */
            /* very tight fit - every pixel counts here */
        case 0xC0F06800:
            return 0x1D0017;
            
            /* raw resolution (end line/column) */
        case 0xC0F06804:
            return 0x438011b + reg_6804_width + (reg_6804_height << 16);
            
            /* HEAD3 timer */
            /* 2E6 in 50p, 2B4 in 60p */
        case 0xC0F0713C:
            return 0x2a9 + reg_713c + YRES_DELTA + delta_head3;
            
            /* HEAD4 timer */
            /* 2B4 in 50p, 26D in 60p */
        case 0xC0F07150:
            return 0x263 + reg_7150 + YRES_DELTA + delta_head4;
            
    }
    
    return reg_override_common(reg, old_val);
}

static inline uint32_t reg_override_3x3_45p(uint32_t reg, uint32_t old_val)
{
    if (!is_720p())
    {
        /* 1080p not patched in 3x3 */
        return 0;
    }
    
    /* change FPS timers to increase vertical resolution */
    if (video_mode_fps >= 50)
    {
        int timerA = 401;
        int timerB = 1330;
        
        /*  reduce to 30fps temporary to be able to go back from x10 zoom without freezes */
        if (lv_dispsize != 1) *(volatile uint32_t*)0xC0F06014 = 0x613;
        
        int a = reg_override_fps(reg, timerA, timerB, old_val);
        if (a) return a;
    }
    
    switch (reg)
    {
            /* for some reason, top bar disappears with the common overrides */
            /* very tight fit - every pixel counts here */
        case 0xC0F06800:
            return 0x1D0017;
            
            /* raw resolution (end line/column) */
        case 0xC0F06804:
            return old_val + (YRES_DELTA << 16);
            
            /* HEAD3 timer */
            /* 2E6 in 50p, 2B4 in 60p */
        case 0xC0F0713C:
            return 0x2A0 + YRES_DELTA + delta_head3;
            
            /* HEAD4 timer */
            /* 2B4 in 50p, 26D in 60p */
        case 0xC0F07150:
            return 0x22f + YRES_DELTA + delta_head4;
    }
    
    return reg_override_common(reg, old_val);
}

static inline uint32_t reg_override_3K(uint32_t reg, uint32_t old_val)
{
    /* FPS timer A, for increasing horizontal resolution */
    /* 25p uses 480 (OK), 24p uses 440 (too small); */
    /* only override in 24p, 30p and 60p modes */
    if (video_mode_fps != 25 && video_mode_fps !=  50)
    {
        int timerA = 455;
        int timerB =
        (video_mode_fps == 24) ? 2200 :
        (video_mode_fps == 30) ? 1760 :
        (video_mode_fps == 60) ?  880 :
        -1 ;
        
        int a = reg_override_fps(reg, timerA, timerB, old_val);
        if (a) return a;
    }
    
    switch (reg)
    {
            /* raw resolution (end line/column) */
            /* X: (3072+140)/8 + 0x17, adjusted for 3072 in raw_rec */
        //case 0xC0F06804:
          //  return (old_val & 0xFFFF0000) + 0x1AA + (YRES_DELTA << 16);

//changing 713c and 7150 for 24 fps mode. Rest stays the same. Couldn't get corruption.
        case 0xC0F06804:
            return (video_mode_fps == 24) ? 0x7be01aa: (old_val & 0xFFFF0000) + 0x1AA + (YRES_DELTA << 16);
        case 0xC0F0713c:
            return (video_mode_fps == 24) ? 0x7be: old_val + YRES_DELTA + delta_head3;
        case 0xC0F07150:
            return (video_mode_fps == 24) ? 0x7bb: old_val + YRES_DELTA + delta_head4;
            
    }
    
    return reg_override_common(reg, old_val);
}

static inline uint32_t reg_override_4K_hfps(uint32_t reg, uint32_t old_val)
{
    /* FPS timer A, for increasing horizontal resolution */
    /* trial and error to allow 4096; 572 is too low, 576 looks fine */
    /* pick some values with small roundoff error */
    int timerA =
    (video_mode_fps < 30)  ?  585 : /* for 23.976/2 and 25/2 fps */
    579 ; /* for all others */
    
    /* FPS timer B, tuned to get half of the frame rate from Canon menu */
    int timerB =
    (video_mode_fps == 24) ? 3422 :
    (video_mode_fps == 25) ? 3282 :
    (video_mode_fps == 30) ? 2766 :
    (video_mode_fps == 50) ? 1658 :
    (video_mode_fps == 60) ? 1383 :
    -1 ;
    
    int a = reg_override_fps(reg, timerA, timerB, old_val);
    if (a) return a;
    
    switch (reg)
    {
            /* raw resolution (end line/column) */
            /* X: (4096+140)/8 + 0x18, adjusted for 4096 in raw_rec */
        case 0xC0F06804:
            return (old_val & 0xFFFF0000) + 0x22A + (YRES_DELTA << 16);
    }
    
    return reg_override_common(reg, old_val);
}

static inline uint32_t reg_override_UHD(uint32_t reg, uint32_t old_val)
{
    /* FPS timer A, for increasing horizontal resolution */
    /* trial and error to allow 3840; 536 is too low */
    int timerA =
    (video_mode_fps == 25) ? 547 :
    (video_mode_fps == 50) ? 546 :
    550 ;
    int timerB =
    (video_mode_fps == 24) ? 1820 :
    (video_mode_fps == 25) ? 1755 :
    (video_mode_fps == 30) ? 1456 :
    (video_mode_fps == 50) ?  879 :
    (video_mode_fps == 60) ?  728 :
    -1 ;
    
    int a = reg_override_fps(reg, timerA, timerB, old_val);
    if (a) return a;
    
    switch (reg)
    {
            /* raw resolution (end line/column) */
            /* X: (3840+140)/8 + 0x18, adjusted for 3840 in raw_rec */
        case 0xC0F06804:
            return (old_val & 0xFFFF0000) + 0x20A + (YRES_DELTA << 16);
    }
    
    return reg_override_common(reg, old_val);
}

static inline uint32_t reg_override_fullres_lv(uint32_t reg, uint32_t old_val)
{
    switch (reg)
    {
        case 0xC0F06800:
            return 0x10018;         /* raw start line/column, from photo mode */
            
        case 0xC0F06804:            /* 1080p 0x528011B, photo 0xF6E02FE */
            return (old_val & 0xFFFF0000) + 0x2FE + (YRES_DELTA << 16);
            
        case 0xC0F06824:
        case 0xC0F06828:
        case 0xC0F0682C:
        case 0xC0F06830:
            return 0x312;           /* from photo mode */
            
        case 0xC0F06010:            /* FPS timer A, for increasing horizontal resolution */
            return 0x317;           /* from photo mode; lower values give black border on the right */
            
        case 0xC0F06008:
        case 0xC0F0600C:
            return 0x3170317;
            
        case 0xC0F06014:
            return (video_mode_fps > 30 ? 856 : 1482) + YRES_DELTA;   /* up to 7.4 fps */
    }
    /* no need to adjust the black bar */
    return reg_override_HEAD34(reg, old_val);
}

/* just for testing */
/* (might be useful for FPS override on e.g. 70D) */
static inline uint32_t reg_override_40_fps(uint32_t reg, uint32_t old_val)
{
    switch (reg)
    {
        case 0xC0F06824:
        case 0xC0F06828:
        case 0xC0F0682C:
        case 0xC0F06830:
        case 0xC0F06010:
            return 0x18F;
            
        case 0xC0F06008:
        case 0xC0F0600C:
            return 0x18F018F;
            
        case 0xC0F06014:
            return 0x5DB;
    }
    
    return reg_override_bits(reg, old_val);
}

static inline uint32_t reg_override_1x3(uint32_t reg, uint32_t old_val)
{
    
    switch (reg)
    {
        case 0xC0F06804:
            return (set_25fps && ratios) ? 0x8d6011b + reg_6804_width + (reg_6804_height << 16):
        (set_25fps && !ratios) ? 0x8f8011b + reg_6804_width + (reg_6804_height << 16):
        (!set_25fps && !ratios) ? 0x962011b + reg_6804_width + (reg_6804_height << 16):
        0x93a011b + reg_6804_width + (reg_6804_height << 16);
        case 0xC0F0713c:
            return (set_25fps && ratios) ? 0x8d6 + reg_713c:
        (set_25fps && !ratios) ? 0x8f8 + reg_713c:
        (!set_25fps && !ratios) ? 0x94a + reg_713c:
        0x93a + reg_713c;
            
        case 0xC0F06014:
            return set_25fps ? 0x9bd + reg_6014: 0xa27 + reg_6014;
        case 0xC0F06008:
        case 0xC0F0600C:
            return 0x1800180 + reg_6008 + (reg_6008 << 16);
        case 0xC0F06010:
            return 0x180 + reg_6008;
    }
    
    return reg_override_bits(reg, old_val);
}

static inline uint32_t reg_override_anamorph_fullres(uint32_t reg, uint32_t old_val)
{
    switch (reg)
    {
        case 0xC0F06804:
            return 0xeee011b + reg_6804_width + (reg_6804_height << 16); /* 1920x3240(perfect 1920x1080) */
            
        case 0xC0F0713c:
            return 0xeee + reg_713c;
            
        case 0xC0F06014:
            return 0x103a + reg_6014;
        case 0xC0F06008:
        case 0xC0F0600C:
            return 0x1800180 + reg_6008 + (reg_6008 << 16);
        case 0xC0F06010:
            return 0x180 + reg_6008;
    }
    
    return reg_override_bits(reg, old_val);
}

static inline uint32_t reg_override_mv1080_mv720p(uint32_t reg, uint32_t old_val)
{
    
    switch (reg)
    {
        case 0xC0F06014: return 0x632 + reg_6014;
        case 0xC0F0600c: return 0x18d018d + reg_6008 + (reg_6008 << 16);
        case 0xC0F06008: return 0x18d018d + reg_6008 + (reg_6008 << 16);
        case 0xC0F06010: return 0x18d + reg_6008;
            
        case 0xC0F0713c: return 0x55e + reg_713c;
        case 0xC0F07150: return 0x527 + reg_7150;
    }
    return reg_override_bits(reg, old_val);
}

static inline uint32_t reg_override_x3(uint32_t reg, uint32_t old_val)
{
    return reg_override_bits(reg, old_val);
}

static inline uint32_t reg_override_fps_nocheck(uint32_t reg, uint32_t timerA, uint32_t timerB, uint32_t old_val)
{
    /* hardware register requires timer-1 */
    timerA--;
    timerB--;
    
    switch (reg)
    {
        case 0xC0F06824:
        case 0xC0F06828:
        case 0xC0F0682C:
        case 0xC0F06830:
        case 0xC0F06010:
        {
            return timerA;
        }
            
        case 0xC0F06008:
        case 0xC0F0600C:
        {
            return timerA | (timerA << 16);
        }
            
        case 0xC0F06014:
        {
            return timerB;
        }
    }
    
    return reg_override_bits(reg, old_val);
}

static inline uint32_t reg_override_zoom_fps(uint32_t reg, uint32_t old_val)
{
    /* attempt to reconfigure the x5 zoom at the FPS selected in Canon menu */
    int timerA =
    (video_mode_fps == 24) ? 512 :
    (video_mode_fps == 25) ? 512 :
    (video_mode_fps == 30) ? 520 :
    (video_mode_fps == 50) ? 512 :  /* cannot get 50, use 25 */
    (video_mode_fps == 60) ? 520 :  /* cannot get 60, use 30 */
    -1 ;
    int timerB =
    (video_mode_fps == 24) ? 1955 :
    (video_mode_fps == 25) ? 1875 :
    (video_mode_fps == 30) ? 1540 :
    (video_mode_fps == 50) ? 1875 :
    (video_mode_fps == 60) ? 1540 :
    -1 ;
    
    switch (reg)
    {
        case 0xC0F06804:
            return (video_mode_fps == 25) ?  0x6ba01eb + reg_6804_width + (reg_6804_height << 16): 0x70001eb + reg_6804_width + (reg_6804_height << 16);
            
        case 0xC0F0713c:
            return (video_mode_fps == 25) ? 0x6ba + reg_713c: 0x700 + reg_713c;
    }
     
    return reg_override_fps_nocheck(reg, timerA, timerB, old_val);
}


static inline uint32_t reg_override_zoom_fps_33k(uint32_t reg, uint32_t old_val)
{
    int timerA = 472 + reg_6008;
    int timerB = 2121 + reg_6014;
    
    // experimenting with expanding width and real time preview
            EngDrvOutLV(0xc0f383d4, 0x81448145 + reg_83d4);
            EngDrvOutLV(0xc0f383dc, 0x845c8270 + reg_83dc);
            EngDrvOutLV(0xC0F38024, 0x5630357 + reg_8024);
        
    switch (reg)
    {
        case 0xC0F06804:
            return 0x76a01c3 + reg_6804_width + (reg_6804_height << 16);

        case 0xC0F0713c:
            return 0x76a + reg_713c;
    }
    
    return reg_override_fps_nocheck(reg, timerA, timerB, old_val);
}

static int engio_vidmode_ok = 0;

static void * get_engio_reg_override_func()
{
    uint32_t (*reg_override_func)(uint32_t, uint32_t) =
    (crop_preset == CROP_PRESET_3X)         ? reg_override_x3     : /* fixme: corrupted image */
    (crop_preset == CROP_PRESET_3X_TALL)    ? reg_override_3X_tall    :
    (crop_preset == CROP_PRESET_3x3_1X_50p)     ? reg_override_3x3_50p   :
    (crop_preset == CROP_PRESET_3x3_1X_60p)     ? reg_override_3x3_60p   :
    (crop_preset == CROP_PRESET_3x3_1X_45p) ? reg_override_3x3_45p    :
    (crop_preset == CROP_PRESET_3x3_1X_48p) ? reg_override_3x3_48p    :
    (crop_preset == CROP_PRESET_3K)         ? reg_override_3K         :
    (crop_preset == CROP_PRESET_4K_HFPS)    ? reg_override_4K_hfps    :
    (crop_preset == CROP_PRESET_UHD)        ? reg_override_UHD        :
    (crop_preset == CROP_PRESET_40_FPS)     ? reg_override_40_fps     :
    (crop_preset == CROP_PRESET_FULLRES_LV) ? reg_override_fullres_lv :
    (crop_preset == CROP_PRESET_CENTER_Z)   ? reg_override_zoom_fps   :
    (crop_preset == CROP_PRESET_33K)        ? reg_override_zoom_fps_33k   :
    (crop_preset == CROP_PRESET_1x3)        ? reg_override_1x3 :
    (crop_preset == CROP_PRESET_anamorph_fullres)  ? reg_override_anamorph_fullres :
    (crop_preset == CROP_PRESET_mv1080_mv720p)    ? reg_override_mv1080_mv720p  :
    0                       ;
    return reg_override_func;
}

static void FAST engio_write_hook(uint32_t* regs, uint32_t* stack, uint32_t pc)
{
    uint32_t (*reg_override_func)(uint32_t, uint32_t) =
    get_engio_reg_override_func();
    
    if (!reg_override_func)
    {
        return;
    }
    
    /* cmos_vidmode_ok doesn't help;
     * we can identify the current video mode from 0xC0F06804 */
    for (uint32_t * buf = (uint32_t *) regs[0]; *buf != 0xFFFFFFFF; buf += 2)
    {
        uint32_t reg = *buf;
        uint32_t old = *(buf+1);
        if (reg == 0xC0F06804)
        {
            if (is_5D3)
            {
                engio_vidmode_ok = (crop_preset == CROP_PRESET_CENTER_Z || crop_preset == CROP_PRESET_33K)
                ? (old == 0x56601EB)                        /* x5 zoom */
                : (old == 0x528011B || old == 0x2B6011B);   /* 1080p or 720p */
            }
            
        }
    }
    
    if (!is_supported_mode() || !engio_vidmode_ok)
    {
        /* don't patch other video modes */
        return;
    }
    
    for (uint32_t * buf = (uint32_t *) regs[0]; *buf != 0xFFFFFFFF; buf += 2)
    {
        uint32_t reg = *buf;
        uint32_t old = *(buf+1);
        
        int new = reg_override_func(reg, old);
        if (new)
        {
            dbg_printf("[%x] %x: %x -> %x\n", regs[0], reg, old, new);
            *(buf+1) = new;
        }
    }
}

static int patch_active = 0;

static void update_patch()
{
    
    if (CROP_PRESET_MENU)
    {
        /* update preset */
        crop_preset = CROP_PRESET_MENU;
        
        /* install our hooks, if we haven't already do so */
        if (!patch_active)
        {
            patch_hook_function(CMOS_WRITE, MEM_CMOS_WRITE, &cmos_hook, "crop_rec: CMOS[1,2,6] parameters hook");
            patch_hook_function(ADTG_WRITE, MEM_ADTG_WRITE, &adtg_hook, "crop_rec: ADTG[8000,8806] parameters hook");
            if (ENGIO_WRITE)
            {
                patch_hook_function(ENGIO_WRITE, MEM_ENGIO_WRITE, engio_write_hook, "crop_rec: video timers hook");
            }
            
            patch_active = 1;
        }
    }
    else
    {
        /* undo active patches, if any */
        if (patch_active)
        {
            unpatch_memory(CMOS_WRITE);
            unpatch_memory(ADTG_WRITE);
            if (ENGIO_WRITE)
            {
                unpatch_memory(ENGIO_WRITE);
            }
            patch_active = 0;
            crop_preset = 0;
        }
    }
}

/* enable patch when switching LiveView (not in the middle of LiveView) */
/* otherwise you will end up with a halfway configured video mode that looks weird */
PROP_HANDLER(PROP_LV_ACTION)
{
    update_patch();
}

/* also try when switching zoom modes */
PROP_HANDLER(PROP_LV_DISPSIZE)
{
    update_patch();
}

static MENU_UPDATE_FUNC(crop_update)
{
    if (CROP_PRESET_MENU && lv)
    {
        if (CROP_PRESET_MENU == CROP_PRESET_CENTER_Z || CROP_PRESET_MENU ==  CROP_PRESET_33K)
        {
            if (lv_dispsize == 1)
            {
                MENU_SET_WARNING(MENU_WARN_NOT_WORKING, "To use this mode, exit ML menu & press the zoom button (set to x5/x10).");
            }
        }
        else /* non-zoom modes */
        {
            if (!is_supported_mode())
            {
                MENU_SET_WARNING(MENU_WARN_NOT_WORKING, "This preset only works in 1080p and 720p video modes.");
            }
            else if (lv_dispsize != 1)
            {
                MENU_SET_WARNING(MENU_WARN_NOT_WORKING, "To use this mode, exit ML menu and press the zoom button (set to x1).");
            }
            else if (!is_720p())
            {
                if (CROP_PRESET_MENU == CROP_PRESET_3x3_1X ||
                    CROP_PRESET_MENU == CROP_PRESET_3x3_1X_50p ||
                    CROP_PRESET_MENU == CROP_PRESET_3x3_1X_60p ||
                    CROP_PRESET_MENU == CROP_PRESET_3x3_1X_45p ||
                    CROP_PRESET_MENU == CROP_PRESET_3x3_1X_48p)
                {
                    /* these presets only have effect in 720p mode */
                    MENU_SET_WARNING(MENU_WARN_NOT_WORKING, "This preset only works in the 720p 50/60 fps modes from Canon menu.");
                    return;
                }
            }
        }
    }
}

static MENU_UPDATE_FUNC(target_yres_update)
{
    MENU_SET_RINFO("from %d", max_resolutions[crop_preset][get_video_mode_index()]);
}

static struct menu_entry crop_rec_menu[] =
{
    {
        .name       = "Presets",
        .priv       = &crop_preset_index,
        .update     = crop_update,
        .depends_on = DEP_LIVEVIEW,
        .children =  (struct menu_entry[]) {
            {
                .name   = "set 25fps",
                .priv   = &set_25fps,
                .max    = 1,
                .choices = CHOICES("OFF", "ON"),
                .help   = "OFF = default 24fps, ON = 25fps (anamorphic)",
                .help2  = "only anamorphic. Change to 25fps in canon menu for other presets\n"
            },
            {
                .name   = "focus aid",
                .priv   = &zoomaid,
                .max    = 1,
                .choices = CHOICES("OFF", "default mode"),
                .help   = "x10 zoom when pressing halfshutter(all presets, manual focus)",
            },
            {
                .name   = "x3crop",
                .priv   = &x3crop,
                .max    = 1,
                .choices = CHOICES("OFF", "ON"),
                .help   = "Turns 45/48/50/60 fps into x3 crop modes)",
            },
            {
                .name   = "iso climb",
                .priv   = &isoclimb,
                .max    = 1,
                .choices = CHOICES("OFF", "ON"),
                .help   = "Fast access to iso (NOT working with autoiso)",
                .help2  = "push up/down to climb between 100-3200 iso\n"
            },
            {
                .name   = "max iso",
                .priv   = &isoauto,
                .max    = 3,
                .choices = CHOICES("OFF", "400", "800", "1600"),
                .help   = "Restrict autoiso to max 400/800/1600",
                .help2  = "Select max iso. Turn on autoiso\n"
            },
            {
                .name   = "frame stop",
                .priv   = &framestop,
                .max    = 2,
                .choices = CHOICES("OFF", "2 frames", "3 frames"),
                .help   =  "cut recording after 2 or 3 frames",
            },
            {
                .name   = "frame burst",
                .priv   = &frameburst,
                .min    = 0,
                .max    = 10000,
                .unit   = UNIT_DEC,
                .help   =  "records specified amount of frames then stops",
            },
            {
                .name   = "iso average",
                .priv   = &isoaverage,
                .max    = 4,
                .choices = CHOICES("OFF", "iso100/400/1600", "iso200/800/3200", "+iso100/400/1600", "+iso200/800/3200"),
                .help   =  "(5D3 only)3 isos 100,400,1600 or iso 200,800,1600, 20 or 31(+) frames",
            },
            {
                .name   = "reg_713c",
                .priv   = &reg_713c,
                .min    = -2000,
                .max    = 2000,
                .unit   = UNIT_DEC,
                .help  = "Corruption? Combine with reg_7150",
                .advanced = 1,
            },
            {
                .name   = "reg_7150",
                .priv   = &reg_7150,
                .min    = -2000,
                .max    = 2000,
                .unit   = UNIT_DEC,
                .help  = "Corruption issues? Combine with reg_713c",
                .advanced = 1,
            },
            {
                .name   = "reg_6014",
                .priv   = &reg_6014,
                .min    = -100000,
                .max    = 100000,
                .unit   = UNIT_DEC,
                .help  = "Alter frame rate. Combine with reg_6008",
                .advanced = 1,
            },
            {
                .name   = "reg_6008",
                .priv   = &reg_6008,
                .min    = -10000,
                .max    = 10000,
                .unit   = UNIT_DEC,
                .help  = "Alter frame rate. Combine with reg_6014",
                .advanced = 1,
            },
            {
                .name   = "reg_800c",
                .priv   = &reg_800c,
                .min    = -500,
                .max    = 500,
                .unit   = UNIT_DEC,
                .help  = "line skipping",
                .advanced = 1,
            },
            {
                .name   = "reg_8000",
                .priv   = &reg_8000,
                .min    = -500,
                .max    = 500,
                .unit   = UNIT_DEC,
                .help  = "x3zoom",
                .advanced = 1,
            },
            {
                .name   = "reg_8183",
                .priv   = &reg_8183,
                .min    = -2000,
                .max    = 2000,
                .unit   = UNIT_DEC,
                .help  = "Aliasing, moiré mcm rewired mode",
                .advanced = 1,
            },
            {
                .name   = "reg_8184",
                .priv   = &reg_8184,
                .min    = -2000,
                .max    = 2000,
                .unit   = UNIT_DEC,
                .help  = "Aliasing, moiré mcm rewired mode",
                .advanced = 1,
            },
            {
                .name   = "reg_6800_height",
                .priv   = &reg_6800_height,
                .min    = -2000,
                .max    = 2000,
                .unit   = UNIT_DEC,
                .help  = "height offset",
                .advanced = 1,
            },
            {
                .name   = "reg_6800_width",
                .priv   = &reg_6800_width,
                .min    = -2000,
                .max    = 2000,
                .unit   = UNIT_DEC,
                .help  = "width offset",
                .advanced = 1,
            },
            {
                .name   = "reg_6804_height",
                .priv   = &reg_6804_height,
                .min    = -2000,
                .max    = 2000,
                .unit   = UNIT_DEC,
                .help  = "Alter height.",
                .advanced = 1,
            },
            {
                .name   = "reg_6804_width",
                .priv   = &reg_6804_width,
                .min    = -2000,
                .max    = 2000,
                .unit   = UNIT_DEC,
                .help  = "Alter width. Scrambles preview",
                .advanced = 1,
            },
            {
                .name   = "reg_83d4",
                .priv   = &reg_83d4,
                .min    = -500,
                .max    = 500,
                .unit   = UNIT_DEC,
                .help  = "Preview engdrvout",
                .advanced = 1,
            },
            {
                .name   = "reg_83dc",
                .priv   = &reg_83dc,
                .min    = -500,
                .max    = 500,
                .unit   = UNIT_DEC,
                .help  = "Preview engdrvout",
                .advanced = 1,
            },
            {
                .name   = "reg_8024",
                .priv   = &reg_8024,
                .min    = -500,
                .max    = 500,
                .unit   = UNIT_DEC,
                .help  = "Preview engdrvout",
                .advanced = 1,
            },
            {
                .name   = "CMOS[1]",
                .priv   = &cmos1,
                .max    = 0xFFF,
                .unit   = UNIT_HEX,
                .help   = "Vertical offset",
                .advanced = 1,
            },
            {
                .name   = "CMOS[2]",
                .priv   = &cmos2,
                .max    = 0xFFF,
                .unit   = UNIT_HEX,
                .help   = "Horizontal offset",
                .advanced = 1,
            },
            {
                .name   = "CMOS[3]",
                .priv   = &cmos3,
                .max    = 0xFFF,
                .unit   = UNIT_HEX,
                .help   = "Analog iso on 6D",
                .advanced = 1,
            },
            {
                .name   = "CMOS[4]",
                .priv   = &cmos4,
                .max    = 0xFFF,
                .unit   = UNIT_HEX,
                .help   = "Hot/cold pixel",
                .advanced = 1,
            },
            {
                .name   = "CMOS[5]",
                .priv   = &cmos5,
                .max    = 0xFFF,
                .unit   = UNIT_HEX,
                .help   = "Fine vertical offset, black area maybe",
                .advanced = 1,
            },
            {
                .name   = "CMOS[6]",
                .priv   = &cmos6,
                .max    = 0xFFF,
                .unit   = UNIT_HEX,
                .help   = "Iso 50 or timing related",
                .advanced = 1,
            },
            {
                .name   = "CMOS[7]",
                .priv   = &cmos7,
                .max    = 0xFFF,
                .unit   = UNIT_HEX,
                .help   = "Image fading out; 6D, 700D: vertical offset",
                .advanced = 1,
            },
            {
                .name   = "CMOS[8]",
                .priv   = &cmos8,
                .max    = 0xFFF,
                .unit   = UNIT_HEX,
                .help   = "Unknown, used on 6D",
                .help2  = "Use for horizontal centering.",
                .advanced = 1,
            },
            {
                .name   = "CMOS[9]",
                .priv   = &cmos9,
                .max    = 0xFFF,
                .unit   = UNIT_HEX,
                .help   = "?",
                .advanced = 1,
            },
            {
                .name   = "reg_gain",
                .priv   = &reg_gain,
                .min    = -500,
                .max    = 500,
                .unit   = UNIT_DEC,
                .help  = "Alter bit depth",
                .advanced = 1,
            },
            {
                .name   = "reg_timing1",
                .priv   = &reg_timing1,
                .min    = -500,
                .max    = 500,
                .unit   = UNIT_DEC,
                .help  = "PowSaveTim reg 8172, 8178, 8196",
                .advanced = 1,
            },
            {
                .name   = "reg_timing2",
                .priv   = &reg_timing2,
                .min    = -500,
                .max    = 500,
                .unit   = UNIT_DEC,
                .help  = "PowSaveTim reg 8179, 8197",
                .advanced = 1,
            },
            {
                .name   = "reg_timing3",
                .priv   = &reg_timing3,
                .min    = -500,
                .max    = 500,
                .unit   = UNIT_DEC,
                .help  = "PowSaveTim reg 8173",
                .advanced = 1,
            },
            {
                .name   = "reg_timing4",
                .priv   = &reg_timing4,
                .min    = -500,
                .max    = 500,
                .unit   = UNIT_DEC,
                .help  = "PowSaveTim reg 82f8, 82f9",
                .advanced = 1,
            },
            {
                .name   = "reg_timing5",
                .priv   = &reg_timing5,
                .min    = -500,
                .max    = 500,
                .unit   = UNIT_DEC,
                .help  = "PowSaveTim reg 6014",
                .advanced = 1,
            },
            {
                .name   = "reg_timing6",
                .priv   = &reg_timing6,
                .min    = -500,
                .max    = 500,
                .unit   = UNIT_DEC,
                .help  = "PowSaveTim reg 82b6",
                .advanced = 1,
            },
            {
                .name   = "reg_6824",
                .priv   = &reg_6824,
                .min    = -500,
                .max    = 500,
                .unit   = UNIT_DEC,
                .help  = "PowSaveTim reg 6824, 6828, 682c, 6830",
                .advanced = 1,
            },
            {
                .name   = "reg_skip_left",
                .priv   = &reg_skip_left,
                .min    = -1000,
                .max    = 1000,
                .unit   = UNIT_DEC,
                .help  = "skip left",
                .advanced = 1,
            },
            {
                .name   = "reg_skip_right",
                .priv   = &reg_skip_right,
                .min    = -1000,
                .max    = 1000,
                .unit   = UNIT_DEC,
                .help  = "skip right",
                .advanced = 1,
            },
            {
                .name   = "reg_skip_top",
                .priv   = &reg_skip_top,
                .min    = -1000,
                .max    = 1000,
                .unit   = UNIT_DEC,
                .help  = "skip top",
                .advanced = 1,
            },
            {
                .name   = "reg_skip_bottom",
                .priv   = &reg_skip_bottom,
                .min    = -1000,
                .max    = 1000,
                .unit   = UNIT_DEC,
                .help  = "skip bottom",
                .advanced = 1,
            },
            {
                .name   = "reg_bl",
                .priv   = &reg_bl,
                .min    = -1000,
                .max    = 1000,
                .unit   = UNIT_DEC,
                .help  = "black level for reduced bitdepths(not affecting recordings)",
                .advanced = 1,
            },
            {
             .name   = "CMOS[1] lo",
             .priv   = &cmos1_lo,
             .max    = 63,
             .unit   = UNIT_DEC,
             .help   = "Start scanline (very rough). Use for vertical positioning.",
             .advanced = 1,
             },
             {
             .name   = "CMOS[1] hi",
             .priv   = &cmos1_hi,
             .max    = 63,
             .unit   = UNIT_DEC,
             .help   = "End scanline (very rough). Increase if white bar at bottom.",
             .help2  = "Decrease if you get strange colors as you move the camera.",
             .advanced = 1,
             },
            /* not used atm
             {
             .name   = "Target YRES",
             .priv   = &target_yres,
             .update = target_yres_update,
             .max    = 3870,
             .unit   = UNIT_DEC,
             .help   = "Desired vertical resolution (only for presets with higher resolution).",
             .help2  = "Decrease if you get corrupted frames (dial the desired resolution here).",
             .advanced = 1,
             },
             {
             .name   = "Delta ADTG 0",
             .priv   = &delta_adtg0,
             .min    = -500,
             .max    = 500,
             .unit   = UNIT_DEC,
             .help   = "ADTG 0x8178, 0x8196, 0x82F8",
             .help2  = "May help pushing the resolution a little. Start with small increments.",
             .advanced = 1,
             },
             {
             .name   = "Delta ADTG 1",
             .priv   = &delta_adtg1,
             .min    = -500,
             .max    = 500,
             .unit   = UNIT_DEC,
             .help   = "ADTG 0x8179, 0x8197, 0x82F9",
             .help2  = "May help pushing the resolution a little. Start with small increments.",
             .advanced = 1,
             },
             {
             .name   = "Delta HEAD3",
             .priv   = &delta_head3,
             .min    = -500,
             .max    = 500,
             .unit   = UNIT_DEC,
             .help2  = "May help pushing the resolution a little. Start with small increments.",
             .advanced = 1,
             },
             {
             .name   = "Delta HEAD4",
             .priv   = &delta_head4,
             .min    = -500,
             .max    = 500,
             .unit   = UNIT_DEC,
             .help2  = "May help pushing the resolution a little. Start with small increments.",
             .advanced = 1,
             }, */
            MENU_ADVANCED_TOGGLE,
            MENU_EOL,
        },
    },
};


static struct menu_entry shutter_range_menu[] =
{
    {
                .name       = "Shutter range",
                .priv       = &shutter_range,
                .max        = 1,
                .choices    = CHOICES("Original", "Full range"),
                .help       = "Full range = from 1fps and faster ",
                .help2      = "Original: default range used by Canon in selected video mode.\n"
    },
};

static struct menu_entry bitdepth_menu[] =
{
    {
    .name   = "Bitdepth",
    .priv   = &bitdepth,
    .max    = 4,
    .choices = CHOICES("OFF", "8 bit", "9 bit", "10 bit", "12 bit"),
    .help   = "Alter bitdepth\n",
    .help2  = "Not active when crop rec set to OFF\n"
    },
};

    static struct menu_entry ratios_menu[] =
    {
        {
        .name   = "Ratio",
        .priv   = &ratios,
        .max    = 3,
        .choices = CHOICES("OFF", "2.39:1", "2.35:1", "16:9"),
        .help       = "Access three global ratios",
        .help2   = "only anamorphic preset\n"
        },
    };

static unsigned int crop_rec_keypress_cbr(unsigned int key)
{
    
    /* iso climbing feature */
    if (is_5D3 && isoclimb != 0x0 && isopatch && !gui_menu_shown() && is_movie_mode() && lens_info.raw_iso != 0x0 && (key == MODULE_KEY_PRESS_UP || key == MODULE_KEY_PRESS_DOWN))
    {
        isopatch = 0;
        isopatchoff = 0;
        if (shamem_read(0xC0F0b12c) == 0x11)
        {
            if (key == MODULE_KEY_PRESS_UP) isoclimb = 0x2;
            if (key == MODULE_KEY_PRESS_DOWN)
            {
                NotifyBox(1000, "iso 100 reached!");
                return 0;
            }
        }
        else if (shamem_read(0xC0F0b12c) == 0x12)
        {
            if (key == MODULE_KEY_PRESS_UP) isoclimb = 0x3;
            if (key == MODULE_KEY_PRESS_DOWN) isoclimb = 0x1;
        }
        else if (shamem_read(0xC0F0b12c) == 0x13)
        {
            if (key == MODULE_KEY_PRESS_UP) isoclimb = 0x4;
            if (key == MODULE_KEY_PRESS_DOWN) isoclimb = 0x2;
        }
        else if (shamem_read(0xC0F0b12c) == 0x14)
        {
            if (key == MODULE_KEY_PRESS_UP) isoclimb = 0x5;
            if (key == MODULE_KEY_PRESS_DOWN) isoclimb = 0x3;
        }
        else if (shamem_read(0xC0F0b12c) == 0x15)
        {
            if (key == MODULE_KEY_PRESS_UP) isoclimb = 0x6;
            if (key == MODULE_KEY_PRESS_DOWN) isoclimb = 0x4;
        }
        else if (shamem_read(0xC0F0b12c) == 0x16)
        {
            if (key == MODULE_KEY_PRESS_UP) isoclimb = 0x7;
            if (key == MODULE_KEY_PRESS_DOWN) isoclimb = 0x5;
        }
        else if (shamem_read(0xC0F0b12c) == 0x17)
        {
            if (key == MODULE_KEY_PRESS_UP)
            {
                NotifyBox(1000, "iso 6400 reached!");
                return 0;
            }
            if (key == MODULE_KEY_PRESS_DOWN) isoclimb = 0x6;
        }
        return 0;
    }
    
    return 1;
}


static int crop_rec_needs_lv_refresh()
{
    if (!lv)
    {
        return 0;
    }
    
    if (ratios == 1) menu_set_str_value_from_script("RAW video", "Aspect ratio", "2.39:1", 5);
    if (ratios == 2) menu_set_str_value_from_script("RAW video", "Aspect ratio", "2.35:1", 6);
    if (ratios == 3) menu_set_str_value_from_script("RAW video", "Aspect ratio", "16:9", 10);
    
    /* We don´t want this when in photo mode I assume */
    if (!is_movie_mode()) return 0;
    
    /* let´s automate liveview start off setting */
    if (CROP_PRESET_MENU == CROP_PRESET_CENTER_Z || CROP_PRESET_MENU ==  CROP_PRESET_33K)
    {
        info_led_on();
        gui_uilock(UILOCK_EVERYTHING);
        set_lv_zoom(5);
        gui_uilock(UILOCK_NONE);
        info_led_off();
    }
    
    if (CROP_PRESET_MENU)
    {
        if (is_supported_mode())
        {
            if (!patch_active || CROP_PRESET_MENU != crop_preset || is_5D3)
            {
                return 1;
            }
        }
    }
    else /* crop disabled */
    {
        if (patch_active)
        {
            return 1;
        }
    }
    
    return 0;
}

static void center_canon_preview()
{
    /* center the preview window on the raw buffer */
    /* overriding these registers once will do the trick...
     * ... until the focus box is moved by the user */
    int old = cli();
    
    uint32_t pos1 = shamem_read(0xc0f383d4);
    uint32_t pos2 = shamem_read(0xc0f383dc);
    
    if ((pos1 & 0x80008000) == 0x80008000 &&
        (pos2 & 0x80008000) == 0x80008000)
    {
        /* already centered */
        sei(old);
        return;
    }
    
    int x1 = pos1 & 0xFFFF;
    int x2 = pos2 & 0xFFFF;
    int y1 = pos1 >> 16;
    int y2 = pos2 >> 16;
    
    if (x2 - x1 != 299 && y2 - y1 != 792)
    {
        /* not x5/x10 (values hardcoded for 5D3) */
        sei(old);
        return;
    }
    
    int raw_xc = (146 + 3744) / 2 / 4;  /* hardcoded for 5D3 */
    int raw_yc = ( 60 + 1380) / 2;      /* values from old raw.c */
    
    if (1)
    {
        /* use the focus box position for moving the preview window around */
        /* don't do that while recording! */
        dbg_printf("[crop_rec] %d,%d ", raw_xc, raw_yc);
        raw_xc -= 146 / 2 / 4;  raw_yc -= 60 / 2;
        /* this won't change the position if the focus box is centered */
        get_afframe_pos(raw_xc * 2, raw_yc * 2, &raw_xc, &raw_yc);
        raw_xc += 146 / 2 / 4;  raw_yc += 60 / 2;
        raw_xc &= ~1;   /* just for consistency */
        raw_yc &= ~1;   /* this must be even, otherwise the image turns pink */
        raw_xc = COERCE(raw_xc, 176, 770);  /* trial and error; image pitch changes if we push to the right */
        raw_yc = COERCE(raw_yc, 444, 950);  /* trial and error; broken image at the edges, outside these limits */
        dbg_printf("-> %d,%d using focus box position\n", raw_xc, raw_yc);
    }
    int current_xc = (x1 + x2) / 2;
    int current_yc = (y1 + y2) / 2;
    int dx = raw_xc - current_xc;
    int dy = raw_yc - current_yc;
    
    if (dx || dy)
    {
        /* note: bits 0x80008000 appear to have no effect,
         * so we'll use them to flag the centered zoom mode,
         * e.g. for focus_box_get_raw_crop_offset */
        dbg_printf("[crop_rec] centering zoom preview: dx=%d, dy=%d\n", dx, dy);
        EngDrvOutLV(0xc0f383d4, PACK32(x1 + dx, y1 + dy) | 0x80008000);
        EngDrvOutLV(0xc0f383dc, PACK32(x2 + dx, y2 + dy) | 0x80008000);
    }
    
    sei(old);
}

/* faster version than the one from ML core */
static void set_zoom(int zoom)
{
    if (RECORDING) return;
    if (is_movie_mode() && video_mode_crop) return;
    zoom = COERCE(zoom, 1, 10);
    if (zoom > 1 && zoom < 10) zoom = 5;
    prop_request_change_wait(PROP_LV_DISPSIZE, &zoom, 4, 1000);
}

static void iso()
{
    if (RECORDING) return;
    
    if (isoclimb == 0x1 && lens_info.raw_iso != 0x48) menu_set_str_value_from_script("Expo", "ISO", "100", 1);
    if (isoclimb == 0x2 && lens_info.raw_iso != 0x50) menu_set_str_value_from_script("Expo", "ISO", "200", 1);
    if (isoclimb == 0x3 && lens_info.raw_iso != 0x58) menu_set_str_value_from_script("Expo", "ISO", "400", 1);
    if (isoclimb == 0x4 && lens_info.raw_iso != 0x60) menu_set_str_value_from_script("Expo", "ISO", "800", 1);
    if (isoclimb == 0x5 && lens_info.raw_iso != 0x68) menu_set_str_value_from_script("Expo", "ISO", "1600", 1);
    if (isoclimb == 0x6 && lens_info.raw_iso != 0x70) menu_set_str_value_from_script("Expo", "ISO", "3200", 1);
    if (isoclimb == 0x7 && lens_info.raw_iso != 0x78) menu_set_str_value_from_script("Expo", "ISO", "6400", 1);
    
    if (lens_info.raw_iso == 0x80)
    {
        menu_set_str_value_from_script("Expo", "ISO", "6400", 1);
        isoclimb = 0x7;
    }
}


/* when closing ML menu, check whether we need to refresh the LiveView */
static unsigned int crop_rec_polling_cbr(unsigned int unused)
{
    if (CROP_PRESET_MENU == CROP_PRESET_OFF && bitdepth && !gui_menu_shown())
    {
        bitdepth = 0;
        NotifyBox(3000, "Movie tab bitdepth reset when crop rec OFF");
    }
    
    
    /* refresh canon menu iso while not recording. Caveat. Flicker but better than nothong */
    if (isoclimb && is_movie_mode())
    {
        iso();
    }
   
/* reset after coming from FPS override or serious hanging will occur */
if (CROP_PRESET_MENU == CROP_PRESET_1x3 || CROP_PRESET_MENU == CROP_PRESET_anamorph_fullres)
{
    int fpz = fps_get_current_x1000();
    
    if (fpz == 0x6b09 && CROP_PRESET_MENU == CROP_PRESET_1x3)
    {
        EngDrvOutLV(0xC0F06014, 0xa27);
    }
    
    if (fpz == 0x6b09 && CROP_PRESET_MENU == CROP_PRESET_anamorph_fullres)
    {
        EngDrvOutLV(0xC0F06014, 0x103a);
    }
}

    //NotifyBox(2000, "lens_info.raw_iso_auto 0x%x", lens_info.raw_iso_auto);
    
    /* For when in photo mode and enabled x10 zoom mode */
    if ((zoomaid && !is_movie_mode()) || (is_5D3 && !RECORDING && zoomaid))
    {
        static int last_hs_photo = 0;
        static int photo = 0;
        
        if (!get_halfshutter_pressed()) last_hs_photo = get_ms_clock();
        if (get_ms_clock() - last_hs_photo > 500 && get_halfshutter_pressed())
        {
            set_lv_zoom(10);
            photo = 1;
            while (get_halfshutter_pressed())
            {
                msleep(10);
            }
        }
        
        if (!get_halfshutter_pressed() && photo)
        {
            set_lv_zoom(1);
            photo = 0;
            return 0;
        }
        
    }
    
    static int once1 = 1;
    if (once1 && (zoomaid == 0x1 || zoomaid == 0x2) && !is_movie_mode())
    {
        once1 = 0;
        NotifyBox(4000, "Crop mode x10 halfshutter focus aid active");
    }
    /* reset this notification once back in movie mode */
    if (is_movie_mode())
    {
        once1 = 1;
    }
    
    /* We don´t want this when in photo mode I assume */
    if (!is_movie_mode() || CROP_PRESET_MENU == CROP_PRESET_OFF) return 0;
    
    /* also check at startup */
    static int lv_dirty = 1;
    
    
    int menu_shown = gui_menu_shown();
    if (lv && menu_shown)
    {
        lv_dirty = 1;
    }
    
    if (!lv || menu_shown || RECORDING_RAW)
    {
        /* outside LV: no need to do anything */
        /* don't change while browsing the menu, but shortly after closing it */
        /* don't change while recording raw, but after recording stops
         * (H.264 should tolerate this pretty well, except maybe 50D) */
        return CBR_RET_CONTINUE;
    }
    
    if (lv_dirty)
    {
        /* do we need to refresh LiveView? */
        if (crop_rec_needs_lv_refresh())
        {
            /* let's check this once again, just in case */
            /* (possible race condition that would result in unnecessary refresh) */
            msleep(500);
            if (crop_rec_needs_lv_refresh())
            {
                info_led_on();
                gui_uilock(UILOCK_EVERYTHING);
                int old_zoom = lv_dispsize;
                set_zoom(lv_dispsize == 1 ? 5 : 1);
                set_zoom(old_zoom);
                gui_uilock(UILOCK_NONE);
                info_led_off();
            }
        }
        lv_dirty = 0;
    }
    
    if ((crop_preset == CROP_PRESET_CENTER_Z || crop_preset ==  CROP_PRESET_33K) &&
        (lv_dispsize == 5 || lv_dispsize == 10))
    {
        center_canon_preview();
    }
    
    /* mv720p modes 5D3. Needs refreshing if iterating through x5,x10 mode */
    
    static int patch = 0;
    
    if (((crop_preset == CROP_PRESET_3x3_1X || crop_preset == CROP_PRESET_3x3_1X_50p ||
          crop_preset == CROP_PRESET_3x3_1X_60p || crop_preset == CROP_PRESET_3x3_1X_48p ||
          crop_preset == CROP_PRESET_3x3_1X_45p)) && lv_dispsize == 5)
    {
        /* workaround for x5,x10 back to mv1080p mode without freezing */
        patch = 1;
    }
    
    if (((crop_preset == CROP_PRESET_3x3_1X || crop_preset == CROP_PRESET_3x3_1X_50p ||
          crop_preset == CROP_PRESET_3x3_1X_60p || crop_preset == CROP_PRESET_3x3_1X_48p ||
          crop_preset == CROP_PRESET_3x3_1X_45p) && patch) && lv_dispsize == 1)
    {
        patch = 0;
        patch_active = 0;
        
        info_led_on();
        gui_uilock(UILOCK_EVERYTHING);
        int old_zoom = lv_dispsize;
        set_zoom(lv_dispsize == 1 ? 5 : 1);
        set_zoom(old_zoom);
        gui_uilock(UILOCK_NONE);
        info_led_off();
    }
    
    
    if (!crop_patch2 && get_halfshutter_pressed() && (zoomaid == 0x1 || zoomaid == 0x2) && !is_5D3)
    {
        /* zoomaid */
        if (get_halfshutter_pressed() && (zoomaid == 0x1 || zoomaid == 0x2))
        {
                        
            crop_patch2 = 1;
            if ((zoomaid == 0x1 || zoomaid == 0x2)) set_lv_zoom(10);
            while (get_halfshutter_pressed())
            {
                msleep(10);
            }
        }
        
        if (!get_halfshutter_pressed() && crop_patch2)
        {
            crop_patch2 = 0;
                    info_led_on();
                    gui_uilock(UILOCK_EVERYTHING);
                    int old_zoom = lv_dispsize;
                    set_zoom(lv_dispsize == 1 ? 5 : 1);
                    set_zoom(old_zoom);
                    gui_uilock(UILOCK_NONE);
                    info_led_off();
                    set_lv_zoom(1);
        }
    }
    
    
    if (zoomaid) crop_patch = 0; //disable patch while off
    
    if ((CROP_PRESET_MENU == CROP_PRESET_CENTER_Z || CROP_PRESET_MENU ==  CROP_PRESET_33K) && lv_dispsize == 1)
    {
        info_led_on();
        gui_uilock(UILOCK_EVERYTHING);
        int old_zoom = lv_dispsize;
        set_zoom(lv_dispsize == 1 ? 5 : 1);
        set_zoom(old_zoom);
        gui_uilock(UILOCK_NONE);
        info_led_off();
        set_lv_zoom(5);
    }
    return CBR_RET_CONTINUE;
}

/* Display recording status in top info bar */
static LVINFO_UPDATE_FUNC(crop_info)
{
    LVINFO_BUFFER(16);
    
    if (patch_active)
    {
        if (lv_dispsize > 1)
        {
            switch (crop_preset)
            {
                case CROP_PRESET_CENTER_Z:
                    snprintf(buffer, sizeof(buffer), "3.5K");
                    break;
                    
                    case CROP_PRESET_33K:
                        snprintf(buffer, sizeof(buffer), "3.3K");
                        break;
            }
        }
    }
    
    /* 5D3 */
    if (CROP_PRESET_MENU == CROP_PRESET_3x3_1X || CROP_PRESET_MENU == CROP_PRESET_3x3_1X_50p || CROP_PRESET_MENU == CROP_PRESET_3x3_1X_60p)
    {
        snprintf(buffer, sizeof(buffer), "3x3 mv1080p");
    }
    if (CROP_PRESET_MENU == CROP_PRESET_3X)
    {
        snprintf(buffer, sizeof(buffer), "1x1 mv1080p");
    }
    if (CROP_PRESET_MENU == CROP_PRESET_3X_TALL)
    {
        snprintf(buffer, sizeof(buffer), "3x_Tall");
    }
    if (CROP_PRESET_MENU == CROP_PRESET_3x3_1X_48p)
    {
        snprintf(buffer, sizeof(buffer), "mv1050p 48fps");
    }
    if (CROP_PRESET_MENU == CROP_PRESET_3x3_1X_45p)
    {
        snprintf(buffer, sizeof(buffer), "mv1080p 45fps");
    }
    if (CROP_PRESET_MENU == CROP_PRESET_3K)
    {
        snprintf(buffer, sizeof(buffer), "3K");
    }
    
    if (CROP_PRESET_MENU ==CROP_PRESET_UHD)
    {
        snprintf(buffer, sizeof(buffer), "UHD");
    }
    
    if (CROP_PRESET_MENU == CROP_PRESET_mv1080_mv720p)
    {
        snprintf(buffer, sizeof(buffer), "passthrough");
    }
    
    if (CROP_PRESET_MENU == CROP_PRESET_1x3)
    {
        if (ratios == 0x1)
        {
            snprintf(buffer, sizeof(buffer), "anamorph 2.39:1");
        }
        if (ratios == 0x2)
        {
            snprintf(buffer, sizeof(buffer), "anamorph 2.35:1");
        }
        if (ratios == 0x3)
        {
            snprintf(buffer, sizeof(buffer), "anamorph 16:9");
        }
        if (ratios == 0x0)
        {
            snprintf(buffer, sizeof(buffer), "anamorphic");
        }
    }
    
    if (CROP_PRESET_MENU == CROP_PRESET_anamorph_fullres)
    {
        snprintf(buffer, sizeof(buffer), "anamorphic fullres");
    }
    
    if (CROP_PRESET_MENU == CROP_PRESET_FULLRES_LV)
    {
        snprintf(buffer, sizeof(buffer), "fullres");
    }
    
}

static struct lvinfo_item info_items[] = {
    {
        .name = "Crop info",
        .which_bar = LV_BOTTOM_BAR_ONLY,
        .update = crop_info,
        .preferred_position = -50,  /* near the focal length display */
        .priority = 1,
    }
};

static unsigned int raw_info_update_cbr(unsigned int unused)
{
    if (patch_active)
    {
        /* not implemented yet */
        raw_capture_info.offset_x = raw_capture_info.offset_y   = SHRT_MIN;
        
        if (lv_dispsize > 1)
        {
            /* raw backend gets it right */
            return 0;
        }
        
        /* update horizontal pixel binning parameters */
        switch (crop_preset)
        {
            case CROP_PRESET_3X:
            case CROP_PRESET_3X_TALL:
            case CROP_PRESET_3K:
            case CROP_PRESET_4K_HFPS:
            case CROP_PRESET_UHD:
            case CROP_PRESET_FULLRES_LV:
            case CROP_PRESET_3x1:
                raw_capture_info.binning_x    = raw_capture_info.binning_y  = 1;
                raw_capture_info.skipping_x   = raw_capture_info.skipping_y = 0;
                break;
                
            case CROP_PRESET_3x3_1X:
            case CROP_PRESET_3x3_1X_50p:
            case CROP_PRESET_3x3_1X_60p:
            case CROP_PRESET_3x3_1X_48p:
            case CROP_PRESET_3x3_1X_45p:
            case CROP_PRESET_1x3:
            case CROP_PRESET_anamorph_fullres:
                raw_capture_info.binning_x = 3; raw_capture_info.skipping_x = 0;
                break;
        }
        
        /* update vertical pixel binning / line skipping parameters */
        switch (crop_preset)
        {
            case CROP_PRESET_3X:
            case CROP_PRESET_3X_TALL:
            case CROP_PRESET_3K:
            case CROP_PRESET_4K_HFPS:
            case CROP_PRESET_UHD:
            case CROP_PRESET_FULLRES_LV:
            case CROP_PRESET_1x3:
            case CROP_PRESET_anamorph_fullres:
                raw_capture_info.binning_y = 1; raw_capture_info.skipping_y = 0;
                break;
                
            case CROP_PRESET_3x3_1X:
            case CROP_PRESET_3x3_1X_50p:
            case CROP_PRESET_3x3_1X_60p:
            case CROP_PRESET_3x3_1X_48p:
            case CROP_PRESET_3x3_1X_45p:
            case CROP_PRESET_3x1:
            {
                int b = (is_5D3) ? 3 : 1;
                int s = (is_5D3) ? 0 : 2;
                raw_capture_info.binning_y = b; raw_capture_info.skipping_y = s;
                break;
            }
                break;
        }
        
        if (is_5D3)
        {
            /* update skip offsets */
            int skip_left, skip_right, skip_top, skip_bottom;
            calc_skip_offsets(&skip_left, &skip_right, &skip_top, &skip_bottom);
            raw_set_geometry(raw_info.width, raw_info.height, skip_left, skip_right, skip_top, skip_bottom);
        }
    }
    
    return 0;
}

static unsigned int crop_rec_init()
{
    is_digic4 = is_camera("DIGIC", "4");
    is_digic5 = is_camera("DIGIC", "5");
    
    if (is_camera("5D3",  "1.1.3") || is_camera("5D3", "1.2.3"))
    {
        /* same addresses on both 1.1.3 and 1.2.3 */
        CMOS_WRITE = 0x119CC;
        MEM_CMOS_WRITE = 0xE92D47F0;
        
        ADTG_WRITE = 0x11640;
        MEM_ADTG_WRITE = 0xE92D47F0;
        
        ENGIO_WRITE = is_camera("5D3", "1.2.3") ? 0xFF290F98 : 0xFF28CC3C;
        MEM_ENGIO_WRITE = 0xE51FC15C;
        
        is_5D3 = 1;
        crop_presets                = crop_presets_5d3;
        crop_rec_menu[0].choices    = crop_choices_5d3;
        crop_rec_menu[0].max        = COUNT(crop_choices_5d3) - 1;
        crop_rec_menu[0].help       = crop_choices_help_5d3;
        crop_rec_menu[0].help2      = crop_choices_help2_5d3;
        
        fps_main_clock = 24000000;
        /* 24p,  25p,  30p,  50p,  60p,   x5 */
        memcpy(default_timerA, (int[]) {  440,  480,  440,  480,  440,  518 }, 24);
        memcpy(default_timerB, (int[]) { 2275, 2000, 1820, 1000,  910, 1556 }, 24);
    }
    
    /* FPS in x5 zoom may be model-dependent; assume exact */
    default_fps_1k[5] = (uint64_t) fps_main_clock * 1000ULL / default_timerA[5] / default_timerB[5];
    
    printf("[crop_rec] checking FPS timer values...\n");
    for (int i = 0; i < COUNT(default_fps_1k); i++)
    {
        if (default_timerA[i])
        {
            int fps_i = (uint64_t) fps_main_clock * 1000ULL / default_timerA[i] / default_timerB[i];
            if (fps_i == default_fps_1k[i])
            {
                printf("%d) %s%d.%03d: A=%d B=%d (exact)\n", i, FMT_FIXEDPOINT3(default_fps_1k[i]), default_timerA[i], default_timerB[i]);
                
                if (i == 5 && default_fps_1k[i] != 29970)
                {
                    printf("-> unusual FPS in x5 zoom\n", i);
                }
            }
            else
            {
                int fps_p = (uint64_t) fps_main_clock * 1000ULL / default_timerA[i] / (default_timerB[i] + 1);
                if (fps_i > default_fps_1k[i] && fps_p < default_fps_1k[i])
                {
                    printf("%d) %s%d.%03d: A=%d B=%d/%d (averaged)\n", i, FMT_FIXEDPOINT3(default_fps_1k[i]), default_timerA[i], default_timerB[i], default_timerB[i] + 1);
                }
                else
                {
                    printf("%d) %s%d.%03d: A=%d B=%d (%s%d.%03d ?!?)\n", i, FMT_FIXEDPOINT3(default_fps_1k[i]), default_timerA[i], default_timerB[i], FMT_FIXEDPOINT3(fps_i));
                    return CBR_RET_ERROR;
                }
                
                /* assume 25p is exact on all models */
                if (i == 1)
                {
                    printf("-> 25p check error\n");
                    return CBR_RET_ERROR;
                }
            }
        }
    }
    
    menu_add("Movie", crop_rec_menu, COUNT(crop_rec_menu));
    menu_add("Movie", shutter_range_menu, COUNT(shutter_range_menu));
    menu_add("Movie", bitdepth_menu, COUNT(bitdepth_menu));
    menu_add("Movie", ratios_menu, COUNT(ratios_menu));
    lvinfo_add_items (info_items, COUNT(info_items));
    
    return 0;
}

static unsigned int crop_rec_deinit()
{
    return 0;
}

MODULE_INFO_START()
MODULE_INIT(crop_rec_init)
MODULE_DEINIT(crop_rec_deinit)
MODULE_INFO_END()

MODULE_CONFIGS_START()
MODULE_CONFIG(crop_preset_index)
MODULE_CONFIG(shutter_range)
MODULE_CONFIG(bitdepth)
MODULE_CONFIG(ratios)
MODULE_CONFIG(x3crop)
MODULE_CONFIG(set_25fps)
MODULE_CONFIG(framestop)
MODULE_CONFIG(frameburst)
MODULE_CONFIG(isoaverage)
MODULE_CONFIG(isoauto)
MODULE_CONFIG(isoclimb)
MODULE_CONFIG(zoomaid)
MODULE_CONFIGS_END()

MODULE_CBRS_START()
MODULE_CBR(CBR_SHOOT_TASK, crop_rec_polling_cbr, 0)
MODULE_CBR(CBR_RAW_INFO_UPDATE, raw_info_update_cbr, 0)
MODULE_CBR(CBR_KEYPRESS, crop_rec_keypress_cbr, 0)
MODULE_CBRS_END()

MODULE_PROPHANDLERS_START()
MODULE_PROPHANDLER(PROP_LV_ACTION)
MODULE_PROPHANDLER(PROP_LV_DISPSIZE)
MODULE_PROPHANDLERS_END()
