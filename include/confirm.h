/**
 * @file confirm.h
 * @brief Hardware-independent state machine for a [No] [Yes] confirmation.
 *
 * The default selection is always "No"; only an explicit OK on "Yes"
 * confirms. Back always cancels.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DhikrConfirmChoiceNo,
    DhikrConfirmChoiceYes,
} DhikrConfirmChoice;

typedef enum {
    DhikrConfirmInputLeft,
    DhikrConfirmInputRight,
    DhikrConfirmInputOk,
    DhikrConfirmInputBack,
} DhikrConfirmInput;

typedef enum {
    DhikrConfirmPending,
    DhikrConfirmAccepted,
    DhikrConfirmCancelled,
} DhikrConfirmOutcome;

typedef struct {
    DhikrConfirmChoice selection;
} DhikrConfirm;

/** Start a new confirmation with "No" selected. */
void dhikr_confirm_begin(DhikrConfirm* confirm);

/** Feed one input event; returns whether the dialog is finished. */
DhikrConfirmOutcome dhikr_confirm_handle(DhikrConfirm* confirm, DhikrConfirmInput input);

#ifdef __cplusplus
}
#endif
