/* Aurora A-Z DashLaunch installer. */

#include <stdint.h>

#include <xecore/xam.h>
#include <xecore/xboxkrnl.h>

typedef uint8_t AzBool;

extern AzBool CopyFileA(char *source, char *destination, AzBool fail_if_exists);
extern void XNotifyQueueUI(uint32_t type, uint32_t user_index, uint32_t area,
    const uint16_t *text, uint64_t parameter);

#define AZ_INI_MAX_SIZE 32768u

static char g_payload_path[] = "game:\\AuroraAZ.xex";
static char g_plugin_path[] = "Hdd:\\Aurora\\Plugins\\AuroraAZ.xex";
static char g_launch_ini_path[] = "Hdd:\\launch.ini";
static char g_launch_ini_backup_path[] = "Hdd:\\launch.ini.AuroraAZ.backup";
static char g_launch_ini_stage_path[] = "Hdd:\\launch.ini.AuroraAZ.stage";
static uint8_t g_ini_input[AZ_INI_MAX_SIZE];
static uint8_t g_ini_output[AZ_INI_MAX_SIZE];
static const uint8_t g_plugin_value[] =
    "Hdd:\\Aurora\\Plugins\\AuroraAZ.xex";

static const uint16_t g_success[] = {
    'A','u','r','o','r','a',' ','A','-','Z',' ','i','n','s','t','a','l','l','e','d',
    '.',' ','D','a','s','h','L','a','u','n','c','h',' ','p','l','u','g','i','n',' ',
    's','l','o','t',' ','s','e','t','.',' ','R','e','b','o','o','t',' ','A','u','r','o','r','a','.',0
};
static const uint16_t g_no_slot[] = {
    'A','u','r','o','r','a',' ','A','-','Z',' ','n','o','t',' ','i','n','s','t','a','l','l','e','d',
    ':',' ','n','o',' ','e','m','p','t','y',' ','D','a','s','h','L','a','u','n','c','h',' ','p','l','u','g','i','n',' ','s','l','o','t','.',0
};
static const uint16_t g_failure[] = {
    'A','u','r','o','r','a',' ','A','-','Z',' ','i','n','s','t','a','l','l',' ','f','a','i','l','e','d',
    '.',' ','l','a','u','n','c','h','.','i','n','i',' ','w','a','s',' ','n','o','t',' ','c','h','a','n','g','e','d','.',0
};

static uint8_t is_space(uint8_t value)
{
    return value == ' ' || value == '\t' ? 1u : 0u;
}

static uint8_t append_bytes(uint32_t *written, const uint8_t *bytes,
    uint32_t size)
{
    uint32_t index;

    if (written == NULL || bytes == NULL || *written > AZ_INI_MAX_SIZE ||
        size > AZ_INI_MAX_SIZE - *written) {
        return 0u;
    }
    for (index = 0u; index < size; ++index) {
        g_ini_output[*written + index] = bytes[index];
    }
    *written += size;
    return 1u;
}

static uint8_t write_complete(const char *path, const uint8_t *bytes,
    uint32_t size)
{
    HANDLE file;
    uint32_t written = 0u;

    file = CreateFileA((char *)path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == NULL || file == INVALID_HANDLE_VALUE) {
        return 0u;
    }
    if (WriteFile(file, (void *)bytes, size, &written, NULL) == 0 ||
        written != size || CloseHandle(file) == 0) {
        return 0u;
    }
    return 1u;
}

static uint8_t read_launch_ini(uint32_t *size)
{
    HANDLE file;
    uint32_t read = 0u;

    if (size == NULL) {
        return 0u;
    }
    file = CreateFileA(g_launch_ini_path, GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == NULL || file == INVALID_HANDLE_VALUE) {
        return 0u;
    }
    if (ReadFile(file, g_ini_input, AZ_INI_MAX_SIZE - 1u, &read, NULL) == 0 ||
        CloseHandle(file) == 0 || read == 0u || read >= AZ_INI_MAX_SIZE - 1u) {
        return 0u;
    }
    *size = read;
    return 1u;
}

static uint8_t line_is_empty_plugin(const uint8_t *line, uint32_t size,
    uint8_t *slot)
{
    uint32_t index = 0u;

    while (index < size && is_space(line[index]) != 0u) {
        ++index;
    }
    if (index + 7u > size || line[index] != 'p' || line[index + 1u] != 'l' ||
        line[index + 2u] != 'u' || line[index + 3u] != 'g' ||
        line[index + 4u] != 'i' || line[index + 5u] != 'n' ||
        line[index + 6u] < '1' || line[index + 6u] > '5') {
        return 0u;
    }
    *slot = (uint8_t)(line[index + 6u] - '0');
    index += 7u;
    while (index < size && is_space(line[index]) != 0u) {
        ++index;
    }
    if (index >= size || line[index++] != '=') {
        return 0u;
    }
    while (index < size && is_space(line[index]) != 0u) {
        ++index;
    }
    return index == size ? 1u : 0u;
}

static uint8_t prepare_launch_ini(uint32_t input_size, uint32_t *output_size)
{
    uint32_t start = 0u;
    uint32_t written = 0u;
    uint8_t installed = 0u;

    while (start < input_size) {
        uint32_t end = start;
        uint32_t content_end;
        uint8_t slot = 0u;

        while (end < input_size && g_ini_input[end] != '\n') {
            ++end;
        }
        content_end = end;
        if (content_end > start && g_ini_input[content_end - 1u] == '\r') {
            --content_end;
        }
        if (installed == 0u && line_is_empty_plugin(g_ini_input + start,
                content_end - start, &slot) != 0u) {
            uint8_t line_prefix[] = "plugin1 = ";

            line_prefix[6u] = (uint8_t)('0' + slot);
            if (append_bytes(&written, line_prefix,
                    (uint32_t)sizeof(line_prefix) - 1u) == 0u ||
                append_bytes(&written, g_plugin_value,
                    (uint32_t)sizeof(g_plugin_value) - 1u) == 0u ||
                (content_end < end && append_bytes(&written,
                    (const uint8_t *)"\r", 1u) == 0u) ||
                (end < input_size && append_bytes(&written,
                    (const uint8_t *)"\n", 1u) == 0u)) {
                return 0u;
            }
            installed = 1u;
        } else if (append_bytes(&written, g_ini_input + start,
                (end < input_size ? end + 1u : end) - start) == 0u) {
            return 0u;
        }
        start = end < input_size ? end + 1u : end;
    }
    if (installed == 0u) {
        return 0u;
    }
    *output_size = written;
    return 1u;
}

int main(void)
{
    uint32_t input_size;
    uint32_t output_size;

    if (read_launch_ini(&input_size) == 0u ||
        prepare_launch_ini(input_size, &output_size) == 0u) {
        XNotifyQueueUI(0u, 0u, 0u, g_no_slot, 0u);
        return 1;
    }
    if (CopyFileA(g_payload_path, g_plugin_path, 0u) == 0u) {
        XNotifyQueueUI(0u, 0u, 0u, g_failure, 0u);
        return 1;
    }
    (void)DeleteFileA(g_launch_ini_backup_path);
    if (CopyFileA(g_launch_ini_path, g_launch_ini_backup_path, 0u) == 0u ||
        write_complete(g_launch_ini_stage_path, g_ini_output, output_size) == 0u ||
        CopyFileA(g_launch_ini_stage_path, g_launch_ini_path, 0u) == 0u) {
        (void)DeleteFileA(g_launch_ini_stage_path);
        XNotifyQueueUI(0u, 0u, 0u, g_failure, 0u);
        return 1;
    }
    (void)DeleteFileA(g_launch_ini_stage_path);
    XNotifyQueueUI(0u, 0u, 0u, g_success, 0u);
    return 0;
}
