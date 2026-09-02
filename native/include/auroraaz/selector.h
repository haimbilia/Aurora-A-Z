#ifndef AURORAAZ_SELECTOR_H
#define AURORAAZ_SELECTOR_H

#include <stdint.h>

#include <auroraaz/filters.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef enum AzSelectorMode {
    AZ_MODE_COVERFLOW = 0,
    AZ_MODE_SELECTING
} AzSelectorMode;

typedef enum AzSelectorCommand {
    AZ_COMMAND_NONE = 0,
    AZ_COMMAND_ENTER,
    AZ_COMMAND_PREVIOUS,
    AZ_COMMAND_NEXT,
    AZ_COMMAND_APPLY
} AzSelectorCommand;

typedef enum AzEdgeBehavior {
    AZ_EDGE_CLAMP = 0,
    AZ_EDGE_WRAP
} AzEdgeBehavior;

typedef struct AzSelectorState {
    AzSelectorMode mode;
    uint8_t selected_index;
    uint8_t applied_index;
} AzSelectorState;

typedef struct AzSelectorResult {
    uint8_t handled;
    uint8_t request_filter;
    uint8_t filter_index;
} AzSelectorResult;

void az_selector_init(AzSelectorState *state);
void az_selector_leave_coverflow(AzSelectorState *state);

AzSelectorResult az_selector_dispatch(
    AzSelectorState *state,
    AzSelectorCommand command,
    AzEdgeBehavior edge_behavior,
    uint8_t coverflow_active);

#ifdef __cplusplus
}
#endif

#endif
