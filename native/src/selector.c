#include <stddef.h>

#include <auroraaz/selector.h>

static AzSelectorResult unhandled_result(void)
{
    AzSelectorResult result = { 0u, 0u, AZ_NO_GLYPH };
    return result;
}

static AzSelectorResult handled_result(void)
{
    AzSelectorResult result = { 1u, 0u, AZ_NO_GLYPH };
    return result;
}

void az_selector_init(AzSelectorState *state)
{
    if (state == NULL) {
        return;
    }

    state->mode = AZ_MODE_COVERFLOW;
    state->selected_index = 0u;
    state->applied_index = AZ_NO_GLYPH;
    state->selection_changed = 0u;
    state->apply_serial = 0u;
}

void az_selector_leave_coverflow(AzSelectorState *state)
{
    if (state == NULL) {
        return;
    }

    state->mode = AZ_MODE_COVERFLOW;
    state->selected_index = 0u;
    state->selection_changed = 0u;
}

AzSelectorResult az_selector_dispatch(
    AzSelectorState *state,
    AzSelectorCommand command,
    AzEdgeBehavior edge_behavior,
    uint8_t coverflow_active)
{
    AzSelectorResult result;

    if (state == NULL) {
        return unhandled_result();
    }

    if (coverflow_active == 0u) {
        az_selector_leave_coverflow(state);
        return unhandled_result();
    }

    if (state->mode != AZ_MODE_COVERFLOW &&
        state->mode != AZ_MODE_SELECTING) {
        return unhandled_result();
    }

    if (state->mode == AZ_MODE_COVERFLOW) {
        if (command != AZ_COMMAND_ENTER) {
            return unhandled_result();
        }

        state->mode = AZ_MODE_SELECTING;
        state->selected_index = 0u;
        state->selection_changed = 0u;
        return handled_result();
    }

    if (state->selected_index >= AZ_GLYPH_COUNT) {
        return unhandled_result();
    }

    switch (command) {
    case AZ_COMMAND_PREVIOUS:
        if (state->selected_index > 0u) {
            --state->selected_index;
            state->selection_changed = 1u;
        }
        else if (edge_behavior == AZ_EDGE_WRAP) {
            state->selected_index = (uint8_t)(AZ_GLYPH_COUNT - 1u);
            state->selection_changed = 1u;
        }
        return handled_result();

    case AZ_COMMAND_NEXT:
        if (state->selected_index + 1u < AZ_GLYPH_COUNT) {
            ++state->selected_index;
            state->selection_changed = 1u;
        }
        else if (edge_behavior == AZ_EDGE_WRAP) {
            state->selected_index = 0u;
            state->selection_changed = 1u;
        }
        return handled_result();

    case AZ_COMMAND_APPLY:
        state->mode = AZ_MODE_COVERFLOW;
        result = handled_result();
        if (state->selection_changed == 0u) {
            return result;
        }
        state->selection_changed = 0u;
        state->applied_index = state->selected_index;
        ++state->apply_serial;
        result.request_filter = 1u;
        result.filter_index = state->applied_index;
        return result;

    case AZ_COMMAND_ENTER:
        return handled_result();

    case AZ_COMMAND_NONE:
    default:
        return unhandled_result();
    }
}
