#include <cstdint>
#include <xcb/xproto.h>

static constexpr xcb_mod_mask_t MOD {XCB_MOD_MASK_4}; /* modifier for mouse resize and move */

/* INNER == BORDERWIDTH - OUTER */
static constexpr uint32_t BORDERWIDTH {4}; /*  full  border width */
static constexpr uint16_t OUTER {1};       /*  outer border width */

/* colors */
static constexpr uint32_t FOCUSCOL {0xD9AA55};
static constexpr uint32_t UNFOCUSCOL {0x4B384C};
static constexpr uint32_t OUTERCOL {0x000000};
