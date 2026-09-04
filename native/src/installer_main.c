/*
 * Aurora A-Z on-console installer.
 *
 * This is deliberately a small, ordinary title XEX.  It is launched from
 * Aurora's file browser with AuroraAZ.xex beside it.  The installed plugin is
 * not loaded until Aurora is rebooted.
 */

#include <stdint.h>

typedef uint8_t AzBool;

/* xam exports CopyFileA (ordinal 1104), DeleteFileA (1096), and
 * XNotifyQueueUI (656).  xecorelib currently does not declare the first and
 * last of these prototypes. */
extern AzBool CopyFileA(char *source, char *destination, AzBool fail_if_exists);
extern AzBool DeleteFileA(char *path);
extern void XNotifyQueueUI(
    uint32_t type,
    uint32_t user_index,
    uint32_t area,
    const uint16_t *text,
    uint64_t parameter);

static char g_payload_path[] = "game:\\AuroraAZ.xex";
static char g_live_path[] = "Hdd:\\Aurora\\Plugins\\NetDbgDll.xex";
static char g_backup_path[] =
    "Hdd:\\Aurora\\Plugins\\NetDbgDll.xex.before-aurora-az";
static char g_stage_path[] =
    "Hdd:\\Aurora\\Plugins\\NetDbgDll.xex.auroraaz-staged";

static const uint16_t g_success[] = {
    'A','u','r','o','r','a',' ','A','-','Z',' ','i','n','s','t','a','l','l','e','d',
    '.',' ','R','e','b','o','o','t',' ','A','u','r','o','r','a',' ','t','o',' ',
    'a','c','t','i','v','a','t','e','.',0
};
static const uint16_t g_failure[] = {
    'A','u','r','o','r','a',' ','A','-','Z',' ','i','n','s','t','a','l','l',' ',
    'f','a','i','l','e','d','.',' ','K','e','e','p',' ','A','u','r','o','r','a','Z',
    '.','x','e','x',' ','b','e','s','i','d','e',' ','t','h','i','s',' ','i','n','s',
    't','a','l','l','e','r','.',0
};

int main(void)
{
    /* Stage before touching the active slot.  A previous slot occupant is
     * preserved under .before-aurora-az; absence simply means this is a first
     * installation. */
    (void)DeleteFileA(g_stage_path);
    if (CopyFileA(g_payload_path, g_stage_path, 0u) == 0u) {
        XNotifyQueueUI(0u, 0u, 0u, g_failure, 0u);
        return 1;
    }
    (void)CopyFileA(g_live_path, g_backup_path, 0u);
    if (CopyFileA(g_stage_path, g_live_path, 0u) == 0u) {
        (void)DeleteFileA(g_stage_path);
        XNotifyQueueUI(0u, 0u, 0u, g_failure, 0u);
        return 1;
    }
    (void)DeleteFileA(g_stage_path);
    XNotifyQueueUI(0u, 0u, 0u, g_success, 0u);
    return 0;
}
