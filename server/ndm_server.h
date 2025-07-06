#include <linux/module.h>
#include <net/genetlink.h>

/* attributes */
enum {
    NDS_SERVER_A_UNSPEC,
    DATA,
    __NDS_SERVER_A_MAX 
};
#define NDS_SERVER_A_MAX  (__NDS_SERVER_A_MAX - 1)
/* commands */
enum {
    CALCULATE_FROM_JSON,
    __NDS_SERVER_C_MAX
};
#define NDS_SERVER_C_MAX (__NDS_SERVER_C_MAX - 1)

/* family */

#define FAMILY_NAME "NDM_Family"
#define FAMILY_VERSION 1
